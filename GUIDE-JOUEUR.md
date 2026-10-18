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

⚠️ **La mort est définitive sur ce serveur.** Une fois ton personnage tué, il reste un fantôme :
ni esprit guérisseur, ni récupération de cadavre, ni résurrection par un autre joueur.
Seul un administrateur peut le réanimer.

*Si la liste des royaumes reste vide ou si la machine du serveur n'apparaît pas dans Tailscale :
vérifie que Tailscale affiche bien « Connected », puis préviens-moi.*
