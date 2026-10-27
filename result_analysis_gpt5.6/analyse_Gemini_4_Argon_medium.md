# Analyse — Gemini_4_Argon_medium

**Sources analysées** :
- `competitors/Gemini_4_Argon_medium/deye_fan.ino`
- `competitors/Gemini_4_Argon_medium/SCHEMA_ET_ARCHITECTURE.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Timer1 NMI one-shot, CCOUNT, Q16, GPIO directs et watchdog NMI à 500 ms.
- Deux canaux indépendants, buffers fixes, HTML PROGMEM et statut JSON borné.

### Protocole / Filtres
- Deux PPR, périodes plausibles et moyenne multi-échantillons.
- La sortie est relâchée par la NMI sans dépendre de loop.

### WiFi / Persistance
- AP+STA, WIFI_NONE_SLEEP, CRC32, validation et interface riche.

### Documentation
- BOM, calculs et architecture logicielle très détaillés.

### Qualité de code générale (positif)
- **Structure / organisation** : Structures par canal, buffers bornés, CRC et séparation NMI/application sont de haut niveau.
- **Lisibilité / nommage** : les unités et rôles principaux sont identifiables dans les constantes et structures.
- **Commentaires / documentation inline** : les choix d'architecture sont explicités, avec une qualité proportionnelle au score du rapport.
- **Gestion d'erreurs / robustesse** : les protections réellement présentes sont détaillées dans les sections techniques ci-dessus.
- **Duplication / DRY** : les deux canaux sont factorisés lorsqu'une structure commune existe; les exceptions sont signalées dans les défauts.
- **Respect des conventions C++/Arduino** : l'évaluation tient compte des types fixes, de `volatile`, d'IRAM et des API de la plateforme.
- **Complexité / maintenabilité** : la séparation des responsabilités a été évaluée indépendamment de la correction fonctionnelle.
- **Sécurité basique** : les copies bornées, échappements, secrets et contrôles HTTP ont été vérifiés.
- **Testabilité** : la présence réelle de tests/builds a été distinguée des simples affirmations documentaires.

---

## ❌ Points techniques négatifs

### Fail-safe / Reprise
- [critique] : Au timeout, la NMI ne réinitialise ni `targetHalfCycles` ni `hasSignal`; elle ne met que `active` à zéro. — impact : l'ancienne consigne reste armée.
- [critique] : Le premier front de reprise rafraîchit `lastEdgeCycle`, remet `hasSignal=1` et retourne sans invalider la consigne. — impact : la NMI redémarre immédiatement l'ancienne fréquence avant une nouvelle période; la reprise n'exige pas deux fronts.

### Temps-réel / Portabilité
- [majeur] : Une multiplication 64 bits dans l'ISR GPIO peut appeler un helper libgcc hors IRAM. — impact : crash possible pendant cache flash indisponible.
- [majeur] : La détection CPU/APB lit le registre non documenté `0x3FF00014`. — impact : base de temps fragile selon révision/core.

### Hardware
- [critique] : La résistance série de 33 Ω ne peut pas alimenter les pics WiFi depuis une rail à 7 V. — impact : chute de plusieurs volts, brownout et redémarrages.
- [critique] : L'OR/prérégulateur expose une jonction VEB inverse proche de sa limite absolue et régule sur VBE. — impact : vieillissement, claquage et tension 5 V non garantie.

### Sécurité
- [majeur] : Aucune authentification HTTP/CSRF. — impact : ratios et réseau modifiables par tout client.

### Qualité de code générale (négatif)
- [majeur] : La machine d'état de reprise oublie d'invalider l'ancienne consigne; monolithe et API privées sans tests. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 5,7/10 — Moteur sophistiqué, mais reprise stale et alimentation critiques

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 7,8/10 — Très structuré, avec un défaut d'état déterminant

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
