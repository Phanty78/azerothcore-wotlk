# Annonces serveur — sélection de défi, paliers de 10 niveaux Hardcore, mort Semi-Hardcore

## Context

Le serveur tourne avec `modules/mod-challenge-modes` (dépôt git séparé, patché localement ; défis opt-in par personnage via le Shrine of Challenge, les 8 défis activés ; déploiement par image Docker `ac-worldserver`). Trois annonces serveur en français sont demandées :

1. **Sélection** : quand un joueur active un défi au Shrine, message serveur global du type « *X a choisi la voie du Hardcore. Quelle insolence !* », avec un texte adapté à chacun des 9 défis.
2. **Félicitations** : pour un personnage **Hardcore**, message serveur à chaque niveau multiple de 10 atteint sans mourir.
3. **Mort** : message serveur quand un personnage **Semi-Hardcore** meurt (toute cause). La mort **Hardcore** est déjà annoncée par `HandleHardcoreDeath` (message « est tombé et ne se relèvera jamais. », clé `Hardcore.AnnounceDeaths`) — **aucun changement** à ce code existant.

Décisions confirmées par l'utilisateur : annonces de sélection **au Shrine of Challenge** ; félicitations **Hardcore uniquement** ; morts Semi-Hardcore annoncées pour **toutes les causes de mort**.

Tout le code tient dans un seul fichier : `modules/mod-challenge-modes/src/ChallengeModes.cpp`. Aucun include à ajouter (`StringFormat.h` et `WorldSessionMgr.h` sont déjà inclus lignes 12-13 ; le motif `sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(...))` est déjà utilisé par `HandleHardcoreDeath`, ligne 478). Aucune nouvelle clé de config, aucun SQL, aucun fichier de conf modifié.

## Approach

### 1. Annonces de sélection à l'activation d'un défi

Fichier : `modules/mod-challenge-modes/src/ChallengeModes.cpp`, méthode `gobject_challenge_modes::OnGossipSelect` (lignes 986-992). Insérer un `switch (action)` entre la ligne 989 (`ChatHandler(...).PSendSysMessage("Le mode de défi est activé.");`) et la ligne 990 (`CloseGossipMenuFor(player);`). `action` vaut exactement l'enum `ChallengeModeSettings` du gossip choisi (0-7, 9) — chaque `case` envoie l'annonce globale :

```cpp
switch (action)
{
    case SETTING_HARDCORE:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie du Hardcore. Quelle insolence ! La mort veille désormais sur ses pas...",
            player->GetName()));
        break;
    case SETTING_SEMI_HARDCORE:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie du Semi-Hardcore. Insolent, et pourtant... chaque chute lui coûtera son équipement.",
            player->GetName()));
        break;
    case SETTING_SELF_CRAFTED:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie de l'artisan. Il ne portera que son propre ouvrage. Quel défi !",
            player->GetName()));
        break;
    case SETTING_CRAFTED_EQUIPMENT_ONLY:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie des créations. Seul l'équipement façonné par des joueurs l'habillera.",
            player->GetName()));
        break;
    case SETTING_ITEM_QUALITY_LEVEL:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie de la modestie. Point d'équipement au-dessus de la qualité Normale !",
            player->GetName()));
        break;
    case SETTING_SLOW_XP_GAIN:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie de la patience. L'expérience viendra goutte à goutte...",
            player->GetName()));
        break;
    case SETTING_VERY_SLOW_XP_GAIN:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie de l'ascèse. Des saisons entières avant chaque niveau !",
            player->GetName()));
        break;
    case SETTING_QUEST_XP_ONLY:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie du héros de quêtes. Seules les missions le feront grandir.",
            player->GetName()));
        break;
    case SETTING_IRON_MAN:
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffa500[Défi]|r {} a choisi la voie du Iron Man. Quelle audace ! Ni talents, ni groupe, ni potion.",
            player->GetName()));
        break;
    default:
        break;
}
```

