/*
 * Copyright (c) 2023 AzerothCore
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 */

#include "WorldPartyScaling.h"

#include "Creature.h"
#include "Formulas.h"
#include "Group.h"
#include "GroupReference.h"
#include "Log.h"
#include "Map.h"
#include "MapMgr.h"
#include "ObjectGuid.h"
#include "Player.h"
#include "ScriptMgr.h"
#include "SharedDefines.h"
#include "SpellAuraDefines.h"
#include "SpellInfo.h"
#include "Tokenize.h"
#include "World.h"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <functional>
#include <limits>
#include <mutex>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace
{
constexpr uint32 MaxGroupSize = WorldPartyScalingConfig::MaxGroupSize;
constexpr uint32 MaxScaledHealth = 1u << 30;
constexpr int32 MaxScaledDamage = std::numeric_limits<int32>::max() / 2;

bool IsAsciiWhitespace(char character)
{
    return character == ' ' || character == '\t' || character == '\n' || character == '\r'
        || character == '\f' || character == '\v';
}

bool IsPositiveHealth(float const& value)
{
    return std::isfinite(value) && value > 0.0f;
}

bool IsNonNegativeDamage(float const& value)
{
    return std::isfinite(value) && value >= 0.0f;
}

bool IsNonNegativeXp(float const& value)
{
    return std::isfinite(value) && value >= 0.0f;
}

struct ActiveScaling
{
    float originalBaseFlat = 0.0f;
    uint32 referenceMaxHealth = 0;
    uint32 scaledBaseMaxHealth = 0;
    float oldHealthRate = 1.0f;
    float oldMeleeRate = 1.0f;
    float oldSpellRate = 1.0f;
    float healthCoefficient = 1.0f;
    float damageCoefficient = 1.0f;
    uint8 groupSize = 1;
};

// ObjectGuid counters are map-scoped, so map and instance are part of the key to avoid aliasing.
struct ScalingKey
{
    uint32 mapId = 0;
    uint32 instanceId = 0;
    ObjectGuid guid;

    bool operator==(ScalingKey const& other) const = default;
};

struct ScalingKeyHash
{
    std::size_t operator()(ScalingKey const& key) const
    {
        std::size_t seed = std::hash<ObjectGuid>()(key.guid);
        seed ^= std::hash<uint32>()(key.mapId) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        seed ^= std::hash<uint32>()(key.instanceId) + 0x9e3779b9u + (seed << 6) + (seed >> 2);
        return seed;
    }
};

uint32 HealthFromPercentage(uint32 maxHealth, float healthPct)
{
    if (!maxHealth || healthPct <= 0.0f)
        return 0;

    double const value = std::clamp(double(maxHealth) * double(healthPct), 1.0, double(maxHealth));
    return static_cast<uint32>(std::llround(value));
}

uint32 ToPositiveHealth(double value)
{
    if (!(value >= 1.0))
        return 1;

    if (value >= double(MaxScaledHealth))
        return MaxScaledHealth;

    return static_cast<uint32>(std::llround(value));
}

double CoefficientFactor(float coefficient, float oldRate)
{
    if (!std::isfinite(coefficient) || coefficient < 0.0f)
        return 1.0;

    if (!std::isfinite(oldRate) || oldRate <= 0.0f)
        return double(coefficient);

    return double(coefficient) / double(oldRate);
}

bool IsWeaponDamageSpell(SpellInfo const* spellInfo)
{
    if (!spellInfo)
        return false;

    for (SpellEffectInfo const& effect : spellInfo->GetEffects())
    {
        if (effect.Effect == SPELL_EFFECT_WEAPON_DAMAGE || effect.Effect == SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL
            || effect.Effect == SPELL_EFFECT_NORMALIZED_WEAPON_DMG
            || effect.Effect == SPELL_EFFECT_WEAPON_PERCENT_DAMAGE)
            return true;
    }

    return false;
}

double SpellDamageFactor(ActiveScaling const& state, SpellInfo const* spellInfo)
{
    // Weapon-damage spells always carry the melee/ranged rate, even with the ignore-caster-modifiers attribute.
    // A mixed weapon + school-damage spell uses this melee factor for the aggregate; documented limitation.
    if (IsWeaponDamageSpell(spellInfo))
        return CoefficientFactor(state.damageCoefficient, state.oldMeleeRate);

    if (spellInfo->HasAttribute(SPELL_ATTR3_IGNORE_CASTER_MODIFIERS))
        return double(state.damageCoefficient);

    return CoefficientFactor(state.damageCoefficient, state.oldSpellRate);
}

bool HasUnambiguousPeriodicDamage(SpellInfo const* spellInfo)
{
    if (!spellInfo || !spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE))
        return false;

    // The hook does not expose the ticking aura effect, so skip spells whose ticks could be heals or special damage.
    return !spellInfo->HasAura(SPELL_AURA_PERIODIC_HEAL)
        && !spellInfo->HasAura(SPELL_AURA_OBS_MOD_HEALTH)
        && !spellInfo->HasAura(SPELL_AURA_PERIODIC_LEECH)
        && !spellInfo->HasAura(SPELL_AURA_PERIODIC_DAMAGE_PERCENT)
        && !spellInfo->HasAura(SPELL_AURA_PERIODIC_HEALTH_FUNNEL);
}

