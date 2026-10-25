# Guide joueur — rejoindre notre serveur WoW

Ton client WoW est **déjà configuré** : rien à modifier dedans. Il te reste une seule chose à faire.

> **Lien de partage à accepter : à te transmettre de vive voix (hors dépôt — jamais dans un repo public).**

## 1. Installer Tailscale (5 minutes)

Tailscale crée un réseau privé entre ton PC et le serveur : personne d'autre sur Internet ne
peut le voir, et tu n'as aucune redirection de port à faire.

Téléchargement direct selon ton système :

- **Windows** : <https://pkgs.tailscale.com/stable/tailscale-setup-latest.exe>
- **macOS** : <https://pkgs.tailscale.com/stable/Tailscale-latest-macos.pkg>
- **Linux** : `curl -fsSL https://tailscale.com/install.sh | sh`
- Autres systèmes / App Store : <https://tailscale.com/download>

Puis :

1. Lance Tailscale, clique sur **Log in** et crée un compte gratuit (Google, Microsoft, GitHub
   ou e-mail). C'est un compte à toi, personnel.
2. **Accepte l'invitation de partage** (le lien en haut de ce document, ou l'e-mail reçu).
   Si besoin : <https://login.tailscale.com/admin/machines> → *appareils partagés avec moi* → **Accept**.
3. Vérifie que la machine **`macbook-pro-de-mal`** apparaît dans l'application, avec un badge
   *Shared* (partagée). Dans son panneau, son adresse `100.100.203.120` doit s'afficher.

Laisse Tailscale **allumé pendant que tu joues** (icône dans la barre des tâches / de menus).

## 2. Lancer le jeu

