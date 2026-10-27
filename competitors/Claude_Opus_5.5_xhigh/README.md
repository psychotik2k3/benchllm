# Deye Fan Simulator — Wemos D1 mini (ESP8266)

Simulateur de signal tachymétrique pour l'onduleur **Deye SUN-8K-SG05LP1-EU-AM2-P** dont les
ventilateurs NMB d'origine (06025VE-12N-CL 6 cm et 09225VE-12N-CU 9 cm) ont été remplacés par
2 × Noctua NF-A6x25 FLX et 2 × Noctua NF-A9 FLX, plus lents. Pour chaque taille de ventilateur, le
Wemos mesure la vitesse réelle d'un Noctua, la multiplie par un ratio réglable depuis une page
web, et renvoie à l'onduleur un signal tach équivalent. Si le Noctua s'arrête, le signal s'arrête
aussi : l'alarme ventilateur de l'onduleur reste fonctionnelle.

## Contenu du dossier

| Fichier | Contenu |
|---|---|
| [`deye_fan/deye_fan.ino`](deye_fan/deye_fan.ino) | Firmware complet et commenté, à ouvrir dans l'IDE Arduino |
| [`SCHEMA.md`](SCHEMA.md) | Schéma détaillé (alimentation, entrées, sorties), calculs, câblage, nomenclature, vérifications et sécurité |
| [`schema_complet.png`](schema_complet.png) / [`.svg`](schema_complet.svg) | Schéma électronique complet dessiné |
| [`outils/generer_schema.py`](outils/generer_schema.py) | Script qui régénère le schéma (`pip install schemdraw matplotlib`) |

## Compilation et téléversement

1. IDE Arduino 2.x → *Fichier › Préférences* → URL de gestionnaire de cartes :
   `https://arduino.esp8266.com/stable/package_esp8266com_index.json`
