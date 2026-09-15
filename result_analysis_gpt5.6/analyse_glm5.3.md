# Analyse — glm5.3

**Sources analysées** : `competitors/glm5.3/deye_fan/deye_fan.ino`, `competitors/glm5.3/deye_fan/README.md`, `competitors/glm5.3/deye_fan/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- ISR GPIO et sortie en `IRAM_ATTR`; calcul flottant et filtrage restent hors interruption.
- FRC1 en `TIM_SINGLE`, échéances absolues par compteur CPU et écritures directes `GPOS/GPOC`.
- Le rattrapage est borné à trois fronts, puis resynchronisé, ce qui évite une boucle ISR non bornée.

### Démarrage / Boot readiness
- GPIO, captures et Timer1 sont initialisés avant le réseau ; les sorties sont forcées au repos.
- Les résistances base-émetteur documentées gardent les NPN bloqués avant la prise de contrôle des GPIO.

### Génération sortie / Timer1
- Deux phases et demi-périodes indépendantes, rapport cyclique 50 %, plage bornée 5–1500 Hz.
- `nextEdge += half` évite la dérive cumulative des architectures à tick relatif.
- Les limites minimale et 23 bits du réarmement FRC1 sont prises en compte.

### Fidélité au protocole tachymétrique / Fail-safe
- Conversion correcte et centralisée pour 2 impulsions/tour (`RPM = Hz × 30`).
- Mesure sur l’intervalle réel entre timestamps plutôt que sur une fenêtre supposée exacte.
- Perte d’entrée détectée, sortie relâchée par défaut et mode fixe optionnel explicitement signalé comme dangereux.

### Filtres / Signal
- Anti-rebond 400 µs, fenêtre jusqu’à 128 timestamps/2 s et EMA 0,25.
- Entrées documentées avec pull-up 10 kΩ vers 3,3 V, série 1 kΩ et RC optionnel.

### WiFi / Réseau
- AP+STA permanent, reconnexion, portail DNS, mDNS, scan asynchrone et `WIFI_NONE_SLEEP`.
- Les mots de passe persistés ne sont pas renvoyés par l’API.

### Gestion mémoire / HEAP
- Page servie directement depuis `PROGMEM`; endpoints principaux sur buffers fixes avec `snprintf`, `strlcpy` et échappement JSON borné.
- Aucune allocation dans les ISR et heap libre exposé dans le statut.

### Interface Web
- Deux canaux complets, états attente/normal/cache-panne, fréquences/RPM, scan, réseau, reboot et reset.
- Validation serveur des plages et codes HTTP d’erreur adaptés.

### Persistance / Stockage
- Structure EEPROM avec magic, version, taille et CRC32; `EEPROM.commit()` est vérifié.
- Écritures uniquement à la demande, adaptées à une petite structure fixe.

### Documentation hardware/code
- ORing Schottky des deux 12 V, buck, réserve/découplage, TVS optionnelle et masse commune détaillés.
- Deux sorties NPN collecteur ouvert dimensionnées avec résistances base et base-émetteur.
- BOM, tableaux de raccordement, contrôles multimètre, test fréquencemètre et calibration fournis.
- Les hypothèses sur NMB, pull-up Deye et pilotage d’alimentation sont identifiées.

### Qualité de code générale (positif)
- **Structure / organisation** : séparation nette acquisition, mesure, génération, contrôle, réseau et web.
- **Lisibilité / nommage** : constantes regroupées, unités et invariants temporels explicites.
- **Commentaires / documentation inline** : le « pourquoi » électrique et temps-réel accompagne le code.
- **Gestion d’erreurs / robustesse** : CRC, validations bornées, fréquence limitée et erreurs EEPROM propagées.
- **Duplication / DRY** : tableaux et fonctions paramétrées factorisent réellement les deux canaux.
- **Respect des conventions C++/Arduino** : `static`, types de largeur fixe, `volatile`, calculs wrap-safe, `F()` et `PROGMEM`.
- **Complexité / maintenabilité** : configuration versionnée et sous-systèmes cohérents malgré un fichier unique.
- **Sécurité basique** : copies bornées, secrets non relus et échappement des contenus réseau.
- **Testabilité** : mode de sortie fixe, API détaillée, logs et procédure de banc facilitent les mesures.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR
- [majeur] `measureChannel()` désactive toutes les interruptions pendant la copie de jusqu’à 128 timestamps — impact : Timer1 est bloqué à chaque mesure (toutes les 100 ms par canal), créant une gigue mesurable potentiellement de plusieurs à dizaines de microsecondes et contredisant « aucune influence ».
- [majeur] Le rattrapage émet jusqu’à trois transitions presque consécutives — impact : impulsions trop courtes pour être comptées par le Deye, même si la phase logicielle est restaurée.
- [mineur] La boîte aux lettres `pend*` n’a ni section critique ni séquence/version — impact : deux demandes proches peuvent écraser ou mélanger transitoirement une mise à jour, malgré les écritures 32 bits atomiques.

### Démarrage / Boot readiness
- [majeur] Aucun tach n’est émis avant au moins quatre fronts valides — impact : délai d’environ 56 ms à 1600 RPM pour un tach unique, davantage à bas régime, sans garantie vis-à-vis du délai d’alarme Deye.
- [majeur] Au premier boot sans front, le cache-panne attend environ 7,5 s — impact : il ne protège pas une surveillance Deye plus rapide.
- [mineur] Les réglages par défaut issus d’une EEPROM invalide sont utilisés mais pas immédiatement persistés — impact : diagnostic « EEPROM invalide » répété à chaque boot jusqu’à la première sauvegarde.

### Génération sortie / Timer1
- [majeur] La revendication de gigue d’environ 1 µs n’est étayée par aucune capture; flash, sections critiques du firmware et WiFi peuvent retarder FRC1 — impact : garantie métrologique non démontrée.
- [mineur] La cadence de veille à 1 kHz reste active sans sortie — impact : consommation CPU évitable.

### Fidélité au protocole tachymétrique / Fail-safe
- [critique] Le modèle « deux tach en parallèle = fréquences additionnées, puis division par deux » est faux en général : les niveaux bas de ventilateurs non synchronisés se chevauchent et masquent des fronts — impact : RPM sous-estimé/irrégulier et sortie Deye fausse. Le besoin demandait de lire un ventilateur par canal, ce qui rend la division par défaut `parallelFans=2` fausse si ce câblage est suivi.

```cpp
meas[ch].fanHz  = emaHz[ch] / par;
meas[ch].rpmIn  = HZ_TO_RPM(meas[ch].fanHz);
// La sortie utilise ensuite fanHz, donc "par" modifie bien le tach Deye.
```

- [critique] Avec deux tach en parallèle, un ventilateur en panne n’est pas identifiable : l’autre maintient des fronts — impact : impossibilité de distinguer panne, recouvrement et simple baisse de régime.
- [majeur] L’EMA n’est jamais réinitialisée après perte — impact : au retour, la sortie converge depuis une mesure ancienne.
- [majeur] Avec cache-panne, la sortie normale s’arrête vers 2,5 s puis la sortie fixe ne commence que vers 7,5 s — impact : trou d’environ 5 s susceptible de déclencher l’alarme recherchée.
- [majeur] Le timeout de 2,5 s conserve la dernière sortie jusque-là — impact : panne réelle masquée durant ce délai.

### Filtres / Signal
- [majeur] L’EMA ajoute une latence sans preuve de compromis avec la dynamique thermique/Deye — impact : adaptation ralentie après variation brusque.
- [mineur] Aucun relevé oscilloscope ne valide l’anti-rebond et le RC dans l’environnement de l’onduleur.

### WiFi / Réseau
- [critique] Aucune authentification HTTP ni protection CSRF — impact : tout client AP/LAN peut modifier ratios, cache-panne, WiFi ou lancer reset/reboot.
- [majeur] Mot de passe AP par défaut public et non forcé à changer — impact : accès d’administration prévisible.
- [mineur] HTTP sans TLS expose les réglages sur un réseau non fiable.

### Gestion mémoire / HEAP
- [majeur] Scan WiFi et nettoyage du hostname utilisent encore des `String` agrandies — impact : scans/configurations répétés peuvent fragmenter le heap.

### Interface Web
- [majeur] Les opérations critiques ne nécessitent aucune session; les confirmations reboot/reset sont seulement côté JavaScript — impact : contournement trivial par requête directe.
- [majeur] `handleConfigPost()` modifie directement `S` au fil de la validation avant de connaître le résultat global — impact : une requête partiellement invalide peut laisser en RAM certains champs modifiés malgré la réponse 400 et sans persistance.
- [mineur] L’âge du dernier front, les rejets parasites et le délai fail-safe ne sont pas affichés — impact : diagnostic tach incomplet.

### Persistance / Stockage
- [majeur] Les secrets sont stockés en clair en flash; le CRC assure l’intégrité accidentelle, pas confidentialité/authenticité — impact : extraction physique/firmware des identifiants.

### Documentation hardware/code
- [critique] README et schéma qualifient le parallèle tach de « parfaitement supporté » alors que le wire-OR ne conserve pas une somme de fréquences — impact : recommandation de câblage fonctionnellement non fiable.
- [majeur] La documentation affirme que `parallelFans` « ne change PAS la fréquence envoyée », mais le code divise `fanHz` avant `controlTick()` — impact : calibration erronée d’un facteur 1 à 4.
- [majeur] Le cache-panne est cité comme protection d’une tension ventilateur trop basse, mais il ne peut fonctionner si le buck/Wemos n’est plus alimenté — impact : faux sentiment de résilience.
- [majeur] Le choix SMBJ14A et la plage réelle du buck nécessitent validation sur la tension maximale des connecteurs — impact : écrêtage ou dropout non maîtrisé.
- [mineur] Le dessin ASCII de sortie est ambigu; un schéma électrique normalisé réduirait les erreurs de montage.

### Qualité de code générale (négatif)
- [majeur] **Gestion d’erreurs / robustesse** : longue section critique de snapshot, cache-panne discontinu et mutation partielle de configuration — impact : gigue et états inattendus.
- [mineur] **Commentaires / documentation inline** : formulations absolues (« aucune influence », addition des tach) dépassent ce qui est démontré — impact : mauvaise décision d’intégration.
- [majeur] **Complexité / maintenabilité** : dépendance forte aux API internes Timer1 ESP8266, sans compilation CI — impact : portage/version de core fragile.
- [critique] **Sécurité basique** : administration complète non authentifiée — impact : contrôle local du système de simulation.
- [majeur] **Testabilité** : aucune suite automatisée ni preuve instrumentée de jitter/parallèle tach — impact : les risques centraux restent non quantifiés.

---

## ⭐ Note globale : 7,2/10 — Avancé mais hypothèse tach critique
La meilleure architecture des quatre pour le timer, la persistance, le web et le matériel, mais le câblage parallèle recommandé fausse la mesure et la section critique d’acquisition compromet l’isolation temporelle revendiquée.

## ⭐ Note qualité de code : 8,0/10 — Solide et maintenable
Code structuré, borné et bien documenté ; sécurité web, concurrence ISR et validation automatisée restent les principales dettes.
