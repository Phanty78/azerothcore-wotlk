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

D'autres options et défis sont proposés au Shrine : Semi-Hardcore, équipement auto-fabriqué,
équipement fabriqué par un joueur, qualité d'équipement, XP lente/très lente, XP quêtes, Iron Man
et **Faim et soif** (option **« Activer la faim et la soif »**). Certains défis sont exclusifs
entre eux et disparaissent du menu dès qu'un défi incompatible est activé. L'option **Faim et
soif**, elle, est **propre à chaque personnage** et **indépendante des défis** : elle se combine
avec tous, ne bloque aucun autre choix et n'accorde **ni bonus d'XP ni récompense**. Le défi
**équipement fabriqué par un joueur** n'autorise que les armes et armures fabriquées par n'importe
quel joueur, toi compris et sans restriction de compte ; ton équipement de départ reste autorisé,
et les consommables comme les objets non équipables ne sont pas concernés.

*Si la liste des royaumes reste vide ou si la machine du serveur n'apparaît pas dans Tailscale :
vérifie que Tailscale affiche bien « Connected », puis préviens-moi.*

## 3. Faim et soif (option au Shrine of Challenge)

La faim et la soif ne s'appliquent **qu'aux personnages qui ont choisi l'option « Activer la faim
et la soif »** au Shrine of Challenge, au **niveau 1** (ou jusqu'au **niveau 55** pour un chevalier
de la mort). L'option est **propre à chaque personnage** : un personnage qui ne l'a pas choisie n'a
**aucune jauge, aucun message et aucun effet**. L'administrateur ne l'active plus pour tout le
monde : il choisit seulement le réglage global des effets, et seuls les personnages qui ont pris
l'option y sont soumis. Rien à installer pour l'option elle-même : tout se passe côté serveur ;
l'addon d'affichage présenté plus bas reste facultatif.

Le choix se fait **une seule fois** et est **définitif** : une fois l'option prise, elle disparaît
du menu du Shrine et aucun retour en arrière n'est possible. Passé le niveau 1 (ou le niveau 55
pour un chevalier de la mort), le Shrine n'est plus accessible : un personnage qui ne l'a pas
choisie à ce moment-là ne peut plus l'activer ensuite. Cette option est **indépendante des
défis** : elle se combine avec eux, ne bloque aucun autre choix et n'accorde **ni bonus d'XP ni
récompense**.

Une fois l'option activée, ton personnage a deux jauges de 0 à 100, la **satiété** et
l'**hydratation**, qui baissent avec le temps de jeu. Ce réglage global détermine ce qu'elles
produisent : en `advisory`, un simple suivi avec avertissements ; en `survival`, les **états
visibles** et le **brouillard visuel** décrits ci-dessous s'y ajoutent. En `off`, les jauges
restent consultables mais ne baissent pas et n'ont aucun effet.

- Tape **`.hungerthirst`** (ou `.hungerthirst status`) pour connaître tes deux jauges.
- Des messages apparaissent automatiquement à **30 %** (avertissement, seuil par défaut), à
  **10 %** (critique) et à **0 %** (jauge vide) — seuils réglables par l'administrateur. Ils ne
  s'affichent qu'en mode `advisory` : en mode `survival`, les **états** ci-dessous les
  remplacent.
- **Mange** et **bois** pour les remonter : les objets de nourriture et de boisson restaurent la
  jauge correspondante. Les buffs de type « Well Fed » ne comptent pas.
- En mode `advisory`, la faim et la soif ne font **aucun dégât** et n'appliquent **aucun
  état** : tu es seulement averti par les messages. En mode `survival`, chaque jauge applique
  un **état visible** (une icône de debuff dans ta barre), mais aucune ne blesse : ni la faim
  ni la soif n'infligent de dégâts. L'administrateur peut désactiver les états d'une jauge ;
  celle-ci n'a alors plus **aucun effet de jeu**.
- **États de faim** (mode `survival`) : dès que la satiété tombe à **30 % ou moins**, tu
  reçois **« Appétit vorace »** : **-10 %** de vitesse de déplacement, **-10 %** de dégâts
  infligés et **+10 %** de dégâts subis. À **0 %**, il devient **« Faim dévorante »** : ces
  trois malus passent à **30 %**. L'infobulle d'origine ne décrit pas ces effets : avec
  l'addon facultatif à jour, survoler le debuff affiche les vrais effets et le remède à
  appliquer ; sans l'addon, le texte d'origine de Blizzard reste affiché et c'est le message
  de chat qui fait foi. Aucun patch du client n'est utilisé.
  Pour faire disparaître l'état, **mange** : il s'efface dès que la satiété repasse
  **au-dessus de 32 %** (seuil de 30 % plus 2 points). Depuis **0 %**, une seule bouchée ne
  te ramène qu'à 30 % : **« Faim dévorante »** devient **« Appétit vorace »**, et une seconde
  bouchée est nécessaire pour que le debuff disparaisse. Le message de chat qui accompagne
  l'état indique désormais « Eat something » (mange quelque chose). L'état revient de
  lui-même après une mort ou une reconnexion tant que la jauge reste basse.
- **États de soif** (mode `survival`) : dès que l'hydratation tombe à **30 % ou moins**, tu
  reçois un **état de soif visible** (une icône de boisson dans ta barre) : **-10 %** de
  régénération de mana, **-10 %** de dégâts magiques infligés et **-10 %** de soins
  prodigués. À **0 %**, il devient l'état **déshydraté** : ces trois malus passent à **30 %**.
  Un seul état de soif est actif à la fois. La régénération réduite est celle qui vient de
  l'**Esprit** ; les bonus plats de mana toutes les 5 secondes ne sont pas concernés. Les
  dégâts physiques ou à distance, ceux de ton familier et les dégâts que tu subis ne le sont
  pas non plus. Comme pour la faim, l'infobulle d'origine ne décrit pas ces effets : avec
  l'addon facultatif à jour, survoler le debuff affiche les vrais effets et le remède ; sans
  l'addon, le texte d'origine de Blizzard reste affiché et c'est le message de chat qui fait
  foi. Aucun patch du client n'est utilisé.
  Pour faire disparaître l'état, **bois** : il s'efface dès que l'hydratation repasse
  **au-dessus de 32 %** (seuil de 30 % plus 2 points). Depuis **0 %**, une seule boisson rend
  35 points par défaut et suffit donc à sortir de l'état. Le message de chat qui accompagne
  l'état indique désormais « Drink something » (bois quelque chose). L'état revient de
  lui-même après une mort ou une reconnexion tant que la jauge reste basse.
- **La soif ne blesse plus** : plus aucun dégât d'épuisement, plus de plancher à 1 point de
  vie, plus de mort causée par la soif — même en mode Hardcore. Le module n'inflige plus aucun
  dégât.
- En **champ de bataille** et en **arène**, avec la configuration par défaut, les jauges sont
  figées et les états restent figés. Si l'administrateur désactive la pause correspondante,
  la décroissance et les états continuent.

### Addon d'affichage — optionnel

Un addon fourni avec le module affiche à l'écran les deux jauges du personnage qui a activé
l'option. Il est **facultatif** : le client est déjà configuré et rien ne t'oblige à l'installer.
Sans lui, les messages de chat et `.hungerthirst` continuent de fonctionner normalement.

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
