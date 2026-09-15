# Analyse — kimi_k3_max (variante Kimi Max)

**Sources analysées** : `competitors/kimi_k3_max/deye_fan.ino`, `competitors/kimi_k3_max/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR

Les ISR GPIO et Timer1 sont en `IRAM_ATTR`. La capture se limite au compteur de cycles et à des écritures entières ; l’ISR de sortie utilise `GPOS`/`GPOC`, sans allocation, `String` ni flottant. Les mises à jour composées faites par `loop()` sont protégées par une section critique courte.

### Démarrage / Boot readiness

Les sorties sont forcées à LOW avant leur passage en OUTPUT, puis les entrées et Timer1 sont installés sans attente WiFi bloquante. Contrairement à la variante `kimi k3`, `setOutput()` réarme Timer1 dès l’activation d’un canal : la première sortie n’attend pas un réveil idle d’une seconde.

### Génération sortie / Timer1

Timer1 matériel fonctionne en one-shot et dessert les deux canaux selon l’échéance absolue la plus proche. Les fronts sont référencés à `ESP.getCycleCount()`, ce qui évite une dérive cumulative :

```cpp
c->nextEdgeCycle += half;
if ((int32_t)(now - c->nextEdgeCycle) >= 0)
  c->nextEdgeCycle = now + half;
