# Analyse — gemma4

**Sources analysées** : `competitors/gemma4/firmware_main.cpp`, `competitors/gemma4/README.md`

---

## ✅ Points techniques positifs

### WiFi / Réseau
- Le mode AP+STA est demandé et l’AP possède un mot de passe WPA2 minimal.

### Interface Web
- La page est compacte et actualise les trois valeurs affichées sans rechargement, donc sans voler le focus des champs.

### Documentation hardware/code
- L’intention d’utiliser un NPN pour adapter la sortie tach à une pull-up Deye inconnue est pertinente.

### Qualité de code générale (positif)
- **Structure / organisation** : fichier court, séparé en callbacks, web, `setup()` et `loop()`.
- **Lisibilité / nommage** : rôle général des compteurs et RPM facilement identifiable.
- **Commentaires / documentation inline** : intention du NPN et de la LED explicitée.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR
- [critique] Les ISR GPIO ne portent pas `IRAM_ATTR` — impact : crash possible lorsque le cache flash ESP8266 est indisponible.
- [majeur] `processData()` s’exécute via `Ticker` et manipule flottants/états partagés sans section critique — impact : contexte timer logiciel non déterministe et lectures incohérentes.
- [majeur] Les compteurs sont lus pendant leur modification par ISR — impact : snapshots non atomiques et calculs instables.

### Démarrage / Boot readiness
- [critique] La sortie n’est jamais basculée, au boot comme ensuite — impact : le Deye voit un ventilateur arrêté en permanence.
- [majeur] Aucun état initial explicite du GPIO de sortie, pull-down de base ou stratégie de readiness — impact : niveau matériel indéterminé avant `setup()`.

### Génération sortie / Timer1
- [critique] Aucun Timer1 ni aucun générateur de pulse n’est implémenté malgré les affirmations du README — impact : fonctionnalité principale totalement absente.
- [critique] Une seule sortie est prévue et les deux voies sont moyennées — impact : impossible d’alimenter séparément les deux entrées tach du Deye.
- [critique] Aucune période, relation 2 PPR, plage de fréquence ou stratégie anti-dérive n’existe — impact : aucun signal tach conforme ne peut être produit.

### Fidélité au protocole tachymétrique / Fail-safe
- [critique] Toutes les 0,5 s, le code calcule `count * ratio` sans facteur temps ni division par 2 PPR — impact : la valeur nommée RPM n’est pas un RPM.
- [critique] Les compteurs ne sont jamais remis à zéro — impact : les valeurs croissent depuis le boot même à vitesse constante puis après arrêt.
- [critique] Aucun timeout de perte, état stall ou fail-safe — impact : panne ventilateur non détectable.
- [majeur] Moyenner un ventilateur 9 cm et un 6 cm n’a aucune signification pour deux canaux Deye indépendants — impact : consigne conceptuellement fausse.

```cpp
void processData() {
    rpm9 = (uint32_t)(count9 * ratio9);
    rpm6 = (uint32_t)(count6 * ratio6);
    simRPM = (rpm9 + rpm6) / 2;
}
```

### Filtres / Signal
- [majeur] Aucun anti-rebond, filtre, plage plausible ou timeout — impact : parasites comptés comme des tours et absence de diagnostic d’arrêt.

### WiFi / Réseau
- [critique] `WiFi.begin()` n’est jamais appelé — impact : STA annoncé mais inexistant.
- [critique] Aucun formulaire/endpoint pour SSID ou mot de passe et aucune reconnexion — impact : WiFi non configurable contrairement au besoin.
- [majeur] AP fixe `12345678`, sans authentification applicative — impact : accès local trivial.

### Gestion mémoire / HEAP
- [majeur] HTML non placé en `PROGMEM` et JSON assemblé par concaténations `String` — impact : RAM gaspillée et fragmentation potentielle.

### Interface Web
- [critique] Les champs ratio n’ont ni bouton, ni handler, ni route de sauvegarde — impact : ils sont purement décoratifs et ne modifient jamais le firmware.
- [majeur] Aucune validation ou gestion d’erreur réseau — impact : valeurs obsolètes présentées sans alerte.

### Persistance / Stockage
- [critique] Ni ratios ni WiFi ne sont persistés ; aucune EEPROM/LittleFS n’est présente — impact : exigence de configuration durable entièrement absente.

### Documentation hardware/code
- [critique] Aucun ORing des deux sources 12 V ni buck 12→5 V — impact : risque de backfeed ou destruction du Wemos en cas de câblage naïf.
- [critique] Une seule sortie NPN est documentée — impact : le second canal Deye ne peut pas être simulé.
- [majeur] Schéma d’entrée ambigu : la résistance 10 kΩ n’est pas clairement un pull-up 3,3 V et aucun raccordement complet n’est donné — impact : entrée flottante ou surtension possible.
- [majeur] Le README parle d’« isolation » alors qu’un NPN avec masse commune n’assure pas d’isolation galvanique — impact : compréhension électrique erronée.
- [majeur] Pas de BOM complète, dimensionnement, découplage, contrôles ou calibration — impact : montage non reproductible et risqué.

### Qualité de code générale (négatif)
- [critique] **Gestion d’erreurs / robustesse** : `PIN_TACH_6m` est utilisé mais jamais déclaré — impact : échec de compilation certain.
- [critique] **Respect des conventions C++/Arduino** : le fichier se termine par des backticks Markdown et emploie GPIO10/11 comme « D1/D2 », alors que D1/D2 valent GPIO5/4 et GPIO10/11 sont liés à la flash — impact : compilation impossible puis boot/crash probable après correction partielle.

```cpp
#define PIN_TACH_9cm  10 // Physical Pin D1
#define PIN_TACH_6cm  11 // Physical Pin D2
// ...
attachInterrupt(digitalPinToInterrupt(PIN_TACH_6m), I_6m, FALLING);
```

- [critique] **Structure / maintenabilité** : exigences centrales absentes et commentaires promettant des fonctions inexistantes — impact : réécriture nécessaire, pas simple correction.
- [majeur] **Lisibilité / documentation** : README et code se contredisent sur Timer, WiFi Manager, ratios et sortie — impact : validation trompeuse.
- [majeur] **Sécurité basique** : AP à secret trivial et aucune authentification.
- [critique] **Testabilité** : aucun signal à mesurer, test, procédure de build ou instrumentation — impact : erreurs élémentaires non détectées.

---

## ⭐ Note globale : 0,3/10 — Fonction absente
Le livrable ne compile pas, ne génère aucun tach, ne fournit qu’une sortie et omet persistance, WiFi STA configurable et alimentation sûre.

## ⭐ Note qualité de code : 0,8/10 — Prototype incohérent
La petite taille facilite la lecture, mais erreurs syntaxiques, mauvais GPIO et fortes divergences entre documentation et code imposent une réécriture.
