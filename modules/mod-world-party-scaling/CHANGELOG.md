# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [1.1.0] - 2026-11-03

### Added

- Six kill-XP coefficients, `WorldPartyScaling.Group1.XP` through
  `WorldPartyScaling.Group6.XP`, with defaults from `1.00` for a solo pull to
  `1.50` for six or more players. The module now exposes 23 configuration
  keys, all reloadable with `.reload config`.
- Net party-tier kill-XP scaling for creatures managed by the module. The tier
  coefficient replaces the core party-size XP bonus instead of stacking on it,
  uses the same frozen engagement tier as health and damage, and applies only
  to `XPSOURCE_KILL` on a scaled open-world creature. The core still splits the
  resulting XP among eligible group members, and `0` disables kill XP for that
  tier.
- Configuration and documentation updates: the six new keys with comments in
  `conf/mod_world_party_scaling.conf.dist`, the README configuration table and
  affected-stats notes, an `Experience` section in `docs/COMPATIBILITY.md`, and
  the updated log-line description in `docs/DEPLOYMENT.md`.

## [1.0.0] - 2026-11-01

### Added

- Open-world creature scaling driven by the online party size that engages the
  creature. Health and damage coefficients are selected from six tiers, with
  defaults from `1.00` for a solo pull up to `5.20` health and `1.95` damage for
  six or more players.
- Group counting from the player who owns the victim: ungrouped players count as
  one, online group members count even when dead, remote, instanced or flying,
  and the result is clamped from 1 to 6. Pets, guardians, totems and summons
  resolve to their owner.
- Coefficients captured once at the first valid engagement and frozen for the
  whole fight; later entrants and group changes do not alter an ongoing fight.
- Scaling of maximum health, melee and ranged physical damage, direct offensive
  spells and standard damage-over-time ticks. Native `Rate.Creature.*` values are
  replaced, not multiplied.
- Restore of the pre-engagement health base and rates on combat exit, death,
  creature removal and configuration reload, preserving the health percentage.
- Exclusions by creature entry, map and zone, plus an optional debug line written
  once per fight when scaling starts and when it is restored.
- 17 configuration keys in `conf/mod_world_party_scaling.conf.dist`, all
  reloadable with `.reload config`.
- `docs/DEPLOYMENT.md` for Docker and native installs, `docs/COMPATIBILITY.md`
  for the interaction with existing server rates and instance modules, and
  `TEST_REPORT.md` recording the executed build and in-game scenarios.

### Fixed

- Creature rates are read through the public `sWorld->getRate(RATE_CREATURE_*)`
  API instead of the server-local `GetHealthRateMod()`/`GetDamageRateMod()`/
  `GetSpellDamageRateMod()` helpers, which do not exist in upstream AzerothCore.
  The module now builds against upstream master without a local core commit and
  without changing the scaling formula.

### Scope

- Open world only (`Map::IsWorldMap()`). Dungeons, raids, battlegrounds and
  arenas are untouched, including their mechanics, spawns and rewards.
- No SQL, no database table, no DBC edit, no creature template modification and
  no global configuration change.

[1.1.0]: https://github.com/Phanty78/mod-world-party-scaling/releases/tag/v1.1.0
[1.0.0]: https://github.com/Phanty78/mod-world-party-scaling/releases/tag/v1.0.0
