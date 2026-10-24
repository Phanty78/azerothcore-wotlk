# Replace mod-hardcore with mod-challenge-modes (French, opt-in hardcore)

> **Status: executed on 2026-10-24.** This document is kept as a migration record. The destructive
> steps (character wipe) must not be re-run as-is.
>
> **Update 2026-10-24:** the live config was later switched to all eight challenges enabled
> (`*.Enable = 1`); the Hardcore-only values in Phase C describe the initial rollout. The AH bot
> was re-enabled with character `Bothv` (GUID 4, account 20), so the post-wipe "AH bot disabled"
> notes below describe the initial state only.

## Context

The server currently runs a vendored `mod-hardcore` that auto-flags **every** character at login (server-wide permadeath). The user wants per-character opt-in hardcore via `mod-challenge-modes` (Shrine of Challenge, level 1 / DK ≤ 55), patched to restore the features the old module had — server-wide death announcements, a persistent death journal (`hardcore_deaths`), and a GM revive command — and translated to French. Per explicit user decision, **all existing characters are wiped from the database (fresh start)**; the AH bot character disappears with the wipe, so the AH bot is disabled until a new bot character is created and configured. Target: private WotLK 3.3.5a Docker deployment (max 10 players) also running `mod-hunger-thirst` (Mode=off) and `mod-ah-bot-plus`.

## Approach

### Phase A — Patch the module source (`modules/mod-challenge-modes`)

1. **Clone and branch** (the directory is ignored by the core `.gitignore`):
   ```bash
   # from the repository root
   git clone https://github.com/ZhengPeiRu21/mod-challenge-modes.git modules/mod-challenge-modes
   git -C modules/mod-challenge-modes checkout 1930525b9530d329cb9fe0504a3c9b5b40a12261
   git -C modules/mod-challenge-modes switch -c local-adaptations
   ```
   Pin: `1930525` (2025-11-25, last upstream commit; repo is dormant, no CI/releases).

2. **Compatibility fix — resurrect hook** (2 occurrences, in `ChallengeMode_Hardcore` ~line 448 and `ChallengeMode_IronMan` ~line 629 of `src/ChallengeModes.cpp`): change
   `void OnPlayerResurrect(Player* player, float /*restore_percent*/, bool /*applySickness*/) override`
   to
   `void OnPlayerResurrect(Player* player, float /*restore_percent*/, bool& /*applySickness*/) override`
   (core `PlayerScript.h:673` now takes `bool&`; without this the module does not compile).

3. **Bug fix — HARDCORE_DEAD gating**: in `ChallengeModes::challengeEnabled` (`src/ChallengeModes.cpp`, switch ~lines 22-47), replace `case HARDCORE_DEAD: break;` with `case HARDCORE_DEAD: return true;`. Without this, `challengeEnabledForPlayer(HARDCORE_DEAD, player)` always returns false and the module's login dead-check is dead code.

4. **Add includes** at the top of `src/ChallengeModes.cpp` (after the existing `#include "ChallengeModes.h"`): `AccountMgr.h`, `DatabaseEnv.h`, `GameTime.h`, `StringFormat.h`, `WorldSessionMgr.h` (mirrors the removed `mod-hardcore`, which used the same APIs).

5. **French strings** in `src/ChallengeModes.cpp` (replace Chinese literals, keep code structure):
   - Kick reason (3 occurrences, in login / released-ghost / resurrect): `Personnage Hardcore définitivement mort.`
   - Semi-hardcore item-loss prefix (~line 489): `Vous avez perdu votre` (format string already appends the item link).
   - Gossip items in `OnGossipHello` (only Hardcore is enabled by config, translate all eight):
     - `启用极限模式` → `Activer le mode Hardcore`
     - `启用半极限模式` → `Activer le mode Semi-Hardcore`
     - `启用自制装备模式` → `Activer le mode Équipement auto-fabriqué`
     - `启用低品质装备模式` → `Activer le mode Bas qualité`
     - `启用慢速经验模式` → `Activer le mode XP lente`
     - `启用极慢经验模式` → `Activer le mode XP très lente`
     - `启用任务经验专属模式` → `Activer le mode XP quêtes`
     - `启用铁人模式` → `Activer le mode Iron Man`
   - Gossip confirmation in `OnGossipSelect`: `Le mode de défi est activé.`
   - Change `SendGossipMenuFor(player, 12669, go->GetGUID())` to `SendGossipMenuFor(player, 1266900, go->GetGUID())` (12669 is an English `npc_text` used elsewhere; 1266900 is free and gets French text, see step 8).

