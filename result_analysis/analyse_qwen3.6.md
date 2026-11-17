# Analyse — qwen3.6

**Sources analysées** : [deye_fan.ino](<../competitors/qwen3.6/deye_fan.ino>) ; [SCHÉMA_CIRCUIT.md](<../competitors/qwen3.6/SCHÉMA_CIRCUIT.md>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Les fonctions de capture portent IRAM_ATTR ; l’intention de séparer capture et sortie est visible.

### Démarrage / Boot readiness

Sorties forcées hautes, attente WiFi jusqu’à 15 s + pause 500 ms avant capture/Timer (471–505/874–881). En l’état, aucun signal valide car les API timer ne ciblent pas l’ESP8266. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### WiFi / Réseau

AP+STA, identifiants configurables et API de lecture.

### Interface Web

Page de réglage et affichage des RPM ; paramètres enregistrables.

### Persistance / Stockage

Emplacements EEPROM explicites et bornes des ratios au chargement.

### Documentation hardware/code

Dossier détaillé fourni, avec schémas d’étages NPN, mais erreurs d’alimentation et de raccordement.

### Qualité de code générale (positif)

- **Structure / organisation** : Sections nommées mais nombreuses variantes d’écriture EEPROM et duplication des deux canaux.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : Quelques bornes de ratios, pas de vérification cohérente des API, temporisations ou circuits.
- **Duplication / DRY** : Duplication importante de capture, scheduling, persistance et construction HTML.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : Sections nommées mais nombreuses variantes d’écriture EEPROM et duplication des deux canaux.
- **Sécurité basique** : — aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.
- **Testabilité** : Aucun test fourni ; les calculs sont mêlés au firmware.

---

## ❌ Points techniques négatifs

### Génération sortie / Timer1

- [critique] La période simulée est multipliée par le ratio (320/344), puis utilisée comme demi-période sans division par deux (392/418). Même avec un timer corrigé, le signal aurait une fréquence ≈entrée/(2×ratio), à l’opposé de l’affichage RPM×ratio.
- [majeur] Quand un canal inactif atteint son échéance, nextTick est mis à UINT32_MAX (378/407) ; un retour de signal ne réarme pas cette échéance. Comparaisons non résistantes au wrap.

### Documentation hardware/code

- [critique] SCHÉMA_CIRCUIT.md (47–77, 315–341) relie le rail OR ≈12 V au Wemos en supposant un AMS1117 et réutilise le pull-up Deye pour le tach d’entrée. Il faut un vrai abaisseur et séparer la mesure Noctua du tach Deye simulé ; 12 V sur 5V peut détruire la carte.

### Démarrage / Boot readiness

- [majeur] WiFi bloquant jusqu’à 15 s avant capture ; un GPIO haut maintient le NPN de sortie conducteur. Aucun signal tach pendant cette phase, donc risque d’alarme au démarrage.

### Temps-réel / ISR

- [majeur] Entrée GPIO15 sans pull-up garanti et broche de boot devant rester basse ; le code ne respecte pas clairement le circuit d’entrée. Les primitives prétendument isolées ne sont pas justifiées.

### Qualité de code générale (négatif)

- [critique] hw_timer_t, timerBegin, timerRead, timerWrite et timerAttachInterrupt (160/368/832–839) sont des API de timer ESP32, non disponibles dans le cœur ESP8266 visé. Le sketch ne compile pas tel quel.
- [mineur] Fonctions EEPROM dupliquées, routes de mutation en GET, interface entière rechargée toutes les 2 s et mots de passe transmis dans l’URL.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 0,9/10 — Mauvaise API de timer et schéma dangereux

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 2,8/10 — Qualité insuffisante

Sections nommées mais nombreuses variantes d’écriture EEPROM et duplication des deux canaux. Quelques bornes de ratios, pas de vérification cohérente des API, temporisations ou circuits.
