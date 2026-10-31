# Compatibility and server audit

This document records how `mod-world-party-scaling` interacts with the difficulty mechanisms already
present in this AzerothCore checkout. It is an audit of the current tree, not a test report.
Compilation and test outcomes live in `TEST_REPORT.md`.

## Audited mechanisms

| Mechanism | Where it lives | Current value or behaviour |
| --- | --- | --- |
| Creature rates | `conf/dist/env.ac` (`Rate.Creature.*`) | Every map, instances included. |
| Global aggro radius | `conf/dist/env.ac`, native `Rate.Creature.Aggro` | `1.20`, applied on every map. |
| Instance difficulty module | `modules/` | No AutoBalance module is installed. |
| Party size cap | `src/server/game/Groups/Group.h` | `MAXGROUPSIZE` is 6. |
| World map test | `src/server/game/Maps/Map.h` | `IsWorldMap()` is true only for `MAP_COMMON`. |

## Global difficulty already in effect

`conf/dist/env.ac` contains the following native rate overrides. They are environment overrides for
`Rate.Creature.*`, so they are read once at worldserver startup; changing them requires a restart.

| Rank | Health | Melee damage | Spell damage |
| --- | --- | --- | --- |
| Normal | `1.20` | `1.35` | `1.30` |
| Elite | `1.50` | `1.40` | `1.30` |
| Rare | `1.50` | `1.40` | `1.30` |
| Rare elite | `1.50` | `1.40` | `1.30` |
| World boss | `1.50` | `1.40` | `1.30` |

The file also sets `AC_RATE_CREATURE_AGGRO=1.20`. Its own comments state that the rates apply on
**every map, dungeons and raids included** (`conf/dist/env.ac`, lines 26-54). There is no instance
exception in the file.

The rates are not a module setting: they are the core's native creature rates. Upstream AzerothCore
keeps `Creature::_GetHealthMod()` protected and exposes `Creature::_GetDamageMod()` and
`Creature::GetSpellDamageMod()`; some custom checkouts replace this shape with public
`GetHealthRateMod()`, `GetDamageRateMod()` and `GetSpellDamageRateMod()` wrappers. Neither shape is a
stable module interface. The module reads the same native rates through the public
`sWorld->getRate(...)` API, selecting the rate for the creature's rank itself. This compiles and
behaves identically on upstream master and on a checkout that carries the custom wrappers.

Creature creation applies the health rate when it computes base health and maximum health, which is
why the rate is already present in a spawned creature's maximum health. The module's eligibility test
rejects pets, guardians, totems, summons, player-controlled and charmed units before reading a rate,
so none of those units is ever scaled regardless of which rate helper the core exposes.

## Open-world isolation

The module's map eligibility test is:

```cpp
creature->GetMap()->IsWorldMap()
```

`Map::IsWorldMap()` (`Map.h:308`) delegates to the map entry, whose implementation is
`map_type == MAP_COMMON` (`src/server/shared/DataStores/DBCStructure.h:1394`). The module also
explicitly rejects `IsDungeon()` and `IsBattlegroundOrArena()` maps. As a result:

- Dungeons, raids, battlegrounds and arenas are never scaled by this module.
- Their native rates, scripts and any instance difficulty module remain exactly as they were.
- The open world is the only place where the module adds or removes a rate.

No map or instance setting is edited to achieve this; the decision is made per creature at
engagement.

## How the old rates are replaced

The module does not multiply its coefficients on top of the global rates. At engagement it stores the
pre-engagement health base flat modifier, the reference health and the current native rates, then
cancels those rates before applying the group coefficient. Damage hooks only act while the creature is
in combat; an out-of-combat tick neither starts nor applies scaling. `OnUnitEnterCombat` remains the
primary start path.

### Health

```text
reference health = GetCreateHealth() / GetHealthRate()
                   fallback: GetMaxHealth() / GetHealthRate() when create health is 0
scaled base      = reference health * GroupN.Health
resulting max    = recomputed from scaled base and active health modifiers
```