uint32 ScaleUnsignedDamage(uint32 damage, double factor)
{
    if (!damage)
        return 0;

    if (!std::isfinite(factor) || factor < 0.0)
        return std::min<uint32>(damage, static_cast<uint32>(MaxScaledDamage));

    double const scaled = std::clamp(double(damage) * factor, 1.0, double(MaxScaledDamage));
    return static_cast<uint32>(std::llround(scaled));
}

int32 ScaleSignedDamage(int32 damage, double factor)
{
    if (damage <= 0)
        return damage;

    if (!std::isfinite(factor) || factor < 0.0)
        return std::min<int32>(damage, MaxScaledDamage);

    double const scaled = std::clamp(double(damage) * factor, 1.0, double(MaxScaledDamage));
    return static_cast<int32>(std::llround(scaled));
}

Player* ResolvePlayer(Unit* unit)
{
    return unit ? unit->GetCharmerOrOwnerPlayerOrPlayerItself() : nullptr;
}

uint32 GetOnlineGroupSize(Player* player)
{
    if (!player)
        return 1;

    Group* group = player->GetGroup();
    if (!group)
        return 1;

    uint32 onlineMembers = 0;
    for (GroupReference* reference = group->GetFirstMember(); reference; reference = reference->next())
    {
        if (reference->GetSource())
            ++onlineMembers;
    }

    return std::clamp<uint32>(onlineMembers, 1, MaxGroupSize);
}

// Mirrors KillRewarder::_InitGroupData/_groupRate: only alive members in the loot recipient's group that are the
// killer or within group reward distance of the victim contribute, and the module only scales open-world maps.
float GetCoreGroupRate(Player* player, Creature* victim)
{
    if (!player || !victim)
        return 1.0f;

    Group* group = player->GetGroup();
    if (!group)
        return 1.0f;

    // Unit::Kill falls back to a group member in loot distance when the stored loot recipient is unavailable.
    // That fallback member is necessarily in group reward distance, so the current reward recipient is a safe
    // stand-in for the killer-count rule when the stored recipient cannot be used.
    Player* killer = victim->GetLootRecipient();
    if (!killer || !killer->IsInMap(victim))
        killer = player;

    uint32 count = 0;
    for (GroupReference* reference = group->GetFirstMember(); reference; reference = reference->next())
    {
        Player* member = reference->GetSource();
        if (!member)
            continue;

        if ((killer == member || member->IsAtGroupRewardDistance(victim)) && member->IsAlive())
            ++count;
    }

    float const rate = Acore::XP::xp_in_group_rate(count, false);
    return std::isfinite(rate) && rate > 0.0f ? rate : 1.0f;
}

bool IsCreatureEligible(Creature* creature, Player* engagingPlayer)
{
    if (!creature || !creature->IsAlive() || !creature->IsInWorld())
        return false;

    Map* map = creature->GetMap();
    if (!map || !map->IsWorldMap() || map->IsDungeon() || map->IsBattlegroundOrArena())
        return false;

    if (creature->IsPet() || creature->IsGuardian() || creature->IsTotem() || creature->IsSummon()
        || creature->IsControlledByPlayer() || creature->IsCharmed())
        return false;

    if (creature->IsGuard() || creature->IsCivilian() || creature->IsCritter())
        return false;

    if (creature->IsInEvadeMode())
        return false;

    if (creature->HasUnitFlag(UNIT_FLAG_NON_ATTACKABLE) || creature->HasUnitFlag(UNIT_FLAG_NOT_SELECTABLE))
        return false;

    if (g_WorldPartyScalingConfig.IsEntryExcluded(creature->GetEntry())
        || g_WorldPartyScalingConfig.IsMapExcluded(creature->GetMapId())
        || g_WorldPartyScalingConfig.IsZoneExcluded(creature->GetZoneId()))
        return false;

    if (engagingPlayer && creature->IsFriendlyTo(engagingPlayer))
        return false;

    return true;
}

