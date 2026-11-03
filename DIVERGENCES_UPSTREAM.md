# Écart avec AzerothCore upstream

État au 2026-11-03, branche `feature/mod-world-party-scaling`.

- Upstream comparé : `5c5fe0bed` (`master`)
- Base commune : `993b45959` (dernière fusion)
- La branche n'a pas encore mergé les 25 commits upstream suivants.

## Réponse courte

- **Groupes de 6 : oui, c'est isolé.** Le core ne contient plus la politique « 6 » : `MAXGROUPSIZE`
  est revenu à 5 et un hook générique `GroupScript::GetMaxPartySize()` permet au module
  `mod-six-player-party` de fournir la valeur 6. Sans le module, la limite de groupe revient à 5.
- **Le reste du core : non, pas encore.** Le core porte encore des patchs de gameplay vanilla
  (régénération, coûts de mana, taux des créatures, factions hostiles, pets/stats). Ce ne sont pas
  des modules.
- **Modules :** un seul est suivi dans le dépôt serveur, `mod-world-party-scaling` (copie
  vendorisée). Les dix autres, dont `mod-six-player-party`, sont des dépôts git séparés, ignorés
  par le dépôt serveur.
- **Upstream :** la branche est en retard de 25 commits ; il faut merger/rebaser pour être
  réellement « core + modules ».

## Ce qui reste dans le core

| Sujet | Fichiers principaux | Pourquoi |
|---|---|---|
| Groupes de 6 | `Groups/*`, `Maps/Map.cpp`, `LFG/*`, `Achievements/*`, `Handlers/ChatHandler.cpp`, `Entities/Unit/Unit.cpp`, `Scripting/*` | Hook runtime et sémantique « une party = un groupe logique » ; `MAXGROUPSIZE=5`. La valeur 6 est dans le module. |
| Régénération vanilla | `Entities/Player/Player.cpp`, `Entities/Unit/StatSystem.cpp`, `World/WorldConfig.*`, `worldserver.conf.dist` | Formules 1.12.1, `VanillaRegen.Enable`, suppression du boost 3.3 bas niveau. |
| Coûts de mana vanilla | `Spells/SpellInfo.cpp`, `Spells/SpellMgr.*` | Table `vanilla_spell_cost` et calcul des coûts. |
| Taux créatures / pets | `Entities/Creature/Creature.*`, `Entities/Pet/Pet.cpp`, `Entities/Unit/Unit.cpp`, `Globals/ObjectMgr.cpp` | Taux vanilla, exclusion pets/totems, `Pet.RankMod.Health` retiré. |
| Factions / élites | SQL world + `ObjectMgr.cpp` | Restauration des rouges/élites vanilla. |

## Ce qui n'est pas dans le core

| Élément | Statut |
|---|---|
| `mod-world-party-scaling` | **Suivi** dans le dépôt serveur (vendorisé), alors qu'un dépôt public existe. |
| `mod-six-player-party` | Dépôt séparé, public : https://github.com/Phanty78/mod-six-player-party |
| `mod-ah-bot-plus`, `mod-challenge-modes`, `mod-combat-ai`, `mod-forever-journal`, `mod-hunger-thirst`, `mod-hunts`, `mod-living-world`, `mod-vanilla-spell-cost`, `mod-weather-vibe` | Clones git séparés, ignorés par le dépôt serveur. |
| Config | `conf/dist/env.ac` (taux, XP −10 %, aggro, regen). |
| SQL | 5 mises à jour `db_world` (regen, élites, factions, coûts de mana). |
| Docker / docs | `apps/docker/entrypoint.sh`, guides, changelog. |

## Pour se rapprocher de « core + modules »

1. Merger/rebaser sur `upstream/master` (25 commits de retard).
2. Décider du sort des patchs vanilla : les garder comme patch core documenté, ou les extraire en
   modules quand c'est possible.
3. Sortir `mod-world-party-scaling` du dépôt serveur et le cloner comme les autres, si son dépôt
   public est à jour.
4. Garder `mod-six-player-party` en dépôt séparé, avec le hook core documenté dans `docs/CORE.md`.

## État déployé

Le conteneur `ac-worldserver` n'a pas été rebuild : il tourne encore sur l'ancien binaire hardcodé
6. Un rebuild depuis ce checkout inclut `modules/mod-six-player-party` et donne 6 ; un clone propre
du dépôt serveur sans le module donne 5.