6. **New config keys** — add to `ChallengeModes` class (`src/ChallengeModes.h`, public bool members): `hardcoreAnnounceDeaths`, `hardcoreLogDeaths`. Read them in `ChallengeModes_WorldScript::LoadConfig` inside the `if (sChallengeModes->enabled())` block:
   ```cpp
   sChallengeModes->hardcoreAnnounceDeaths = sConfigMgr->GetOption<bool>("Hardcore.AnnounceDeaths", true);
   sChallengeModes->hardcoreLogDeaths       = sConfigMgr->GetOption<bool>("Hardcore.LogDeaths", true);
   ```
   Add both keys to `conf/challenge_modes.conf.dist` in the Hardcore section (values `1`, French comments: "Annonce la mort d'un personnage Hardcore à tout le serveur." / "Journalise chaque mort Hardcore dans la table hardcore_deaths.").

7. **Journal bootstrap** — add to `ChallengeModes_WorldScript` an `OnStartup()` override that runs (port of the removed module's schema, identical so the 3 existing rows survive until the wipe):
   ```sql
   CREATE TABLE IF NOT EXISTS `hardcore_deaths` (
     `id` INT UNSIGNED NOT NULL AUTO_INCREMENT,
     `guid` INT UNSIGNED NOT NULL,
     `name` VARCHAR(12) NOT NULL,
     `level` TINYINT UNSIGNED NOT NULL,
     `map` INT UNSIGNED NOT NULL,
     `zone` INT UNSIGNED NOT NULL,
     `ts` INT UNSIGNED NOT NULL,
     PRIMARY KEY (`id`),
     KEY `idx_hardcore_deaths_guid` (`guid`)
   ) ENGINE=InnoDB DEFAULT CHARSET=utf8mb4;
   ```
   via `CharacterDatabase.DirectExecute(...)`.

8. **French shrine header** — new file `data/sql/db-world/base/challenge_mode_gossip_fr.sql`:
   ```sql
   DELETE FROM `npc_text` WHERE `ID`=1266900;
   INSERT INTO `npc_text` (`ID`, `text0_0`, `Probability0`) VALUES
   (1266900, 'Vous ressentez une étrange présence devant cette antique idole.', 1);
   ```

9. **Centralized death handling** in `ChallengeMode_Hardcore` (`src/ChallengeModes.cpp`):
   - Add a file-scope static helper above the class:
     ```cpp
     static void HandleHardcoreDeath(Player* player)
     {
         if (!sChallengeModes->challengeEnabledForPlayer(SETTING_HARDCORE, player))
             return;
         if (player->GetPlayerSetting("mod-challenge-modes", HARDCORE_DEAD).value)
             return; // already recorded (KillPlayer can re-fire via resurrect/login)

         player->UpdatePlayerSetting("mod-challenge-modes", HARDCORE_DEAD, 1);

         if (sChallengeModes->hardcoreLogDeaths)
             CharacterDatabase.Execute(
                 "INSERT INTO `hardcore_deaths` (`guid`, `name`, `level`, `map`, `zone`, `ts`) "
                 "VALUES ({}, '{}', {}, {}, {}, {})",
                 player->GetGUID().GetCounter(), player->GetName(), uint32(player->GetLevel()),
                 player->GetMapId(), player->GetZoneId(), uint32(GameTime::GetGameTime().count()));

         ChatHandler(player->GetSession())
             .SendSysMessage("|cffff0000[Hardcore]|r Ce personnage Hardcore est tombé et ne peut plus être joué.");

         if (sChallengeModes->hardcoreAnnounceDeaths)
             sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING,
                 Acore::StringFormat("|cffff0000[Hardcore]|r {} (niveau {}) est tombé et ne se relèvera jamais.",
                     player->GetName(), player->GetLevel()));
     }
     ```
   - Add `void OnPlayerJustDied(Player* player) override { HandleHardcoreDeath(player); }` (core calls it once per death from `Player::KillPlayer`, `Player.cpp:4672` — covers creature, PvP and environmental deaths).
   - In `OnPlayerReleasedGhost`: replace the `UpdatePlayerSetting(...HARDCORE_DEAD...)` line with `HandleHardcoreDeath(player);` and keep the kick (French string).
   - **Delete** the `OnPlayerPVPKill` and `OnPlayerKilledByCreature` overrides from `ChallengeMode_Hardcore` (now centralized; do not touch the SemiHardcore class which has its own).
   - Keep `OnPlayerLogin` as-is (it now works thanks to the step-3 fix). Keep the kill+kick in `OnPlayerResurrect`, but gate it on `challengeEnabledForPlayer(SETTING_HARDCORE, player) && GetPlayerSetting("mod-challenge-modes", HARDCORE_DEAD).value`: `ResurrectPlayer` is also called by `.hardcore revive` after clearing the dead flag, and an ungated hook would immediately re-kill the revived character.

10. **GM commands** — add a `CommandScript` class `hardcore_commandscript` in `src/ChallengeModes.cpp` registering:
    ```cpp
    { "revive", HandleHardcoreReviveCommand, SEC_GAMEMASTER, Console::No },
    { "status", HandleHardcoreStatusCommand, SEC_PLAYER, Console::No },
    ```
    under `{ "hardcore", hardcoreTable }` (same command names as the replaced module; `.hardcore on` is intentionally NOT added — the shrine is the opt-in mechanism).
    - `HandleHardcoreStatusCommand`: reads `GetPlayerSetting("mod-challenge-modes", HARDCORE_DEAD)` then `SETTING_HARDCORE`; replies exactly `Hardcore : TOMBÉ — ce personnage est définitivement mort.` / `Hardcore : ACTIVÉ — la mort définitive est en vigueur.` / `Hardcore : désactivé pour ce personnage.`
    - `HandleHardcoreReviveCommand(ChatHandler*, Optional<PlayerIdentifier> target)` (offline-capable, pattern of the removed module):
      1. `target = PlayerIdentifier::FromTargetOrSelf(handler)` if empty; fail if still empty.
      2. `uint32 guid = target->GetGUID().GetCounter();`
      3. `CharacterDatabase.Query("SELECT `data` FROM `character_settings` WHERE `guid`={} AND `source`='mod-challenge-modes'", guid)` — no row → reply `{} n'est pas tombé.`, `SetSentErrorMessage(true)`, return false.
      4. Split `data` on spaces into tokens (core stores space-separated values; `SerializeSettingsData` in `PlayerSettings.cpp:45-57`). If `tokens.size() < 9 || tokens[8] != "1"` → same "n'est pas tombé" reply.
      5. Set `tokens[8] = "0"` (clear HARDCORE_DEAD only; index 0 = SETTING_HARDCORE stays untouched so the character remains Hardcore), rebuild the space-separated string with one trailing space, and `CharacterDatabase.Execute("UPDATE `character_settings` SET `data`='{}' WHERE `guid`={} AND `source`='mod-challenge-modes'", newData, guid)` (data contains only digits/spaces — SQL-safe).
      6. If `target->IsConnected()`: `UpdatePlayerSetting("mod-challenge-modes", HARDCORE_DEAD, 0)` on the live player, `RemoveAurasDueToSpell(27827)`, `ResurrectPlayer(!AccountMgr::IsPlayerAccount(security) ? 1.0f : 0.5f)`, `SpawnCorpseBones()`, `SaveToDB(false, false)`. Else: `CharacterDatabaseTransaction trans(nullptr); Player::OfflineResurrect(target->GetGUID(), trans);`
      7. Reply `{} réanimé. Toujours Hardcore : la prochaine mort sera définitive.`
    - Register in `AddSC_mod_challenge_modes()`: `new hardcore_commandscript();`

11. **French config comments** — translate every `#` comment line in `conf/challenge_modes.conf.dist` to French; keep all key names, default values, ordering and the two new keys' values byte-identical.

12. **Compile check** (mod-hardcore still present at this stage, harmless; removed in Phase B): `docker compose build ac-worldserver` must succeed.

### Phase B — Core repository switch

1. `git rm -r modules/mod-hardcore` (tracked files) and remove line 14 `!/modules/mod-hardcore` from `.gitignore`.
2. Delete stale untracked runtime files: `env/dist/etc/modules/mod_hardcore.conf` and `env/dist/etc/modules/mod_hardcore.conf.dist`.
3. Update the French guides (all prose French):
   - `GUIDE-ADMINISTRATEUR.md` line 269 table row → `Mort définitive | Module modules/mod-challenge-modes (dépôt cloné, patché local) | opt-in par personnage via le Shrine of Challenge (niveau 1 / DK ≤ 55) ; annonces serveur ; journal hardcore_deaths ; sauvetage GM .hardcore revive`.
   - `GUIDE-ADMINISTRATEUR.md` lines 379-397 → rewrite: `.hardcore revive [nom]` (offline-capable, character stays Hardcore), `.hardcore status`; config is `env/dist/etc/modules/challenge_modes.conf` with keys `ChallengeModes.Enable`, `Hardcore.Enable`, `Hardcore.AnnounceDeaths`, `Hardcore.LogDeaths` (all other `*.Enable = 0`); prerequisite `EnablePlayerSettings = 1` in `worldserver.conf`; shrine/npc_text SQL imported manually; journal table `hardcore_deaths`; note the characters DB was wiped (fresh start) and the AH bot is disabled until a new bot character exists — document re-enable: create a character, `SELECT guid, name FROM characters ORDER BY guid DESC LIMIT 5;`, set `AuctionHouseBot.GUIDs = <guid>` and `AuctionHouseBot.EnableSeller = true` in `env/dist/etc/modules/mod_ahbot.conf`, restart worldserver.
   - `GUIDE-JOUEUR.md` lines 38-44 → rewrite: death is permanent **only** for characters that activated Hardcore at the Shrine of Challenge (visible uniquement au niveau 1, DK ≤ 55) in a starting-area graveyard; announce serveur; `.hardcore status`; seul un administrateur peut réanimer (`.hardcore revive`).
   - `GUIDE-JOUEUR.md` lines 61-62 → change "la mort reste définitive, hardcore oblige" to "si tu as activé le mode Hardcore, la mort reste définitive".

### Phase C — Runtime configuration and SQL

1. Create the live config from the French dist and set the toggles:
   ```bash
   cp modules/mod-challenge-modes/conf/challenge_modes.conf.dist env/dist/etc/modules/challenge_modes.conf
   ```
   In `env/dist/etc/modules/challenge_modes.conf` set exactly: `ChallengeModes.Enable = 1`, `Hardcore.Enable = 1`, `Hardcore.AnnounceDeaths = 1`, `Hardcore.LogDeaths = 1`, and `SemiHardcore.Enable = 0`, `SelfCrafted.Enable = 0`, `ItemQualityLevel.Enable = 0`, `SlowXpGain.Enable = 0`, `VerySlowXpGain.Enable = 0`, `QuestXpOnly.Enable = 0`, `IronMan.Enable = 0` (reward keys stay empty/default).
2. `env/dist/etc/worldserver.conf`: set `EnablePlayerSettings = 1` (line ~2116; without it player settings reset every logout).
3. Disable the AH bot (its character is wiped): in `env/dist/etc/modules/mod_ahbot.conf` set `AuctionHouseBot.EnableSeller = false` and `AuctionHouseBot.GUIDs = 0`.
4. Import the world SQL (server can stay up; shrine appears after restart):
   ```bash
   docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" acore_world' < modules/mod-challenge-modes/data/sql/db-world/base/challenge_mode_gobject.sql
   docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" acore_world' < modules/mod-challenge-modes/data/sql/db-world/base/challenge_mode_gossip_fr.sql
   ```

### Phase D — Build, wipe, restart

1. `docker compose build ac-worldserver` (image now has challenge-modes, no mod-hardcore).
2. `docker compose stop ac-worldserver`.
3. Backup before the irreversible wipe:
   ```bash
   docker exec ac-database sh -c 'mysqldump -uroot -p"$MYSQL_ROOT_PASSWORD" acore_characters' > /tmp/backup-characters-$(date +%Y%m%d-%H%M%S).sql
   ```
4. Wipe all character data — preserve only account-level and system tables. Generate then execute:
   ```bash
   docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" -N -B' <<'SQL' > /tmp/wipe-characters.sql
   SELECT CONCAT('TRUNCATE TABLE `', table_name, '`;')
   FROM information_schema.tables
   WHERE table_schema = 'acore_characters'
     AND table_name NOT IN (
       'account_data','account_instance_times','account_tutorial','active_arena_season',
       'addons','banned_addons','chat_filter','channels_rights',
       'creature_respawn','game_event_condition_save','game_event_save','gameobject_respawn',
       'mail_server_template','mail_server_template_conditions','mail_server_template_items',
       'pool_quest_save','profanity_name','reserved_name','updates','updates_include',
       'world_state','worldstates');
   SQL
   (echo "SET FOREIGN_KEY_CHECKS=0;"; cat /tmp/wipe-characters.sql; echo "SET FOREIGN_KEY_CHECKS=1;") \
     | docker exec -i ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" acore_characters'
   docker exec ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" acore_characters -e "DROP TABLE IF EXISTS hardcore_flags;"'
   ```
   This truncates `characters`, all `character_*`, `item_instance`, `auctionhouse`, `mail`, `guild*`, `groups`, `arena_team*`, `petition*`, `corpse`, `pet_*`, `instance*`, `hardcore_deaths`, `hunger_thirst_state`, `character_settings`, logs/reports — and drops the orphaned `hardcore_flags` table from the removed module. `updates`/`updates_include` are preserved so the db-import service never re-runs.
5. `docker compose up -d ac-worldserver` (authserver and database stay up; db-import is already `Exited (0)` and is not recreated).

### Phase E — Commits (after verification)

1. Module repo: `git -C modules/mod-challenge-modes add -A && git -C modules/mod-challenge-modes commit -m "Adapt to current AzerothCore API; French strings; death announcements, journal and GM revive"` (local branch only, never push — no credentials).
2. Core repo: `git add -A && git commit -m "feat(Hardcore): replace mod-hardcore with mod-challenge-modes"` (deleted module, `.gitignore`, guides; `modules/mod-challenge-modes` is ignored and not part of this commit).

## Critical files & anchors

- `modules/mod-challenge-modes/src/ChallengeModes.cpp` — all code patches: `challengeEnabled` switch (~lines 22-47), WorldScript `LoadConfig`/new `OnStartup` (~230-320), `ChallengeMode_Hardcore` hooks (~405-460), gossip (~784-845), script registration (~851+).
- `modules/mod-challenge-modes/src/ChallengeModes.h` — `ChallengeModeSettings` enum (SETTING_HARDCORE=0 … HARDCORE_DEAD=8) and new `hardcoreAnnounceDeaths`/`hardcoreLogDeaths` members.
- `modules/mod-challenge-modes/conf/challenge_modes.conf.dist` — French comments plus `Hardcore.AnnounceDeaths`/`Hardcore.LogDeaths`.
- `env/dist/etc/modules/mod_ahbot.conf` — disable seller/GUIDs (lines ~76-79) because the bot character is wiped.
- `GUIDE-ADMINISTRATEUR.md` lines 269 and 379-397 (plus `GUIDE-JOUEUR.md` lines 38-44 and 61-62) — the old hardcore docs to rewrite.

## Verification

Automated (no game client needed), from the repository root:

1. Build: `docker compose build ac-worldserver` exits 0.
2. After restart, `docker logs ac-worldserver` shows `challenge_modes.conf` in the config list, **no** `mod_hardcore.conf`, and no module errors. With `AuctionHouseBot.EnableSeller = false` and `AuctionHouseBot.Buyer.Enabled = false`, mod-ah-bot-plus returns from `IsModuleEnabled()` before the "GUIDs is not configured" log line; the expected proof is the absence of the `AuctionHouseBot: (Re)populating item candidate lists` line, not that one.
3. Wipe proof: `docker exec ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" acore_characters -e "SELECT COUNT(*) FROM characters; SELECT COUNT(*) FROM auctionhouse; SELECT COUNT(*) FROM hardcore_deaths; SHOW TABLES LIKE '\''hardcore_flags'\'';"'` → `0`, `0`, `0`, empty result set.
4. Shrine proof: `docker exec ac-database sh -c 'mysql -uroot -p"$MYSQL_ROOT_PASSWORD" acore_world -e "SELECT COUNT(*) FROM gameobject WHERE id=254605; SELECT COUNT(*) FROM npc_text WHERE ID=1266900;"'` → `9` and `1`.
5. Config proof: `grep -E '^EnablePlayerSettings' env/dist/etc/worldserver.conf` → `1`; `grep -E '^(ChallengeModes|Hardcore)\.' env/dist/etc/modules/challenge_modes.conf` shows the toggles; `grep -E '^AuctionHouseBot\.(EnableSeller|GUIDs)' env/dist/etc/modules/mod_ahbot.conf` → `false` / `0`.
6. French strings compiled in: `docker exec ac-worldserver sh -c 'grep -a -c "Activer le mode Hardcore" /azerothcore/env/dist/bin/worldserver'` → ≥ 1.
7. Repos clean after commits: `git status --short --branch` clean in the core repo; `git -C modules/mod-challenge-modes status --short --branch` clean.

Manual in-game checklist (requires the game client — hand to the user if unavailable):

1. Create a level-1 character; the Shrine of Challenge is visible near the starting-area graveyard; interact → `Activer le mode Hardcore` → `Le mode de défi est activé.`; DB: `SELECT data FROM character_settings WHERE source='mod-challenge-modes';` shows `1 ` for the new GUID.
2. `.hardcore status` → `Hardcore : ACTIVÉ — la mort définitive est en vigueur.`
3. Die → server-wide French announcement, one new `hardcore_deaths` row, ghost state; releasing the ghost kicks with `Personnage Hardcore définitivement mort.`; relogging kills+kicks.
4. GM `.hardcore revive <nom>` on the offline fallen character → resurrects, `character_settings` token 8 becomes `0`, `.hardcore status` → ACTIVÉ.
5. AH bot re-enable (after creating a bot character): set `AuctionHouseBot.GUIDs = <guid>`, `AuctionHouseBot.EnableSeller = true`, `docker compose restart ac-worldserver`, auctions appear again.

## Assumptions & contingencies

- **Characters wipe is irreversible by design** (explicit user decision); the mysqldump in Phase D is the only rollback. If the user wants the old state back, restore that dump into `acore_characters` and rebuild without this plan's module changes.
- **AH bot after the wipe**: its original character (GUID 6) was deleted. It was later re-enabled with `Bothv` (GUID 4); the re-enable procedure is documented in the admin guide.
- `.hardcore status` is added for parity with the replaced module (the player guide references it); `.hardcore on` is not added — the shrine is the opt-in mechanism by design.
- Shrine level restriction (level 1 / DK ≤ 55) is kept unchanged; the wipe makes existing characters moot.
- The module README stays English (upstream developer doc); French documentation lives in the server guides.
- If `npc_text` ID 1266900 turns out occupied at execution (verified free now), use `1266901` in both the SQL file and the `SendGossipMenuFor` call.
- If compilation fails after the patches, fix errors iteratively; only the step-2 `bool&` change is known-mandatory. `Hardcore.DisableLevel` stays `0` and `Hardcore.XPMultiplier` stays `1` (no behavior change requested).
- If a `TRUNCATE` fails, first check the generated statement: table names must be backtick-quoted (`groups` is a reserved word in MySQL 8). The wrapped `SET FOREIGN_KEY_CHECKS=0` handles FK constraints; any other failure means the preserve list is wrong — inspect the table and decide (system tables are preserved, character-data tables are not).
- `docker compose up -d ac-worldserver` does not re-run `ac-db-import` (it is `Exited (0)` and `updates`/`updates_include` are preserved); if it ever re-ran, it would be a no-op.