float GetHealthRate(Creature const* creature)
{
    CreatureTemplate const* info = creature->GetCreatureTemplate();
    if (!info)
        return 1.0f;

    switch (info->rank)
    {
        case CREATURE_ELITE_NORMAL:
            return sWorld->getRate(RATE_CREATURE_NORMAL_HP);
        case CREATURE_ELITE_ELITE:
            return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_HP);
        case CREATURE_ELITE_RAREELITE:
            return sWorld->getRate(RATE_CREATURE_ELITE_RAREELITE_HP);
        case CREATURE_ELITE_WORLDBOSS:
            return sWorld->getRate(RATE_CREATURE_ELITE_WORLDBOSS_HP);
        case CREATURE_ELITE_RARE:
            return sWorld->getRate(RATE_CREATURE_ELITE_RARE_HP);
        default:
            return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_HP);
    }
}

float GetDamageRate(Creature const* creature)
{
    CreatureTemplate const* info = creature->GetCreatureTemplate();
    if (!info)
        return 1.0f;

    switch (info->rank)
    {
        case CREATURE_ELITE_NORMAL:
            return sWorld->getRate(RATE_CREATURE_NORMAL_DAMAGE);
        case CREATURE_ELITE_ELITE:
            return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_DAMAGE);
        case CREATURE_ELITE_RAREELITE:
            return sWorld->getRate(RATE_CREATURE_ELITE_RAREELITE_DAMAGE);
        case CREATURE_ELITE_WORLDBOSS:
            return sWorld->getRate(RATE_CREATURE_ELITE_WORLDBOSS_DAMAGE);
        case CREATURE_ELITE_RARE:
            return sWorld->getRate(RATE_CREATURE_ELITE_RARE_DAMAGE);
        default:
            return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_DAMAGE);
    }
}

float GetSpellDamageRate(Creature const* creature)
{
    CreatureTemplate const* info = creature->GetCreatureTemplate();
    if (!info)
        return 1.0f;

    switch (info->rank)
    {
        case CREATURE_ELITE_NORMAL:
            return sWorld->getRate(RATE_CREATURE_NORMAL_SPELLDAMAGE);
        case CREATURE_ELITE_ELITE:
            return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE);
        case CREATURE_ELITE_RAREELITE:
            return sWorld->getRate(RATE_CREATURE_ELITE_RAREELITE_SPELLDAMAGE);
        case CREATURE_ELITE_WORLDBOSS:
            return sWorld->getRate(RATE_CREATURE_ELITE_WORLDBOSS_SPELLDAMAGE);
        case CREATURE_ELITE_RARE:
            return sWorld->getRate(RATE_CREATURE_ELITE_RARE_SPELLDAMAGE);
        default:
            return sWorld->getRate(RATE_CREATURE_ELITE_ELITE_SPELLDAMAGE);
    }
}

ScalingKey GetScalingKey(Creature const* creature)
{
    return ScalingKey{ creature->GetMapId(), creature->GetInstanceId(), creature->GetGUID() };
}

void ApplyScaling(Creature* creature, ActiveScaling const& state, float healthPct)
{
    creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, static_cast<float>(state.scaledBaseMaxHealth));
    creature->UpdateMaxHealth();
    creature->SetHealth(HealthFromPercentage(creature->GetMaxHealth(), healthPct));
}

void RestoreScaling(Creature* creature, ActiveScaling const& state)
{
    uint32 const previousHealth = creature->GetHealth();
    uint32 const previousMaxHealth = creature->GetMaxHealth();
    float const healthPct = previousMaxHealth
        ? static_cast<float>(previousHealth) / static_cast<float>(previousMaxHealth)
        : 0.0f;

    creature->SetStatFlatModifier(UNIT_MOD_HEALTH, BASE_VALUE, state.originalBaseFlat);
    creature->UpdateMaxHealth();
    creature->SetHealth(HealthFromPercentage(creature->GetMaxHealth(), healthPct));
}

class WorldPartyScalingMgr
{
public:
    static WorldPartyScalingMgr& Instance()
    {
        static WorldPartyScalingMgr instance;
        return instance;
    }

    void OnCreatureEnterCombat(Creature* creature, Unit* victim);
    void OnCreatureExitCombat(Creature* creature);
    void OnCreatureDeath(Creature* creature);
    void OnCreatureRemoveWorld(Creature* creature);
    void OnConfigReloaded();

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage);
    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo);
    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo);
    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource);

private:
    WorldPartyScalingMgr() = default;

    [[nodiscard]] bool FindState(ScalingKey const& key, ActiveScaling& out) const;
    [[nodiscard]] bool FindOrStartState(Creature* creature, Unit* target, ActiveScaling& out);
    [[nodiscard]] bool TryStartScaling(Creature* creature, Player* engagingPlayer);
    void RestoreAndErase(Creature* creature);
    void LogStart(Creature* creature, ActiveScaling const& state) const;
    void LogRestore(Creature* creature, ActiveScaling const& state) const;

    mutable std::mutex _activeMutex;
    std::unordered_map<ScalingKey, ActiveScaling, ScalingKeyHash> _active;
};

