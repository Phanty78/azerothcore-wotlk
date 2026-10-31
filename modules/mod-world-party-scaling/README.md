# World Party Scaling

`mod-world-party-scaling` is an AzerothCore WotLK module that scales open-world creatures to the size of
the online party that engages them. A creature pulled by a group of six receives the tier 6 health and
damage coefficients; with the default coefficients, a solo pull uses tier 1 (1.00) and therefore the
creature's reference (rate-free) statistics.

The module is server-side only. It needs no client addon, no SQL, and no database table, and it does not
edit creature templates or DBC data.

Detailed deployment steps are in [docs/DEPLOYMENT.md](docs/DEPLOYMENT.md); the server difficulty audit
and compatibility notes are in [docs/COMPATIBILITY.md](docs/COMPATIBILITY.md).

## Scope

- **Open world only.** Scaling is applied only when `Map::IsWorldMap()` is true. Dungeon, raid,
  battleground and arena maps are explicitly rejected as well.
- **Instances untouched.** Dungeons and raids keep every rate, mechanic and script they have today.
- **Battlegrounds and arenas untouched.** Their creatures and players are never modified.
- **AutoBalance-compatible by map gating.** If AutoBalance (or another instance difficulty module) is
  installed for dungeons and raids, it keeps ownership of instances while this module owns the open
  world. The module's map test prevents it from touching any instance creature, so the two do not
  overlap when AutoBalance is limited to instances, which is its intended use.
- **No global configuration changes.** `worldserver.conf`, `conf/dist/env.ac`, the database and other
  modules are not modified. Every effect is applied at runtime to one creature and reverted when the
  fight ends.

## Group counting

The group size is resolved from the player who owns the victim at the moment combat starts.

- An ungrouped player counts as one.
- With a group, the module walks `Group::GetFirstMember()` and counts entries whose `GetSource()` is
  non-null. Those are the online members. A member who is dead, on another continent, in an instance,
  flying or otherwise far away still counts.
- Disconnected or logged-out characters are not in the group reference list and are excluded.
- The result is clamped from 1 to 6. Groups larger than six use the tier 6 coefficients. The server's
  six-player party support sets `MAXGROUPSIZE` to 6; larger sizes only occur in raid groups.
- Pets, guardians, totems and summons resolve to their owner through
  `GetCharmerOrOwnerPlayerOrPlayerItself()`. The owner's group is counted. A creature with no player
  owner is not scaled, and a summon is never scaled itself even when its owner's group is counted.
- The first valid engagement decides the tier for the whole fight. If a solo player pulls a creature
  and a full group arrives afterwards, the creature keeps the solo tier until the fight ends.

## Combat lifecycle

1. **First valid engagement.** On `OnUnitEnterCombat`, the victim is resolved to a player (directly or
   through its owner). The creature is checked for eligibility: alive, in world, on a world map, not a
   pet, guardian, totem, summon, controlled or charmed unit, not a guard, civilian or critter, not
   non-attackable or non-selectable, not evading, not friendly to the engaging player, and not excluded
   by entry, map or zone.
2. **Coefficients captured once.** The online group size selects a tier. Health and damage coefficients,
   the pre-engagement health base flat modifier, the reference health and the current native rates are
   stored. The scaled base is applied, `UpdateMaxHealth()` composes auras, and the pre-engagement
   health percentage is reapplied.
3. **Frozen for the fight.** The stored state is keyed by map ID, instance ID and creature GUID;
   `ObjectGuid` counters are map-scoped, so all three are required to avoid aliasing. Later entrants,
   group changes, members leaving or a second group engaging do not change the coefficients of an
   ongoing fight: the first valid engagement keeps the difficulty for the whole fight.
4. **Reset on death, respawn or full out-of-combat.** `OnUnitExitCombat` and `OnUnitDeath` restore the
   stored health base flat modifier and call `UpdateMaxHealth()`, which recomposes auras. The current
   health percentage is sampled and reapplied (a dead creature stays at zero), the state is removed and
   the creature is ready for a fresh engagement. The next pull after the creature leaves combat
   completely, dies or respawns captures new values.