2. *Outils › Carte › Gestionnaire de carte* : installer **esp8266** (version 3.1.x ; testé avec 3.1.2).
3. Réglages du menu *Outils* :
   - Carte : **LOLIN(WEMOS) D1 R2 & mini**
   - CPU Frequency : **160 MHz** (80 MHz fonctionne aussi, le code s'adapte)
   - Flash Size : **4MB (FS:2MB OTA:~1019KB)**, nécessaire pour la mise à jour par le WiFi
4. Ouvrir `deye_fan/deye_fan.ino`, brancher le Wemos **seul, sur table** (jamais quand il est
   installé dans l'onduleur, voir `SCHEMA.md` §10) et téléverser.

Aucune bibliothèque externe : uniquement celles livrées avec le core ESP8266.

## Premier démarrage

1. Le Wemos crée le point d'accès WiFi **`DeyeFan-XXXXXX`** (XXXXXX = identifiant de la puce),
   mot de passe **`deyefan-setup`**.
2. En s'y connectant, un téléphone ouvre automatiquement l'interface (portail captif) ; sinon
   aller sur **http://192.168.4.1/**.
3. Carte *WiFi* : choisir le réseau domestique (bouton *Scanner*), saisir son mot de passe,
   changer le mot de passe du point d'accès, et éventuellement définir un mot de passe admin pour
   protéger l'interface (utilisateur `admin`). Le point d'accès reste toujours actif (AP + STA
   simultanés), ce qui permet de reprendre la main si le réseau domestique change.
4. Ensuite, l'interface est aussi accessible via l'IP obtenue sur le réseau domestique ou
   **http://deye-fan.local/** (mDNS).

Tous les réglages (ratios, WiFi, mots de passe, nom d'hôte) sont enregistrés en flash et
survivent aux coupures. Bouton *Config. usine* ou commande série `factory` pour revenir aux
valeurs par défaut.

## Interface web

- **Une carte par ventilateur** : RPM lus (Noctua), RPM simulés (envoyés à l'onduleur), fréquences,
  état *SIMULATION* ou *ATTENTE SIGNAL*, compteurs d'impulsions et de parasites rejetés.
  Rafraîchissement toutes les 500 ms.
- **Ratios** : un champ par canal, appliqués dès le cycle de sortie suivant et enregistrés,
  sans recompiler ni recâbler.
- **WiFi** : réseau domestique, point d'accès, nom d'hôte, mot de passe admin.
- **Système et isolation temps réel** : fréquence et durée max de la NMI, **retard max d'un front
  de sortie** et nombre de fronts en retard (mesurés dans la NMI elle-même : c'est la preuve, sur
  le matériel réel, que le WiFi ne perturbe pas le signal), mémoire libre, mise à jour du firmware
  (`/update`), redémarrage.

API JSON utilisée par la page (utilisable aussi en domotique) : `GET /api/status`,
`GET /api/config`, `POST /api/ratios` (`r0` = 9 cm, `r1` = 6 cm), `POST /api/wifi`,
`GET /api/scan`, `POST /api/reboot`, `POST /api/factory`, `POST /api/diag/reset`.

## Réglage des ratios

**ratio = RPM simulés / RPM lus.** Valeurs par défaut = vitesse nominale du NMB d'origine /
vitesse maximale du Noctua :

| Canal | NMB d'origine | Noctua | Ratio par défaut |
|---|---|---|---|
| 9 cm | 09225VE-12N-CU, ≈ 4500 tr/min | NF-A9 FLX, 1600 tr/min | 2,8125 |
| 6 cm | 06025VE-12N-CL, 7000 tr/min (fiche NMB) | NF-A6x25 FLX, 3000 tr/min | 2,3333 |

NMB ne publie pas la vitesse exacte du 09225VE-12N : 4500 tr/min correspond à la classe de
vitesse « N » de NMB (09225VA-12N). Plage autorisée : 0,25 à 8. Exemple : Noctua 9 cm lu à
1500 tr/min avec le ratio 2,8125 → 4219 tr/min envoyés à l'onduleur. En pratique, régler le ratio
pour que, ventilateurs à pleine vitesse, les RPM simulés correspondent à la vitesse que
l'onduleur attendait des NMB. Si les adaptateurs basse vitesse Noctua (L.N.A.) sont utilisés,
augmenter le ratio en conséquence.

## LED d'état (LED bleue du Wemos)

| LED | Signification |
|---|---|
| allumée fixe | simulation active sur les deux canaux |
| clignotement 1 Hz | simulation active sur un seul canal, l'autre attend un signal |
| flash bref toutes les 2 s | aucun signal reçu, attente des ventilateurs |

## Console série (115200 bauds)

`help`, `status`, `ratio <9|6> <valeur>`, `sta <ssid> [motdepasse]`, `ap <ssid> <motdepasse>`,
`factory`, `reboot`. Un état complet est affiché toutes les 10 secondes.

## Architecture logicielle

L'ESP8266 n'a qu'un cœur, et la pile WiFi masque régulièrement les interruptions ordinaires
pendant des dizaines à des centaines de microsecondes. `attachInterrupt`, `loop()` et les timers
logiciels sont donc exclus du traitement du signal.

- **Toute la chaîne tach tourne dans la NMI du Timer1 matériel** (interruption non masquable, que
  le SDK WiFi ne peut pas retarder). Code en IRAM, données en DRAM, aucune fonction appelée,
  aucune division ni flottant : elle s'exécute même pendant une écriture en flash ou une mise à
  jour du firmware.
- **Entrées** : échantillonnage à 20 kHz (toutes les 50 µs) avec anti-rebond, horodatage sur la
  grille planifiée du compteur de cycles CPU, moyenne glissante sur 8 périodes (4 tours, ce qui
  annule l'asymétrie entre pôles du moteur) puis filtre du 1er ordre.
- **Sorties** : chaque front est planifié à un instant absolu en cycles CPU. Le timer réveille la
  NMI 8 µs en avance, puis elle attend le cycle exact avant d'écrire le GPIO : la latence
  d'entrée en interruption est absorbée. Une nouvelle période n'est appliquée qu'en début de
  cycle, ce qui évite toute impulsion tronquée.
- **Échanges avec le reste du programme** par mots 32 bits alignés (atomiques), sans verrou : le
  serveur web, le WiFi ou une sauvegarde de configuration ne peuvent ni bloquer ni retarder la NMI.
- Le moteur tach démarre **avant** le WiFi : le signal ne dépend jamais du réseau.

Attention : la NMI du Timer1 est exclusive. Ne pas ajouter `analogWrite()`, `tone()`, `Servo` ni
`startWaveform()` au programme.

## Vérifications effectuées

- **Compilation** avec arduino-cli, core esp8266 3.1.2, carte LOLIN(WEMOS) D1 R2 & mini, Flash
  4MB (FS:2MB OTA:~1019KB), à **160 MHz et 80 MHz** : réussie, **aucun avertissement** dans le
  sketch (`--warnings all`). Flash 327 632 octets (31 %), RAM globale 31 416 octets sur 80 192
  (39 %).
- **Isolation de la NMI** vérifiée dans le fichier objet compilé : la routine (1 199 octets en
  IRAM) ne référence que ses deux blocs de données et ses propres sauts, sans aucun appel de
  fonction (ni en flash, ni ailleurs).
- **Simulation sur PC** du code réel de la NMI (extrait automatiquement du `.ino` et compilé avec
  g++), avec une latence d'entrée en NMI aléatoire de 0,5 à 6 µs, une asymétrie de pôles de 2 % et
  un débordement du compteur de cycles en cours d'essai, à 80 et 160 MHz :
  - précision des RPM simulés : **erreur ≤ 0,0014 %** (≤ 0,003 % juste après un changement de ratio) ;
  - avec une gigue mécanique de 5 µs RMS sur le tach Noctua : écart-type de la période de sortie
    **≈ 0,3 µs** pour des périodes de 4,35 et 6,72 ms (0,004 à 0,008 %) ;
  - fronts émis à **0,02–0,08 µs** de leur instant planifié ;
  - arrêt du Noctua : sortie stoppée et ligne libérée **0,5 s** après la dernière impulsion, puis
    reprise correcte au redémarrage ;
  - changement de ratio en marche : aucune impulsion tronquée.
- **Interface web** : JavaScript validé (`node --check`), cohérence vérifiée entre identifiants
  HTML et gestionnaires.

Non vérifié faute de matériel : comportement sur un vrai Wemos et un vrai onduleur. La page
web affiche les mesures internes (retard max d'un front, durée de la NMI, parasites) pour le
contrôler une fois installé.

## Limites connues

- Un seul Noctua par canal est mesuré, comme demandé : la panne du second Noctua d'un même canal
  n'est pas signalée à l'onduleur. Des GPIO libres (D7) permettraient de l'ajouter.
- En simulation, une latence de NMI supérieure à 8 µs (pic artificiel de 30 µs) retarde le front
  concerné de l'excédent. Ce cas n'est pas attendu pour une NMI, mais il serait visible dans le
  compteur « fronts en retard » de l'interface.
- Le montage suppose des masses communes aux deux prises ventilateur de l'onduleur (à vérifier
  avant installation, `SCHEMA.md` §8).
- Les Noctua brassent moins d'air que les NMB : surveiller les températures de l'onduleur, et
  baisser les ratios si l'onduleur se révèle réguler la vitesse des ventilateurs à partir du tach.
