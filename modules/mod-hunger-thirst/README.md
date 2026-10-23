# mod-hunger-thirst

Module [AzerothCore](https://www.azerothcore.org/) (WotLK 3.3.5a) ajoutant la **faim et la soif** :
chaque personnage possède une jauge de satiété et une jauge d'hydratation (0 à 100), qui
diminuent avec le temps, se restaurent en mangeant et en buvant, et peuvent — si le module est
configuré pour cela — infliger des dégâts.

Aucun patch client n'est nécessaire. Un addon graphique est fourni, mais il est **optionnel** :
sans lui, les messages de chat et la commande `.faim` restent disponibles.

## Fonctionnement

- La satiété et l'hydratation démarrent à **100** (`HungerThirst.StartingSatiety`,
  `HungerThirst.StartingHydration`) et décroissent de `SatietyDecayPerMinute` /
  `HydrationDecayPerMinute` points par minute de jeu (aucune décroissance hors ligne).
- La décroissance est ralentie par `RestMultiplier` lorsque le personnage est en zone de repos,
  et multipliée par `InstanceMultiplier` dans les instances ; les deux facteurs se cumulent si
  le personnage est au repos dans une instance.
- La décroissance, les alertes et les mises à jour périodiques de l'addon sont **suspendues** en
  taxi, en champ de bataille et en arène (`PauseOnTaxi`, `PauseInBattleground`, `PauseInArena`),
  ainsi que pendant un duel ; les maîtres de jeu sont exemptés si `ExemptGMs = 1`. Les **dégâts
  d'épuisement sont toujours supprimés** en champ de bataille et en arène, même si
  `PauseInBattleground = 0` ou `PauseInArena = 0` : ces deux clés ne contrôlent que la
  décroissance, les alertes et les mises à jour addon.
- Deux seuils déclenchent les alertes : `WarningThreshold` (défaut **25 %**, bande *Warning*) et
  `CriticalThreshold` (défaut **10 %**, bande *Critical*). À **0 %** la jauge est *Empty*. Une
  hystérésis (`Hysteresis`, défaut 2 points) évite le clignotement des messages autour d'un seuil.
- **Manger** (nourriture, `SPELL_SPECIFIC_FOOD`) rend `FoodSatietyGain` points de satiété ;
  **boire** (`SPELL_SPECIFIC_DRINK`) rend `DrinkHydrationGain` points d'hydratation ; les objets
  à la fois nourriture et boisson rendent les deux. Un même item ne peut créditer qu'une fois par
  `RestoreCooldownMs` (anti-double-clic), et une aura de fête lancée par un GameObject n'est
  créditée qu'une fois par `AuraCreditCooldownMs` (ou par la durée de l'aura si elle est plus
  longue). Les buffs de type « Well Fed » ne sont **pas** crédités.
- Les jauges sont persistées en base et rechargées au login.

### Modes

| `HungerThirst.Mode` | Décroissance | Messages / résumé login | Dégâts |
|---|---|---|---|
| `off` *(défaut)* | non | non | non |
| `advisory` | oui | oui | non (même à 0/0) |
| `survival` | oui | oui | oui, dès qu'une jauge entre dans la bande critique (≤ `CriticalThreshold`) |

`survival` inflige des dégâts **dès qu'une jauge entre dans la bande critique**
(`<= CriticalThreshold`, 10 par défaut). Tant qu'aucune jauge n'est à 0, les ticks utilisent
`StarvingDamagePct` toutes les `StarvingIntervalMs`. Dès qu'au moins une jauge est à 0, ils
passent à `DehydratedDamagePct` toutes les `DehydratedIntervalMs`. Si les deux jauges sont à 0,
les dégâts valent `DehydratedDamagePct × DoubleEmptyMultiplier` : le multiplicateur s'applique
**une seule fois** et les deux pourcentages ne s'additionnent pas.