5. **Removal safety.** `OnCreatureRemoveWorld` runs the same restore path as combat exit and death:
   it restores the stored health base flat modifier, calls `UpdateMaxHealth()` and reapplies the health
   percentage, then erases the state. The debug line is written when enabled. A creature without state
   is a no-op. This also covers grid unload and re-add, so a re-added creature cannot keep scaled
   health with no state and compound on the next engagement.
6. **Reload safety.** Reloading the configuration with the module disabled restores every active
   creature before clearing the state.

## Affected stats

- **Maximum health.** Reference health comes from `GetCreateHealth()` divided by the native health
  rate, falling back to the current maximum divided by that rate when create health is zero. The tier's
  `Health` coefficient scales that base; `UpdateMaxHealth()` then composes active auras, and the health
  percentage is preserved.
- **Melee and ranged physical damage.** Damage is multiplied by `Damage / native melee rate`, which
  replaces the server's global melee rate with the tier coefficient.
- **Direct offensive spells.** Scaling depends on the spell's effects and attributes, not on its
  school. Weapon-damage spells (`EffectWeaponDmg`: `SPELL_EFFECT_WEAPON_DAMAGE`,
  `SPELL_EFFECT_WEAPON_DAMAGE_NOSCHOOL`, `SPELL_EFFECT_NORMALIZED_WEAPON_DMG` or
  `SPELL_EFFECT_WEAPON_PERCENT_DAMAGE`) are checked first and use `Damage / native melee rate`,
  including mixed weapon/school spells. Other spells with `SPELL_ATTR3_IGNORE_CASTER_MODIFIERS` use the
  tier's `Damage` coefficient alone. All remaining spells use `Damage / native spell rate`.
- **Standard damage-over-time ticks.** Scaled only when the spell has a `SPELL_AURA_PERIODIC_DAMAGE`
  effect and none of `SPELL_AURA_PERIODIC_HEAL`, `SPELL_AURA_OBS_MOD_HEALTH`,
  `SPELL_AURA_PERIODIC_LEECH`, `SPELL_AURA_PERIODIC_DAMAGE_PERCENT` or
  `SPELL_AURA_PERIODIC_HEALTH_FUNNEL`; ambiguous spells are skipped. The factor is
  `Damage / native spell rate`, or `Damage` alone for ignore-caster spells. The hook runs before the
  spell bonus step, so the result is correct once that step completes.

The native `Rate.Creature.*` values are **replaced, not multiplied**. A creature managed by this module
does not receive the global open-world rate on top of the group coefficient.

## Unchanged

- Experience, loot, drop rates and quest rewards.
- Creature level, armor, resistances and damage schools.
- Healing, absorbs and friendly abilities.
- Special abilities and scripted mechanics, including their damage unless it goes through the melee,
  spell or periodic damage hooks.
- Creature AI, spells chosen, threat logic and movement.
- Spawn counts, respawn timers and population.

## Configuration

The module reads `env/dist/etc/modules/mod_world_party_scaling.conf`. The distributed file
`conf/mod_world_party_scaling.conf.dist` contains the same keys with comments. An empty exclusion list
means no exclusion; entries are comma-separated and spaces around entries are ignored. Invalid or
out-of-range entries are ignored.

