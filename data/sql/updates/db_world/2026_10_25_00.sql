-- DB update 2026_10_22_00 -> 2026_10_25_00
-- Restauration de l'agressivité (faction) des créatures des zones de départ TBC
-- (Azuremyst Isle / Eversong Woods) devenues jaunes en 3.3.5a.
--
-- Source TBC : cmangos/tbc-db @ 9257b1381ea689404de4268ba667135e273f1301
--   (Full_DB/TBCDB_1.11.0_Vengeance_One_A_Cmangos_Story.sql.gz, client 2.4.3,
--    SHA256 dfd269a33923debba9954dfe8d120a04ffef3a3d683a513a9a5634f98808d8fb).
-- Comparaison acore_world.creature_template (live) vs tbc_ref_audit.creature_template
--   sur les créatures spawnées dans les bboxes des deux zones.
--
-- Sémantique hostile/rouge : acore_world.factiontemplate_dbc, EnemyGroup & 0b111
--   (PLAYER=1, ALLIANCE=2, HORDE=4). Faction 14/16 -> EnemyGroup = 1 (PLAYER) = rouge ;
--   faction 7 -> EnemyGroup = 0 = jaune.
--
-- 9 entrées concernées (faction live = 7, faction TBC = 14/16) :
--   Azuremyst Isle (Draenei) : 16517, 16521, 16522, 16537
--   Eversong Woods (Elfe de sang) : 15271, 15273, 15294, 15298, 15367
-- Aucune de ces entrées n'est couverte par 2026_10_22_00.sql (passe Vanilla/ClassicDB 1.12.1).
-- Contrôle Bloodmyst Isle / Ghostlands : aucun écart de couleur TBC -> live.
--
-- Idempotent (garde `faction` = 7, valeur live actuelle).

UPDATE `creature_template` SET `faction` = 14 WHERE `faction` = 7 AND `npcflag` = 0 AND `entry` IN (15367, 16517, 16537);
UPDATE `creature_template` SET `faction` = 16 WHERE `faction` = 7 AND `npcflag` = 0 AND `entry` IN (15271, 15273, 15294, 15298, 16521, 16522);
