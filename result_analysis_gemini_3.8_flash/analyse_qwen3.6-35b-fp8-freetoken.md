# Analyse — qwen3.6-35b-fp8-freetoken

**Sources analysées** : `competitors/qwen3.6-35b-fp8-freetoken/deye_fan.ino`, `competitors/qwen3.6-35b-fp8-freetoken/circuit.h`, `competitors/qwen3.6-35b-fp8-freetoken/README.md`

---

## ✅ Points techniques positifs

### Démarrage / Boot readiness
- Sorties configurées et mises à l'état inactif au démarrage dans `setup()`.

### WiFi / Réseau
- Configuration simultanée en mode `WIFI_AP_STA`.
- Serveur captif DNS implémenté pour faciliter l'accès au portail de configuration.

### Interface Web
- Interface HTML complète avec tableau de bord responsive, statut WiFi, métriques et formulaire de mise à jour des paramètres.

### Documentation hardware/code
- Fourniture d'un fichier `circuit.h` et d'un `README.md` détaillant les intentions de câblage avec des diagrammes ASCII clairs.

---

## ❌ Points techniques négatifs

### Compilation / API Timer inexistante (critique)
- [critique] : Le code utilise des fonctions de timer non déclarées dans le core Arduino ESP8266 officiel :
  ```cpp
  timer1Write(0);
  timer1AttachInterrupt(tachOutputISR);
  ```
  Sur ESP8266, les fonctions du core sont écrites en casse serpent (`timer1_write()`, `timer1_attachInterrupt()`). De plus, la fonction d'activation du timer `timer1_enable()` n'est **jamais appelée** dans le programme. La compilation échoue immédiatement avec des erreurs du type `error: 'timer1Write' was not declared in this scope` — impact : le code ne compile pas sur la cible Wemos D1 Mini / ESP8266.

### Temps-réel / ISR & Registres matériels (critique)
- [critique] : Dans `configureOutputTimer()`, la valeur de rechargement du timer `loadVal` est calculée mais **n'est jamais utilisée** :
  ```cpp
  uint32_t loadVal = (312500UL / freqHz) - 1;
  if (loadVal > 65535) loadVal = 65535; // Max 16 bits

  currentOutputPin = pin;
  pinMode(pin, OUTPUT);
  digitalWrite(pin, LOW);

  timer1Write(0); // <--- loadVal n'est JAMAIS écrit nulle part !
  timer1AttachInterrupt(tachOutputISR);
  ```
  La variable locale `loadVal` est calculée puis abandonnée. Le timer se voit assigner la valeur `0` en dur. En conséquence, la fréquence calculée n'est absolument jamais appliquée au matériel — impact : le timer matériel ne peut en aucun cas générer la fréquence demandée.
- [critique] : Attachement de l'interruption d'entrée en mode `LOW` :
  ```cpp
  attachInterrupt(digitalPinToInterrupt(TACH_INPUT_PIN), tachInputISR, LOW);
  ```
  Sur ESP8266 comme sur tous les microcontrôleurs Arduino, le mode `LOW` déclenche l'interruption de manière continue et ininterrompue tant que la broche reste au niveau bas. Lors d'une impulsion tachymétrique (qui dure plusieurs millisecondes), l'ISR `tachInputISR` s'exécute en boucle des milliers de fois par seconde, bloquant totalement le CPU et provoquant un crash immédiat par redémarrage du Watchdog (*Watchdog Reset / WDT crash*) — impact : plantage récurrent du système dès la première impulsion reçue.

### Architecture logicielle & Simulation des canaux (critique)
- [critique] : Le canal 2 n'est **jamais généré** !
  Dans `calculateRPMS()`, seule la sortie du canal 1 est pilotée :
  ```cpp
  uint32_t freqOut = (uint32_t)((float)rpm * 2.0f / 60.0f * fanRatio1);
  if (freqOut > 0 && freqOut <= 50000) {
      configureOutputTimer(freqOut, TACH_OUTPUT_1);
  }
  ```
  L'appel à `configureOutputTimer(..., TACH_OUTPUT_2)` n'existe nulle part dans le code. Le canal 2 reste perpétuellement éteint et inactif — impact : l'onduleur Deye détecte immédiatement la défaillance totale du second ventilateur.