bool WorldPartyScalingMgr::FindState(ScalingKey const& key, ActiveScaling& out) const
{
    std::lock_guard<std::mutex> lock(_activeMutex);
    auto const itr = _active.find(key);
    if (itr == _active.end())
        return false;

    out = itr->second;
    return true;
}

bool WorldPartyScalingMgr::TryStartScaling(Creature* creature, Player* engagingPlayer)
{
    if (!g_WorldPartyScalingConfig.Enabled() || !creature || !engagingPlayer)
        return false;

    ScalingKey const key = GetScalingKey(creature);

    if (!IsCreatureEligible(creature, engagingPlayer))
        return false;

    uint32 const groupSize = GetOnlineGroupSize(engagingPlayer);

    float const healthPct = creature->GetMaxHealth()
        ? static_cast<float>(creature->GetHealth()) / static_cast<float>(creature->GetMaxHealth())
        : 1.0f;

    ActiveScaling state;
    state.groupSize = static_cast<uint8>(groupSize);
    state.healthCoefficient = g_WorldPartyScalingConfig.HealthMultiplier(groupSize);
    state.damageCoefficient = g_WorldPartyScalingConfig.DamageMultiplier(groupSize);
    state.originalBaseFlat = creature->GetFlatModifierValue(UNIT_MOD_HEALTH, BASE_VALUE);
    state.oldHealthRate = GetHealthRate(creature);
    state.oldMeleeRate = GetDamageRate(creature);
    state.oldSpellRate = GetSpellDamageRate(creature);

    uint32 const createHealth = creature->GetCreateHealth();
    uint32 const referenceSource = createHealth ? createHealth : creature->GetMaxHealth();
    double const healthDivisor = state.oldHealthRate > 0.0f ? double(state.oldHealthRate) : 1.0;
    state.referenceMaxHealth = ToPositiveHealth(double(referenceSource) / healthDivisor);
    double const healthCoefficient = state.healthCoefficient > 0.0f ? double(state.healthCoefficient) : 1.0;
    state.scaledBaseMaxHealth = ToPositiveHealth(double(state.referenceMaxHealth) * healthCoefficient);

    {
        std::lock_guard<std::mutex> lock(_activeMutex);
        if (!g_WorldPartyScalingConfig.Enabled() || _active.contains(key))
            return false;

        if (!_active.emplace(key, state).second)
            return false;

        ApplyScaling(creature, state, healthPct);
    }

    LogStart(creature, state);

    return true;
}

bool WorldPartyScalingMgr::FindOrStartState(Creature* creature, Unit* target, ActiveScaling& out)
{
    if (!creature || !creature->IsInCombat())
        return false;

    ScalingKey const key = GetScalingKey(creature);

    if (FindState(key, out))
        return true;

    if (!TryStartScaling(creature, ResolvePlayer(target)))
        return false;

    return FindState(key, out);
}

void WorldPartyScalingMgr::OnCreatureEnterCombat(Creature* creature, Unit* victim)
{
    if (!creature)
        return;

    (void)TryStartScaling(creature, ResolvePlayer(victim));
}

void WorldPartyScalingMgr::OnCreatureExitCombat(Creature* creature)
{
    RestoreAndErase(creature);
}

void WorldPartyScalingMgr::OnCreatureDeath(Creature* creature)
{
    RestoreAndErase(creature);
}

void WorldPartyScalingMgr::OnCreatureRemoveWorld(Creature* creature)
{
    RestoreAndErase(creature);
}

void WorldPartyScalingMgr::OnConfigReloaded()
{
    if (g_WorldPartyScalingConfig.Enabled())
        return;

    std::vector<std::pair<ScalingKey, ActiveScaling>> snapshot;
    {
        std::lock_guard<std::mutex> lock(_activeMutex);
        snapshot.reserve(_active.size());
        for (auto const& entry : _active)
            snapshot.emplace_back(entry.first, entry.second);

        _active.clear();
    }

    for (auto const& entry : snapshot)
    {
        ScalingKey const& key = entry.first;
        Map* map = sMapMgr->FindMap(key.mapId, key.instanceId);
        if (!map)
            continue;

        Creature* creature = map->GetCreature(key.guid);
        if (!creature)
            continue;

        RestoreScaling(creature, entry.second);
        LogRestore(creature, entry.second);
    }
}