For example, a normal creature with 1000 reference health and the current `1.20` normal rate has a
create health of 1200. A six-player engagement computes `1200 / 1.20 = 1000`, then applies `5.20`, for a
scaled base of 5200. `UpdateMaxHealth()` recomputes the resulting maximum from that base and any active
health modifiers, so it can be higher. It does **not** compute `1200 * 5.20 = 6240`. The health
percentage is preserved across the change.

On combat exit, death or creature removal, the stored `BASE_VALUE` flat modifier is restored and
`UpdateMaxHealth()` recomputes the maximum so active auras compose again; the sampled health percentage
is then reapplied. The module does not call `SetCreateHealth` or `SetMaxHealth`. Debug output reports the
stored base flat, reference health, scaled base and resulting maximum.

### Melee and ranged physical damage

`ModifyMeleeDamage` receives damage that already contains `DamageModifier * GetDamageRate()` and
has not yet been reduced by armor. The hook multiplies it by:

```text
GroupN.Damage / GetDamageRate()
```

so the global melee rate is canceled and the tier coefficient takes its place. With the current normal
rate, a pre-rate hit of 100 becomes 135 natively and 195 under tier 6, not `135 * 1.95`.

### Direct offensive spells

`ModifySpellDamageTaken` runs after `SpellDamageBonusDone`. A weapon-damage spell is checked first and
resolves through `EffectWeaponDmg`, so it already carries `GetDamageRate()` (the melee/ranged rate).
Otherwise, a spell with `SPELL_ATTR3_IGNORE_CASTER_MODIFIERS` uses the coefficient alone. All remaining
spells carry `GetSpellDamageRate()`. The hook uses:

```text
GroupN.Damage / GetDamageRate()       weapon-damage spell (EffectWeaponDmg)
GroupN.Damage                         ignore-caster-modifiers case, non-weapon spell
GroupN.Damage / GetSpellDamageRate()  all remaining spells
```

The weapon-damage branch is selected when any spell effect is `SPELL_EFFECT_WEAPON_DAMAGE`,
`SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL`, `SPELL_EFFECT_NORMALIZED_WEAPON_DMG` or
`SPELL_EFFECT_WEAPON_PERCENT_DAMAGE`. Because this check runs first, a mixed weapon + school-damage
spell uses the melee factor for its aggregate damage; the school portion is not separated. An ordinary
normal-school spell without one of these effects still takes the spell-rate branch.

### Standard damage-over-time ticks

`ModifyPeriodicDamageAurasTick` runs before `SpellDamageBonusDone` for standard DOT ticks, so the native
spell rate is applied after the hook. The module scales a tick only when the spell has a
`SPELL_AURA_PERIODIC_DAMAGE` effect and none of `SPELL_AURA_PERIODIC_HEAL`,
`SPELL_AURA_OBS_MOD_HEALTH`, `SPELL_AURA_PERIODIC_LEECH`,
`SPELL_AURA_PERIODIC_DAMAGE_PERCENT` or `SPELL_AURA_PERIODIC_HEALTH_FUNNEL`; ambiguous spells are
skipped. The factor is `GroupN.Damage / GetSpellDamageRate()`, or `GroupN.Damage` alone when the spell
ignores caster modifiers. That yields the replacement once the later rate application completes.

### Safety caps

The scaled health base is capped at `1,073,741,824` (`1 << 30`) before `UpdateMaxHealth()` composes
active modifiers, so the resulting maximum can be higher. Scaled melee, spell and periodic damage
results keep the 1-point floor and are capped at `1,073,741,823` (`INT32_MAX / 2`), so the core's
critical-hit doubling cannot overflow. These are internal safety limits only: configuration has no
user-facing coefficient ceiling, and ordinary values are unaffected.

### Aggro radius

The module does not touch `Rate.Creature.Aggro`. The existing `1.20` aggro radius stays in effect on
every map, including the open world.

