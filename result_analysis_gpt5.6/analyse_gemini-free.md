# Analyse — gemini-free

**Sources analysées** : `competitors/gemini-free/gemini-code-1787264142602.cpp` (liste exhaustive du dossier)

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Les deux captures GPIO et le callback Timer1 portent `IRAM_ATTR`.
- Les fronts d’entrée sont horodatés en interruption et la génération ne dépend pas directement de `loop()`.

### Démarrage / Boot readiness
- Les sorties sont configurées et forcées à `LOW` avant le réseau ; aucune attente de connexion STA ne bloque `setup()`.

### Génération sortie / Timer1
- Deux consignes et deux états de sortie indépendants sont prévus.
- L’intention mathématique de `15 000 000 / RPM` est correcte pour la demi-période d’un tach à 2 impulsions/tour.

### Fidélité au protocole tachymétrique / Fail-safe
- Le calcul d’entrée applique explicitement 2 impulsions/tour.
- Après une seconde sans front accepté, la consigne est annulée et la ligne est remise au repos.

### Filtres / Signal
- Un rejet des intervalles inférieurs à 5 ms élimine les rebonds très rapides.

### WiFi / Réseau
- Le mode AP+STA est non bloquant et le serveur asynchrone limite le travail réseau dans `loop()`.

### Gestion mémoire / HEAP
- HTML en `PROGMEM`, documents ArduinoJson statiques et identifiants copiés avec `strlcpy`.

### Interface Web
- Tableau de bord mobile, actualisation AJAX et réglage séparé des deux ratios.

### Persistance / Stockage
- LittleFS et JSON rendent la configuration extensible ; l’écriture n’a lieu qu’après action utilisateur.

### Qualité de code générale (positif)
- **Structure / organisation** : sections nettes pour acquisition, génération, stockage et web.
- **Lisibilité / nommage** : unités et rôle des variables principales sont compréhensibles.
- **Commentaires / documentation inline** : la relation RPM/période est explicitée.
- **Respect des conventions C++/Arduino** : emploi pertinent de `volatile`, `IRAM_ATTR`, `PROGMEM` et `digitalPinToInterrupt`.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR
- [majeur] `digitalWrite()` est appelé dans une ISR cadencée et les voies sont traitées séquentiellement — impact : coût et gigue supérieurs aux écritures directes `GPOS/GPOC`, avec décalage des fronts simultanés.
- [majeur] Le générateur est un compteur relatif sans rattrapage temporel — impact : toute ISR retardée décale la phase et la dérive s’accumule.
- [mineur] Quand une voie est arrêtée, son `digitalWrite(LOW)` est répété à chaque interruption — impact : charge ISR inutile.

### Démarrage / Boot readiness
- [majeur] LittleFS et le réseau sont initialisés avant les interruptions et Timer1 — impact : le Deye ne reçoit aucun tach pendant cette partie du boot.
- [majeur] `lastPulseTime=0` fait accepter le temps boot→premier front comme période — impact : une fréquence arbitraire est brièvement émise avant la deuxième impulsion réelle.
- [majeur] Aucun signal nominal de démarrage, watchdog/brownout applicatif ou délai de grâce documenté — impact : risque d’alarme Deye au redémarrage.

### Génération sortie / Timer1
- [critique] Avec `TIM_DIV16`, Timer1 avance à 5 MHz (0,2 µs/tick), mais le réarmement utilise `80 * 50 = 4000` ticks comme si le diviseur était 1 — impact : l’ISR s’exécute toutes les 800 µs tout en comptabilisant 50 µs, donc la sortie est environ **16 fois trop lente**.

```cpp
timer1_enable(TIM_DIV16, TIM_EDGE, TIM_SINGLE);
timer1_write(4000);
// puis dans l'ISR :
timer1_write(80 * timerStepUs);
```