void WorldPartyScalingMgr::ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage)
{
    if (!damage || !g_WorldPartyScalingConfig.Enabled())
        return;

    Creature* creature = attacker ? attacker->ToCreature() : nullptr;
    if (!creature)
        return;

    ActiveScaling state;
    if (!FindOrStartState(creature, target, state))
        return;

    damage = ScaleUnsignedDamage(damage, CoefficientFactor(state.damageCoefficient, state.oldMeleeRate));
}

void WorldPartyScalingMgr::ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage,
    SpellInfo const* spellInfo)
{
    if (damage <= 0 || !spellInfo || !g_WorldPartyScalingConfig.Enabled())
        return;

    Creature* creature = attacker ? attacker->ToCreature() : nullptr;
    if (!creature)
        return;

    ActiveScaling state;
    if (!FindOrStartState(creature, target, state))
        return;

    damage = ScaleSignedDamage(damage, SpellDamageFactor(state, spellInfo));
}

void WorldPartyScalingMgr::ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
    SpellInfo const* spellInfo)
{
    if (!damage || !g_WorldPartyScalingConfig.Enabled() || !HasUnambiguousPeriodicDamage(spellInfo))
        return;

    Creature* creature = attacker ? attacker->ToCreature() : nullptr;
    if (!creature)
        return;

    ActiveScaling state;
    if (!FindOrStartState(creature, target, state))
        return;

    double const factor = spellInfo->HasAttribute(SPELL_ATTR3_IGNORE_CASTER_MODIFIERS)
        ? double(state.damageCoefficient)
        : CoefficientFactor(state.damageCoefficient, state.oldSpellRate);

    damage = ScaleUnsignedDamage(damage, factor);
}

void WorldPartyScalingMgr::OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource)
{
    if (!amount || xpSource != PlayerXPSource::XPSOURCE_KILL || !player || !victim
        || !g_WorldPartyScalingConfig.Enabled())
        return;

    Creature* creature = victim->ToCreature();
    if (!creature)
        return;

    ActiveScaling state;
    if (!FindState(GetScalingKey(creature), state))
        return;

    float const desired = g_WorldPartyScalingConfig.XpMultiplier(state.groupSize);
    if (!(desired > 0.0f))
    {
        amount = 0;
        return;
    }

    // The core already applied its party-size rate to this amount; divide it out so the configured coefficient is
    // the net multiplier versus the solo baseline, then clamp to a valid non-zero XP value.
    double const coreRate = double(GetCoreGroupRate(player, creature));
    double const scaled = double(amount) * double(desired) / coreRate;
    double const upperBound = double(std::numeric_limits<uint32>::max());
    amount = static_cast<uint32>(std::llround(std::clamp(scaled, 1.0, upperBound)));
}

void WorldPartyScalingMgr::RestoreAndErase(Creature* creature)
{
    if (!creature)
        return;

    ScalingKey const key = GetScalingKey(creature);
    ActiveScaling state;

    {
        std::lock_guard<std::mutex> lock(_activeMutex);
        auto const itr = _active.find(key);
        if (itr == _active.end())
            return;

        state = itr->second;
        _active.erase(itr);
    }

    RestoreScaling(creature, state);
    LogRestore(creature, state);
}

void WorldPartyScalingMgr::LogStart(Creature* creature, ActiveScaling const& state) const
{
    if (!g_WorldPartyScalingConfig.DebugEnabled())
        return;

    LOG_DEBUG("module",
        "mod-world-party-scaling: scaling entry {} guid {} for online group size {} (tier {}): health x{:.2f} "
        "base flat {:.2f} -> {} (reference {}, resulting max {}), melee x{:.2f} (old rate {:.2f}), "
        "spell x{:.2f} (old rate {:.2f}), xp x{:.2f}, old health rate {:.2f}",
        creature->GetEntry(), creature->GetGUID().ToString(), static_cast<uint32>(state.groupSize),
        static_cast<uint32>(state.groupSize), state.healthCoefficient, state.originalBaseFlat,
        state.scaledBaseMaxHealth, state.referenceMaxHealth, creature->GetMaxHealth(),
        state.damageCoefficient, state.oldMeleeRate, state.damageCoefficient, state.oldSpellRate,
        g_WorldPartyScalingConfig.XpMultiplier(state.groupSize), state.oldHealthRate);
}

