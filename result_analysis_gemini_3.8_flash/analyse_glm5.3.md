# Analyse — glm5.3

**Sources analysées** : `competitors/glm5.3/deye_fan/deye_fan.ino`, `competitors/glm5.3/deye_fan/README.md`, `competitors/glm5.3/deye_fan/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR
- Utilisation du Timer1 matériel FRC1 en mode one-shot réarmé à la demande (`TIM_SINGLE`), avec programmation dynamique sur échéances absolues calées sur `ESP.getCycleCount()`.
- Sorties pilotées directement par les registres matériels instantanés `GPOS` et `GPOC`.
- Tout le code de traitement des interruptions est placé en mémoire d'instruction RAM via l'attribut `IRAM_ATTR`.
- Absence d'opérations en virgule flottante au sein des interruptions.

### Démarrage / Boot readiness
- Initialisation explicite des broches de sortie à l'état bas avant l'appel à `pinMode(..., OUTPUT)`.
- Démarrage du moteur Timer1 dans `setup()` avant la configuration réseau WiFi.

### Génération sortie / Timer1
- Programmation à l'échéance exacte avec plancher de sécurité de 50 cycles d'horloge pour éviter le dépassement de rechargement du registre matériel.

### Fidélité au protocole tachymétrique / Fail-safe
- Règle standard des 2 impulsions par tour appliquée.
- Timeout de sécurité déclenchant la coupure de la simulation en cas d'arrêt du ventilateur.
- Mode de repli paramétrable (injection d'un régime de secours en cas de perte de signal).

### Filtres / Signal
- Filtrage passe-bas exponentiel sur les fréquences d'entrée (`EMA_ALPHA = 0.25f`).
- Fenêtrage temporel sur la validité des impulsions éliminant les rebonds parasites.

### WiFi / Réseau
- Mode `WIFI_AP_STA` simultané avec désactivation de la veille modem (`WIFI_NONE_SLEEP`).
- Serveur mDNS intégré (`http://deye-fan.local`).
- Gestion de portail captif DNS pour faciliter la première connexion en mode Point d'Accès.

### Gestion mémoire / HEAP
- Zéro allocation dynamique sur le tas lors du traitement des requêtes HTTP : utilisation exclusive de tampons statiques pré-alloués et de `snprintf`.
- Interface web embarquée en mémoire Flash (`INDEX_HTML[] PROGMEM`).

### Interface Web
- Interface graphique moderne avec tableaux de bord, bargraphes de statut et mise à jour dynamique par requêtes asynchrones `/api/status`.
- Validation stricte des données soumises via l'API REST.

### Persistance / Stockage
- Sauvegarde en mémoire EEPROM protégée par un mot magique de 4 octets (`0x44455945`), un numéro de version et un calcul d'intégrité par CRC32 complet.

### Documentation hardware/code
- Schéma électronique particulièrement soigné dans `schema_electronique.md` : OU-diode avec Schottky SS14, diode TVS SMBJ14A pour la protection contre les surtensions, convertisseur buck MP1584EN et étage de sortie à transistor NPN BC337-25.

### Qualité de code générale (positif)
- **Structure / organisation** : Code structuré avec une grande rigueur typographique et architecturale.
- **Lisibilité / nommage** : Conventions de nommage claires et constantes bien répertoriées.
- **Commentaires / documentation inline** : Commentaires abondants et précis expliquant le fonctionnement de la machine d'état.
- **Gestion d'erreurs / robustesse** : Contrôle systématique des bornes et des valeurs numériques.

---

## ❌ Points techniques négatifs

### Conception matérielle & logique / Tachymétrie (critique)
- [critique] : Erreur conceptuelle majeure sur le raccordement en parallèle des signaux tachymétriques :
  Le code et le schéma introduisent un paramètre `parallelFans` (par défaut fixé à 2 ventilateurs câblés sur une même entrée tachymétrique) et postulent que relier deux sorties collecteur ouvert en parallèle "additionne simplement les impulsions" :
  ```cpp
  meas[ch].fanHz = (float)p / (float)S.parallelFans;
  ```
  En réalité, deux ventilateurs physiques distincts tournent à des vitesses légèrement différentes et avec une phase totalement asynchrone. Leurs impulsions vont périodiquement se chevaucher ou se décaler de façon chaotique, créant des fronts parasites impossibles à discriminer. Pire encore, en divisant mathématiquement la fréquence mesurée par `parallelFans` pour piloter la sortie :
  ```cpp
  float simHz = meas[ch].fanHz * S.ratio[ch];
  ```
  Le système applique un rapport erroné d'un facteur 2 sur la vitesse simulée renvoyée à l'onduleur Deye ! — impact : mesure d'entrée perturbée par des chevauchements de créneaux et faussage complet du régime moteur simulé envoyé à l'onduleur.

### Fidélité au protocole tachymétrique / Fail-safe (majeur)
- [majeur] : Le mode "cache-panne" introduit un comportement trompeur : si le ventilateur physique s'arrête brutalement (panne mécanique ou blocage), le système continue d'injecter une fréquence artificielle fixe vers l'onduleur pendant plusieurs secondes. L'onduleur Deye est ainsi induit en erreur et ne peut pas déclencher son alerte de sécurité thermique en temps opportun — impact : risque de surchauffe de l'onduleur sans que l'alarme ventilateur ne retentisse.

### Qualité de code générale (négatif)
- [mineur] : Présence de code expérimental conditionnel avec des options peu réalistes en production (modes de simulation aveugles) encombrant la clarté du flux d'exécution.

---

## ⭐ Note globale : 6.9/10 — Code d'une grande rigueur affaibli par une fausse hypothèse physique
L'architecture logicielle (FRC1 one-shot, CRC32, zéro allocation web) et la documentation électronique sont d'une qualité remarquable, mais le postulat erroné selon lequel deux ventilateurs en parallèle "additionnent" proprement leurs fréquences dégrade la note globale.

## ⭐ Note qualité de code : 7.6/10 — Très haute technicité d'écriture
Code C++ extrêmement propre, robuste et bien documenté, faisant preuve d'une excellente maîtrise des primitives embarquées.