- [majeur] Le dépassement remet le compteur à zéro sans conserver le reliquat — impact : arrondi systématique au tick supérieur et erreur de fréquence supplémentaire.

### Fidélité au protocole tachymétrique / Fail-safe
- [critique] Si un ratio devient nul, négatif ou non numérique, le bloc `rpmOut > 0` est sauté sans effacer l’ancienne consigne — impact : l’ancien faux tach peut continuer indéfiniment.
- [majeur] Aucun bornage des ratios ou de la fréquence générée — impact : overflow, période nulle ou signal irréaliste possible via HTTP/configuration.
- [majeur] `diff > 5000` limite les fronts acceptés à moins de 200 Hz, soit moins de 6000 RPM à 2 PPR, contrairement au commentaire « 12000 RPM » — impact : hauts régimes sous-estimés.
- [majeur] Le timeout d’une seconde maintient l’ancienne vitesse jusque-là — impact : panne ventilateur masquée pendant jusqu’à 1 s.

### Filtres / Signal
- [majeur] Aucun filtre de période, EMA, médiane ou rejet d’outlier après le seuil minimal — impact : un parasite accepté modifie immédiatement la sortie.

### WiFi / Réseau
- [critique] API sans authentification ni protection CSRF, avec AP fixe `12345678` — impact : tout client local peut modifier les ratios, le WiFi et redémarrer le module.
- [majeur] Aucune supervision de reconnexion STA — impact : accès station potentiellement perdu jusqu’au reboot.
- [majeur] Les paramètres métier HTTP ne sont pas validés — impact : état dangereux persisté.

### Gestion mémoire / HEAP
- [majeur] Les réponses JSON et paramètres asynchrones créent des `String` — impact : fragmentation possible du heap après de nombreux polls.

### Interface Web
- [majeur] Les champs ratio n’ont pas de bornes et les erreurs `fetch` sont ignorées — impact : saisie dangereuse et affichage silencieusement périmé.
- [mineur] La LED clignote en activité et reste éteinte à l’arrêt, sans état « attente » conforme au besoin — impact : diagnostic terrain ambigu.

### Persistance / Stockage
- [majeur] Échec de `LittleFS.begin()`, ouverture et sérialisation non remontés ; absence de version, CRC et écriture atomique — impact : perte/corruption silencieuse des réglages.

### Documentation hardware/code
- [critique] Aucun schéma ni document matériel n’est fourni — impact : absence de sortie NPN collecteur ouvert démontrée, d’ORing des deux 12 V et de buck ; un câblage direct peut endommager l’ESP8266 ou rétroalimenter l’onduleur.
- [majeur] Aucune BOM, instruction de compilation, procédure de contrôle ou mesure de jitter — impact : montage et build non reproductibles.

### Qualité de code générale (négatif)
- [majeur] **Structure / DRY** : logique des deux voies copiée dans l’ISR et `loop()` — impact : corrections divergentes probables.
- [majeur] **Gestion d’erreurs / robustesse** : premier échantillon faux, consigne obsolète et erreurs FS ignorées — impact : comportement tach non sûr.
- [majeur] **Conventions C++/Arduino** : fichier `.cpp` sans `Arduino.h` explicite et dépendances externes non versionnées — impact : compilation non reproductible selon l’environnement.
- [critique] **Sécurité basique** : administration distante ouverte — impact : modification non autorisée du comportement de refroidissement.
- [critique] **Testabilité** : aucun test, build CI ni mesure Timer1 — impact : l’erreur centrale ×16 n’a pas été détectée.

---

## ⭐ Note globale : 2,5/10 — Tach central incorrect
L’interface et l’architecture générale sont exploitables comme point de départ, mais la fréquence émise est environ seize fois trop basse et la partie matérielle obligatoire est absente.

## ⭐ Note qualité de code : 3,5/10 — Lisible mais non validé
Le code est facile à suivre, mais duplication, validations absentes, sécurité faible et absence de tests laissent passer des défauts critiques.
