# Analyse — glm5.3

**Sources analysées** :
- `competitors/glm5.3/deye_fan/deye_fan.ino`
- `competitors/glm5.3/deye_fan/README.md`
- `competitors/glm5.3/deye_fan/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Capture dans un anneau de 128 timestamps, mesure multi-fronts, EMA et ordonnanceur à échéances absolues.
- Deux sorties, boîte aux lettres loop→ISR et resynchronisation sans rafale.

### WiFi / Persistance / UI
- Portail captif, mDNS, scan asynchrone, AP+STA et interface complète.
- CRC32 via `offsetof`, validation et JSON échappé.

### Hardware
- OR-ing, buck, TVS, pull-ups 3,3 V et sorties BC337 sont correctement décrits pris isolément.

### Qualité de code générale (positif)
- **Structure / organisation** : Très bon découpage, parsing borné, CRC correct et documentation structurée.
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

### Compilation
- [critique] : `0xDEYEF001UL` contient la lettre Y. — impact : le sketch ne compile pas.

### Mesure / Protocole
- [critique] : Les deux sorties tach open-collector de chaque paire sont mises en parallèle puis la fréquence est divisée par le nombre de ventilateurs. — impact : les fronts se chevauchent et ne s'additionnent pas de façon déterministe; RPM faux.
- [majeur] : Le code et la documentation traitent cette division comme une correction d'affichage alors qu'elle influence la consigne. — impact : comportement conceptuellement incohérent.

### Temps-réel / Sécurité
- [majeur] : Le mode `timer1_attachInterrupt` n'établit pas la priorité NMI revendiquée. — impact : isolation WiFi non démontrée.
- [majeur] : Le mode cache-panne peut émettre un RPM fixe après perte réelle. — impact : panne de ventilation masquée.
- [majeur] : Aucune authentification des routes de reset/reconfiguration. — impact : contrôle distant non protégé.

### Qualité de code générale (négatif)
- [majeur] : L'absence d'une compilation minimale et la fausse hypothèse d'addition des tach annulent l'ambition du design. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 2,8/10 — Très ambitieux, bloqué par compilation et conception tach parallèle

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 6,5/10 — Architecture soignée mais non validée

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
