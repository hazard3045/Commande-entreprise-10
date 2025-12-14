# Notice d'utilisation: Dispositif de prise de vue pour drone

**Auteurs:** Achile PINSARD - Astrid MARION - Lianne SOO - Nihal LACHGUER - Tinihen MENICHE - Thomas BRUYERE

**Groupe:** 10

**Partenaire:** CEREMA

---

## Résumé

Ce document vise à donner une marche à suivre quant à l'utilisation du dispositif de prise de vue fourni au Cerema dans le cadre du projet Commande entreprise de l'IMT Atlantique. Vous y trouverez le mode d'emploi pour l'utilisation et la manipulation du dispositif.

---

## Table des matières

1. [Matériel Nécessaire](#matériel-nécessaire)
2. [Initialisation du Système](#initialisation-du-système)
3. [Utilisation et Modes d'Acquisition](#utilisation-et-modes-dacquisition)

---

## Matériel Nécessaire

Le dispositif est constitué de:

- Carte Raspberry Pi Zéro 2 W
- Nappe Raspberry Pi Mini 200mm MIPI/CSI
- Module caméra v3 Raspberry Pi
- Carte SD 32 Go
- Un connecteur micro-USB/USB et une clé USB (selon le cas d'utilisation)

Celui-ci est connecté à la sortie TIMEPULSE du module GPS par l'intermédiaire d'un câble Dupont.

**⚠️ Attention:** Il est **impératif** d'éteindre la Raspberry Pi Zero 2 W avant d'ajouter ou de retirer tout élément. Il est, par exemple, fortement déconseillé de connecter un clavier alors que la carte est allumée.

---

## Initialisation du Système

Le dispositif fourni remplit les pré-requis ci-dessous. Si le dispositif a été formaté ou ne fonctionne plus correctement, il est nécessaire de repasser par ces étapes d'installation.

### Installation de l'OS sur la carte SD

Il est nécessaire d'installer sur la carte l'OS trouvable sur le lien suivant (la première archive `.img.xz` d'une taille de 508 MB, `raspios_lite_armhf-2024-11-19`):

https://downloads.raspberrypi.com/raspios_lite_armhf/images/raspios_lite_armhf-2024-11-19/

Cette archive sera également disponible dans notre rendu au CEREMA (mais pas sur le dépôt GITHUB du fait de sa taille).

Veuillez également télécharger le logiciel **Raspberry Pi Imager** trouvable sur le site ci-contre:

https://www.raspberrypi.com/software/

#### Procédure d'installation:

Une fois ces deux éléments acquis, insérez dans le port micro-SD de votre ordinateur la carte utilisée pour l'OS, rendez-vous sur le logiciel Raspberry Pi Imager:

1. Dans l'onglet *Device*, sélectionnez la carte *Raspberry Pi 0 2W*.
2. Dans l'onglet *OS*, choisissez "**Utiliser image personnalisée**" et sélectionnez l'archive téléchargée précédemment.
3. L'installation est ensuite guidée.

Une fois la carte formatée, introduisez-la dans le port de la Raspberry et alimentez-la par l'intermédiaire du port micro-USB *PWR IN*.

### Première Connexion au Système

#### Cas de réinstallation de l'OS (Première fois)

Au démarrage de la carte Raspberry Pi, un écran de connexion s'affiche:

1. Choisissez la configuration de votre clavier.
2. Saisissez le nom d'utilisateur (*login*) et le mot de passe que vous souhaitez utiliser. Nous conseillons "**rpi0**" et "**0000**" pour une utilisation simplifiée. Validez pour accéder au système et utiliser les fonctionnalités de la carte.

#### Cas de base (Utilisation habituelle)

- Fournissez simplement votre login et mot de passe choisis précédemment pour accéder au shell.

### Configuration Initiale du Système

Dans le terminal, renseignez la commande pour accéder à l'outil de configuration:

```bash
sudo raspi-config
```

#### Configuration du réseau sans fil:

1. Choisissez **System Options** → **Wireless LAN**.
2. Choisissez le pays, puis rentrez le nom SSID du réseau auquel vous voulez connecter la Raspberry Pi et enfin son mot de passe.

#### Activation de SSH:

1. Dans la partie **Interface Options**, activez le support **SSH** (utile pour la gestion à distance).

### Connexion à un nouveau réseau Wi-Fi

Si vous souhaitez vous connecter à un nouveau réseau, utilisez l'outil **nmcli** pour gérer les connexions réseau directement depuis le terminal.

#### Commandes essentielles:

- Lister les réseaux disponibles:
```bash
nmcli device wifi list
```

- Se connecter à un réseau:
```bash
sudo nmcli device wifi connect "SSID" password "MotDePasse"
```

### Installation des Librairies

Dans le cas d'utilisation optimisé, il est nécessaire d'installer les outils natifs de `libcamera`:

1. Mise à jour des paquets:
```bash
sudo apt update
```

2. Installation de `libcamera-dev`:
```bash
sudo apt install libcamera-dev
```

Bien que `libcamera` soit présente sur l'OS de base, cette installation assure la présence des bibliothèques natives utilisées pour le cas optimisé.

#### Autres bibliothèques utiles (Optionnel):

- **fbi** (Pour visualiser une photo depuis le terminal):
```bash
sudo apt install fbi
sudo fbi -T 1 NomDuFichier.jpg
```

- **ExifTool** (Pour afficher les métadonnées des photos):
```bash
sudo apt install libimage-exiftool-perl
exiftool NomDuFichier.jpg
```

### Créer une connexion SSH (Recommandé)

L'accès SSH simplifie la gestion et le transfert de fichiers.

1. **Vérification du réseau sur la Raspberry Pi:**
   Vérifiez que votre carte est bien connectée au réseau (par exemple, en lançant une requête ping):
```bash
ping google.com
```

2. **Récupération de l'adresse IP et vérification de la communication (depuis votre PC):**
   - **Sur Linux:** Essayez `ping rpi0.local`.
   - **En cas d'échec ou sur Windows:** Récupérez d'abord l'adresse IP de votre RPi avec la commande `ip a` sur la carte. L'adresse devrait se trouver dans la partie `inet`.
   
   Sur votre ordinateur, vérifiez que la communication est établie (les deux appareils doivent être sur le même réseau):
```bash
ping adresse_ip
```

3. **Connexion SSH:**
   Vous pouvez dès à présent vous connecter en SSH avec la commande:
```bash
ssh login@adresse_ip
```

Vous êtes maintenant connecté!

#### Configuration sur VS Code (Optionnel)

Pour une connexion plus facile via l'éditeur:

1. Installez l'extension **Remote-SSH**.
2. En bas à gauche, cliquez sur l'icône avec les symboles `><`, puis sélectionnez "**Connect to Host**".
3. Sélectionnez "**Add New Host**", renseignez une nouvelle fois la commande `ssh login@adresse_ip`.
4. Sélectionnez le fichier se terminant par `ssh/config`.
5. Renseignez le mot de passe. Vous êtes maintenant connecté (il peut être nécessaire de relancer la fenêtre).

Vous pouvez dès à présent ouvrir votre environnement de travail et utiliser le terminal intégré.

### Importation du Code

Il est maintenant nécessaire de transférer le code d'acquisition vers votre carte.

#### Solution Recommandée (via SSH):

Après s'être connecté en SSH via VS Code ou un autre IDE, créez un nouveau fichier via le terminal et servez-vous de l'interface fournie par votre IDE pour copier-coller le code.

#### Solution Alternative (via Git - nécessite une installation):

1. Installez Git:
```bash
sudo apt update
sudo apt install git-all
```

2. Clonez le dépôt GitHub (attention à la taille):
```bash
git clone https://github.com/hazard3045/Commande-entreprise-10.git
```

#### Création du répertoire de stockage des images

Dans le même répertoire où vous avez copié votre code, créez le dossier `images`:

```bash
mkdir images
```

### Lancement du code au démarrage de la Raspberry

Si vous souhaitez que la caméra soit fonctionnelle dès l'allumage de la Raspberry pi Zéro, et que le code se lance automatiquement, veuillez suivre les étapes suivantes:

1. Ouvrir crontab depuis l'invite de commande:
```bash
crontab -e
```

2. Ajouter la ligne suivante:
```bash
@reboot /usr/bin/python3 /home/rpi2/Documents/cerema-10/Commande-entreprise-10/impulsions/test_pwm.py &
```

### Entrées/Sorties de la Raspberry Pi 0

**Raspberry Pi Zero (Récepteur)**

| Nom du signal | PIN | Direction | Description |
|---------------|-----|-----------|-------------|
| DATA_IN | 11 | entrée | Reçoit les impulsions |
| CLK_IN | 13 | entrée | Reçoit les fronts d'horloge |

![Schéma de câblage de la carte Pi Zero](Figures/schema%20de%20cablage.png)

### Convertir le signal 5V de l'horloge du GPS pour la raspberry

La Raspberry Pi 0 fonctionne en 3.3V, l'horloge du GPS quant à elle fournit un signal en 5V logique. Puisqu'il est dangereux de fournir du 5V directement sur les pins GPIO de la raspberry pi, il est nécessaire d'implémenter un pont diviseur de tension pour protéger la carte.

- R₁ = 3.3 kΩ
- R₂ = 2.7 kΩ

![Schéma du pont diviseur de tension](Figures/schema.png)

*Source: https://forums.raspberrypi.com/viewtopic.php?t=160923*

---

## Utilisation et Modes d'Acquisition

Chaque photo sera nommée ainsi : `photo_nbImpulsions_clk_externe_clk_interne.dng` où :

- `nbImpulsions` : le compteur d'impulsions (sur 4 chiffres).
- `clk_externe` : le nombre de fronts d'horloge envoyés par le GPS depuis l'activation du programme (qui correspond également au nombre de secondes) (sur 4 chiffres).
- `clk_interne` : L'horloge interne de la Raspberry (revient à 0 toutes les heures) (sur 12 chiffres).

#### Exemple de nom de fichier:

`photo_0017_0150_001826347678.dng`

Cette méthode de nommage permet d'observer si des photos n'ont pas été prises et de classer les photos chronologiquement, facilitant la concordance avec les métadonnées de l'autopilote.

### Cas Classique: Fréquence 0.4Hz

Programme d'acquisition : `main.cpp`

#### Compilation:
```bash
g++ -o exe main.cpp -lpigpio
```

#### Exécution:
```bash
sudo ./exe
```

#### Récupération des données:

Utilisez un ordinateur sous Linux muni d'un lecteur de carte SD afin de récupérer les images dans le dossier `images/`.

### Cas Classique avec Sauvegarde USB (Conseillé pour l'instant): Fréquence 0.5Hz

Ce mode permet de sauvegarder les images directement sur une clé USB.

#### Montage de la clé USB:

1. Vérifiez que la clé USB est bien détectée (disque de type `sda1`):
```bash
lsblk
```

2. Créez le point de montage:
```bash
sudo mkdir -p /mnt/usb
```

3. Définissez le propriétaire:
```bash
sudo chown -R rpi0:rpi0 /mnt/usb
```

4. Montez la clé USB (Le message d'erreur initial est normal):
```bash
sudo mount /dev/sda1 /mnt/usb
sudo umount /dev/sda1
sudo mount -o uid=login,gid=mot_de_passe,umask=000 /dev/sda1 /mnt/usb
```

#### Test de l'installation (Optionnel):

```bash
echo "test" > /mnt/usb/test.txt
```

Vérifiez que le fichier `test.txt` est bien présent dans `/mnt/usb/`.

Programme d'acquisition : `main_with_usb.cpp`

#### Compilation:
```bash
g++ -o exe main_with_usb.cpp -lpigpio
```

#### Exécution:
```bash
sudo ./exe
```

#### Récupération des données:

Il suffit de retirer la clé USB.

### Cas Optimisé: Fréquence 1.7Hz

Ce mode utilise les fonctionnalités natives de `libcamera` pour une fréquence d'acquisition plus élevée.

Programme d'acquisition : `native.cpp`

#### Compilation:
```bash
g++ -o nat native.cpp $(pkg-config --cflags --libs libcamera) -lpigpio -lpthread -std=c++17
```

#### Exécution:
```bash
sudo ./nat
```

#### Récupération et Conversion de données (Post-acquisition):

Après avoir récupéré le dossier `images` depuis la carte SD, vous devriez avoir pour chaque photo un fichier `.raw` et un fichier `.raw.info`.

1. Conversion d'une seule photo en `.Tif`:
```bash
python3 convert.py photo.raw
```

2. Conversion de toutes les images du dossier en une fois:
```bash
python3 convert.py --batch
```

---

**Document réalisé dans le cadre du projet Commande Entreprise - IMT Atlantique**
