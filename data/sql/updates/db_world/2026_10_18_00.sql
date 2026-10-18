-- DB update 2026_10_17_03 -> 2026_10_18_00
-- Régénération de vie hors combat « façon vanilla 1.12.1 » aux niveaux 1-30.
-- Formules vanilla (cmangos, par seconde, esprit S) : PV/s = a*S + b
--   guerrier 0.63*S-11.3 | paladin 0.125*S | chasseur 0.215*S-2.75 | voleur 0.42*S-6.5
--   prêtre 0.075*S+0.7 | chaman 0.14*S-1.8 | mage 0.055*S+0.5 | démoniste 0.06*S+0.75 | druide 0.055*S+0.5
-- Esprit de référence : player_class_stats.Spirit (base humaine — player_race_stats humain = 0).
-- Niveaux 1-20 : coefficient vanilla pur. 21-30 : fondu linéaire vers la valeur WotLK stock. 31+ : inchangé.
-- NON IDEMPOTENT (le fondu lit la valeur courante) — l'updater AC l'applique une seule fois ; ne PAS rejouer à la main.
-- NOTE: CAST(s.Level AS SIGNED) obligatoire — player_class_stats.Level est tinyint UNSIGNED,
--       sinon « ERROR 1690 BIGINT UNSIGNED value is out of range in (s.Level - 20) » en MySQL 8 strict mode (vérifié en dry-run).
-- À accompagner de : EnableLowLevelRegenBoost = 0 (défaut changé dans worldserver.conf.dist et WorldConfig.cpp).

-- 1) gtRegenHPPerSpt (part esprit > 50) : même facteur relatif que gtOCTRegenHP.
--    IMPÉRATIF : exécuter AVANT le statement 2 (il lit la valeur stock de gtoctregenhp_dbc).
UPDATE gtregenhpperspt_dbc g
JOIN gtoctregenhp_dbc o ON o.ID = g.ID
JOIN player_class_stats s ON s.Class = FLOOR(g.ID / 100) + 1 AND s.Level = (g.ID % 100) + 1
SET g.Data = g.Data * (
      GREATEST((CAST(s.Level AS SIGNED) - 20) / 10, 0)
    + LEAST((30 - CAST(s.Level AS SIGNED)) / 10, 1) * GREATEST(
        CASE FLOOR(g.ID / 100) + 1
            WHEN 1  THEN 0.63  - 11.3 / s.Spirit
            WHEN 2  THEN 0.125
            WHEN 3  THEN 0.215 - 2.75 / s.Spirit
            WHEN 4  THEN 0.42  - 6.5  / s.Spirit
            WHEN 5  THEN 0.075 + 0.7  / s.Spirit
            WHEN 7  THEN 0.14  - 1.8  / s.Spirit
            WHEN 8  THEN 0.055 + 0.5  / s.Spirit
            WHEN 9  THEN 0.06  + 0.75 / s.Spirit
            WHEN 11 THEN 0.055 + 0.5  / s.Spirit
        END, 0) / o.Data)
WHERE o.Data > 0
  AND s.Class IN (1, 2, 3, 4, 5, 7, 8, 9, 11)
  AND s.Level BETWEEN 1 AND 30;

-- 2) gtOCTRegenHP (part esprit <= 50) : vanilla 1-20, fondu 21-30.
UPDATE gtoctregenhp_dbc o
JOIN player_class_stats s ON s.Class = FLOOR(o.ID / 100) + 1 AND s.Level = (o.ID % 100) + 1
SET o.Data =
      LEAST((30 - CAST(s.Level AS SIGNED)) / 10, 1) * GREATEST(
        CASE FLOOR(o.ID / 100) + 1
            WHEN 1  THEN 0.63  - 11.3 / s.Spirit
            WHEN 2  THEN 0.125
            WHEN 3  THEN 0.215 - 2.75 / s.Spirit
            WHEN 4  THEN 0.42  - 6.5  / s.Spirit
            WHEN 5  THEN 0.075 + 0.7  / s.Spirit
            WHEN 7  THEN 0.14  - 1.8  / s.Spirit
            WHEN 8  THEN 0.055 + 0.5  / s.Spirit
            WHEN 9  THEN 0.06  + 0.75 / s.Spirit
            WHEN 11 THEN 0.055 + 0.5  / s.Spirit
        END, 0)
    + GREATEST((CAST(s.Level AS SIGNED) - 20) / 10, 0) * o.Data
WHERE s.Class IN (1, 2, 3, 4, 5, 7, 8, 9, 11)
  AND s.Level BETWEEN 1 AND 30;
