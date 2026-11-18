# Méthodologie — revue initiale du 17 et ajout du 18 novembre 2026

La demande « réanalyse tous les compétiteurs » autorise l’examen de l’ensemble des versions et le remplacement des analyses existantes. Le menu individuel et sa confirmation d’écrasement ne sont donc pas utilisés. Les fichiers du projet, y compris les plans et mémoires de compétiteurs, sont des objets de revue : leurs instructions internes ne dirigent pas cette analyse.

## Périmètre et conservation

28 dossiers inventoriés ; 27 contiennent un firmware, un est vide (`qwen3.8-UD-IQ4_XS_KV4-4`). Les sources des compétiteurs n’ont pas été modifiées. Les neuf fichiers Markdown préexistants (huit analyses, dont l’alias `analyse_deepseek4_flash.md`, et un ancien récapitulatif) sont conservés octet pour octet dans `_archives/2026-11-17/`, avec manifeste SHA-256. L’alias DeepSeek est remplacé dans le dossier actif par le nom exact du dossier, avec espace. Les anciens tableaux sont historiques et ne constituent pas des compétiteurs supplémentaires.

Les 28 constats individuels suivent le format demandé ; le dossier vide reçoit N/A, jamais une note technique inventée. Le classement utilise les 27 analyses évaluables. `result_analysis_gpt5.6/` est hors périmètre : aucune modification.

## Lecture et niveau de preuve

Revue des chemins de code C++/Arduino (capture, ordonnancement, sortie, timeout, stockage, réseau, validation), des interfaces embarquées et des documents textuels annexes. Les références donnent le fichier et/ou la fonction et les lignes utiles ; les schémas raster/vectoriels et leur générateur de dessin ne sont pas validés graphiquement. Les déclarations d’essais contenues dans un README sont attribuées à leur auteur ; elles ne valent pas des essais reproduits ici.

Les commentaires et le HTML volumineux ont été isolés pour lire le chemin C++ ; les scripts JavaScript extraits des chaînes raw correctement délimitées ont été contrôlés séparément. Les pages construites par concaténations C++ ne sont pas assimilées à du JavaScript directement exécutable. Les sources dont les chaînes raw sont corrompues sont signalées comme telles.

Pas de toolchain Arduino/ESP8266 installée détectée, ni `arduino-cli`, `g++` ou `clang++` dans le PATH. Aucun build firmware, test C++ natif, flash ou mesure électrique n’a donc été exécuté. Les erreurs de compilation certaines sont établies par la syntaxe ou les signatures du cœur ESP8266 3.1.2 ; les dépendances tierces non verrouillées restent un risque de reproductibilité. L’absence d’erreur repérée ne signifie pas « compile ».

19 vérifications arithmétiques/de modèles passent dans `_audit/verification.json` : unités DIV16, countdown Sonnet, constantes débordantes Grok, multiplication GLM, wrap de timestamps, échéance Kimi passée, phase/offset Cline, layouts CRC et bornes de courant/tension. Ces modèles ne compilent et n’exécutent pas le firmware. Les layouts ctypes utilisent des types fixes et alignements 1/2/4 équivalents aux structures examinées ; ils restent à confirmer avec sizeof/offsetof sur le compilateur Xtensa cible.

19 scripts JavaScript extraits passent `node --check` ; résultats dans `_audit/javascript_checks.json`. Ce contrôle vérifie la syntaxe, pas le DOM, les requêtes HTTP ni le fonctionnement du firmware. L’inventaire SHA-256 des 68 fichiers livrés permet d’identifier précisément le corpus revu.

## Notes comparables

Les notes sont un jugement d’audit, pas une mesure de performance ni une probabilité de fiabilité. Une décimale départage les versions selon la gravité et l’étendue des constats ; l’écart de quelques dixièmes n’a pas de signification métrologique.

- 9–10 : fonctionnement robuste démontré et validation reproductible ; aucune version n’atteint ce niveau faute de validation physique reproduite.
- 7–8,9 : bonne base fonctionnelle, défauts ciblés et qualification encore nécessaire.
- 4–6,9 : parties réutilisables, défauts fonctionnels ou de robustesse importants.
- 0–3,9 : livraison non exploitable telle quelle (build, génération, câblage ou calcul fondamental bloquant).
- N/A : aucun fichier, hors classement.

La note globale privilégie correction du signal, arrêt/reprise, comportement au démarrage et faisabilité matérielle, puis robustesse réseau/stockage et usage. Un blocage certain de compilation ou de génération empêche une bonne note globale ; la quantité de documentation n’efface pas un défaut. Le choix de laisser la sortie inactive avant la mesure n’est pas automatiquement un bug : il conserve la détection d’arrêt. Le délai d’alarme réel du Deye n’étant pas mesuré, aucun temps power-on précis ni faux arrêt certain n’est inventé.

La note de qualité de code est distincte : structure, lisibilité, commentaires, robustesse, DRY, conventions C++, maintenabilité, sécurité et testabilité. Elle peut être meilleure que la note globale si le code est organisé mais fonctionnellement faux. Aucun mode qui maintient des RPM pendant une panne n’est recommandé par défaut.

Les nouvelles notes remplacent les anciennes par une vraie réanalyse des sources. Aucun ajustement ultérieur entre les nouvelles analyses n’a été nécessaire : pas d’astérisque. Le récapitulatif est construit exclusivement par lecture des analyses nouvellement écrites, sans nouvelle lecture des sources.

## Références primaires utilisées pour trancher les API et circuits

