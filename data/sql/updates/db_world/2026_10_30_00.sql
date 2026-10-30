-- DB update 2026_10_28_01 -> 2026_10_30_00
-- Restauration de l'agressivité (faction) des monstres rouges de Vanilla devenus jaunes en 3.3.5a.
--
-- Source vanilla : cmangos/classic-db @ ec4f596146be6467ea93c57397858e329e2db852
--   (Full_DB/ClassicDB_1_12_1_z2815.sql.gz, table creature_template ; 10 384 entrées).
-- Comparaison avec acore_world.creature_template (base live, 29 945 entrées) sur les 10 383 entrées communes.
-- Sémantique hostile/rouge : acore_world.factiontemplate_dbc, EnemyGroup & 0b111 (PLAYER=1, ALLIANCE=2, HORDE=4)
--   = IsHostileToPlayers/AlliancePlayers/HordePlayers du cœur (src/server/shared/DataStores/DBCStructure.h:1017-1019).
--   « Rouge » = hostile à au moins un groupe de joueurs ; « jaune » = hostileMask & 0b111 == 0.
--
-- Transitions vanilla -> live : 145 rouges devenus jaunes. 138 restaurées (colonne `faction` = valeur vanilla).
--   7 EXCLUES (npcflag != 0 : PNJ d'interaction — quête/vendeur/gossip — jamais hostiles en pratique) :
--     7956 Kindal Moonweaver (124->42), 10800 Warosh the Redeemed (14->35), 12580 Reginald Windsor (12->534),
--     13431 Whulwert Copperpinch (85->35), 13449 Warmaster Garrick (85->1554),
--     15351 Alliance Brigadier General (55->534), 17209 William Kielar (84->35).
--   232 transitions inverses (jaune vanilla -> rouge live) NON touchées.
--   Toutes les factions vanilla restaurées existent dans factiontemplate_dbc (aucune manquante) et y sont
--   bien rouges selon la DBC 3.3.5 (interprétation de la valeur restaurée = comportement en jeu réel).
--
-- Répartition (niveau vanilla min) : 1-9 : 72 ; 10-19 : 12 ; 20-29 : 8 ; 30-39 : 12 ; 40-49 : 6 ; 50-60 : 25 ; >60 : 3
--   (les >60 sont des PNJ d'événement/test sans spawn vivant : 12339 Demetria, 14452 Enslaved Doomguard
--    Commander, 16474 Blizzard).
-- 95 des 138 n'ont aucun spawn dans `creature` (invoquées par script ou retirées) : restauration inoffensive,
--   conservée pour la fidélité 1.12.1 (même convention que 2026_10_18_01.sql).
-- 2 spawnent aussi en instance (1706 Defias Prisoner, map 34 ; 4625 Death's Head Ward Keeper, map 47) :
--   elles redeviennent rouges dans les deux contextes, comme en 1.12.1.
-- 2 figurent aussi dans la restauration de rang élite (11726, 14467) : colonnes indépendantes.
--
-- Idempotent (garde `faction` = valeur live courante). Appliqué automatiquement au démarrage (Updates.EnableDatabases = 7).

UPDATE `creature_template` SET `faction` = 14 WHERE `faction` = 7 AND `entry` IN (2931, 2966, 3101, 3102, 3229, 8554, 14452);
UPDATE `creature_template` SET `faction` = 16 WHERE `faction` = 7 AND `entry` IN (3183, 3503);
UPDATE `creature_template` SET `faction` = 17 WHERE `faction` = 7 AND `entry` IN (38, 103);
UPDATE `creature_template` SET `faction` = 18 WHERE `faction` = 7 AND `entry` IN (6250);
UPDATE `creature_template` SET `faction` = 22 WHERE `faction` = 7 AND `entry` IN (1505, 1688, 1986, 1994);
UPDATE `creature_template` SET `faction` = 45 WHERE `faction` = 7 AND `entry` IN (2667);
UPDATE `creature_template` SET `faction` = 59 WHERE `faction` = 7 AND `entry` IN (6017);
UPDATE `creature_template` SET `faction` = 67 WHERE `faction` = 7 AND `entry` IN (1506, 1507, 1667);
UPDATE `creature_template` SET `faction` = 73 WHERE `faction` = 7 AND `entry` IN (6013);
UPDATE `creature_template` SET `faction` = 74 WHERE `faction` = 7 AND `entry` IN (6016);
UPDATE `creature_template` SET `faction` = 89 WHERE `faction` = 7 AND `entry` IN (12339);
UPDATE `creature_template` SET `faction` = 91 WHERE `faction` = 7 AND `entry` IN (5354, 5893, 5896, 6492);
UPDATE `creature_template` SET `faction` = 92 WHERE `faction` = 7 AND `entry` IN (5889, 5898);
UPDATE `creature_template` SET `faction` = 107 WHERE `faction` = 7 AND `entry` IN (946);
UPDATE `creature_template` SET `faction` = 111 WHERE `faction` = 7 AND `entry` IN (2953, 2954);
UPDATE `creature_template` SET `faction` = 14 WHERE `faction` = 15 AND `entry` IN (2188);
UPDATE `creature_template` SET `faction` = 26 WHERE `faction` = 25 AND `entry` IN (80);
UPDATE `creature_template` SET `faction` = 1 WHERE `faction` = 35 AND `entry` IN (12257, 14362);
UPDATE `creature_template` SET `faction` = 2 WHERE `faction` = 35 AND `entry` IN (6867);
UPDATE `creature_template` SET `faction` = 4 WHERE `faction` = 35 AND `entry` IN (2671);
UPDATE `creature_template` SET `faction` = 12 WHERE `faction` = 35 AND `entry` IN (12581);
UPDATE `creature_template` SET `faction` = 14 WHERE `faction` = 35 AND `entry` IN (29, 6240, 7863, 8776, 11195, 14021, 14667, 16129, 16474);
UPDATE `creature_template` SET `faction` = 28 WHERE `faction` = 35 AND `entry` IN (11817, 11819);
UPDATE `creature_template` SET `faction` = 40 WHERE `faction` = 35 AND `entry` IN (7013);
UPDATE `creature_template` SET `faction` = 55 WHERE `faction` = 35 AND `entry` IN (1352, 1380, 15745);
UPDATE `creature_template` SET `faction` = 67 WHERE `faction` = 35 AND `entry` IN (13158);
UPDATE `creature_template` SET `faction` = 79 WHERE `faction` = 35 AND `entry` IN (14645);
UPDATE `creature_template` SET `faction` = 84 WHERE `faction` = 35 AND `entry` IN (3946);
UPDATE `creature_template` SET `faction` = 85 WHERE `faction` = 35 AND `entry` IN (10657);
UPDATE `creature_template` SET `faction` = 90 WHERE `faction` = 35 AND `entry` IN (8668);
UPDATE `creature_template` SET `faction` = 91 WHERE `faction` = 35 AND `entry` IN (5902, 7364, 7365, 11256);
UPDATE `creature_template` SET `faction` = 115 WHERE `faction` = 35 AND `entry` IN (510);
UPDATE `creature_template` SET `faction` = 151 WHERE `faction` = 35 AND `entry` IN (5044, 5045, 5046, 5088);
UPDATE `creature_template` SET `faction` = 168 WHERE `faction` = 35 AND `entry` IN (10290);
UPDATE `creature_template` SET `faction` = 1074 WHERE `faction` = 35 AND `entry` IN (5194, 5196, 5197);
UPDATE `creature_template` SET `faction` = 1154 WHERE `faction` = 35 AND `entry` IN (11153, 11154, 11155);
UPDATE `creature_template` SET `faction` = 1214 WHERE `faction` = 35 AND `entry` IN (14744);
UPDATE `creature_template` SET `faction` = 1216 WHERE `faction` = 35 AND `entry` IN (14745);
UPDATE `creature_template` SET `faction` = 1606 WHERE `faction` = 35 AND `entry` IN (15458);
UPDATE `creature_template` SET `faction` = 1 WHERE `faction` = 58 AND `entry` IN (14752);
UPDATE `creature_template` SET `faction` = 2 WHERE `faction` = 58 AND `entry` IN (15112);
UPDATE `creature_template` SET `faction` = 4 WHERE `faction` = 58 AND `entry` IN (14465);
UPDATE `creature_template` SET `faction` = 6 WHERE `faction` = 58 AND `entry` IN (12141);
UPDATE `creature_template` SET `faction` = 37 WHERE `faction` = 58 AND `entry` IN (8179, 14987);
UPDATE `creature_template` SET `faction` = 82 WHERE `faction` = 58 AND `entry` IN (3560, 10183);
UPDATE `creature_template` SET `faction` = 109 WHERE `faction` = 58 AND `entry` IN (3844);
UPDATE `creature_template` SET `faction` = 152 WHERE `faction` = 58 AND `entry` IN (2992);
UPDATE `creature_template` SET `faction` = 1335 WHERE `faction` = 58 AND `entry` IN (14466, 14751);
UPDATE `creature_template` SET `faction` = 72 WHERE `faction` = 120 AND `entry` IN (8421);
UPDATE `creature_template` SET `faction` = 154 WHERE `faction` = 153 AND `entry` IN (4625);
UPDATE `creature_template` SET `faction` = 1 WHERE `faction` = 188 AND `entry` IN (7391, 7543, 7545, 7549, 7560, 8376, 9657, 11325, 16548);
UPDATE `creature_template` SET `faction` = 3 WHERE `faction` = 188 AND `entry` IN (11326);
UPDATE `creature_template` SET `faction` = 4 WHERE `faction` = 188 AND `entry` IN (7392, 7550, 10259, 10598, 11327, 15706, 16701);
UPDATE `creature_template` SET `faction` = 5 WHERE `faction` = 188 AND `entry` IN (9656);
UPDATE `creature_template` SET `faction` = 14 WHERE `faction` = 188 AND `entry` IN (7568, 7569);
UPDATE `creature_template` SET `faction` = 55 WHERE `faction` = 188 AND `entry` IN (7380);
UPDATE `creature_template` SET `faction` = 67 WHERE `faction` = 188 AND `entry` IN (7566);
UPDATE `creature_template` SET `faction` = 115 WHERE `faction` = 188 AND `entry` IN (7387, 7544, 9662, 12419, 16549);
UPDATE `creature_template` SET `faction` = 116 WHERE `faction` = 188 AND `entry` IN (7383);
UPDATE `creature_template` SET `faction` = 14 WHERE `faction` = 189 AND `entry` IN (14273);
UPDATE `creature_template` SET `faction` = 79 WHERE `faction` = 189 AND `entry` IN (4066);
UPDATE `creature_template` SET `faction` = 91 WHERE `faction` = 189 AND `entry` IN (2749, 8981);
UPDATE `creature_template` SET `faction` = 111 WHERE `faction` = 189 AND `entry` IN (2952);
UPDATE `creature_template` SET `faction` = 1055 WHERE `faction` = 189 AND `entry` IN (10817);
UPDATE `creature_template` SET `faction` = 107 WHERE `faction` = 190 AND `entry` IN (706);
UPDATE `creature_template` SET `faction` = 17 WHERE `faction` = 290 AND `entry` IN (1706);
UPDATE `creature_template` SET `faction` = 310 WHERE `faction` = 311 AND `entry` IN (11726);
UPDATE `creature_template` SET `faction` = 92 WHERE `faction` = 474 AND `entry` IN (15061);
UPDATE `creature_template` SET `faction` = 92 WHERE `faction` = 534 AND `entry` IN (9601);
UPDATE `creature_template` SET `faction` = 694 WHERE `faction` = 534 AND `entry` IN (8024, 8025);
UPDATE `creature_template` SET `faction` = 14 WHERE `faction` = 1194 AND `entry` IN (14467);
UPDATE `creature_template` SET `faction` = 6 WHERE `faction` = 1555 AND `entry` IN (14878);
UPDATE `creature_template` SET `faction` = 56 WHERE `faction` = 2000 AND `entry` IN (1000);
UPDATE `creature_template` SET `faction` = 37 WHERE `faction` = 2136 AND `entry` IN (808);