| Key | Default | Meaning |
| --- | --- | --- |
| `WorldPartyScaling.Enable` | `1` | Master switch. `0` disables scaling and new engagements. |
| `WorldPartyScaling.Group1.Health` | `1.00` | Health coefficient for 1 player. |
| `WorldPartyScaling.Group1.Damage` | `1.00` | Damage coefficient for 1 player. |
| `WorldPartyScaling.Group2.Health` | `1.70` | Health coefficient for 2 players. |
| `WorldPartyScaling.Group2.Damage` | `1.35` | Damage coefficient for 2 players. |
| `WorldPartyScaling.Group3.Health` | `2.50` | Health coefficient for 3 players. |
| `WorldPartyScaling.Group3.Damage` | `1.50` | Damage coefficient for 3 players. |
| `WorldPartyScaling.Group4.Health` | `3.30` | Health coefficient for 4 players. |
| `WorldPartyScaling.Group4.Damage` | `1.65` | Damage coefficient for 4 players. |
| `WorldPartyScaling.Group5.Health` | `4.20` | Health coefficient for 5 players. |
| `WorldPartyScaling.Group5.Damage` | `1.80` | Damage coefficient for 5 players. |
| `WorldPartyScaling.Group6.Health` | `5.20` | Health coefficient for 6 or more players. |
| `WorldPartyScaling.Group6.Damage` | `1.95` | Damage coefficient for 6 or more players. |
| `WorldPartyScaling.EnableDebug` | `0` | Emit one diagnostic line when scaling starts and when it is restored. |
| `WorldPartyScaling.ExcludedCreatureEntries` | `""` | Creature template entries that are never scaled. |
| `WorldPartyScaling.ExcludedMaps` | `""` | Map IDs that are never scaled. |
| `WorldPartyScaling.ExcludedZones` | `""` | Zone IDs that are never scaled, using the creature's current zone. |

Health coefficients must be finite and greater than zero. Damage coefficients must be finite and
greater than or equal to zero; `0` reduces each damaging hit to the 1-point floor. Invalid values fall
back to the listed default and an error is written to the log.

There is no user-facing coefficient ceiling. Internally, the scaled health base is capped at
`1,073,741,824` before `UpdateMaxHealth()` composes active modifiers, and scaled damage is capped at
`1,073,741,823`, keeping the existing 1-point floor. These limits only matter for extreme
configurations and stop the core's critical-hit doubling from overflowing; ordinary values are
unaffected.

The coefficients are experimental. Adjust them to the desired pace.
Because they apply to reference statistics and replace the native open-world rates, changing
`Rate.Creature.*` in `worldserver.conf` or `conf/dist/env.ac` does not change the result for a scaled
creature.

## Installation

### Build requirement

The module is a static AzerothCore script module: its sources are compiled into `worldserver`. Adding or
removing the module therefore requires a CMake reconfigure and a worldserver rebuild. No client patch
and no database update are required. Only public core APIs are used: native `Rate.Creature.*` values are
read through `sWorld->getRate(...)` by creature rank, so the module does not depend on custom
`GetHealthRateMod()`/`GetDamageRateMod()`/`GetSpellDamageRateMod()` wrappers that are absent from
upstream AzerothCore.

### Docker

The independent repository is <https://github.com/Phanty78/mod-world-party-scaling>. Clone it into the
core as `modules/mod-world-party-scaling`:

```bash
git clone https://github.com/Phanty78/mod-world-party-scaling.git \
  modules/mod-world-party-scaling
```

Then rebuild the worldserver image so CMake picks up the module:

```bash
docker compose build ac-worldserver
docker compose up -d
```

`docker compose up -d --build` performs both steps. The container entrypoint copies the installed
`.conf.dist` to `mod_world_party_scaling.conf` on start if the file does not exist yet.

The live file is bind-mounted from the repository:

```text
env/dist/etc/modules/mod_world_party_scaling.conf
```

Edit that file, then apply it with `.reload config` as a GM or `reload config` in the worldserver
console. A restart also applies it.

### Native

From the repository root:

```bash
./acore.sh compiler build
```

That command configures CMake, compiles the worldserver and installs the binaries and configuration
files under `env/dist`. The installer copies each module `.conf.dist` to its `.conf` counterpart when
`AC_ENABLE_CONF_COPY_ON_INSTALL` is `1` (the default). If the copy was skipped:

```bash
cp env/dist/etc/modules/mod_world_party_scaling.conf.dist \
   env/dist/etc/modules/mod_world_party_scaling.conf
```

Start the server with:

```bash
./acore.sh run-worldserver
```

or directly with `env/dist/bin/worldserver`.

### Verify that the module loaded

The build output lists every module and its config file. At runtime the core logs the module
configuration files it loaded:

```text
Using modules configuration:
> mod_world_party_scaling.conf
```

The module also writes an info line through the `module` logger. When it is enabled, the line begins
with `mod-world-party-scaling: enabled (open world only);` and then reports the six health tiers and
six damage tiers currently loaded. When it is disabled, the line is exactly
`mod-world-party-scaling: disabled`.

With Docker:

```bash
docker compose logs ac-worldserver | grep -F "mod-world-party-scaling"
```

With a native install the same line is in `env/dist/logs/Server.log` (the default `Logger.module`
settings route info to the console and to `Server.log`):

```bash
grep -F "mod-world-party-scaling" env/dist/logs/Server.log
```

To see `EnableDebug` lines, raise the module logger to debug level in `worldserver.conf`:

```ini
Logger.module=5,Console Server
```

Debug output is emitted only when scaling starts and when it is restored, never per hit. Each line
reports the health base flat value, reference health, scaled base and resulting maximum health, along
with the group tier and coefficients.

### Reload behavior

- `.reload config` reruns the core configuration reload, then the module's `ON_BEFORE_CONFIG_LOAD` and
  `ON_AFTER_CONFIG_LOAD` hooks. Every key in this module is reloadable; no worldserver restart is
  needed.
- A reload with `WorldPartyScaling.Enable = 0` immediately restores all creatures currently scaled and
  clears the module state.
- Changing coefficients or exclusions affects new engagements. A fight already in progress keeps the
  coefficients frozen at its start, unless scaling was disabled and all active fights were restored.
- The summary line is written again after a reload, with the new values.

### Smoke check

After a rebuild and restart, pull one open-world creature solo and note how long it takes to kill and
how hard it hits. Release it, then pull the same type of creature with a full party. The second pull
should last noticeably longer and hit harder. Repeat inside a dungeon: the party size must make no
difference there. This is a manual procedure; no test result is claimed here.

## Uninstall and rollback

### Disable without removing the module

1. Set `WorldPartyScaling.Enable = 0` in `env/dist/etc/modules/mod_world_party_scaling.conf`.
2. Run `.reload config` as a GM, or `reload config` in the worldserver console.

Active creatures are restored during the reload. New engagements are not scaled. No rebuild and no
restart are required. Set the key back to `1` and reload to re-enable.

### Remove the module completely

1. Stop the worldserver.
2. Delete `modules/mod-world-party-scaling`.
3. Delete `env/dist/etc/modules/mod_world_party_scaling.conf` and
   `env/dist/etc/modules/mod_world_party_scaling.conf.dist`. A rebuilt server no longer lists this
   file, so it would not be read; delete it anyway so no stale module settings remain.
4. Reconfigure and rebuild, because the module is linked into the static `worldserver` binary:
   - native: `./acore.sh compiler build`;
   - Docker: `docker compose build ac-worldserver` then `docker compose up -d`.
5. Start the server.

There is no database table, no character flag and no persistent state to clean. The module never
edited `worldserver.conf`, `conf/dist/env.ac` or any other module, so there is no saved configuration
to revert either.

## Compatibility

### Existing server difficulty

The server currently applies global difficulty through native rate overrides in `conf/dist/env.ac`:
normal damage `1.35`, elite/rare/rare elite/world boss damage `1.40`, normal spell damage `1.30`,
elite spell damage `1.30`, normal health `1.20`, elite/rare/rare elite/world boss health `1.50`, and
aggro radius `1.20`. They are native `Rate.Creature.*` values read at startup and applied on every map,
instances included.