## AutoBalance

No AutoBalance module exists in `modules/` in this checkout: the directory listing contains no
AutoBalance entry. Consequently there is no AutoBalance instance-scaling behaviour to coordinate with
today. Beyond the global rates described above, no AutoBalance scaling is active.

If AutoBalance is added later, the separation is structural:

- AutoBalance is expected to operate on dungeon and raid maps.
- This module requires `Map::IsWorldMap()` and explicitly rejects dungeon and battleground/arena maps.
- The map check happens before any creature is scaled, health is changed or damage is modified. As long
  as AutoBalance stays limited to instances, no creature is handled by both systems.

Instances therefore keep all existing rates and, when AutoBalance is limited to instances, its scaling
behaviour too, without interference. The module does not read or write AutoBalance configuration.

## Six-player party support

This checkout carries a custom party-size patch. `Group.h:44` defines `MAXGROUPSIZE` as 6, and
`Group::GetFirstMember()` (`Group.h:254`) exposes the group's member reference list. The module walks
that list and counts only entries with a non-null `GetSource()`, i.e. online members. Position, map,
instance, distance and alive/dead state do not matter for the count. Disconnected members are removed
from the list and are not counted.

The module clamps the result to 1-6. A six-player party maps to tier 6. A raid group larger than six
also maps to tier 6, because no tier above 6 is defined.

## Other modules and creatures

- The module never edits `creature_template`, spawn data, DBC rows or SQL.
- State is held per creature using a composite map ID, instance ID and `ObjectGuid` key only for the
  duration of a scaled fight; GUID counters are map-scoped. On reload the restore path resolves the
  stored map and instance before looking up the GUID.
- When a creature is removed from the world, the module restores the stored health base flat modifier,
  calls `UpdateMaxHealth()` to recompose auras, reapplies the health percentage, writes the debug line
  when enabled, and then erases the state. This also covers grid unload and re-add, so a re-added
  creature cannot keep scaled health without state and compound on the next engagement.
- Other modules can still spawn, possess, charm or script creatures. Pets, guardians, totems, summons,
  charmed units, guards, civilians and critters are excluded from scaling. Use the exclusion lists for
  any other scripted or quest-critical creature whose authored values must be preserved.
- The module restores the maximum health it captured at engagement when the fight ends. If another
  system rewrites the same creature's maximum health during a scaled fight, that change can be
  overwritten at combat exit or death.
- `WorldPartyScaling.ExcludedCreatureEntries`, `ExcludedMaps` and `ExcludedZones` provide an explicit
  opt-out for quest bosses, scripted encounters or custom areas.
- A creature excluded by entry, map or zone keeps the native rates exactly as if the module were not
  installed.

## Instances keep the existing rates

Nothing in this module runs on a dungeon, raid, battleground or arena map. In those maps:

- Normal health remains at `1.20`; elite, rare, rare elite and world boss health remains at `1.50`.
- Melee damage remains at `1.35` or `1.40`.
- Spell damage remains at `1.30`.
- Aggro radius remains at `1.20`.
- Any future AutoBalance scaling continues to apply as configured.

The module also does not alter instance resets, lockouts, difficulty selection or creature scripts.

## Reversibility

- The module writes no database rows, no character flags and no cache files.
- It never edits `worldserver.conf`, `conf/dist/env.ac` or another module's configuration.
- Setting `WorldPartyScaling.Enable = 0` and running `.reload config` restores every creature that is
  currently scaled and clears the runtime state.
- Removing the module and its config files, then rebuilding `worldserver`, leaves no server-side
  residue. No SQL cleanup is required.
- A worldserver restart also releases all runtime state, because nothing is persisted.

## Audit boundaries

This audit covers the mechanisms listed at the top and the module's own source. It does not evaluate
unrelated modules' behaviour, client-side addons or database content. It makes no claim that the module
has been compiled or tested; those results belong to `TEST_REPORT.md`.