- [critique] : Logique de mesure RPM détruite à chaque itération de la boucle `loop()` :
  La boucle `loop()` tourne avec un `delay(10)`. Dans `calculateRPMS()`, si aucun nouveau front n'a été validé pendant les 10 dernières millisecondes (`valid == false`), le code considère immédiatement que le ventilateur est à l'arrêt :
  ```cpp
  } else {
      rawRPM1 = 0;
      simRPM1 = 0;
      inputActive1 = false;
      ...
      stopOutputTimer(TACH_OUTPUT_1);
      stopOutputTimer(TACH_OUTPUT_2);
  }
  ```
  À 1 200 tr/min (40 impulsions/s), une impulsion n'arrive que toutes les 25 ms. Le système bascule donc en état d'arrêt 2 fois sur 3, coupant le timer de sortie et remettant les RPM à 0 ! Au lieu d'un timeout de sécurité de 1 à 2 secondes, le signal hache perpétuellement entre marche et arrêt à 40 Hz — impact : signal de sortie discontinu et inexploitable pour l'onduleur.

### Conception matérielle & Électronique (critique)
- [critique] : Calcul d'étage d'entrée erroné : la broche GPIO13 ne recevra jamais de niveau logique HAUT :
  Le schéma `circuit.h` prévoit un pull-up de 47 kΩ relié au +12V, suivi en série d'un diviseur composé de 39 kΩ et 10 kΩ vers la masse.
  La tension au point de mesure GPIO13 à l'état de repos (transistor ventilateur bloqué) vaut :
  \[
  V_{\text{GPIO13}} = 12\,\text{V} \times \frac{10\,\text{k}\Omega}{47\,\text{k}\Omega + 39\,\text{k}\Omega + 10\,\text{k}\Omega} = 12 \times \frac{10}{96} \approx 1,25\,\text{V}
  \]
  Or, la tension minimale pour un état logique HAUT (\(V_{IH\min}\)) sur l'ESP8266 est de \(0,75 \times V_{DD} \approx 2,47\,\text{V}\) (ou 2,0 V en TTL). La ligne plafonne à 1,25 V : l'ESP8266 restera perpétuellement à l'état BAS — impact : aucune détection de front possible sur l'entrée tachymétrique.
- [critique] : Mise en parallèle directe de 4 ventilateurs asynchrones sur une seule broche :
  Le schéma regroupe les 4 fils tachymétriques (les deux ventilateurs de 9 cm et les deux ventilateurs de 6 cm) sur la broche unique GPIO13. Les ventilateurs tournant à des vitesses et phases différentes, leurs impulsions se chevauchent de manière chaotique. De plus, le firmware attribue aveuglément cette valeur unique à `rawRPM1` et `rawRPM2` — impact : impossibilité de distinguer le régime des ventilateurs de 9 cm de celui des 6 cm.
- [critique] : Alimentation 3,3 V injectée sur la broche VIN (5V) du Wemos D1 Mini :
  `circuit.h` recommande de régler le convertisseur buck sur 3,3 V et de brancher sa sortie sur la broche VIN du Wemos. La broche VIN alimente le régulateur LDO interne (3,3 V) qui nécessite une chute de tension (*dropout*) minimale de 300 mV à 1 V. L'ESP8266 se retrouve sous-alimenté à environ 2,5 V — impact : redémarrages intempestifs (*brown-outs*) et dysfonctionnements majeurs du module WiFi.

### Persistance / Stockage (majeur)
- [majeur] : Absence totale de somme de contrôle, de CRC ou de mot magique dans l'EEPROM. En cas de mémoire non initialisée (valeurs à `0xFF`), `passLen` est lu comme 255, provoquant un débordement de tampon potentiel.

---

## ⭐ Note globale : 1.5/10 — Projet lourdement défaillant et non fonctionnel
Le livrable ne compile pas sur le core ESP8266, utilise un mode d'interruption `LOW` qui sature et fait crasher le processeur, ne transmet jamais la fréquence calculée au timer matériel, ne génère jamais le canal 2, hache le signal toutes les 10 ms et intègre un étage d'entrée plafonnant à 1,25 V.

## ⭐ Note qualité de code : 2.5/10 — Illusion de structure masquant des fautes bas niveau majeures
Le code présente une bonne mise en page documentaire, mais trahit une incompréhension totale des mécanismes d'interruption, de l'architecture matérielle des timers ESP8266 et des lois élémentaires des circuits diviseurs de tension.