Le message privé « Le mode de défi est activé. » reste inchangé ; l'annonce s'ajoute après lui. Aucune annonce pour une `action` inconnue (`default`). Les libellés sont volontairement cohérents avec les intitulés du gossip (lignes 948-980 : « Activer le mode … »).

### 2. Félicitations Hardcore à chaque palier de 10 niveaux

Même fichier, `ChallengeMode_Hardcore::OnPlayerLevelChanged` (lignes 534-537). Remplacer le corps par :

```cpp
void OnPlayerLevelChanged(Player* player, uint8 oldlevel) override
{
    ChallengeMode::OnPlayerLevelChanged(player, oldlevel);

    if (!sChallengeModes->challengeEnabledForPlayer(SETTING_HARDCORE, player))
    {
        return;
    }

    uint8 level = player->GetLevel();
    if (level > 0 && level % 10 == 0)
    {
        sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
            "|cffffd700[Hardcore]|r {} a atteint le niveau {} sans mourir. Respect et félicitations !",
            player->GetName(), level));
    }
}
```

Le garde `challengeEnabledForPlayer` est obligatoire même si la classe de base le vérifie aussi (son `return` anticipé ne protège pas le code placé après l'appel de base). Le hook `OnPlayerLevelChanged` est déclenché par `Player::GiveLevel` (`src/server/game/Entities/Player/Player.cpp:2613`) pour tout changement de niveau. « Sans mourir » est exact par construction : un Hardcore mort est bloqué/kické (`HARDCORE_DEAD`), il ne peut pas regagner d'XP ; le palier 80 (niveau max) est le dernier message possible.

### 3. Annonce de mort Semi-Hardcore (toutes causes)

Même fichier, classe `ChallengeMode_SemiHardcore` (lignes 540-574). Insérer cette méthode juste après le constructeur (ligne 543), avant `OnPlayerKilledByCreature` (ligne 545) :

```cpp
void OnPlayerJustDied(Player* player) override
{
    if (!sChallengeModes->challengeEnabledForPlayer(SETTING_SEMI_HARDCORE, player))
    {
        return;
    }
    sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(
        "|cffff0000[Semi-Hardcore]|r {} (niveau {}) est tombé. La voie du Semi-Hardcore est impitoyable.",
        player->GetName(), player->GetLevel()));
}
```

`OnPlayerJustDied` est appelé depuis `Player::KillPlayer()` (`src/server/game/Entities/Player/Player.cpp`, hook `sScriptMgr->OnPlayerJustDied(this)`), le point de passage unique de toute mort de joueur (créature, chute, PvP, sorts). Le texte est volontairement générique (sans mention de perte d'équipement) car la perte ne s'applique qu'aux morts par créature (`OnPlayerKilledByCreature`, inchangé). Le préfixe rouge `|cffff0000[Semi-Hardcore]|r` reprend le style du message de mort Hardcore existant (`HandleHardcoreDeath`, ligne 479). Un personnage ne peut pas être à la fois Hardcore et Semi-Hardcore (exclusivité au gossip, lignes 946-953), donc aucune double annonce entre `HandleHardcoreDeath` et ce nouveau hook.

## Critical files & anchors

- `modules/mod-challenge-modes/src/ChallengeModes.cpp` — les 3 sites d'édition : `gobject_challenge_modes::OnGossipSelect` (l. 986-992), `ChallengeMode_Hardcore::OnPlayerLevelChanged` (l. 534-537), `ChallengeMode_SemiHardcore` (l. 543-545). Motif d'annonce à recopier : `sWorldSessionMgr->SendServerMessage(SERVER_MSG_STRING, Acore::StringFormat(...))` (l. 478). Relire ces plages avant d'éditer ; le module est un dépôt git séparé (les modifications y apparaîtront non suivies — commit au choix de l'utilisateur).
- `env/dist/logs/Server.log` — contrôle du démarrage propre après rebuild (cf. Verification).

## Verification

Prérequis : dépôt à jour, Docker Desktop lancé. Toutes les commandes depuis `/Users/test/Documents/git/wow/azerothcore-wotlk`.

1. **Build (preuve de compilation)** : `docker compose build ac-worldserver` — chemin de déploiement documenté (GUIDE-ADMINISTRATEUR.md, « Après une modification de code »). Toute erreur de syntaxe/type dans le module fait échouer le build ici.
2. **Déploiement + démarrage propre** : `docker compose up -d`, attendre `WORLD: World Initialized … (worldserver-daemon) ready...` dans `env/dist/logs/Server.log`, puis `grep -ai "challenge" env/dist/logs/Server.log` → aucune erreur `mod-challenge-modes`. (Aucune clé de config ajoutée : le piège `cp -n` des fichiers de conf ne s'applique pas.)
3. **Comportement en jeu** (nécessite un client 3.3.5a ; l'implémenteur confie ces étapes à l'utilisateur si aucun client n'est disponible) :
   - **Sélection** : personnage niveau 1 → Shrine of Challenge → « Activer le mode Hardcore » → *tous* les clients en ligne voient `[Défi] X a choisi la voie du Hardcore. Quelle insolence ! La mort veille désormais sur ses pas...`, et le joueur voit toujours « Le mode de défi est activé. ». Refaire avec un autre défi (ex. XP lente) → texte adapté correspondant au switch.
   - **Félicitations** : sur ce personnage Hardcore, GM `.level up 9` → message serveur `[Hardcore] X a atteint le niveau 10 sans mourir. Respect et félicitations !` ; `.level up 1` → **aucun** message au niveau 11 ; `.level up 9` → message à nouveau au niveau 20.
   - **Mort Semi-Hardcore** : autre personnage niveau 1 → Shrine → « Activer le mode Semi-Hardcore » → le faire mourir (`.die` GM ciblé, chute, ou créature) → message serveur `[Semi-Hardcore] X (niveau 1) est tombé. La voie du Semi-Hardcore est impitoyable.` ; en cas de mort par créature, les messages « Vous avez perdu votre … » et la perte d'or restent inchangés.
   - **Non-régression Hardcore** : le personnage Hardcore meurt → message existant `X (niveau N) est tombé et ne se relèvera jamais.` + kick, et `.hardcore status` affiche toujours `Hardcore : TOMBÉ`.

## Assumptions & contingencies

- **Toujours actives** : aucune clé de config n'est ajoutée — les annonces sont inconditionnelles tant que `ChallengeModes.Enable = 1`. Si l'utilisateur veut plus tard des interrupteurs, recopier le motif `Hardcore.AnnounceDeaths` (bool membre + lecture dans `ChallengeModes_WorldScript::LoadConfig` + entrée `conf/challenge_modes.conf.dist`).
- **Textes proposés** : les libellés exacts ci-dessus sont modifiables à la lecture du plan ; seul le format « a choisi la voie de … » a été demandé explicitement, les saveurs par défi sont de la rédaction nouvelle.
- **Bord connu (accepté)** : si `Hardcore.DisableLevel` vaut un multiple de 10 (défaut 0 = jamais), le message de félicitation ne s'envoie pas à ce niveau précis — la classe de base désactive le réglage avant le contrôle. Config par défaut non concernée.
- **Bord connu (accepté)** : un rétrogradage GM de niveau passant par un multiple de 10 déclencherait aussi la félicitation (le hook ignore le sens du changement). Cas GM-only, non corrigé.
- **Contingence build** : si `Acore::StringFormat` refuse un argument `uint8` dans le format `{}` (improbable — motif identique déjà compilé avec `player->GetLevel()` à la ligne 479), caster en `uint32`.
- **Contingence déploiement** : si le Shrine n'apparaît pas en jeu après rebuild, vérifier `ChallengeModes.Enable = 1` dans `env/dist/etc/modules/challenge_modes.conf` (fichier existant non écrasé par l'entrypoint) — pas une régression du code.