```

La fréquence est bornée entre 1 Hz et 2 kHz et les accès GPIO sont directs.

### Fidélité au protocole tachymétrique / Fail-safe

La convention 2 impulsions/tour est correctement appliquée. Chaque voie possède mesure, filtre, ratio et sortie propres. Après 1,5 s sans front, la sortie est arrêtée et la ligne tach Deye relâchée, ce qui conserve le défaut réel au lieu d’inventer indéfiniment une vitesse.

### Filtres / Signal

Un anti-parasite de 400 µs précède une EMA 75/25 hors ISR. Le filtre est remis à zéro après stall, et les soustractions temporelles restent correctes au débordement 32 bits.

### WiFi / Réseau

Le mode AP+STA reste disponible sans blocage. `WIFI_NONE_SLEEP` réduit les latences radio, `WiFi.persistent(false)` évite les écritures SDK imprévues et une reconnexion STA non bloquante est tentée toutes les 15 s.

### Gestion mémoire / HEAP

La page réside en `PROGMEM`; le statut JSON est produit dans un buffer `char` borné avec `snprintf`. Cette méthode est plus sobre en heap que la construction de pages dynamiques de la variante GPT.

### Interface Web

L’interface sombre affiche RPM lus, RPM réellement émis, ratios, état et uptime. Le polling protège les champs ratio ayant le focus, amélioration absente de `kimi k3`.

### Persistance / Stockage

La configuration EEPROM est protégée par magic et CRC16. Les chaînes sont terminées explicitement, les ratios sont bornés et les NaN sont neutralisés par `clampRatio()`.

### Documentation hardware/code

La documentation fournit synoptique, BOM, câblage, calculs de courant et procédure de mise en service. Les entrées utilisent un pull-up 3,3 V ; les sorties NPN ont une résistance base-émetteur qui garantit leur blocage pendant le reset. L’OR-ing Schottky évite le retour de courant entre connecteurs.

### Qualité de code générale (positif)

- **Structure / organisation** : séparation claire entre capture, ordonnanceur, traitement, réseau et persistance.
- **Lisibilité / nommage** : unités et rôles sont explicites.
- **Commentaires / documentation inline** : invariants de phase, polarités et budgets temporels sont expliqués.
- **Gestion d’erreurs / robustesse** : CRC, bornes, timeout et reconnexion sont présents.
- **Duplication / DRY** : structures et fonctions communes aux deux canaux.
- **Respect des conventions C++/Arduino** : types fixes, `volatile`, calculs wrap-safe et accès atomiques adaptés.
- **Complexité / maintenabilité** : ordonnanceur compact malgré le partage d’un timer.
- **Sécurité basique** : buffers bornés et mots de passe non exposés par le statut.
- **Testabilité** : les calculs sont localisés, quoique toujours liés au matériel ESP8266.

---

## ❌ Points techniques négatifs

### Temps-réel / ISR

- [majeur] `ESP.getCycleCount()` et `timer1_write()` sont appelés depuis des fonctions IRAM sans verrouillage de version du core ni contrôle de map mémoire — impact : la sûreté pendant indisponibilité du cache flash n’est pas démontrée pour toutes les versions annoncées.
- [mineur] `scheduleNext()` est appelé dans l’ISR et dans une section globale `noInterrupts()` — impact : couplage délicat à auditer et risque de mauvaise réutilisation si l’état d’interruption initial n’est pas actif.

### Démarrage / Boot readiness

- [majeur] aucun signal nominal provisoire n’est produit avant deux fronts réels et le traitement périodique — impact : le Deye peut diagnostiquer un arrêt durant la montée en vitesse initiale.
- [majeur] l’alimentation dépend des rails ventilateur et le schéma n’offre aucun maintien auxiliaire — impact : aucun tach n’est possible si les deux rails sont coupés lors de l’autotest.

### Génération sortie / Timer1

- [mineur] la saturation à 2 kHz modifie le signal réel mais l’interface ne publie ni drapeau de saturation ni fréquence contrainte — impact : diagnostic trompeur pour une demande hors plage.
- [mineur] aucune mesure instrumentée du jitter ne confirme les chiffres annoncés — impact : la compatibilité sous trafic WiFi reste une hypothèse à valider à l’oscilloscope.

### Fidélité au protocole tachymétrique / Fail-safe

- [majeur] le premier intervalle est calculé depuis `lastEdgeCycle == 0` et peut être accepté sans état « premier front » — impact : une période liée au temps depuis boot peut lancer une sortie erronée.
- [majeur] aucune borne maximale de période n’est appliquée avant stockage — impact : un front manquant ou une longue reprise contamine l’EMA et sous-estime temporairement le RPM.
- [majeur] une seule période suffit à déclarer la voie active — impact : un parasite isolé peut maintenir un faux tach jusqu’à 1,5 s.
- [majeur] le timeout de 1,5 s masque un stall pendant de nombreux tours attendus — impact : détection de panne retardée.

### Filtres / Signal

- [majeur] l’anti-rebond seul ne valide ni plage plausible Noctua ni variation relative — impact : outliers longs et fronts manqués traversent le filtre.
- [mineur] l’EMA fixe ralentit les changements brusques sans indicateur de confiance — impact : sortie transitoirement en retard sur le ventilateur réel.

### WiFi / Réseau

- [majeur] l’API HTTP ne comporte ni authentification applicative ni protection CSRF — impact : tout client AP/LAN peut modifier ratios et WiFi.
- [majeur] les identifiants transitent en HTTP clair — impact : interception possible sur le réseau local.
- [mineur] les retours de `EEPROM.commit()`, `WiFi.softAP()` et `WiFi.begin()` ne sont pas exploités — impact : succès affiché malgré un échec réel.

### Interface Web

- [mineur] les entrées invalides sont ramenées silencieusement aux bornes au lieu d’obtenir HTTP 400 — impact : l’utilisateur ne sait pas que sa valeur a été modifiée.
- [mineur] aucun diagnostic ne distingue entrée débranchée, stall et période aberrante — impact : dépannage terrain limité.

### Persistance / Stockage

- [mineur] chaque POST écrit l’EEPROM même sans changement et sans double-slot — impact : usure évitable et retour complet aux défauts après coupure pendant commit.

### Documentation hardware/code

- [majeur] le MP1584 est un buck simple ; si le Deye module son rail sous environ 5,5–6 V, le Wemos peut brown-out — impact : reboot au moment où le ventilateur tourne lentement.
- [mineur] aucune TVS, PTC ni validation de différence de potentiel entre masses n’est prévue — impact : robustesse moindre aux transitoires et erreurs de câblage.
- [mineur] les valeurs de jitter sont calculées mais non mesurées — impact : absence de preuve de conformité réelle.

### Qualité de code générale (négatif)

- [majeur] l’état de capture ne distingue pas premier front, période confirmée et reprise — impact : activation possible sur une mesure non représentative.
- [mineur] un sketch de plus de 500 lignes mêle moteur temps-réel, HTML et réseau — impact : tests et revues plus coûteux.
- [mineur] aucune suite de tests, configuration de build figée ni compteur de fronts rejetés — impact : régressions temporelles difficiles à détecter.

---

## ⭐ Note globale : 8,1/10 — Excellente architecture temps-réel, validation d’entrée incomplète

Kimi Max corrige le réveil tardif de `kimi k3`, offre une sortie Timer1 propre et une documentation solide, mais sa première mesure, l’absence de borne haute et le boot sans tach provisoire limitent la sûreté.

## ⭐ Note qualité de code : 8,3/10 — Code rigoureux et factorisé

Le code est lisible, sobre en heap et mieux synchronisé que `kimi k3`; le monolithe, l’absence de tests et l’état de capture trop implicite restent ses principales faiblesses.
