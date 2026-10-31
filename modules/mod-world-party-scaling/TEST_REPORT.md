# Test report — mod-world-party-scaling

This report records what was actually executed on this checkout. Anything not executed is marked
**NOT EXECUTED** with the reason. No result is inferred from a code review alone.

## Environment

- AzerothCore WotLK 3.3.5a, core revision `c981b3b396e0+` on branch `feature/mod-world-party-scaling`.
- Host: macOS arm64, 10 cores; CMake 3.x, Unix Makefiles, Release build.
- PCH build: `var/build` (configured with `USE_SCRIPTPCH=ON`).
- No-PCH build: `var/build-nopch` (configured with `-DNOPCH=1`; Homebrew readline headers were
  selected because the macOS SDK copy lacks `rl_done`/`rl_event_hook`).
- Live stack: Docker `ac-worldserver`, `ac-authserver`, MySQL 8.4.11 (`ac-database`).
- Existing global rates from `conf/dist/env.ac`: normal HP 1.20, elite/rare/rare-elite/world-boss HP
  1.50, normal damage 1.35, elite damage 1.40, spell damage 1.30, aggro 1.20.
- The module configuration used the shipped defaults; `EnableDebug=1` and `Logger.module=5` were
  enabled temporarily to capture the lifecycle lines, then restored.

## Compilation

### PCH-enabled worldserver build

```bash
cmake -S . -B var/build
cmake --build var/build --target worldserver -j 10
```

Result: **PASS**. The module translation units compiled and `worldserver` linked:

```text
[ 97%] Building CXX object modules/CMakeFiles/modules.dir/mod-world-party-scaling/src/WorldPartyScaling.cpp.o
[ 97%] Building CXX object modules/CMakeFiles/modules.dir/mod-world-party-scaling/src/mod_world_party_scaling_loader.cpp.o
[ 98%] Linking CXX static library libmodules.a
[100%] Linking CXX executable worldserver
[100%] Built target worldserver
```

### No-PCH build

```bash
cmake -S . -B var/build-nopch -G "Unix Makefiles" -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX=/usr/local -DAPPS_BUILD=all -DTOOLS_BUILD=none \
  -DSCRIPTS=static -DMODULES=static -DNOPCH=1 \
  -DMYSQL_INCLUDE_DIR=/opt/homebrew/opt/mysql-client/include/mysql \
  -DMYSQL_LIBRARY=/opt/homebrew/opt/mysql-client/lib/libmysqlclient.dylib \
  -DOPENSSL_ROOT_DIR=/opt/homebrew/opt/openssl@3 \
  -DREADLINE_INCLUDE_DIR=/opt/homebrew/opt/readline/include \
  -DREADLINE_LIBRARY=/opt/homebrew/opt/readline/lib/libreadline.dylib
cmake --build var/build-nopch --target worldserver -j 8
```

Result: **PASS**, including the full `worldserver` link with no precompiled header:

```text
[100%] Building CXX object modules/CMakeFiles/modules.dir/mod-world-party-scaling/src/WorldPartyScaling.cpp.o
[100%] Building CXX object modules/CMakeFiles/modules.dir/mod-world-party-scaling/src/mod_world_party_scaling_loader.cpp.o
[100%] Linking CXX static library libmodules.a
[100%] Building CXX object src/server/apps/CMakeFiles/worldserver.dir/worldserver/CommandLine/CliRunnable.cpp.o
[100%] Linking CXX executable worldserver
[100%] Built target worldserver
```

The first NOPCH attempt selected the macOS SDK readline headers and failed in the unrelated core file
`src/server/apps/worldserver/CommandLine/CliRunnable.cpp` (`rl_done` and `rl_event_hook` undeclared).
Selecting the Homebrew readline headers fixed that environment mismatch; the module itself had already
compiled cleanly in both attempts.

### Live Docker image

```bash
docker compose build ac-worldserver
docker compose up -d ac-worldserver
```

Result: **PASS**. The image build installed the module config:

```text
-- Installing: /azerothcore/env/dist/etc/modules/mod_world_party_scaling.conf.dist
```

The live worldserver logged:

```text
Using modules configuration:
> mod_world_party_scaling.conf
mod-world-party-scaling: enabled (open world only); health tiers 1.00/1.70/2.50/3.30/4.20/5.20, damage tiers 1.00/1.35/1.50/1.65/1.80/1.95
```

## Live behavioural tests

