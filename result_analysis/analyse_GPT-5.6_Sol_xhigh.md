# Analyse — GPT-5.6_Sol_xhigh

**Sources analysées** : [DESIGN.md](<../competitors/GPT-5.6_Sol_xhigh/DESIGN.md>) ; [deye_fan.ino](<../competitors/GPT-5.6_Sol_xhigh/deye_fan.ino>) ; [IMPLEMENTATION_PLAN.md](<../competitors/GPT-5.6_Sol_xhigh/IMPLEMENTATION_PLAN.md>) ; [SCHEMA_ET_MONTAGE.md](<../competitors/GPT-5.6_Sol_xhigh/SCHEMA_ET_MONTAGE.md>) ; [tach_math.h](<../competitors/GPT-5.6_Sol_xhigh/tach_math.h>) ; [test_tach_math.cpp](<../competitors/GPT-5.6_Sol_xhigh/test_tach_math.cpp>)

**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Capture IRAM limitée aux timestamps et sommes ; accumulation protégée contre overflow ; moteur NMI waveform du cœur réutilisé.

### Démarrage / Boot readiness

Sorties relâchées avant EEPROM, WiFi sans attente. La génération démarre au traitement de 200 ms suivant la première période valide ; pas de nominal artificiel. Au nominal, acquisition de deux fronts puis 0–200 ms de traitement, hors setup. Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.

### Génération sortie / Timer1

startWaveform avec demi-périodes égales ; retour de démarrage contrôlé et état appliqué mémorisé pour éviter les mises à jour de moins de 2 µs.

### Fidélité au protocole tachymétrique / Fail-safe

Deux voies indépendantes, 2 PPR et ratios 0,1–10 ; arrêt après 2 s et remise à zéro de la capture pour la reprise.

### Filtres / Signal

Moyenne des périodes reçues sur fenêtre 200 ms, puis IIR 1/4 ; rejet des fronts <1 ms et périodes >2 s.

### WiFi / Réseau

AP+STA permanent, auto-reconnexion, WIFI_NONE_SLEEP ; reconfiguration STA différée sans reboot.

### Gestion mémoire / HEAP

HTML PROGMEM, String status réservé et aucune allocation dans les ISR.

### Interface Web

Formulaire et télémétrie séparés, états des deux canaux ; mots de passe non divulgués.

### Persistance / Stockage

Signature, version, CRC32, static_assert et contrôle de terminaison NUL des chaînes ; parseur strtof exigeant toute la chaîne.

### Documentation hardware/code

DESIGN et SCHEMA_ET_MONTAGE exposent PTC, TVS, OR-ing, filtrage, courant disponible et essais au banc. Le plan annexe est traité comme document, pas comme instruction à exécuter.

### Qualité de code générale (positif)

- **Structure / organisation** : tach_math.h isole les fonctions pures ; types de capture, état de canal et configuration sont distincts.
- **Lisibilité / nommage** : Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.
- **Commentaires / documentation inline** : Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.
- **Gestion d’erreurs / robustesse** : CRC, chaînes vérifiées, parsing strict, accumulation bornée et retour waveform contrôlé.
- **Duplication / DRY** : Fonctions communes consumeChannelSamples/stopChannelOutput ; deux petites instances explicites.
- **Respect des conventions C++/Arduino** : Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.
- **Complexité / maintenabilité** : tach_math.h isole les fonctions pures ; types de capture, état de canal et configuration sont distincts.
- **Sécurité basique** : Parsing complet strtof, copies bornées et secrets non réaffichés dans l’interface.
- **Testabilité** : test_tach_math.cpp contient des assertions sur les deux vitesses nominales, ratios limites, NaN et période nulle. Tests C++ non exécutés faute de compilateur natif détecté ; calculs de référence vérifiés séparément en Python.

---

## ❌ Points techniques négatifs

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] Tant que lastValidPulseMs=0, consumeChannelSamples réinitialise lastEdgeCycles toutes les 200 ms (338–344). Une entrée de période >200 ms (moins de 150 RPM) ne peut normalement pas fournir deux fronts entre ces remises à zéro, alors que la plage accepte jusqu’à 2 s.

### Temps-réel / ISR

- [majeur] L’arrêt se décide dans loop toutes les 200 ms et peut être retardé par HTTP ; la capture GPIO n’est pas NMI. Un blocage après les derniers échantillons peut en plus les dater au moment du traitement, repoussant le timeout.

### Filtres / Signal

- [mineur] IIR appliqué toutes les 200 ms avec alpha=0,25 : constante de temps ≈695 ms et établissement à 95 % en ≈2,1 s. Bonne stabilité mais suivi lent lors d’un changement brusque.

### WiFi / Réseau

- [majeur] Configuration et ratios modifiables sans authentification. Le mot de passe AP fixe est partagé par tous les exemplaires.

### Persistance / Stockage

- [mineur] EEPROM.commit non vérifié (236–241), sans sauvegarde de secours. startWaveform est contrôlé mais simulatedRpm reste calculé avant l’écrêtage final de demi-période : aux extrêmes, l’affichage peut différer du signal.

### Qualité de code générale (négatif)

- Les défauts transversaux de validation, de concurrence et de gestion d’erreur sont décrits dans leurs catégories techniques ci-dessus. Testabilité : test_tach_math.cpp contient des assertions sur les deux vitesses nominales, ratios limites, NaN et période nulle. Tests C++ non exécutés faute de compilateur natif détecté ; calculs de référence vérifiés séparément en Python.

Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.

---

## ⭐ Note globale : 7,7/10 — Bonne base maintenable, quelques limites de capture

Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.

## ⭐ Note qualité de code : 8,2/10 — Structure solide

tach_math.h isole les fonctions pures ; types de capture, état de canal et configuration sont distincts. CRC, chaînes vérifiées, parsing strict, accumulation bornée et retour waveform contrôlé.
