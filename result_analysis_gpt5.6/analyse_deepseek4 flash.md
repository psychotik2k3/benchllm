# Analyse — deepseek4 flash

**Sources analysées** :
- `competitors/deepseek4 flash/deye_fan.ino`
- `competitors/deepseek4 flash/README.md`
- `competitors/deepseek4 flash/SCHEMA_ELECTRONIQUE.md`

**Méthode** : réanalyse statique intégrale depuis les sources actuelles du compétiteur, sans réutiliser les conclusions des rapports précédents. Aucune compilation ESP8266 ni mesure oscilloscope n'a été revendiquée lorsqu'elle n'a pas été effectivement reproduite.

---

## ✅ Points techniques positifs

### Temps-réel / Génération
- Event-calendar FRC1/NMI à deux voies, GPIO directs et ratio Q16 évitent les calculs flottants dans le moteur.
- Échéances relatives bornées et sorties relâchées au stall.

### Protocole / Filtres
- Deux PPR, EMA et timeout adaptatif max(2 s, 4 périodes) sont cohérents en intention.

### WiFi / Mémoire
- AP+STA, mDNS, HTML PROGMEM et buffers statiques pour le statut.

### Documentation hardware/code
- Schéma détaillé avec buck, Schottky, clamp d'entrée et collecteurs ouverts.

### Qualité de code générale (positif)
- **Structure / organisation** : Bonne séparation des blocs, noms précis et explications des formules.
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

### Démarrage / Mesure
- [critique] : Le premier front valide une période depuis `g_inLastUs == 0` car la borne maximale vaut 120 s. — impact : sortie fausse au premier front.
- [majeur] : Les fronts invalides mettent malgré tout à jour le timestamp et le compteur. — impact : bruit susceptible de prolonger la validité d'une période ancienne.

### Persistance
- [critique] : La CRC est calculée sur `sizeof(CfgT)-1`, qui inclut un octet du champ CRC à cause de l'alignement. — impact : la configuration sauvegardée échoue généralement au prochain chargement.

### Temps-réel / Portabilité
- [majeur] : Le code programme directement FRC1/NMI et suppose des bits/adresses sans preuve de version. — impact : comportement ou compilation non garantis.
- [majeur] : Des sections `noInterrupts()` sont présentées comme protection face à une NMI. — impact : snapshots multi-champs potentiellement incohérents.

### Sécurité
- [majeur] : Les mots de passe sont renvoyés dans l'API d'état et le JSON n'est pas échappé. — impact : fuite de secrets et JSON cassable.

### Qualité de code générale (négatif)
- [majeur] : Le code bas niveau n'est accompagné ni d'un build, ni d'une map IRAM, ni de tests; plusieurs garanties absolues ne sont donc pas établies. — impact : coût de validation, de maintenance ou de portage accru.

## ⭐ Note globale : 4,8/10 — Architecture ambitieuse, persistence et bas niveau fragiles

La note globale mesure la viabilité de l'ensemble firmware + électronique + comportement de panne, pas seulement la sophistication apparente.

## ⭐ Note qualité de code : 6,5/10 — Bien commenté mais trop affirmatif

Cette note isole structure, lisibilité, robustesse et testabilité; elle peut donc rester élevée même lorsqu'un défaut matériel réduit fortement la note globale.