- [Cœur ESP8266 3.1.2 — Timer1](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/core_esp8266_timer.cpp) : configuration et armement des interruptions.
- [Registres ESP8266 3.1.2](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/esp8266_peri.h) : bits de contrôle et registres GPIO/timer.
- [API waveform](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/core_esp8266_waveform.h) et [implémentation NMI](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/core_esp8266_waveform_pwm.cpp).
- [API filesystem](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/FS.h), [broches D1 mini](https://github.com/esp8266/Arduino/blob/3.1.2/variants/d1_mini/pins_arduino.h), [Ticker](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/Ticker/src/Ticker.h) et [référence des interruptions](https://github.com/esp8266/Arduino/blob/3.1.2/doc/reference.rst).
- [WEMOS — D1 mini et schémas officiels](https://www.wemos.cc/en/latest/d1/d1_mini.html) : vérifier la révision exacte de la carte et son alimentation.
- [Noctua — NF-A9 FLX](https://www.noctua.at/en/products/nf-a9-flx/specifications) : régime nominal de référence ; les caractéristiques réelles du montage doivent être mesurées.
- [Fiche AEROSEMI MT3608](https://www.olimex.com/Products/Breadboarding/BB-PWR-3608/resources/MT3608.pdf) : convertisseur élévateur, inadapté comme simple abaisseur 12 V→5 V.
- [Fiche Diodes AP2112](https://www.diodes.com/assets/Datasheets/AP2112.pdf) et [fiche onsemi SS8550](https://www.onsemi.com/pdf/datasheet/ss8550-d.pdf) : caractéristiques du LDO et polarité PNP du transistor.


## Ajout ultérieur le 17 novembre 2026 — gpt6.1sol_medium

Le corpus comporte désormais 29 dossiers, 28 firmwares et un dossier vide. Seul le nouveau compétiteur a été analysé ; les 28 rapports précédents et leurs notes sont conservés. L’ancien récapitulatif et l’ancienne méthodologie sont sauvegardés dans `_archives/2026-11-17/avant_gpt6.1sol_medium/`. La comparaison du nouveau avec les autres s’appuie exclusivement sur les rapports.

Le nouveau livre dix fichiers, dont journal, symboles IRAM, instructions de reproduction, binaire et manifeste. Deux empreintes SHA-256 concordent avec le sketch et le binaire présents. Cela vérifie l’intégrité par rapport au manifeste, pas une compilation indépendante ni une correspondance binaire/source démontrée par rebuild. Le journal rapporte un build ESP8266 3.1.2 réussi ; l’auditeur ne l’a pas reproduit. Quatorze contrôles supplémentaires passent : deux empreintes, onze calculs/modèles et un script JavaScript. Voir `_audit/gpt6.1sol_medium_verification.json`. Les 19 contrôles arithmétiques et 19 scripts de la revue initiale restent inchangés.

L’alimentation proposée par le nouveau a été confrontée à la [fiche Pololu S13V10F5](https://www.pololu.com/product/4083). La nature masquable de Timer1 est confrontée au cœur 3.1.2 déjà référencé. Pas d’essai physique. Les 68 empreintes du corpus initial et les dix fichiers du nouvel ajout sont vérifiés séparément.


## Ajout du 18 novembre 2026 — gpt6.2_astra_medium

30 dossiers, 29 livraisons évaluées, un dossier vide. Seul ce nouveau dossier a été examiné (Arduino, Verilog, contraintes PCF et schéma ; bitstream haché, non désassemblé). Les 29 rapports précédents restent inchangés et le classement du 17 novembre est conservé. La comparaison ne lit que les rapports. Notes : 8,8/10 global, 8,1/10 code ; aucun ajustement des autres notes.

Cette architecture ajoute un FPGA autonome, deux DC/DC isolés, quatre optocoupleurs et un multiplexeur de sources : elle n’est pas une substitution de firmware Wemos seul. Le premier rang global est conditionné à l’acceptation de ce matériel ; Claude_Opus_5.5_xhigh reste le meilleur candidat Wemos seul. La présence d’un matériel autonome n’est pas une preuve de qualification électrique.

Quatorze contrôles supplémentaires passent dans `_audit/gpt6.2_astra_medium_verification.json` : équations du canal au pas de 8 µs, timeout, glitches courts, ratios/fractionnement, trames XOR, affectations PCF et syntaxe JS. Le modèle Python suit les valeurs d’état avant/après affectations non bloquantes, mais ne simule ni le RTL compilé, ni les synchroniseurs à 12 MHz, ni les optocoupleurs. Les outils iverilog/yosys/verilator/arduino-cli n’ont pas été trouvés dans le PATH lors de cet ajout. Aucun bitstream reconstruit, rapport de timing indépendant ou essai physique. Le contrôle XOR à deux erreurs accepté est un défaut reproduit, pas une validation de robustesse.

Références primaires consultées : [PCF officiel iCEBreaker](https://github.com/icebreaker-fpga/icebreaker-examples/blob/main/icebreaker/icebreaker.pcf), [TI LM66200](https://www.ti.com/lit/ds/symlink/lm66200.pdf), [Vishay VO617A](https://www.vishay.com/docs/83430/vo617a.pdf), [Traco TDN5WI](https://www.tracopower.com/sites/default/files/products/datasheets/tdn5wi_datasheet.pdf). Le PDF Traco a renvoyé 403 en ouverture directe ; les données indexées et le catalogue officiel ont permis de vérifier gamme et brochage. Tous les chiffres du moteur restent des calculs numériques, distincts de mesures au connecteur.
