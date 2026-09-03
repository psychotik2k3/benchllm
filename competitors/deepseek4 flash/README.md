# deye_fan — simulateur tach pour onduleur Deye

Remplacement des ventilateurs d'origine (NMB 06025VE / 09225VE) par des
**Noctua silencieux** sur onduleur Deye SUN-8K, avec **génération de signaux
tach (RPM) simulés** par un **Wemos D1 mini**.

## Contenu du dossier

| Fichier | Contenu |
|---|---|
| `deye_fan.ino` | Firmware Arduino/ESP8266 complet (compilable Arduino IDE) |
| `SCHEMA_ELECTRONIQUE.md` | Schéma électronique complet + BOM + mise au point |

## Fonctions livrées

- Lecture des 2 tach Noctua (2 impulsions/tour), conversion RPM.
- Génération de 2 tach simulés avec **ratio multiplicateur réglé par site
  web**, par voie, sans recompiler.
- WiFi **AP + STA simultanés**, paramètres persistés (EEPROM).
- LED d'état : allumée = simulation active ; clignote = attente signal.
- Interface web : ratios, états, RPM réels/simulés en temps réel (polling 1,5 s).
- **Sortie temps réel isolée du WiFi** : Timer1 matériel FRC1 en **NMI** ;
  l'ISR (IRAM, registres seuls) ne peut pas être retardé par la pile réseau.

## Compilation (Arduino IDE)

1. Boards Manager → installer **esp8266 by ESP8266 Community** (2.7.4 ou plus).
2. Carte sélectionnée : **LOLIN(WEMOS) D1 mini** (CPU 80/160 MHz : le calcul
   du timer n'en dépend pas, APB fixe 80 MHz).
3. Vérifier dans `deye_fan.ino` le bloc `USE_NMI_TIMER` (défaut `1` = NMI,
   le plus stable ; mettre `0` pour une ISR classique si besoin).
4. Compiler / flasher (port USB).

## Démarrage rapide

- Après flash : le point d'accès **« DeyeFanAP »** (passe : `deye-fan`)
  apparaît. Ouvrir `http://192.168.4.1`.
- Renvoyer : réglages des ratios, SSID/mot de passe STA (maison) et AP, état
  des 2 voies (RPM réels/simulés).
- Vérifier le câblage dans `SCHEMA_ELECTRONIQUE.md`.

## Notes techniques

- Les accès registres (FRC1, GPIO W1TS/W1TC) sont volontairement faits en
  bas niveau pour rester indépendants de la version du noyau.
- Jitter de sortie ≈ unité du compteur matériel (0,2 µs) ; le passage en NMI
  supprime tout délai introduit par les sections critiques WiFi.
- Deux voies indépendantes gérées par un seul timer (event-calendar).