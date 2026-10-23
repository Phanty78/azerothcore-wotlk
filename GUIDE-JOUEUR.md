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

⚠️ **La mort est définitive sur ce serveur.** Une fois ton personnage tué, il devient un
fantôme bloqué à jamais à l'endroit de sa mort : ni esprit guérisseur, ni récupération de
cadavre, ni résurrection par un autre joueur. Ta mort est annoncée à tout le serveur.
Seul un administrateur peut te réanimer. Vérifie ton état avec `.hardcore status`.

*Si la liste des royaumes reste vide ou si la machine du serveur n'apparaît pas dans Tailscale :
vérifie que Tailscale affiche bien « Connected », puis préviens-moi.*

## 3. Faim et soif (si activée par l'administrateur)

Le serveur peut activer un module de **faim et de soif**. Il ne demande aucune installation :
tout se passe côté serveur. Ton personnage a alors deux jauges de 0 à 100, la **satiété** et
l'**hydratation**, qui baissent avec le temps de jeu.

- Tape **`.faim`** (ou `.faim status`) pour connaître tes deux jauges.
- Des messages apparaissent automatiquement à **25 %** (avertissement), à **10 %** (critique)
  et à **0 %** (jauge vide) — seuils par défaut, réglables par l'administrateur.
- **Mange** et **bois** pour les remonter : les objets de nourriture et de boisson restaurent la
  jauge correspondante. Les buffs de type « Well Fed » ne comptent pas.
- En mode `advisory`, la faim et la soif ne font **aucun dégât**. En mode `survival`, les dégâts
  commencent dès qu'une jauge atteint le seuil critique (**10 %** ou moins par défaut) et
  s'aggravent à **0 %**. Avec l'option létale désactivée (`Lethal=0`, le défaut), ils ne peuvent
  **jamais te tuer** : ils te laissent au moins 1 point de vie. Si l'administrateur active
  `Lethal=1`, tu peux mourir de faim ou de soif — et sur ce serveur, **la mort reste définitive**,
  hardcore oblige.
- En **champ de bataille** et en **arène**, les dégâts de faim et de soif ne s'appliquent
  **jamais** (les jauges peuvent continuer à baisser et les alertes à s'afficher selon la
  configuration).

### Addon d'affichage — optionnel

Un addon fourni avec le module affiche les deux jauges à l'écran. Il est **facultatif** : le
client est déjà configuré et rien ne t'oblige à l'installer. Sans lui, les messages de chat et
`.faim` continuent de fonctionner normalement.

Pour l'installer :

1. Dans le dépôt du serveur, copie le dossier
   `modules/mod-hunger-thirst/client/Interface/AddOns/FaimSoif` dans le dossier
   `Interface/AddOns/` de ton client WoW.
2. Redémarre le client. Dans la liste des addons, **Faim & Soif** doit apparaître (coché).
3. En jeu, `/faim` affiche ou masque la fenêtre ; `/faim reset` la replace à sa position par défaut.

L'addon affiche aussi, par défaut, deux petites icônes d'état discrètes (satiété et hydratation)
en permanence, même lorsque la fenêtre des jauges est fermée. Placées en bas à droite, elles se
déplacent par glisser-déposer et leur position est retenue. Leur bordure reste grise en temps
normal, passe au jaune à l'avertissement, à l'orange au critique et au rouge à 0 %. Survole une
icône pour voir la valeur sur 100, l'état (normal, avertissement, critique ou vide) et le mode ;
clique dessus pour afficher ou masquer la fenêtre des jauges. `/faim icones` (ou `/faim icons`)
masque ou réaffiche les icônes et retient ton choix ; `/faim reset` replace la fenêtre et les
icônes à leurs positions par défaut et réaffiche la fenêtre, en conservant ton choix d'afficher
ou masquer les icônes. Aucune texture personnalisée ni patch client n'est nécessaire : les
icônes utilisent les textures d'origine de 3.3.5a.
