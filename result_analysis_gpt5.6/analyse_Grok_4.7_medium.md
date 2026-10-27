# Analyse — Grok_4.7_medium

**Sources analysées** :
- `competitors/Grok_4.7_medium/deye_fan/deye_fan.ino`
- `competitors/Grok_4.7_medium/schema_electronique.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- NMI one-shot à 32 µs pour échantillonner les entrées et planifier deux sorties par CCOUNT.
- GPIO directs, division 32 bits seulement, seqlock NMI→loop et resynchronisation sans rafale.

### Fail-safe / Reprise
- Les glitches n'actualisent pas `lastGood`; le stall efface filtre, amorçage et sortie.
- Deux vrais fronts sont requis pour redémarrer après une perte.

### Persistance / Interface
- CRC32, `offsetof`, buffers JSON fixes, AP+STA et validation des ratios.

### Hardware partiel
- Entrées isolées et sorties 2N2222 open-collector correctement dimensionnées.

### Qualité de code générale (positif)
- **Structure / organisation** : Machine d'état de capture correcte, commentaires précis, arithmétique IRAM prudente et snapshots cohérents.
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

### Hardware / Alimentation
- [critique] : L'OR-ing relie le +12 V au collecteur d'un SS8050 et VRAW à sa base, émetteur ouvert. — impact : la jonction collecteur-base est polarisée en inverse dans le sens proposé; aucun courant normal n'alimente VRAW.
- [critique] : Le régulateur discret VBE/ballast dépend fortement de température et charge. — impact : 5 V non garanti et dissipation TO-92 élevée lors des pics WiFi.

### Temps-réel / Portabilité
- [majeur] : La NMI se réveille au moins 31 250 fois/s même sans signal. — impact : charge CPU permanente notable.
- [majeur] : Timer1 NMI et appel bas niveau dépendent du core sans build/map livré. — impact : portabilité et IRAM non établies.

### Démarrage / Sécurité
- [majeur] : Aucun tach de garde avant deux fronts. — impact : 0 RPM au boot.
- [majeur] : Interface sans authentification. — impact : modification distante des paramètres.

### Qualité de code générale (négatif)
- [majeur] : Le excellent firmware ne compense pas un circuit d'alimentation dont l'OR-ing ne conduit pas. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 4,4/10 — Excellent firmware, alimentation non fonctionnelle

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 8,2/10 — Moteur temps réel parmi les meilleurs

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