1. Lance **`Wow.exe`** directement (fichier `Wow.exe` ou l'application `Wow.app`).
   **N'utilise pas** le launcher `Launcher.exe` : il réécrit la configuration et tenterait de
   se connecter aux serveurs officiels Blizzard.
2. Connecte-toi avec l'identifiant et le mot de passe que je t'ai donnés, choisis le royaume
   **AzerothCore** (il doit apparaître **en ligne**), crée ton personnage et entre dans le monde.

⚠️ **La mort n'est définitive que si tu as activé le mode Hardcore.** Le **Shrine of
Challenge** — une antique idole dans le cimetière de la zone de départ — n'est visible qu'au
**niveau 1** (ou jusqu'au **niveau 55** pour un chevalier de la mort) : parle-lui et choisis le
défi souhaité, par exemple **Activer le mode Hardcore**. À partir de là, la mort est définitive :
ton personnage devient un fantôme bloqué à jamais à l'endroit de sa mort — ni esprit guérisseur,
ni récupération de cadavre, ni résurrection par un sort ou par un autre joueur. Ta mort est
annoncée à tout le serveur. Vérifie ton état avec `.hardcore status` ; seul un administrateur
peut te réanimer (`.hardcore revive`). Un personnage **sans** Hardcore réapparaît normalement
(esprit guérisseur ou résurrection classique).

D'autres défis sont proposés au Shrine (Semi-Hardcore, équipement auto-fabriqué, équipement
fabriqué par un joueur, qualité d'équipement, XP lente/très lente, XP quêtes, Iron Man) ; certains
sont exclusifs entre eux et disparaissent du menu dès qu'un défi incompatible est activé. Le défi
**équipement fabriqué par un joueur** n'autorise que les armes et armures fabriquées par n'importe
quel joueur, toi compris et sans restriction de compte ; ton équipement de départ reste autorisé,
et les consommables comme les objets non équipables ne sont pas concernés.

*Si la liste des royaumes reste vide ou si la machine du serveur n'apparaît pas dans Tailscale :
vérifie que Tailscale affiche bien « Connected », puis préviens-moi.*

## 3. Faim et soif (si activée par l'administrateur)

Le serveur peut activer un module de **faim et de soif**. Il ne demande aucune installation :
tout se passe côté serveur. Ton personnage a alors deux jauges de 0 à 100, la **satiété** et
l'**hydratation**, qui baissent avec le temps de jeu.

- Tape **`.hungerthirst`** (ou `.hungerthirst status`) pour connaître tes deux jauges.
- Des messages apparaissent automatiquement à **30 %** (avertissement, seuil par défaut), à
  **10 %** (critique) et à **0 %** (jauge vide) — seuils réglables par l'administrateur. En mode
  `survival`, ces messages ne concernent plus que la **soif** : la faim, elle, s'affiche par des
  états (voir ci-dessous).
- **Mange** et **bois** pour les remonter : les objets de nourriture et de boisson restaurent la
  jauge correspondante. Les buffs de type « Well Fed » ne comptent pas.
- En mode `advisory`, la faim et la soif ne font **aucun dégât** et la faim n'applique **aucun
  état** : tu es seulement averti par les messages. En mode `survival`, la faim ne blesse plus :
  elle applique des **états visibles** (une icône de debuff dans ta barre), tandis que la soif
  conserve ses dégâts d'épuisement (voir plus bas). Si l'administrateur désactive les états
  (`HungerStates=0`), la faim n'a plus **aucun effet de jeu** — mais la soif continue de
  blesser.
- **États de faim** (mode `survival`) : dès que la satiété tombe à **30 % ou moins**, tu
  reçois **« Appétit vorace »** : **-10 %** de vitesse de déplacement, **-10 %** de dégâts
  infligés et **+10 %** de dégâts subis. À **0 %**, il devient **« Faim dévorante »** : ces
  trois malus passent à **30 %**. L'infobulle de l'icône garde le texte d'origine de Blizzard
  et ne décrit pas ces effets : fie-toi au message de chat affiché quand l'état apparaît.
  L'état disparaît quand la satiété repasse au-dessus du seuil (30 %) plus 2 points ; depuis
  0 %, une seule bouchée ne te ramène qu'à 30 %, donc tu restes affaibli et une seconde
  bouchée est parfois nécessaire. L'état revient de lui-même après une mort ou une
  reconnexion tant que la jauge reste basse.
- En mode `survival`, la **soif** inflige des dégâts d'épuisement dès que l'hydratation atteint
  le seuil critique (**10 %** ou moins par défaut) ; ils s'aggravent à **0 %**. Si les deux
  jauges sont à **0 %**, les dégâts de soif sont **amplifiés**. Avec `Lethal=0` (le défaut),
  ils ne peuvent **jamais te tuer** : ils te laissent au moins 1 point de vie. Avec
  `Lethal=1`, la soif peut te tuer — et si tu as activé le mode Hardcore, la mort est
  définitive.
- En **champ de bataille** et en **arène**, les dégâts d'épuisement dus à la **soif** ne
  s'appliquent **jamais** (les jauges peuvent continuer à baisser et les alertes à s'afficher
  selon la configuration).

### Addon d'affichage — optionnel

Un addon fourni avec le module affiche les deux jauges à l'écran. Il est **facultatif** : le
client est déjà configuré et rien ne t'oblige à l'installer. Sans lui, les messages de chat et
`.hungerthirst` continuent de fonctionner normalement.

Pour l'installer :

1. Le module est un dépôt git séparé, cloné dans `modules/mod-hunger-thirst` sur le serveur.
   Copie son dossier `modules/mod-hunger-thirst/client/Interface/AddOns/HungerThirst` dans le
   dossier `Interface/AddOns/` de ton client WoW.
2. Redémarre le client. Dans la liste des addons, **Hunger & Thirst** doit apparaître (coché).
3. En jeu, `/hungerthirst` (ou `/ht`) affiche ou masque la fenêtre ; `/hungerthirst reset` la
   replace à sa position par défaut.

L'addon affiche aussi, par défaut, deux petites icônes d'état discrètes (satiété et hydratation)
en permanence, même lorsque la fenêtre des jauges est fermée. Placées en bas à droite, elles se
déplacent par glisser-déposer et leur position est retenue. Leur bordure reste grise en temps
normal, passe au jaune à l'avertissement, à l'orange au critique et au rouge à 0 %. Survole une
icône pour voir la valeur sur 100, l'état (normal, avertissement, critique ou vide) et le mode ;
clique dessus pour afficher ou masquer la fenêtre des jauges. `/hungerthirst icons` masque ou
réaffiche les icônes et retient ton choix ; `/hungerthirst reset`
replace la fenêtre et les icônes à leurs positions par défaut et réaffiche la fenêtre, en
conservant ton choix d'afficher ou masquer les icônes. Aucune texture personnalisée ni patch
client n'est nécessaire : les icônes utilisent les textures d'origine de 3.3.5a.
