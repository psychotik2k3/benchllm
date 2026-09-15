# Analyse — gemini 3.1 pro

**Sources analysées** : `competitors/gemini 3.1 pro/deye_fan.ino`, `competitors/gemini 3.1 pro/schema_et_explications.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Les ISR GPIO et Timer1 portent `IRAM_ATTR`; les ISR d’entrée se limitent à incrémenter des compteurs.
- Les compteurs sont lus/remis à zéro sous section critique et les sorties utilisent directement `GPOS/GPOC`.

### Démarrage / Boot readiness
- Les sorties sont mises au repos avant le WiFi, qui ne fait l’objet d’aucune attente bloquante.

### Génération sortie / Timer1
- Deux voies indépendantes sont générées par FRC1 à pas de 50 µs.
- La formule visée `300000/RPM` est cohérente avec 2 impulsions/tour et des demi-périodes exprimées en ticks de 50 µs.

### Fidélité au protocole tachymétrique / Fail-safe
- Le comptage sur fronts descendants respecte les sorties tach à collecteur ouvert.
- Une fenêtre sans impulsion annule la sortie ; les NPN sont alors bloqués et les lignes Deye relâchées.

### WiFi / Réseau
- AP+STA simultanés et endpoint JSON de suivi des quatre RPM.

### Gestion mémoire / HEAP
- HTML en `PROGMEM` et état temps-réel de taille fixe.

### Interface Web
- Affichage mobile, polling sans rechargement et ratios réglables sans recompilation.

### Persistance / Stockage
- Configuration compacte en EEPROM, écrite uniquement sur sauvegarde explicite.

### Documentation hardware/code
- Le document retient correctement deux NPN en collecteur ouvert, un buck 12→5 V, des masses communes et un ORing des deux alimentations.
- Les entrées Noctua sont tirées à 3,3 V et les sorties tolèrent la pull-up Deye inconnue.

### Qualité de code générale (positif)
- **Structure / organisation** : sections courtes et responsabilités identifiables.
- **Lisibilité / nommage** : canaux 9/6 cm et unités sont explicites.
- **Commentaires / documentation inline** : calculs RPM et Timer1 sont détaillés.
- **Respect des conventions C++/Arduino** : usage pertinent de `volatile`, `IRAM_ATTR`, `PROGMEM` et registres GPIO.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR
- [majeur] L’ISR fixe tourne à 20 kHz même lorsque les deux sorties sont arrêtées — impact : charge CPU permanente inutile et pression accrue sur WiFi/watchdog.
- [majeur] Le comptage est relatif et ne conserve aucune échéance absolue — impact : une ISR retardée décale durablement la phase ; l’affirmation « zéro jitter » est fausse.

### Démarrage / Boot readiness
- [majeur] La première consigne n’est calculée qu’après environ une seconde — impact : absence de tach suffisamment longue pour déclencher une alarme Deye si sa surveillance démarre tôt.
- [majeur] Aucun signal nominal de boot, délai de grâce ou gestion brownout documentée — impact : readiness non maîtrisée.
- [mineur] Le schéma omet un pull-down base-émetteur — impact : transistor potentiellement flottant avant la configuration GPIO.

### Génération sortie / Timer1
- [critique] Un ratio sans maximum peut porter `rpm_out` au-delà de 300000 ; `300000 / rpm_out` devient zéro, interprété comme arrêt — impact : une saisie excessive provoque un faux stall.
- [majeur] La résolution de 50 µs et la troncature imposent une erreur de fréquence, croissante aux hauts régimes.
- [majeur] Aucun rattrapage de phase n’existe après une interruption retardée — impact : dérive cumulative sous charge.

### Fidélité au protocole tachymétrique / Fail-safe
- [majeur] `RPM = pulses * 30` suppose une fenêtre exactement égale à 1 s, alors que `server.handleClient()` peut retarder le calcul — impact : RPM surestimés proportionnellement au retard.
- [majeur] Aucun anti-rebond, filtre, bornage de fréquence ou timeout explicite — impact : parasites transformés en survitesse et panne détectée avec 0 à plus de 1 s de retard.
- [mineur] Le premier changement de consigne conserve l’avancement de l’ancienne période — impact : période transitoire non maîtrisée.

### Filtres / Signal
- [majeur] Aucun rejet des fronts rapprochés ni EMA/médiane — impact : une impulsion parasite augmente immédiatement le RPM de 30 sur chaque fenêtre.

### WiFi / Réseau
- [critique] Le mot de passe STA est réinjecté en clair dans la page HTML — impact : tout client réseau ouvrant `/` récupère le secret.
- [critique] Endpoints sans authentification/CSRF et AP fixe documenté — impact : modification ou redémarrage non autorisé.
- [mineur] Pas de reconnexion supervisée, mDNS, portail captif ou diagnostic des échecs réseau.

### Gestion mémoire / HEAP
- [critique] `strcpy(cfg.ssid, s.c_str())` et `strcpy(cfg.pass, p.c_str())` copient des entrées HTTP sans borne — impact : dépassement distant de la structure `cfg`, corruption mémoire ou crash.
- [majeur] La page PROGMEM est copiée dans une grande `String` puis modifiée, et le JSON est concaténé — impact : fragmentation du heap à long terme.

### Interface Web
- [majeur] La validation HTML est contournable et le backend n’impose aucune longueur maximale ni ratio maximal/fini — impact : crash ou configuration dangereuse via POST forgé.
- [majeur] Les erreurs de polling ne sont pas affichées — impact : valeurs anciennes présentées comme actuelles.

### Persistance / Stockage
- [majeur] Un magic seul ne détecte pas la corruption partielle ; pas de version, taille ou CRC — impact : valeurs aberrantes acceptées comme configuration.
- [majeur] Le retour de `EEPROM.commit()` est ignoré — impact : succès annoncé malgré une sauvegarde échouée.

### Documentation hardware/code
- [critique] La proposition d’utiliser un NPN base-collecteur court-circuités comme remplacement générique des diodes d’alimentation n’est ni dimensionnée ni sûre pour ce rail — impact : panne, échauffement ou tenue inverse inadéquate.
- [majeur] `1N4148` est proposée pour les pointes d’alimentation du Wemos, sans condensateur de réserve ni calcul thermique — impact : brownout ou diode hors domaine ; une Schottky 1 A est plus appropriée.
- [majeur] Pas de BOM complète, calcul du NPN de sortie, contrôle multimètre ou test fréquencemètre — impact : montage peu reproductible.

### Qualité de code générale (négatif)
- [critique] **Gestion d’erreurs / robustesse** : `#define EEPROM_MAGIC 0xDEYE0001` contient `Y`, interdit dans un littéral hexadécimal C++ — impact : **échec de compilation certain**.

```cpp
#define EEPROM_MAGIC 0xDEYE0001
```

- [majeur] **Duplication / DRY** : génération copiée pour les deux canaux — impact : correctifs susceptibles de diverger.
- [majeur] **Respect des conventions C++/Arduino** : macro invalide et `strcpy` non borné aux frontières HTTP — impact : build impossible puis mémoire non sûre après correction.
- [majeur] **Commentaires / documentation inline** : « zéro jitter » et « stabilité absolue » ne correspondent pas à un ordonnanceur relatif — impact : niveau de garantie trompeur.
- [critique] **Sécurité basique** : secret exposé, API ouverte et overflow distant — impact : compromission locale complète.
- [critique] **Testabilité** : aucun build reproductible, test ni banc timer — impact : l’erreur de syntaxe a atteint le livrable.

---

## ⭐ Note globale : 2,8/10 — Non compilable
L’électronique de principe et l’usage de Timer1 sont prometteurs, mais le firmware ne compile pas et reste vulnérable à des erreurs de fréquence, de mémoire et de démarrage.

## ⭐ Note qualité de code : 2,8/10 — Clair mais non livrable
La présentation est propre, cependant une constante invalide, un overflow distant et l’absence de validations/tests dominent la note.
