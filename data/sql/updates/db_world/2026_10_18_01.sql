-- DB update 2026_10_18_00 -> 2026_10_18_01
-- Restauration du statut « élite » des créatures rétrogradées en « normal » par le patch 2.3
-- (« Many elite creatures and quests in the level-1-60 experience have been changed to accommodate solo play »).
--
-- Source vanilla : cmangos/classic-db @ ec4f596146be6467ea93c57397858e329e2db852
--   (Full_DB/ClassicDB_1_12_1_z2815.sql.gz, table creature_template ; 10 384 entrées).
-- Comparaison avec acore_world.creature_template (base live, 29 945 entrées) sur les 10 383 entrées communes,
-- sémantique `rank` = CreatureEliteType (src/server/shared/SharedDefines.h:2963-2969) :
--   0 normal, 1 élite, 2 rare-élite, 3 boss, 4 rare.
--
-- Transitions retenues (vanilla -> live) :
--   rank 1 -> 0 : 267 entrées (261 de niveau vanilla <= 60, dont répartition par palier : 1-9 : 1 ; 10-19 : 42 ; 20-29 : 38 ; 30-39 : 49 ; 40-49 : 60 ; 50-60 : 71 ;
--                 6 à niveau vanilla > 60 : élites de Silithus 11734, 11896, 15215, 15286, 15288, 15290).
--   rank 2 -> 0 : 1 entrée (723 Mosh'Ogg Butcher ; aucun spawn en base, restauration sans effet en jeu).
--   Les transitions inverses (normal -> élite : 9 ; élite -> rare-élite : 12 ; élite -> boss : 6) ne sont PAS touchées.
--   40 des 261 entrées <= 60 n'ont aucun spawn dans `creature` (mobs invoqués par script,
--   ex. les 9 Councilmen de Pyrewood Village) : restauration inoffensive, conservée pour la fidélité 1.12.1.
--   5 entrées spawnent à la fois en extérieur et en instance (1725 Deadmines, 4283 Scarlet Monastery,
--   4851 Uldaman, 5263 & 5269 Sunken Temple) : elles redeviennent élites dans les deux contextes, comme en 1.12.1.
--
-- Effets en jeu (aucune modification de niveau) :
--   * XP : x2 pour les élites (src/server/game/Miscellaneous/Formulas.cpp:89, via Creature::isElite()) ;
--   * montée de compétence de récolte x2 (src/server/game/Spells/SpellEffects.cpp:5166) ;
--   * délai de décès du cadavre « élite » (Creature.cpp:1202-1220, CONFIG_CORPSE_DECAY_ELITE) ;
--   * temps de réapparition sauvegardé à la mort (Creature.cpp:1984) ;
--   * rang transmis au client dans SMSG_CREATURE_QUERY_RESPONSE (Creature.cpp:192) — cadre « élite ».
--   * PV/dégâts : INCHANGÉS sur ce serveur, car conf/dist/env.ac fixe tous les Rate.Creature.* (donc aussi
--     AC_RATE_CREATURE_ELITE_ELITE_HP/DAMAGE/SPELL_DAMAGE) à 1,35 — le rang ne pilote que ce multiplicateur
--     côté cœur (Creature.cpp:1577-1632). Pour retrouver des élites plus coriaces, il faut relever les clés
--     AC_RATE_CREATURE_ELITE_ELITE_* / RAREELITE_* (décision de tuning, non appliquée ici).
--
-- Idempotent (garde `rank` = 0). Appliqué automatiquement au démarrage (Updates.EnableDatabases = 7).

-- Élites extérieures (vanilla rank 1) repassées en normal par le patch 2.3 : 267 entrées.
UPDATE `creature_template` SET `rank` = 1
WHERE `rank` = 0 AND `entry` IN (
      314, 397, 436, 594, 619, 623, 624, 625, 626, 678, 679, 680,
    709, 710, 728, 730, 813, 818, 871, 873, 875, 877, 879, 1051,
    1052, 1053, 1054, 1178, 1179, 1180, 1181, 1183, 1225, 1364, 1388, 1559,
    1725, 1726, 1760, 1788, 1827, 1832, 1834, 1891, 1892, 1893, 1894, 1895,
    1896, 1947, 2060, 2061, 2062, 2063, 2064, 2065, 2066, 2067, 2068, 2091,
    2106, 2166, 2254, 2255, 2256, 2257, 2287, 2304, 2344, 2345, 2346, 2416,
    2420, 2421, 2422, 2558, 2569, 2570, 2571, 2583, 2584, 2585, 2588, 2590,
    2591, 2597, 2599, 2607, 2611, 2612, 2635, 2641, 2642, 2643, 2644, 2645,
    2646, 2647, 2648, 2681, 2738, 2757, 2759, 2763, 2773, 2780, 2781, 2782,
    2783, 2794, 2835, 2892, 2932, 3528, 3529, 3530, 3531, 3532, 3533, 3630,
    3631, 3632, 3633, 3634, 3638, 3641, 3655, 4050, 4052, 4056, 4061, 4064,
    4065, 4280, 4281, 4282, 4283, 4284, 4285, 4394, 4409, 4462, 4464, 4465,
    4468, 4469, 4499, 4788, 4789, 4802, 4803, 4844, 4845, 4846, 4851, 4856,
    4872, 5224, 5225, 5235, 5243, 5261, 5263, 5269, 5401, 5402, 5645, 5646,
    5647, 5780, 5833, 5860, 5861, 5862, 6132, 6140, 6208, 6210, 6213, 6231,
    6523, 6669, 6733, 7040, 7041, 7042, 7043, 7044, 7045, 7046, 7053, 7135,
    7136, 7728, 7734, 7735, 7872, 7873, 7874, 7977, 7995, 7996, 8075, 8391,
    8400, 8419, 8447, 8518, 8636, 9043, 9044, 9461, 10581, 10608, 10737, 10738,
    10741, 10802, 10806, 10807, 10882, 10953, 11141, 11440, 11442, 11443, 11698, 11721,
    11722, 11723, 11724, 11725, 11726, 11728, 11729, 11730, 11731, 11732, 11733, 11734,
    11777, 11778, 11781, 11782, 11785, 11786, 11787, 11788, 11896, 11897, 11920, 11921,
    12262, 12263, 12579, 12865, 14388, 14467, 14621, 15209, 15211, 15212, 15215, 15286,
    15288, 15290, 15307);

-- Rare-élite (vanilla rank 2) rétrogradée en normal : 1 entrée (aucun spawn en jeu).
UPDATE `creature_template` SET `rank` = 2
WHERE `rank` = 0 AND `entry` IN (
      723);
