# Analyse — kimi k3

**Sources analysées** :
- `competitors/kimi k3/deye_fan.ino`
- `competitors/kimi k3/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Timer1 one-shot commun à deux sorties, GPIO directs et resynchronisation d'échéance.
- Deux PPR et calcul de demi-période corrects.

### Filtres / Stockage
- EMA d'entrée, timeout, CRC16 et AP+STA.
- Un seul tach par paire et schéma avec Schottky, buck et NPN.

### Qualité de code générale (positif)
- **Structure / organisation** : Code factorisé, constantes explicites, CRC et interface compréhensible.
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

### Démarrage / Reprise
- [critique] : La première ISR calcule `dt = now - 0` et l'accepte si elle tombe dans la large plage. — impact : faux RPM et sortie après un seul front.
- [critique] : Après stall, la première période très longue n'a pas de borne maximale suffisamment stricte et peut amorcer le filtre. — impact : sortie lente/aberrante puis convergence progressive.

### Temps-réel
- [majeur] : `timer1_attachInterrupt` n'est pas une preuve de NMI et l'ISR appelle `micros()`. — impact : jitter/flash safety non démontrés.
- [mineur] : Une nouvelle période est recalée sur `now` après retard. — impact : phase non conservée, même si la fréquence moyenne reste proche.

### Sécurité / Hardware
- [majeur] : Aucune authentification HTTP. — impact : paramètres modifiables depuis le réseau.
- [mineur] : Le schéma omet un pull-down base-émetteur sur les sorties. — impact : glitch possible pendant reset.

### Qualité de code générale (négatif)
- [majeur] : L'absence d'état d'amorçage/reprise et de tests dégrade fortement le comportement transitoire. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 5,9/10 — Base fonctionnelle, faux premier échantillon et timer non NMI

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 6,8/10 — Lisible mais trop confiant

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