void WorldPartyScalingMgr::LogRestore(Creature* creature, ActiveScaling const& state) const
{
    if (!g_WorldPartyScalingConfig.DebugEnabled())
        return;

    LOG_DEBUG("module",
        "mod-world-party-scaling: restored entry {} guid {} to base flat {:.2f} (scaled base was {}, reference {}, "
        "online group size {} (tier {}), health x{:.2f}, damage x{:.2f}, xp x{:.2f}), resulting max {}",
        creature->GetEntry(), creature->GetGUID().ToString(), state.originalBaseFlat, state.scaledBaseMaxHealth,
        state.referenceMaxHealth, static_cast<uint32>(state.groupSize), static_cast<uint32>(state.groupSize),
        state.healthCoefficient, state.damageCoefficient,
        g_WorldPartyScalingConfig.XpMultiplier(state.groupSize), creature->GetMaxHealth());
}
}

WorldPartyScalingConfig g_WorldPartyScalingConfig;

WorldPartyScalingConfig::WorldPartyScalingConfig() : ConfigValueCache(WorldPartyScalingConfigKey::Count)
{
}

void WorldPartyScalingConfig::BuildConfigCache()
{
    SetConfigValue<bool>(WorldPartyScalingConfigKey::Enable, "WorldPartyScaling.Enable", true);
    SetConfigValue<bool>(WorldPartyScalingConfigKey::EnableDebug, "WorldPartyScaling.EnableDebug", false);

    constexpr std::array<WorldPartyScalingConfigKey, MaxGroupSize> HealthKeys = {
        WorldPartyScalingConfigKey::Group1Health,
        WorldPartyScalingConfigKey::Group2Health,
        WorldPartyScalingConfigKey::Group3Health,
        WorldPartyScalingConfigKey::Group4Health,
        WorldPartyScalingConfigKey::Group5Health,
        WorldPartyScalingConfigKey::Group6Health
    };
    constexpr std::array<std::string_view, MaxGroupSize> HealthNames = {
        "WorldPartyScaling.Group1.Health",
        "WorldPartyScaling.Group2.Health",
        "WorldPartyScaling.Group3.Health",
        "WorldPartyScaling.Group4.Health",
        "WorldPartyScaling.Group5.Health",
        "WorldPartyScaling.Group6.Health"
    };
    constexpr std::array<float, MaxGroupSize> HealthDefaults = { 1.00f, 1.70f, 2.50f, 3.30f, 4.20f, 5.20f };

    constexpr std::array<WorldPartyScalingConfigKey, MaxGroupSize> DamageKeys = {
        WorldPartyScalingConfigKey::Group1Damage,
        WorldPartyScalingConfigKey::Group2Damage,
        WorldPartyScalingConfigKey::Group3Damage,
        WorldPartyScalingConfigKey::Group4Damage,
        WorldPartyScalingConfigKey::Group5Damage,
        WorldPartyScalingConfigKey::Group6Damage
    };
    constexpr std::array<std::string_view, MaxGroupSize> DamageNames = {
        "WorldPartyScaling.Group1.Damage",
        "WorldPartyScaling.Group2.Damage",
        "WorldPartyScaling.Group3.Damage",
        "WorldPartyScaling.Group4.Damage",
        "WorldPartyScaling.Group5.Damage",
        "WorldPartyScaling.Group6.Damage"
    };
    constexpr std::array<float, MaxGroupSize> DamageDefaults = { 1.00f, 1.35f, 1.50f, 1.65f, 1.80f, 1.95f };

    constexpr std::array<WorldPartyScalingConfigKey, MaxGroupSize> XpKeys = {
        WorldPartyScalingConfigKey::Group1Xp,
        WorldPartyScalingConfigKey::Group2Xp,
        WorldPartyScalingConfigKey::Group3Xp,
        WorldPartyScalingConfigKey::Group4Xp,
        WorldPartyScalingConfigKey::Group5Xp,
        WorldPartyScalingConfigKey::Group6Xp
    };
    constexpr std::array<std::string_view, MaxGroupSize> XpNames = {
        "WorldPartyScaling.Group1.XP",
        "WorldPartyScaling.Group2.XP",
        "WorldPartyScaling.Group3.XP",
        "WorldPartyScaling.Group4.XP",
        "WorldPartyScaling.Group5.XP",
        "WorldPartyScaling.Group6.XP"
    };
    constexpr std::array<float, MaxGroupSize> XpDefaults = { 1.00f, 1.10f, 1.20f, 1.30f, 1.40f, 1.50f };

    for (uint32 tier = 0; tier < MaxGroupSize; ++tier)
    {
        SetConfigValue<float>(HealthKeys[tier], std::string(HealthNames[tier]), HealthDefaults[tier],
            ConfigValueCache::Reloadable::Yes, IsPositiveHealth, "> 0");
        _healthMultipliers[tier] = GetConfigValue<float>(HealthKeys[tier]);

        SetConfigValue<float>(DamageKeys[tier], std::string(DamageNames[tier]), DamageDefaults[tier],
            ConfigValueCache::Reloadable::Yes, IsNonNegativeDamage, ">= 0");
        _damageMultipliers[tier] = GetConfigValue<float>(DamageKeys[tier]);

        SetConfigValue<float>(XpKeys[tier], std::string(XpNames[tier]), XpDefaults[tier],
            ConfigValueCache::Reloadable::Yes, IsNonNegativeXp, ">= 0");
        _xpMultipliers[tier] = GetConfigValue<float>(XpKeys[tier]);
    }

    SetConfigValue<std::string>(WorldPartyScalingConfigKey::ExcludedCreatureEntries,
        "WorldPartyScaling.ExcludedCreatureEntries", "");
    SetConfigValue<std::string>(WorldPartyScalingConfigKey::ExcludedMaps, "WorldPartyScaling.ExcludedMaps", "");
    SetConfigValue<std::string>(WorldPartyScalingConfigKey::ExcludedZones, "WorldPartyScaling.ExcludedZones", "");

    RebuildIdSets();
}

