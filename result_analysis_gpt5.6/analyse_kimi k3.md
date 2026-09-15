# Analyse — kimi k3 (variante Kimi distincte)

**Sources analysées** : `competitors/kimi k3/deye_fan.ino`, `competitors/kimi k3/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Les ISR GPIO et Timer1 sont marquées `IRAM_ATTR`. L'ISR de sortie n'alloue rien, n'utilise ni `String`, ni flottants, ni serveur web, et commute les GPIO via `GPOS`/`GPOC`. Les échanges de demi-période entre `loop()` et l'ISR reposent sur des écritures 32 bits atomiques.

### Démarrage / Boot readiness

Les deux sorties sont configurées à `LOW` dès le début de `setup()`, avant EEPROM et WiFi : les NPN restent donc bloqués et les lignes Deye sont relâchées. Les entrées sont attachées avant le chargement réseau, puis Timer1 est démarré avant l'initialisation WiFi.

### Génération sortie / Timer1

Un unique Timer1 matériel en mode `TIM_SINGLE`, avec `TIM_DIV16`, ordonnance les deux canaux à la prochaine échéance. Les échéances sont absolues (`nextToggleUs += half`) plutôt que recalculées depuis l'instant tardif, ce qui limite la dérive cumulée. Le code gère correctement les deux GPIO dans une même ISR et borne le prochain réveil aux capacités du compteur 23 bits.

### Fidélité au protocole tachymétrique / Fail-safe

Les deux entrées et les deux sorties sont réellement indépendantes. La convention Noctua de 2 impulsions par tour est appliquée correctement :

```cpp
rpm = 60000000.0f / ((float)ema * PULSES_PER_REV);
float fout = rpm / 60.0f * PULSES_PER_REV * ch[i].ratio;
uint32_t half = (fout >= 0.5f) ? (uint32_t)(500000.0f / fout) : 0;
```

Après 1,5 seconde sans front, la sortie est arrêtée et le filtre remis à zéro. Une sortie inactive correspond bien à un GPIO bas, donc à un transistor NPN bloqué.

### Filtres / Signal

Les fronts espacés de moins de 300 µs sont rejetés sans rafraîchir `lastPulseUs`. Une EMA entière 3/4–1/4 lisse la période sans flottant dans l'ISR, puis elle est réinitialisée après perte du signal.

### WiFi / Réseau

Le mode AP+STA est non bloquant, `WiFi.persistent(false)` évite les écritures implicites du SDK, l'auto-reconnexion STA est activée et mDNS publie `deyefan.local`.

### Gestion mémoire / HEAP

La page web réside en `PROGMEM` et est envoyée avec `send_P`. Le JSON de statut utilise deux tableaux `char` bornés et `snprintf`, ce qui limite fortement les allocations par requête.

### Interface Web

L'interface sombre fournit un polling AJAX des RPM lus/simulés et des états réseau. Les ratios et identifiants STA sont modifiables par des routes POST distinctes.

### Persistance / Stockage

La structure EEPROM contient un magic et un CRC16. Les longueurs de chaînes sont bornées et leur terminaison NUL est forcée au chargement. Les ratios sont validés dans la plage 0,5–10.

### Documentation hardware/code

Le schéma sépare correctement un tach NF-A9 et un tach NF-A6, avec pull-up locale 3,3 V. Les sorties NPN utilisent le pull-up inconnu du Deye et acceptent donc 3,3/5/12 V. L'alimentation réalise un OR-ing Schottky des deux rails, suivi d'un vrai buck MP1584 réglé à 5 V pour la broche `5V` du Wemos. La documentation explique le courant de base et la nécessité de ne pas appliquer 12 V au Wemos.

### Qualité de code générale (positif)

- **Structure / organisation** : responsabilités bien séparées malgré un sketch unique.
- **Lisibilité / nommage** : constantes avec unités, structure `Channel` et fonctions explicites.
- **Commentaires / documentation inline** : invariants Timer1, polarités et formules sont généralement expliqués.
- **Gestion d'erreurs / robustesse** : bornes de ratios, CRC et timeout sont présents.
- **Duplication / DRY** : tableau de deux canaux et boucles communes évitent le copier-coller.
- **Respect des conventions C++/Arduino** : `constexpr`, types fixes, `volatile` et calculs wrap-safe sont bien employés.
- **Complexité / maintenabilité** : ordonnanceur court et symétrique, ajout de métriques facile.
- **Sécurité basique** : buffers réseau bornés, aucun secret renvoyé par l'API de statut.
- **Testabilité** : mathématiques assez localisées, mais encore couplées au matériel.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR

- [majeur] `micros()` et `timer1_write()` sont appelés transitivement depuis l'ISR sans preuve par map de liens qu'ils restent entièrement accessibles pendant toute opération flash sur la version de core choisie — impact : l'affirmation « aucun accès flash possible » n'est pas démontrée.
- [mineur] l'ISR parcourt toujours les deux canaux et utilise `micros()` plutôt que le compteur Timer1/CCOUNT — impact : latence supérieure à un ordonnanceur minimal, sans mesure réelle du jitter.
- [mineur] les valeurs `emaPeriodUs` et `lastPulseUs` sont lues séparément dans `loop()` sans snapshot — impact : une interruption entre les lectures peut associer temporairement une nouvelle période à un ancien timestamp.

### Démarrage / Boot readiness

- [majeur] aucun tach provisoire n'est généré avant plusieurs fronts réels — impact : le Deye peut observer un arrêt pendant son autodiagnostic au démarrage.
- [majeur] lorsque les canaux sont inactifs, Timer1 dort pendant 1 seconde. L'arrivée d'une mesure ne reprogramme pas immédiatement le timer depuis `loop()` — impact : le premier front simulé peut être retardé de presque 1 seconde après détection du ventilateur.

### Génération sortie / Timer1

- [majeur] `nextToggleUs` reste à zéro lors de l'activation et n'est initialisé qu'au prochain passage de l'ISR — impact : démarrage de phase tardif et dépendant du réveil idle.
- [mineur] le plancher de 30 µs limite implicitement la fréquence, mais l'interface continue d'afficher `rpmIn × ratio` sans signaler cette saturation — impact : télémétrie différente de la fréquence réellement générée dans les cas extrêmes.
- [mineur] si l'ISR est retardée de moins d'une demi-période, elle conserve la phase mais produit un intervalle court suivant ; aucune métrique ne quantifie cette correction — impact : jitter local invisible à l'utilisateur.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] le premier front calcule `dt = now - 0` et tout intervalle supérieur à 300 µs est accepté, sans maximum — impact : au premier démarrage, l'EMA peut représenter le temps écoulé depuis le boot plutôt que la période du ventilateur.
- [majeur] après un timeout, `emaPeriodUs` est remis à zéro mais `lastPulseUs` conserve l'ancien front ; le premier front de reprise réinjecte donc toute la durée d'arrêt dans l'EMA — impact : RPM très sous-estimé jusqu'aux fronts suivants et réactivation lente.
- [majeur] un seul intervalle suffit à activer le canal, sans validation de plusieurs périodes cohérentes — impact : une impulsion parasite isolée peut produire un tach simulé pendant 1,5 seconde.
- [majeur] le timeout fixe de 1,5 seconde est long par rapport aux périodes normales des Noctua — impact : une panne réelle peut rester masquée pendant des dizaines de tours attendus.

### Filtres / Signal

- [majeur] aucune borne maximale ni validation relative de période n'est appliquée — impact : longues pauses, fronts manquants et changements aberrants contaminent l'EMA.
- [majeur] `ema * 3u` peut déborder sur 32 bits lorsqu'une première période très longue est acceptée — impact : période filtrée arbitraire après un long uptime ou une reprise.
- [mineur] l'EMA est effectuée directement dans l'ISR d'entrée — impact : ISR plus longue que nécessaire ; une boîte de timestamps puis filtrage hors ISR serait plus auditable.

### WiFi / Réseau

- [majeur] AP et API n'ont aucune authentification applicative ni protection CSRF, avec un mot de passe AP identique dans tous les firmwares — impact : tout client connaissant le mot de passe publié peut modifier ratios et WiFi.
- [mineur] les échecs de `MDNS.begin()`, `EEPROM.commit()` et `WiFi.softAP()` sont ignorés — impact : l'interface peut annoncer un état sauvegardé ou disponible qui ne l'est pas.

### Interface Web

- [majeur] le polling réécrit les valeurs des champs ratio chaque seconde, même pendant leur édition — impact : saisie utilisateur écrasée ou difficile avant soumission.
- [mineur] une valeur invalide est simplement ignorée, puis le serveur redirige sans message d'erreur — impact : l'utilisateur peut croire que le réglage a été accepté.
- [mineur] les chaînes JSON ne disposent pas d'une fonction d'échappement générique — impact : faible aujourd'hui car seules des adresses IP et chaînes fixes sont renvoyées, mais extension risquée.

### Persistance / Stockage

- [majeur] la validation n'utilise pas `isfinite()` — impact : un NaN muni d'un CRC cohérent traverse les comparaisons de bornes et peut arrêter la génération.
- [mineur] chaque soumission écrit toute la structure sans vérifier si elle a changé et sans double-slot atomique — impact : usure flash inutile et configuration perdue en cas de coupure pendant `commit()`.
- [mineur] les valeurs par défaut après CRC invalide ne sont pas immédiatement réécrites — impact : le même défaut est redétecté à chaque démarrage jusqu'à une sauvegarde.

### Documentation hardware/code

- [majeur] aucun pull-down base-émetteur n'impose l'arrêt des NPN pendant le reset, lorsque les GPIO sont en haute impédance — impact : impulsions ou niveau bas parasite possibles avant `setup()`.
- [majeur] le buck MP1584 est seulement abaisseur ; si les sorties ventilateur sont commandées en tension et descendent sous environ 5,5–6 V, le 5 V Wemos n'est plus régulé — impact : brownout et redémarrage précisément à basse vitesse.
- [mineur] il manque fusibles/PTC, TVS et protection contre les transitoires de l'environnement onduleur — impact : robustesse électrique inférieure aux meilleurs schémas.
- [mineur] le schéma ASCII des entrées peut visuellement suggérer un nœud partagé, même si le texte et le firmware décrivent deux entrées séparées — impact : risque d'erreur de câblage.
- [mineur] annoncer que tous les NPN listés conviennent sans rappeler leurs brochages différents peut provoquer un montage inversé.

### Qualité de code générale (négatif)

- [majeur] l'état de capture ne distingue pas « aucun front », « premier front reçu » et « période validée » — impact : les bugs de première mesure et de reprise sont structurels.
- [majeur] aucun état explicite de readiness/fault ni compteur de fronts rejetés n'est disponible — impact : diagnostic terrain limité.
- [mineur] moteur temps réel, stockage, réseau et HTML restent dans un fichier de près de 400 lignes — impact : tests hôte et évolution moins simples qu'avec des modules séparés.
- [mineur] aucune configuration de build verrouillée, aucun test et aucune mesure oscilloscope ne vérifient les promesses de jitter.

---

## ⭐ Note globale : 7,6/10 — Très bonne base, reprise et première mesure à corriger

Le firmware est cohérent, probablement compilable et couvre correctement les deux canaux, Timer1, le réseau, la persistance et le hardware ; la latence de réveil et l'acceptation d'une fausse première période restent toutefois importantes pour le Deye.

## ⭐ Note qualité de code : 8,0/10 — Structure claire et moteur compact

Le code est lisible, factorisé et attentif au heap, mais gagnerait à formaliser l'état de capture, les snapshots ISR et les erreurs de persistance/réseau.
