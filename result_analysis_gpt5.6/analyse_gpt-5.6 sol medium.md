# Analyse — gpt-5.6 sol medium

**Sources analysées** :
- `competitors/gpt-5.6 sol medium/deye_fan.ino`
- `competitors/gpt-5.6 sol medium/SCHEMA_ET_INSTALLATION.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Timer1 one-shot partagé entre deux sorties, échéances en cycles absolus et accès GPIO direct.
- Boîte aux lettres 32 bits et publication seqlock NMI/loop correctement raisonnées.

### Mesure / Fail-safe
- Premier front seulement comme référence, périodes bornées, glitches qui ne rafraîchissent pas `lastGood` et reprise sur deux fronts.
- Deux PPR, IIR entier et relâchement complet au stall à 350 ms.

### WiFi / Persistance / Interface
- AP+STA, WIFI_NONE_SLEEP, validation stricte, CRC32 avec `offsetof`, mots de passe non relus et JSON borné.
- Moteur tach lancé avant la radio.

### Hardware
- Buck-boost/UVLO, Schottky, PTC, TVS et étages NPN correctement dimensionnés; limites physiques explicitement reconnues.

### Qualité de code générale (positif)
- **Structure / organisation** : État par canal compact, fonctions pures isolables, parsing prudent, sérialisation bornée et commentaires utiles.
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

### Temps-réel / Portabilité
- [majeur] : `timer1_attachInterrupt` utilise une interruption masquable plutôt qu'une NMI explicite. — impact : jitter sous sections critiques WiFi/flash possible.
- [mineur] : Les garanties transitives IRAM ne sont pas vérifiées par linker map. — impact : comportement pendant flash à confirmer.

### Démarrage / Fonctionnel
- [majeur] : Aucune consigne provisoire avant deux fronts valides. — impact : fenêtre de 0 RPM au boot.
- [majeur] : Un seul ventilateur est surveillé par paire. — impact : panne du second rotor invisible.

### Sécurité / Tests
- [majeur] : Aucune authentification ni CSRF. — impact : ratios et WiFi modifiables depuis le LAN/AP.
- [majeur] : Aucun test automatisé ou build reproductible. — impact : les transitions boot/stall/wrap ne sont pas protégées contre les régressions.

### Qualité de code générale (négatif)
- [majeur] : Le moteur bas niveau reste dans le sketch et n'est pas couvert par des tests; authentification absente. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 7,8/10 — Solution solide et prudente, sans NMI ni tests

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 8,3/10 — Propre, défensive et maintenable

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