`HungerThirst.Lethal = 0` *(défaut)* : les dégâts ne peuvent **jamais tuer** — ils sont plafonnés
pour laisser au moins 1 PV. `Lethal = 1` autorise la mort ; dès qu'au moins une jauge est à 0,
une rampe additive de `LethalRampPct` % des PV maximum par tick s'ajoute aux dégâts, le total
étant plafonné à `LethalMaxDamagePct` % des PV maximum.

`HungerThirst.Enable = 0` est un interrupteur général : aucun état n'est chargé, rien n'est
persisté, les jauges et les commandes répondent « désactivé ». `Mode = off` avec `Enable = 1`
laisse le module en veille : l'état est chargé et consultable, mais aucun effet de jeu.

## Prérequis

- AzerothCore WotLK 3.3.5a (base `acore_characters` accessible en écriture).
- Rebuild du **worldserver** après installation.
- Un client 3.3.5a build 12340 **uniquement** si vous voulez l'addon (facultatif).

## Installation

1. Copier le dossier `mod-hunger-thirst` dans `modules/` de votre installation AzerothCore :

   ```bash
   cp -r mod-hunger-thirst /chemin/vers/azerothcore/modules/
   ```

2. Reconstruire le worldserver (Docker) :

   ```bash
   docker compose build ac-worldserver
   docker compose up -d
   ```

