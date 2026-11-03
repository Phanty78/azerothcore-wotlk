# Rapport de mise à jour des modules

Audit du 2026-11-03. Les mises à jour git ont été appliquées ensuite ; aucun rebuild ni restart n'a été lancé.

Méthode : `git fetch --all --prune` dans chaque dépôt imbriqué, comparaison `HEAD...@{u}`,
`git status --porcelain` pour les modifications locales. Pour `mod-world-party-scaling`, qui est
vendorisé dans le dépôt serveur et n'a pas de `.git`, comparaison avec le dépôt public
`Phanty78/mod-world-party-scaling`.

## Mises à jour appliquées (git uniquement)

| Module | Avant | Après | Remarque |
|---|---|---|---|
| `mod-forever-journal` | `4300e6b` | `988a1d3` (2.7.0) | modification SQL locale préservée (stash/pop) |
| `mod-combat-ai` | `b09aa11` | `7303e39` | arbre propre |
| `mod-hunger-thirst` | `9479227` | `5ae6fc2` | arbre propre |
| `mod-world-party-scaling` (public) | `930c373` | `d1761c0` | docs synchronisées ; vendorisé identique hors `README.md.orig` |

Aucun build, aucun redémarrage : le conteneur `ac-worldserver` est resté `running` sur son binaire
existant.

## Résumé de l'audit (avant application)

| Module | HEAD | Upstream | Écart | État | Action |
|---|---|---|---|---|---|
| `mod-ah-bot-plus` | `f685832` 2026-07-02 | `origin/master` `f685832` | 0 / 0 | à jour | rien ; HEAD détaché, optionnel `git checkout master` |
| `mod-challenge-modes` | `4c5444b` 2026-10-25 | `origin/master` `1930525` 2025-11-25 | +7 / 0 | à jour upstream, 7 commits locaux | ne pas pull ; conserver `local-adaptations` |
| `mod-combat-ai` | `b09aa11` 2026-10-25 | `origin/main` `7303e39` 2026-11-01 | 0 / 1 | **en retard** | fast-forward + rebuild |
| `mod-forever-journal` | `4300e6b` 2026-10-29 | `origin/main` `988a1d3` 2026-11-02 | 0 / 8 | **en retard** | sauver le SQL local, update module + addon + rebuild |
| `mod-hunger-thirst` | `9479227` 2026-10-25 | `origin/main` `5ae6fc2` 2026-11-01 | 0 / 1 | **en retard** | fast-forward ; docs/CI seulement, pas de rebuild |
| `mod-hunts` | `417ef2d` 2026-09-14 | `origin/main` `417ef2d` | 0 / 0 | à jour | rien ; tag `pre-native-split` non fusionné, ne pas pull |
| `mod-living-world` | `116926e` 2026-09-01 | `origin/main` `116926e` | 0 / 0 | à jour | rien ; clone shallow |
| `mod-six-player-party` | `73c5e59` 2026-11-03 | `origin/main` `73c5e59` | 0 / 0 | à jour | rien |
| `mod-vanilla-spell-cost` | `e4ab38c` 2026-11-03 | `origin/master` `e4ab38c` | 0 / 0 | à jour | rien ; rappel build `ac-db-import` pour le SQL |
| `mod-weather-vibe` | `cb854eb` 2026-04-06 | `origin/main` `cb854eb` | 0 / 0 | à jour | rien ; amont figé depuis avril |
| `mod-world-party-scaling` | vendorisé, code/conf = public `930c373` | public `main` `930c373` (v1.1.0) | docs divergentes | **à synchroniser** | pousser les docs locales vers le dépôt public ; retirer `README.md.orig` |

## Détails des mises à jour

### `mod-forever-journal` — priorité haute

- 8 commits d'avance côté amont, de la 2.5.7 à la 2.7.0.
- Dernier commit : `988a1d3` (2026-11-02), « Previews: turn, move and zoom, saved per creature ».
- **Modification locale non commitée** : `data/sql/db-world/updates/mod_forever_journal_script_spells.sql`
  (diff : 152 insertions / 300 suppressions, net −148 lignes ; retrait des entrées `_40` et
  réorganisation de quelques `INSERT`/`Sort`, notamment `boss_eregos` et `boss_gormok`). La
  sauvegarder avant toute mise à jour.
