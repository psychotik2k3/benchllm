# Analyse — qwen35

**Sources analysées** : `competitors/qwen35/firmware_main.cpp`, `competitors/qwen35/schematic.md`, `competitors/qwen35/calibration_guide.md`, `competitors/qwen35/README.md`

---

## ✅ Points techniques positifs

### Documentation hardware/code
- Fourniture d'un guide de calibration et d'un schéma au format texte.

---

## ❌ Points techniques négatifs

### Compilation / Syntaxe (critique)
- [critique] : Erreur de syntaxe grossière dans `firmware_main.cpp` (ligne 76) :
  ```cpp
  samples9[`] = rpm9_raw;
  ```
  La présence d'un accent grave / backtick `` ` `` au milieu de l'indice de tableau provoque une erreur de compilation immédiate `error: stray '`' in program` — impact : le code ne compile absolument pas.
- [critique] : Utilisation erronée du registre matériel `GPOS` comme s'il s'agissait d'une fonction :
  ```cpp
  GPOS(PIN_OUTPUT, !outState);
  ```
  Sur ESP8266, `GPOS` est un pointeur vers un registre matériel 32 bits (`#define GPOS (*(volatile uint32_t *)0x60000304)`), et non une macro fonctionnelle à deux arguments ! Cela génère une erreur fatale `error: called object is not a function or function pointer` — impact : deuxième erreur de compilation bloquante.

### Hardware / Affectation des broches (critique)
- [critique] : Affectation des broches GPIO 10 et 11 :
  ```cpp
  #define PIN_TACH_9CM 10 // Physical Pin D1 (Faux ! D1 est GPIO5)
  #define PIN_TACH_6CM 11 // Physical Pin D2 (Faux ! D2 est GPIO4)
  ```
  L'auteur confond gravement la numérotation des broches physiques du Wemos D1 Mini et les GPIO du microcontrôleur : la broche D1 correspond à GPIO 5 et D2 à GPIO 4. Les GPIO 10 et 11 sont les lignes directes du bus SPI de la mémoire Flash interne ! L'ESP8266 plante immédiatement lors du démarrage — impact : incompatibilité matérielle totale.

### Électronique / Transistors (critique)
- [critique] : Confusion de polarité de transistor dans `schematic.md` : le document préconise le transistor **SS8550** en le qualifiant d'étage de sortie "NPN". Or, le SS8550 est un transistor **PNP** ! Câblé comme un NPN (émetteur à la masse), le transistor ne commutera jamais correctement et risque d'être polarisé en inverse — impact : destruction ou non-commutation de l'étage de sortie.

### Temps-réel / Architecture logicielle (critique)
- [critique] : Une seule broche de sortie `PIN_OUTPUT 14` est configurée pour simuler les deux ventilateurs en même temps — impact : impossibilité de piloter indépendamment les deux canaux tachymétriques de l'onduleur Deye.
- [critique] : La minuterie Ticker est initialisée avec `calculationTicker.attach(250, updateSmoothedRPM)`. Dans la bibliothèque Arduino ESP8266, la méthode `attach()` prend un délai en **secondes** (flottant) et non en millisecondes ! La mise à jour des RPM n'a donc lieu qu'une fois toutes les **250 secondes** (soit plus de 4 minutes d'attente !) — impact : la vitesse simulée ne s'ajuste pratiquement jamais.

### Hallucination de contenu (critique)
- [critique] : Le fichier `README.md` inclut une configuration Home Assistant YAML complètement hors sujet destinée à un onduleur hybride de marque **Growatt SPF** avec des commutations de batterie, démontrant une hallucination sévère du modèle de langage — impact : confusion totale des livrables.

---

## ⭐ Note globale : 1.2/10 — Code incompilable, erreurs matérielles critiques et hallucinations
Le projet accumule toutes les défaillances possibles : syntaxe invalide, registres traités comme des fonctions, broches Flash SPI utilisées, transistor PNP confondu avec un NPN, rafraîchissement programmé toutes les 4 minutes et configuration d'un autre onduleur injectée par hallucination.

## ⭐ Note qualité de code : 1.8/10 — Qualité déplorable
Code manifestement non vérifié, truffé de confusions matérielles et conceptuelles majeures.