3. Au démarrage, l'entrypoint Docker copie les `.conf.dist` manquants de l'image vers
   `env/dist/etc/modules/`, puis crée chaque `.conf` manquant (`cp -n`, jamais d'écrasement) —
   le module obtient donc `env/dist/etc/modules/mod_hunger_thirst.conf`. **Le module est livré en
   `Mode = off`** : ouvrez ce fichier et choisissez votre mode, puis redémarrez le worldserver
   (ou `.reload config`).
   (Hors Docker, copiez vous-même `conf/mod_hunger_thirst.conf.dist` vers
   `etc/modules/mod_hunger_thirst.conf` sur Linux, `configs/modules/` sur Windows.)
4. Facultatif — installer l'addon client (voir plus bas).

## Configuration

Fichier : `conf/mod_hunger_thirst.conf.dist` (section `[worldserver]`), déployé en
`env/dist/etc/modules/mod_hunger_thirst.conf`. Rechargement à chaud : `.reload config` — les
changements s'appliquent au tick suivant. Exception : passer `Enable` de `0` à `1` ne charge pas
l'état des joueurs déjà en ligne (il a été déchargé pendant la désactivation) ; ils doivent se
reconnecter ou être initialisés avec `.faim set`.

| Clé | Défaut | Description |
|---|---|---|
| `HungerThirst.Enable` | `1` | Interrupteur général. `0` = aucun état, aucune persistance ; un unique paquet addon « désactivé » est encore envoyé au login si `AddonEnable=1`. |
| `HungerThirst.Mode` | `off` | `off` \| `advisory` \| `survival` (insensible à la casse ; valeur inconnue = `off` + erreur au log). |
| `HungerThirst.Lethal` | `0` | `1` = les dégâts de famine/soif peuvent tuer. `0` = plancher à 1 PV. |
| `HungerThirst.LethalRampPct` | `0.50` | Rampe additive en % des PV max par tick de dégâts (mode létal). |
| `HungerThirst.LethalMaxDamagePct` | `5.00` | Plafond de la rampe, en % des PV max. |
| `HungerThirst.SatietyDecayPerMinute` | `0.50` | Points de satiété perdus par minute. |
| `HungerThirst.HydrationDecayPerMinute` | `0.75` | Points d'hydratation perdus par minute. |
| `HungerThirst.RestMultiplier` | `0.50` | Multiplicateur de décroissance en zone de repos. |
| `HungerThirst.InstanceMultiplier` | `1.00` | Multiplicateur de décroissance en instance. |
| `HungerThirst.PauseOnTaxi` | `1` | Suspend la décroissance en taxi. |
| `HungerThirst.PauseInBattleground` | `1` | Suspend décroissance, alertes et mises à jour addon en champ de bataille et en arène. Les dégâts y restent toujours supprimés. |
| `HungerThirst.PauseInArena` | `1` | Suspend décroissance, alertes et mises à jour addon en arène. Les dégâts y restent toujours supprimés, même avec `PauseInBattleground = 0`. |
| `HungerThirst.ExemptGMs` | `1` | Exempte les maîtres de jeu. |
| `HungerThirst.WarningThreshold` | `25` | Seuil d'alerte (%) : bande *Warning*. |
| `HungerThirst.CriticalThreshold` | `10` | Seuil critique (%) : bande *Critical*. |
| `HungerThirst.StarvingDamagePct` | `0.50` | Dégâts par tick en bande critique (aucune jauge à 0), en % des PV max. |
| `HungerThirst.StarvingIntervalMs` | `10000` | Intervalle des ticks en bande critique (ms). |
| `HungerThirst.DehydratedDamagePct` | `1.00` | Dégâts par tick dès qu'au moins une jauge est à 0, en % des PV max. |
| `HungerThirst.DehydratedIntervalMs` | `5000` | Intervalle des ticks dès qu'au moins une jauge est à 0 (ms). |
| `HungerThirst.DoubleEmptyMultiplier` | `1.50` | Multiplicateur appliqué une seule fois (sans addition) quand les deux jauges sont à 0. |
| `HungerThirst.FoodSatietyGain` | `30.00` | Satiété rendue par une nourriture. |
| `HungerThirst.DrinkHydrationGain` | `35.00` | Hydratation rendue par une boisson. |
| `HungerThirst.RestoreCooldownMs` | `1000` | Délai minimal avant qu'un même item ne crédite à nouveau (ms). |
| `HungerThirst.AuraCreditCooldownMs` | `30000` | Délai minimal pour créditer une aura de fête (ms). |
| `HungerThirst.StartingSatiety` | `100.00` | Satiété initiale d'un nouvel état (0–100). |
| `HungerThirst.StartingHydration` | `100.00` | Hydratation initiale d'un nouvel état (0–100). |
| `HungerThirst.Hysteresis` | `2.00` | Marge d'hystérésis autour des seuils (points). |
| `HungerThirst.ShowLoginSummary` | `1` | Envoie un résumé des jauges à la connexion. |
| `HungerThirst.DebugLog` | `0` | Journalisation de débogage détaillée. |
| `HungerThirst.AddonEnable` | `1` | Envoie l'état à l'addon client. |
| `HungerThirst.AddonUpdateIntervalMs` | `1000` | Fréquence d'envoi à l'addon (ms). |
| `HungerThirst.AddonPrefix` | `FS` | Préfixe du message addon (doit correspondre à l'addon). |

Validation au chargement : `CriticalThreshold < WarningThreshold <= 100` (sinon le seuil critique
est ajusté), seuils, jauges, gains, dégâts et décroissance bornés à 0–100, `RestMultiplier` et
`InstanceMultiplier` bornés à 0–10, `DoubleEmptyMultiplier` borné à **1.00–10.00**, intervalles
et cooldowns minimum 1000 ms (ramenés à 1000 + erreur au log).

## Commandes

| Commande | Accès | Effet |
|---|---|---|
| `.faim` ou `.faim status` | joueur | Affiche la satiété, l'hydratation et le mode actif. |
| `.faim aide` | joueur | Aide courte en français. |
| `.faim set <joueur> <satiété> <hydratation>` | maître de jeu (`SEC_GAMEMASTER`) | Fixe les deux jauges (bornées de 0 à 100). Le joueur doit être **en ligne**. |
| `.faim reset <joueur>` | maître de jeu (`SEC_GAMEMASTER`) | Remet le joueur à l'état de départ. Le joueur doit être **en ligne**. |

Disponibles **en jeu uniquement** (pas dans la console du serveur).

## Addon client (optionnel)

Le module fournit `client/Interface/AddOns/FaimSoif` :

```bash
cp -r modules/mod-hunger-thirst/client/Interface/AddOns/FaimSoif \
      "/chemin/vers/WoW/Interface/AddOns/"
```

Au login, l'addon affiche deux jauges (satiété, hydratation) et les met à jour via le préfixe
`FS` (`HungerThirst.AddonPrefix`). `/faim` affiche/masque la fenêtre, `/faim reset` la replace à
sa position par défaut. L'addon ne fait que **lire** l'état envoyé par le serveur.

Sans addon, tout reste fonctionnel : messages de chat aux seuils, résumé au login et `.faim`.

L'addon reçoit aussi un paquet lorsque le module est en mode `off` (mode 0), envoyé à la
connexion : il affiche alors « Désactivé » (aucun rafraîchissement périodique tant que le mode
reste `off`). Si la persistance est indisponible, l'état envoyé au login lui fait afficher
« Persistance indisponible ». Tant qu'aucun paquet n'a été reçu (serveur sans le module), il
reste sur « Chargement... ».

## Persistance

Table `hunger_thirst_state` (base `acore_characters`), créée automatiquement au démarrage
(`CREATE TABLE IF NOT EXISTS`) — aucun fichier SQL à importer :

| Colonne | Type | Description |
|---|---|---|
| `guid` | `INT UNSIGNED` (PK) | Identifiant bas du personnage. |
| `satiety` | `DECIMAL(5,2)` | Satiété (0–100), défaut `100.00`. |
| `hydration` | `DECIMAL(5,2)` | Hydratation (0–100), défaut `100.00`. |
| `updated_at` | `BIGINT UNSIGNED` | Horodatage de la dernière sauvegarde (epoch, ms). |

La ligne est supprimée avec le personnage. Si la table ne peut pas être vérifiée/créée, la
simulation est **suspendue** pour les joueurs concernés : aucun tick (ni décroissance, ni alerte,
ni dégât), aucune sauvegarde, aucun crédit de nourriture/boisson et aucune mise à jour périodique
de l'addon. La connexion n'est jamais bloquée. En mode actif, un message « Persistance
indisponible » est envoyé au joueur (si `ShowLoginSummary = 1`) ; l'addon (si
`AddonEnable = 1`) affiche le même état. L'état reste non chargé pendant toute la
session, donc une **reconnexion** est nécessaire après restauration de la table.

## Intégration hardcore

Aucun couplage direct avec `mod-hardcore` : le module ne réanime personne et n'écrit aucune
table de morts. Une mort par famine ou déshydratation (mode `survival` **et** `Lethal = 1`) est
une mort de joueur ordinaire : la politique de `mod-hardcore` s'applique donc normalement, et un
personnage hardcore qui meurt de faim le paie définitivement.

Avec `Lethal = 0` (défaut), le module ne peut jamais tuer un personnage : la faim et la soif ne
peuvent déclencher une mort définitive.

Chaque mort causée par le module produit une ligne `LOG_INFO` (nom, guid, niveau, carte, zone,
cause, jauges finales et bandes) ; aucune annonce serveur, aucune écriture en base.

## Limites

- **Pas de jauge native** : le client 3.3.5a n'expose aucune API de faim/soif ; l'affichage
  repose sur l'addon, qui est **optionnel**.
- `Lethal = 0` par défaut : la faim et la soif n'infligent jamais de coup fatal.
- Le module est livré en `Mode = off` : sans configuration explicite, il n'a aucun effet en jeu.
- Les dégâts n'existent qu'en mode `survival`.
- Les buffs « Well Fed » et auras similaires ne restaurent pas les jauges ; seuls les effets
  Food/Drink sont crédités (objets et auras de fête/GameObject), pas Well Fed.
- Aucune annonce globale ni historique en base des morts de famine (uniquement le log serveur).

## Licence

Distribué sous GNU GPL v2 (ou version ultérieure).
