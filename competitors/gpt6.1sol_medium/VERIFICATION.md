# Vérifications — 17 novembre 2026

## Effectuées

Compilation réelle avec Arduino CLI 1.3.1, core officiel ESP8266 3.1.2, carte `esp8266:esp8266:d1_mini`, options par défaut (CPU 80 MHz), option `--warnings all`. Résultat final : **succès, code de sortie 0, aucun avertissement du sketch**. Le journal est dans `verification/compilation.txt`.

| Ressource | Occupation rapportée |
|---|---:|
| Variables et constantes RAM | 30 172 / 80 192 octets |
| IRAM incluant 32 768 octets réservés au cache | 61 571 / 65 536 octets |
| Code IRAM proprement dit | 28 803 octets |
| Code flash | 285 480 / 1 048 576 octets |

La marge IRAM est limitée ; tout ajout de bibliothèque doit être recompilé et revérifié. Cette répartition n'est pas une mesure de la mémoire libre minimale sous forte charge HTTP.

Inspection du binaire ELF par `xtensa-lx106-elf-objdump` : `capture`, `tach9`, `tach6`, `tachTimer`, `micros` et `timer1_write` sont en section `.text1`, aux adresses IRAM `0x401...`. Les traitements entiers de période sont compilés sans calcul flottant dans les ISR. Les symboles utiles sont conservés dans `verification/iram-symbols.txt`. Leur placement en IRAM vérifie une condition nécessaire, pas une garantie d'isolation du WiFi.

Le binaire `verification/deye_fan.ino.bin` correspond au sketch final ; empreintes dans `verification/sha256.txt`. Il est destiné à cette carte et cette configuration, pas à une carte générique de taille flash différente. Les outils, paquets téléchargés et fichiers intermédiaires ont été retirés pour éviter de livrer plusieurs centaines de Mo inutiles.

Pour reproduire avec Arduino CLI disponible dans le PATH, depuis le dossier de ce livrable :

```powershell
arduino-cli core update-index --config-file .\verification\arduino-cli.yaml
arduino-cli core install esp8266:esp8266@3.1.2 --config-file .\verification\arduino-cli.yaml
arduino-cli compile --fqbn esp8266:esp8266:d1_mini --warnings all --config-file .\verification\arduino-cli.yaml .\deye_fan
```

Les chemins du fichier YAML correspondent au dossier demandé sur cette machine ; les adapter si le projet est déplacé. L'IDE Arduino peut également compiler le sketch selon README.md.

## Non effectuées

Aucun Wemos, ventilateur ou onduleur n'est connecté à cet environnement. Pas de téléversement, pas de mesure électrique, pas de mesure de jitter, pas d'essai WiFi réel ni de validation thermique. La page web n'a pas été exécutée sur une carte réelle. Le projet est compilable ; il n'est pas validé pour exploitation sur l'onduleur.

## Essais de banc nécessaires avant raccordement Deye

1. **Hors onduleur**, vérifier brochages E/B/C et connecteurs. Alimenter le circuit avec deux alimentations de laboratoire limitées en courant et à masse commune. Tester 12 V/0 V, 0 V/12 V, 12 V/8 V, puis permutation, et coupure des deux. Mesurer 5 V, 3,3 V et l'absence de courant de retour vers chaque source éteinte, hors courants de fuite spécifiés des diodes.
2. Charger le 5 V par les pointes réelles du Wemos et contrôler les creux lors des transmissions et commutations de source. Vérifier l'échauffement du régulateur, des diodes et des protections, ainsi que la capacité du connecteur à alimenter sa paire de fans et l'électronique.
3. Tester chaque sortie NPN avec un **rappel de laboratoire** de 4,7 kΩ successivement à 3,3 V, 5 V et 12 V. Vérifier à l'oscilloscope le niveau bas, le niveau haut, le rapport cyclique et la fréquence. Ces résistances sont des charges de test, pas des composants à ajouter au Deye.
4. Injecter sur les entrées un signal **collecteur ouvert ou logique 3,3 V**, jamais un générateur réglé à 5/12 V. Essayer 53,333 Hz (1 600 RPM à deux impulsions/tour) et 100 Hz (3 000 RPM) sur les deux canaux, puis des fréquences différentes en même temps. Ratio 2 attendu : environ 106,667 Hz et 200 Hz, dans les erreurs d'horloge et de quantification documentées. Modifier un seul ratio et vérifier que l'autre canal conserve sa fréquence.
5. Vérifier les transitoires : démarrage après quatre fronts qualifiés, changements de vitesse, changements de ratio, entrée bloquée à LOW ou HIGH et perte d'un canal. La sortie correspondante doit être libérée après le timeout documenté ; l'autre doit continuer. Tester le wrap de `micros()` par une acquisition de plus de 72 minutes, avec et sans activité réseau.
6. Mesurer les périodes **sur les collecteurs**, simultanément à la référence d'entrée, avec analyseur logique ou oscilloscope. Comparer distribution des intervalles et erreur de fréquence au repos, sous scrutation HTTP intensive authentifiée, reconnexions STA et changement WiFi. Faire aussi un test de reset. **Aucun résultat favorable ne transforme cette architecture en garantie stricte.** Les compteurs web ne remplacent pas ces captures externes.
7. Tester AP et STA simultanés, routeur absent, mot de passe invalide, reconnexion automatique, modification SSID et clés, reset par bouton. Vérifier qu'aucune clé n'est exposée dans `/api/status`, que l'accès sans authentification est refusé et qu'un POST sans jeton est refusé.
8. Modifier ratios et WiFi lorsque les fans tournent : constater l'état « sauvegarde en attente ». Arrêter les entrées en maintenant l'alimentation électronique, attendre la confirmation, redémarrer et vérifier la restauration. Tester la coupure avant sauvegarde (anciennes valeurs attendues) et une coupure pendant sauvegarde (CRC invalide possible, défauts attendus). Vérifier que le système ne présente pas comme persistante une modification seulement en RAM.
9. Seulement après caractérisation des masses, du brochage, du rappel tach et du courant admissible Deye, tester sur cet onduleur. Mesurer les signaux des NMB d'origine pour établir PPR et fréquence cible ; régler ensuite les ratios à partir de ces mesures. Ne pas supposer les seuils d'alarme. Vérifier ensuite le refroidissement dans les conditions défavorables : le comptage RPM simulé ne constitue pas ce contrôle.