void WorldPartyScalingConfig::RebuildIdSets()
{
    _excludedEntries = ParseIdSet(GetConfigValue(WorldPartyScalingConfigKey::ExcludedCreatureEntries));
    _excludedMaps = ParseIdSet(GetConfigValue(WorldPartyScalingConfigKey::ExcludedMaps));
    _excludedZones = ParseIdSet(GetConfigValue(WorldPartyScalingConfigKey::ExcludedZones));
}

std::unordered_set<uint32> WorldPartyScalingConfig::ParseIdSet(std::string_view value)
{
    std::unordered_set<uint32> ids;

    for (std::string_view token : Acore::Tokenize(value, ',', false))
    {
        while (!token.empty() && IsAsciiWhitespace(token.front()))
            token.remove_prefix(1);

        while (!token.empty() && IsAsciiWhitespace(token.back()))
            token.remove_suffix(1);

        if (token.empty())
            continue;

        uint32 id = 0;
        std::from_chars_result const parsed = std::from_chars(token.data(), token.data() + token.size(), id);
        if (parsed.ec != std::errc() || parsed.ptr != token.data() + token.size())
            continue;

        ids.insert(id);
    }

    return ids;
}

bool WorldPartyScalingConfig::Enabled() const
{
    return GetConfigValue<bool>(WorldPartyScalingConfigKey::Enable);
}

bool WorldPartyScalingConfig::DebugEnabled() const
{
    return GetConfigValue<bool>(WorldPartyScalingConfigKey::EnableDebug);
}

float WorldPartyScalingConfig::HealthMultiplier(uint32 groupSize) const
{
    uint32 const index = std::clamp<uint32>(groupSize, 1, MaxGroupSize) - 1;
    return _healthMultipliers[index];
}

float WorldPartyScalingConfig::DamageMultiplier(uint32 groupSize) const
{
    uint32 const index = std::clamp<uint32>(groupSize, 1, MaxGroupSize) - 1;
    return _damageMultipliers[index];
}

float WorldPartyScalingConfig::XpMultiplier(uint32 groupSize) const
{
    uint32 const index = std::clamp<uint32>(groupSize, 1, MaxGroupSize) - 1;
    return _xpMultipliers[index];
}

bool WorldPartyScalingConfig::IsEntryExcluded(uint32 entry) const
{
    return _excludedEntries.contains(entry);
}

bool WorldPartyScalingConfig::IsMapExcluded(uint32 mapId) const
{
    return _excludedMaps.contains(mapId);
}

bool WorldPartyScalingConfig::IsZoneExcluded(uint32 zoneId) const
{
    return _excludedZones.contains(zoneId);
}

namespace
{
class WorldPartyScalingWorldScript final : public WorldScript
{
public:
    WorldPartyScalingWorldScript()
        : WorldScript("WorldPartyScalingWorldScript",
            { WORLDHOOK_ON_BEFORE_CONFIG_LOAD, WORLDHOOK_ON_AFTER_CONFIG_LOAD })
    {
    }

    void OnBeforeConfigLoad(bool reload) override
    {
        g_WorldPartyScalingConfig.Initialize(reload);
        WorldPartyScalingMgr::Instance().OnConfigReloaded();
    }