- Les 8 commits amont ne touchent pas ce fichier SQL, donc un `pull --ff-only` devrait conserver
  la modification locale ; sauvegarde recommandée malgré tout.
- Mettre à jour **tout** le dossier `addon/Journal` en même temps que le module (nouveaux
  `ModelView.lua`, `GoalTracker.lua`, `Journal.toc` 2.5.6 → 2.7.0).
- Rebuild `worldserver` et redémarrage nécessaires.
- Pas de migration SQL dans ces 8 commits, mais le SQL local modifié reste à réappliquer.

### `mod-combat-ai` — priorité moyenne

- 1 commit d'avance : `7303e39` (2026-11-01), licence/CI/templates d'issues + formatage des GUID
  dans les logs DEBUG (`GetGUID().GetCounter()` → `GetGUID().ToString()`).
- Aucun changement d'API, de configuration ni de comportement IA.
- Fast-forward possible ; rebuild nécessaire pour les 6 `.cpp` modifiés.

### `mod-hunger-thirst` — priorité basse

- 1 commit d'avance : `5ae6fc2` (2026-11-01), badges CI/licence dans le README.
- Aucun changement de code, de configuration ou de SQL.
- Fast-forward possible ; aucun rebuild nécessaire.

### `mod-world-party-scaling` — synchronisation de docs

- Le code et la configuration vendorisés correspondent au dépôt public `main` `930c373` (v1.1.0).
- 4 fichiers divergeaient lors de l'audit : `README.md`, `TEST_REPORT.md`, `docs/COMPATIBILITY.md`,
  `.github/workflows/core_codestyle.yml`.
- Cause : les docs locales ont été mises à jour pour le hook runtime `GroupScript::GetMaxPartySize()`
  et `mod-six-player-party`, alors que le README public décrivait encore l'ancien patch core
  `MAXGROUPSIZE = 6`.
- Le fichier `README.md.orig` existe seulement dans le dossier vendorisé ; il est ignoré et doit
  être supprimé.
- Action : docs locales poussées dans `d1761c0` ; le dossier vendorisé et le dépôt public sont
  identiques hors `README.md.orig`. Pas de mise à jour de code à faire.

## Points d'attention

- **`mod-challenge-modes`** : 7 commits locaux sur `local-adaptations`, upstream figé depuis
  novembre 2025. Ne pas faire de `pull` aveugle ; un futur update devra rebaser sur `origin/master`.
- **`mod-hunts`** : `main` est à jour, mais le tag `pre-native-split` contient 10 commits jamais
  fusionnés (EPF 4.3.0, devise native, `mod-content-manager`, migration `hunt_currency_realm`).
  Ce n'est pas une mise à jour de `main` ; à traiter comme une montée de version majeure.
- **`mod-ah-bot-plus`** : HEAD détaché sur `f685832`, identique à `origin/master`. Optionnel :
  `git checkout master` pour réattacher la branche.
- **`mod-living-world`** : clone shallow, mais le tip local est identique au tip amont. Aucun
  approfondissement nécessaire sauf pour inspecter l'historique.
- **`mod-vanilla-spell-cost`** : à jour. Rappel : son SQL world nécessite un rebuild de l'image
  `ac-db-import`, pas seulement de `worldserver`.
- **`mod-six-player-party`** : à jour. Rappel : il exige le hook core `GetMaxPartySize()` ; le core
  local le fournit, un core AzerothCore stock non.

## Reste à faire (hors git)

1. Rebuild + restart quand tu le décides : `mod-forever-journal` 2.7.0 et `mod-combat-ai` ne sont
   pas actifs tant que le binaire n'est pas reconstruit.
2. Déployer le nouvel addon client `addon/Journal` 2.7.0 en même temps que le module.
3. Avant le prochain démarrage du worldserver, appliquer les migrations DB du merge upstream
   (`acore_string` breaking + nouveaux SQL `db_characters`/`db_world`).
4. Ne pas mettre à jour `mod-challenge-modes` ni le tag `pre-native-split` de `mod-hunts` sans
   traitement dédié.

Aucun de ces points n'a été exécuté : le serveur tourne toujours sur l'ancien binaire.
