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

/*
 * Open-world party-size scaling.
 *
 * Only open-world creatures (not dungeons, arenas or battlegrounds) are
 * scaled. Pets, guardians, totems, summons, player-controlled or charmed
 * units, guards, civilians and critters are never scaled, nor are creatures
 * excluded by the configuration. An engagement captures its coefficients
 * once; later group arrivals do not change an active state.
 */

#pragma once

#include "ConfigValueCache.h"

#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_set>

enum class WorldPartyScalingConfigKey : uint32
{
    Enable,
    EnableDebug,
    Group1Health,
    Group2Health,
    Group3Health,
    Group4Health,
    Group5Health,
    Group6Health,
    Group1Damage,
    Group2Damage,
    Group3Damage,
    Group4Damage,
    Group5Damage,
    Group6Damage,
    ExcludedCreatureEntries,
    ExcludedMaps,
    ExcludedZones,
    Count
};

class WorldPartyScalingConfig final : public ConfigValueCache<WorldPartyScalingConfigKey>
{
public:
    static constexpr uint32 MaxGroupSize = 6;

    WorldPartyScalingConfig();

    void BuildConfigCache() override;

    [[nodiscard]] bool Enabled() const;
    [[nodiscard]] bool DebugEnabled() const;
    [[nodiscard]] float HealthMultiplier(uint32 groupSize) const;
    [[nodiscard]] float DamageMultiplier(uint32 groupSize) const;
    [[nodiscard]] bool IsEntryExcluded(uint32 entry) const;
    [[nodiscard]] bool IsMapExcluded(uint32 mapId) const;
    [[nodiscard]] bool IsZoneExcluded(uint32 zoneId) const;

private:
    static std::unordered_set<uint32> ParseIdSet(std::string_view value);
    void RebuildIdSets();

    std::array<float, MaxGroupSize> _healthMultipliers{};
    std::array<float, MaxGroupSize> _damageMultipliers{};
    std::unordered_set<uint32> _excludedEntries;
    std::unordered_set<uint32> _excludedMaps;
    std::unordered_set<uint32> _excludedZones;
};

extern WorldPartyScalingConfig g_WorldPartyScalingConfig;

void AddWorldPartyScalingScripts();