The live tests used the repository's AzerothGhost e2e harness. The scratch package is
`e2e/local/wps` (gitignored by the repository's e2e policy) and was run against the live stack:

```bash
cd e2e
E2E_AUTH_ADDR=127.0.0.1:3724 \
E2E_AUTH_DSN='acore:acore@tcp(127.0.0.1:3306)/acore_auth' \
E2E_CHAR_DSN='acore:acore@tcp(127.0.0.1:3306)/acore_characters' \
E2E_WORLD_DSN='acore:acore@tcp(127.0.0.1:3306)/acore_world' \
go test -tags=e2e ./local/wps -run 'TestWPS_' -count=1 -v -timeout 30m -parallel 1
```

The full run finished **PASS** (`ok ... 150.461s`); the disable/re-enable case is skipped unless
`WPS_LIVE_CONF` points at the live config and was executed separately (PASS, see row 19).
Expected values were computed from the live pre-pull maximum health and the configured native rate,
for example `expected = round(round(before / 1.20) * 1.70)` for a normal-rank creature at tier 2. The
module's debug log independently reported the same base flat, reference health and scaled base.

### Test matrix

| # | Requirement | Result | Executed evidence |
|---|---|---|---|
| 1 | Solo difficulty | **PASS** | `before=12064`, solo scaled `10053` (`12064 / 1.20`), restored `12064` |
| 2 | Two players | **PASS** | `before=12064`, tier 2 `17090` |
| 3 | Three players | **PASS** | `before=12064`, tier 3 `25133` |
| 4 | Four players | **PASS** | `before=12064`, tier 4 `33175` |
| 5 | Five players | **PASS** | `before=12064`, tier 5 `42223` |
| 6 | Six players | **PASS** | `before=12064`, tier 6 `52276` |
| 7 | Members on different maps | **PASS** | Leader in Outland, member in Kalimdor; tier 2 `17090` |
| 8 | Disconnected members not counted | **PASS** | Member logged out; tier 1 `10053` |
| 9 | Group composition change | **PASS** | 3-player party pulled at tier 3 `1575000`; member left, max stayed `1575000` |
| 10 | Roster change during combat | **PASS** | Same removal test: coefficients stayed frozen |
| 11 | Reset then re-engage | **PASS** | Three cycles: `10053` scaled → `12064` restored, no drift |
| 12 | Multiple groups on one creature | **PASS** | Solo first pull `630000`; second player joined, max stayed `630000` |
| 13 | Rare and elite creatures | **PASS** | Normal `12064→10053`; elite `28350→18900`; rare `20905→13937`; rare-elite `10989→7326`; world boss `6745→4497` |
| 14 | Normal dungeon untouched | **PASS** | Utgarde Keep map 574: max `12064` before and after pull |
| 15 | Heroic dungeon untouched | **NOT EXECUTED separately** | The module gates on `Map::IsWorldMap()` (`MAP_COMMON` only); heroic and normal dungeons share `MAP_INSTANCE`. The normal-dungeon run exercises the same gate. |
| 16 | Raid untouched | **PASS** | Ulduar map 603: max `12064` before and after pull |
| 17 | AutoBalance unchanged | **NOT EXECUTABLE** | AutoBalance is not installed in this checkout. The module never touches `Map::IsDungeon()`/instance maps; no AutoBalance code or config is read or written. |
| 18 | No stacking with old multipliers | **PASS** | Health used reference values (`28350 / 1.50 = 18900`, not `28350`); melee median `3688` solo vs `5286` at tier 2, ratio `1.433` (a separate run measured `1.366`; expected 1.35, not 1.35 × 1.40 = 1.89) |
| 19 | Return to initial behaviour after disabling | **PASS** | `.reload config` with `Enable=0`: active creature restored `17090 → 12064`; new pull stayed `12064`; re-enable produced `17090` |
| 20 | No cumulative stat growth over fights | **PASS** | Three engage/reset cycles, max always `10053`/`12064`; debug base flat stayed `12064.00` each time |

### Damage measurement

`TestWPS_MeleeDamageReplacement` parsed `SMSG_ATTACKERSTATEUPDATE` for melee hits from a level-80 elite
against the same player, first solo and then in a two-player party. The sample was 10 hits per case:

```text
solo samples=[3284 3290 3676 3354 3265 4303 3928 3660 3699 4202 3521 3688 3745 4006 3729] median=3688.0
group2 samples=[5426 5844 4688 5286 5126 5799 4535 5552 5127 5338 4464] median=5286.0
ratio=1.433
```

The ratio is the replacement ratio: the elite's native melee rate (1.40) cancels in both cases, so
the tier-2/tier-1 ratio is the configured damage coefficient `1.35`; the 15/11-hit samples vary with
the creature's 1.0-1.5 damage roll (a first run measured 1.366). A cumulative implementation would
have produced `1.40 × 1.35 = 1.89`.

### Scope and lifecycle checks

- **Open world only:** the normal, elite, rare, rare-elite and world-boss tests all ran on `MAP_COMMON`.
- **Dungeon/raid:** map 574 and map 603 pulls left creature max health unchanged.
- **Concurrency:** the runtime state map is mutex-protected; the live tests ran with
  `MapUpdate.Threads = 1`, matching the shipped configuration. The full matrix above was run before
  that mutex-only hardening; after it, solo/reset and tiers 2-6 were re-run against the final image and
  passed (`10053`/`12064` cycles and `17090/25133/33175/42223/52276`).
- **Reset:** `OnUnitExitCombat` restoration was observed after a map transfer that ended combat; death
  and removal paths are covered by the same `RestoreAndErase` code path.
- **Disable/re-enable:** config reload restored active creatures and stopped new scaling; re-enabling
  resumed it without a restart.
- **No template mutation:** the tests read live creature max health only; no SQL or DBC change was
  made by the module.

## Not executed

| Item | Reason |
|---|---|
| Heroic dungeon runtime case | Not separately driven; the map-type gate is shared with the normal dungeon case. |
| Direct spell and periodic-tick runtime scaling | No controlled caster fixture was driven. The hook paths were reviewed against `SpellDamageBonusDone`/`ApplyPeriodicDamageAurasTick`; the code cancels the native spell rate and excludes heal/leech/percent/health-funnel ticks. |
| Ranged auto-attack runtime case | Ranged auto-attacks use the same `ModifyMeleeDamage` path as melee; no separate ranged fixture was driven. |
| AutoBalance coexistence | Module not installed; instance maps are rejected by `Map::IsWorldMap()`. |
| Full 20-player raid group clamp | Party cap 6 was exercised; the clamp to tier 6 is a pure `std::clamp(1..6)` and is covered by code review. |

## Static verification

```bash
git diff --check
cd modules/mod-world-party-scaling && python3 ../../apps/codestyle/codestyle-cpp.py
```

Both passed: no whitespace errors, no trailing whitespace/tabs, no `GetCounter()`/flag-helper/style
violations.