    void OnAfterConfigLoad(bool /*reload*/) override
    {
        if (!g_WorldPartyScalingConfig.Enabled())
        {
            LOG_INFO("module", "mod-world-party-scaling: disabled");
            return;
        }

        LOG_INFO("module",
            "mod-world-party-scaling: enabled (open world only); health tiers "
            "{:.2f}/{:.2f}/{:.2f}/{:.2f}/{:.2f}/{:.2f}, damage tiers {:.2f}/{:.2f}/{:.2f}/{:.2f}/{:.2f}/{:.2f}, "
            "xp tiers {:.2f}/{:.2f}/{:.2f}/{:.2f}/{:.2f}/{:.2f}",
            g_WorldPartyScalingConfig.HealthMultiplier(1), g_WorldPartyScalingConfig.HealthMultiplier(2),
            g_WorldPartyScalingConfig.HealthMultiplier(3), g_WorldPartyScalingConfig.HealthMultiplier(4),
            g_WorldPartyScalingConfig.HealthMultiplier(5), g_WorldPartyScalingConfig.HealthMultiplier(6),
            g_WorldPartyScalingConfig.DamageMultiplier(1), g_WorldPartyScalingConfig.DamageMultiplier(2),
            g_WorldPartyScalingConfig.DamageMultiplier(3), g_WorldPartyScalingConfig.DamageMultiplier(4),
            g_WorldPartyScalingConfig.DamageMultiplier(5), g_WorldPartyScalingConfig.DamageMultiplier(6),
            g_WorldPartyScalingConfig.XpMultiplier(1), g_WorldPartyScalingConfig.XpMultiplier(2),
            g_WorldPartyScalingConfig.XpMultiplier(3), g_WorldPartyScalingConfig.XpMultiplier(4),
            g_WorldPartyScalingConfig.XpMultiplier(5), g_WorldPartyScalingConfig.XpMultiplier(6));
    }
};

class WorldPartyScalingAllCreatureScript final : public AllCreatureScript
{
public:
    WorldPartyScalingAllCreatureScript()
        : AllCreatureScript("WorldPartyScalingAllCreatureScript")
    {
    }

    void OnCreatureRemoveWorld(Creature* creature) override
    {
        WorldPartyScalingMgr::Instance().OnCreatureRemoveWorld(creature);
    }
};

class WorldPartyScalingUnitScript final : public UnitScript
{
public:
    WorldPartyScalingUnitScript()
        : UnitScript("WorldPartyScalingUnitScript", true,
            { UNITHOOK_ON_UNIT_ENTER_COMBAT, UNITHOOK_ON_UNIT_EXIT_COMBAT, UNITHOOK_ON_UNIT_DEATH,
                UNITHOOK_MODIFY_MELEE_DAMAGE, UNITHOOK_MODIFY_SPELL_DAMAGE_TAKEN,
                UNITHOOK_MODIFY_PERIODIC_DAMAGE_AURAS_TICK })
    {
    }

    void OnUnitEnterCombat(Unit* unit, Unit* victim) override
    {
        if (Creature* creature = unit ? unit->ToCreature() : nullptr)
            WorldPartyScalingMgr::Instance().OnCreatureEnterCombat(creature, victim);
    }

    void OnUnitExitCombat(Unit* unit) override
    {
        if (Creature* creature = unit ? unit->ToCreature() : nullptr)
            WorldPartyScalingMgr::Instance().OnCreatureExitCombat(creature);
    }

    void OnUnitDeath(Unit* unit, Unit* /*killer*/) override
    {
        if (Creature* creature = unit ? unit->ToCreature() : nullptr)
            WorldPartyScalingMgr::Instance().OnCreatureDeath(creature);
    }

    void ModifyMeleeDamage(Unit* target, Unit* attacker, uint32& damage) override
    {
        WorldPartyScalingMgr::Instance().ModifyMeleeDamage(target, attacker, damage);
    }

    void ModifySpellDamageTaken(Unit* target, Unit* attacker, int32& damage, SpellInfo const* spellInfo) override
    {
        WorldPartyScalingMgr::Instance().ModifySpellDamageTaken(target, attacker, damage, spellInfo);
    }

    void ModifyPeriodicDamageAurasTick(Unit* target, Unit* attacker, uint32& damage,
        SpellInfo const* spellInfo) override
    {
        WorldPartyScalingMgr::Instance().ModifyPeriodicDamageAurasTick(target, attacker, damage, spellInfo);
    }
};

class WorldPartyScalingPlayerScript final : public PlayerScript
{
public:
    WorldPartyScalingPlayerScript()
        : PlayerScript("WorldPartyScalingPlayerScript", { PLAYERHOOK_ON_GIVE_EXP })
    {
    }

    void OnPlayerGiveXP(Player* player, uint32& amount, Unit* victim, uint8 xpSource) override
    {
        WorldPartyScalingMgr::Instance().OnPlayerGiveXP(player, amount, victim, xpSource);
    }
};
}

void AddWorldPartyScalingScripts()
{
    new WorldPartyScalingWorldScript();
    new WorldPartyScalingAllCreatureScript();
    new WorldPartyScalingUnitScript();
    new WorldPartyScalingPlayerScript();
}