For a scaled open-world creature, the module divides the native health, melee and spell rates out at
engagement and applies the tier coefficient instead. The old rates are replaced, not multiplied. For
everything else, including every instance, the rates are unchanged:

| Context | Behaviour |
| --- | --- |
| Open world, scaled creature | Tier coefficient replaces the native health, melee and spell rates. |
| Open world, excluded creature | Native rates, exactly as without the module. |
| Dungeon or raid | Native rates and all existing instance behaviour, including AutoBalance if installed. |
| Battleground or arena | Untouched. |

With the default tier 1 coefficients of `1.00`, a solo open-world pull uses reference values, below the
current global `1.20` health and `1.35` melee rates. That is intentional: the tiers are absolute
replacements, not additions to the server's rates.

### AutoBalance

No AutoBalance module is installed in this repository today. If one is added, its instance-only map
checks and this module's `Map::IsWorldMap()` check are disjoint, so the same creature is never handled
by both. Keep AutoBalance configured for dungeons and raids; the open world belongs to this module.

### Six-player party support

This server has a custom core patch that sets `MAXGROUPSIZE` to 6 for parties. The module counts the
same online members and clamps at six, so a six-player party always maps to tier 6. A raid group larger
than six also uses tier 6 by design.

### Other modules and creatures

The module never edits `creature_template`, spawn data or DBC rows. It stores state per creature using
a composite map ID, instance ID and `ObjectGuid` key for the duration of a fight, so other modules that
spawn, possess or script creatures continue to work. Use `WorldPartyScaling.ExcludedCreatureEntries`
for quest bosses, scripted encounters or any creature whose exact health and damage must not change.

### Reversibility

Disabling the module restores active creatures at the next configuration reload. Removing the module
and rebuilding leaves no trace: no SQL migration, no saved variables and no global rate edits.

## Performance

- **Event-driven.** Work happens on combat enter, combat exit, death, creature removal and the three
  damage hooks. There is no periodic sweep over creatures and no per-tick map scan.
- **Constant-time state lookup.** Active fights are kept in an `unordered_map` keyed by a composite
  `ScalingKey` of map ID, instance ID and `ObjectGuid`; each damage hook performs one lookup. State
  exists only while a creature is in a scaled fight.
- **Templates untouched.** No creature template, spawn, DBC or database row is copied or modified, so
  there is no startup cost proportional to the world population.
- **No per-hit logging.** Diagnostics are written once per fight when debug is enabled; otherwise the
  hooks are silent.

## Limitations

- **Scripted raw damage is not scaled.** An ability that calls `DealDamage`/`DealDamageMods` directly,
  bypassing the melee, spell and periodic hooks, keeps its scripted value.
- **Out-of-combat damage ticks are ignored.** Damage hooks require the creature to be in combat; a
  periodic tick outside combat neither starts nor applies scaling. `OnUnitEnterCombat` remains the
  primary start path.
- **Mixed weapon/school spells use the melee factor.** When a spell has a weapon-damage effect and
  school damage, its aggregate damage is scaled with the melee factor; the school portion is not
  separated.
- **Special periodic ticks are not scaled.** Only standard `SPELL_AURA_PERIODIC_DAMAGE` ticks on
  unambiguous spells are affected. Percent-health ticks, leech ticks, health funnels, periodic heals,
  observed-health ticks and spells that combine a damage tick with any of those effects are left alone.
- **First engagement wins.** Reinforcements cannot raise the difficulty of an ongoing fight.
- **Exclusions are evaluated at engagement.** Adding an entry, map or zone to an exclusion list does
  not unscale a creature already fighting; it applies to the next engagement.
- **Open world only.** This is not a dungeon or raid difficulty system, and it does not change
  mechanics, abilities, spawns or rewards.
- **Tuning is experimental.** The shipped coefficients are a starting point, not a balance guarantee.

## Test results

Compilation and test results are documented in `TEST_REPORT.md`. This README does not reproduce them
and makes no claim about whether a build or test run has passed.
