# Analyse — Gemini_4_Argon_medium

**Sources analysées** (liste exhaustive du dossier) :

- `competitors/Gemini_4_Argon_medium/deye_fan.ino`
- `competitors/Gemini_4_Argon_medium/SCHEMA_ET_ARCHITECTURE.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les ISR GPIO, la NMI Timer1 et les fonctions appelées sur leur chemin portent `IRAM_ATTR`. La capture utilise `CCOUNT`, `GPI`, `GPOS` et `GPOC`, sans `digitalRead()`, `digitalWrite()`, allocation, réseau ou flottant dans la NMI.
- Le calcul de consigne emploie un ratio inverse Q16.16 et des mots 32 bits alignés. La soustraction non signée des cycles gère correctement le wrap de `CCOUNT` tant que les intervalles restent inférieurs à 2³¹ cycles, ce qui est vrai avec les bornes présentes.
- Les deux canaux ont des états séparés et la NMI choisit leur prochaine échéance commune. Le trafic HTTP ne participe donc pas directement à la génération des fronts.

```cpp
uint32_t newHalfCycles = computeHalfPeriodCyclesIRAM(avgPeriodCycles, c.invRatioQ16);
c.targetHalfCycles     = newHalfCycles;

if (T1L > 800UL) {
  T1L = 400UL;
}
```

### Démarrage / Boot readiness

- Les sorties sont configurées LOW dès le début de `setup()`, puis le moteur tach est initialisé avant le WiFi et le serveur. Les résistances base-émetteur de 47 kΩ documentées maintiennent aussi les NPN bloqués durant le reset.
- Au premier démarrage, un front sert uniquement de référence ; une période complète est requise avant de produire une consigne. À 3 000 RPM, hors temps de spin-up, la première période est disponible en environ 10 ms ; à 1 600 RPM, en environ 18,75 ms.
- L’initialisation WiFi n’attend jamais une association STA, ce qui évite plusieurs secondes de silence supplémentaires.

### Génération sortie / Timer1

- L’ESP8266 Timer1/FRC1 est bien utilisé en `TIM_SINGLE` via le vecteur NMI, et non via `Ticker` ou le polling de `loop()`. La signature `ETS_FRC_TIMER1_NMI_INTR_ATTACH(func)` et la séquence `timer1_enable(TIM_DIV1, TIM_EDGE, TIM_SINGLE)` correspondent aux API internes du core ESP8266 actuel.
- Les échéances suivent `nextToggleCycle += halfCycle`, ce qui évite la dérive cumulative normale. Un retard trop important est recalé sur `now + halfCycle` au lieu de produire une rafale de fronts.
- Les GPIO de sortie 4 et 5 sont compatibles avec le boot ESP8266 et sont commutés atomiquement par registres.
- Le plancher de 500 µs par demi-période borne la sortie à 1 kHz, soit 30 000 RPM à 2 PPR. L’interface calcule la fréquence depuis la consigne effectivement bornée.

### Fidélité au protocole tachymétrique / Fail-safe

- La formule est correcte pour une période mesurée entre deux fronts descendants : `Tsortie = Tentrée / ratio`, donc `demi-période = Tentrée / (2 × ratio)`.
- Les affichages appliquent explicitement `RPM = fréquence × 60 / 2`. Les ratios par défaut ×2,33 et ×3,50 produisent bien environ 7 000 et 5 600 RPM depuis les vitesses nominales Noctua.
- En fonctionnement continu normal, l’absence de front pendant 500 ms relâche le NPN de sortie. Ce délai est plus réactif que les 1,2 à 2 s de plusieurs concurrents.
- Un seul tach de chaque paire est raccordé, ce qui évite de mettre directement en parallèle deux collecteurs ouverts asynchrones.

### Filtres / Signal

- Les fronts espacés de moins de 2,5 ms sont rejetés sans rafraîchir `lastEdgeCycle` en régime normal ; les glitches rapides ne repoussent donc pas continuellement le timeout.
- La moyenne circulaire sur quatre périodes atténue le jitter et couvre deux tours à 2 PPR. Son amorçage utilise une, puis deux périodes, sans lire de cases non initialisées.
- L’entrée matérielle emploie une pull-up externe vers 3,3 V et une résistance série ; aucune tension tach 12 V n’est injectée sur le GPIO.

### WiFi / Réseau

- Le mode AP+STA est non bloquant, `WiFi.persistent(false)` évite les écritures SDK implicites et `WIFI_NONE_SLEEP` réduit la variabilité de latence.
- La reconnexion STA est surveillée toutes les 15 s sans boucle d’attente. La reconfiguration WiFi est différée après la réponse HTTP.
- Le mot de passe STA n’est pas renvoyé par l’API de statut.

### Gestion mémoire / HEAP

- La SPA réside en `PROGMEM` et est envoyée par `send_P()`. Le JSON de statut utilise un buffer pile de 768 octets et `snprintf`, plutôt qu’une longue concaténation dynamique.
- Le chemin temps-réel n’emploie aucune allocation. Les `String` restent cantonnés aux paramètres HTTP.

### Interface Web

- L’interface sombre affiche les fréquences et RPM d’entrée/sortie, l’état des deux voies, les ratios, les IP, le RSSI et l’uptime.
- Le DOM est alimenté avec `textContent`. Les ratios ont des bornes navigateur et serveur, et les mots de passe vides conservent les secrets existants.
- Les routes sont simples et cohérentes : statut en GET, ratios/WiFi/reset en POST.

### Persistance / Stockage

- La structure EEPROM possède magic, version et CRC32. Les ratios sont revalidés, y compris contre `NaN`, et les chaînes sont terminées explicitement après lecture.
- `WiFi.persistent(false)` sépare la configuration applicative contrôlée des écritures automatiques du SDK.

### Documentation hardware/code

- Le document donne brochage, BOM, courants, polarités, calculs 2 PPR et schémas d’entrée/sortie. Le collecteur ouvert NPN de sortie est adapté à une pull-up Deye de 3,3, 5 ou 12 V.
- Le texte interdit correctement la mise en parallèle des deux fils tach d’une paire et précise d’isoler celui qui n’est pas mesuré.
- Les résistances de base 4,7 kΩ fournissent environ 0,55 mA et les pull-down de 47 kΩ évitent une conduction indéterminée au boot.

### Qualité de code générale (positif)

- **Structure / organisation** : sections nettes pour temps-réel, persistance, réseau, Web et configuration.
- **Lisibilité / nommage** : broches, unités temporelles, masques et rôles des structures sont explicites.
- **Commentaires / documentation inline** : formules, polarité NPN et intentions de concurrence sont abondamment expliquées.
- **Gestion d’erreurs / robustesse** : CRC, bornes, timeout, terminaison NUL et sorties au repos sont présents.
- **Duplication / DRY** : structures et calculs sont factorisés pour les deux canaux ; seules les petites ISR wrappers sont dupliquées.
- **Respect des conventions C++/Arduino** : `constexpr`, types fixes, `volatile`, `PROGMEM` et `snprintf` sont employés de façon cohérente.
- **Complexité / maintenabilité** : les paramètres matériels et temporels sont centralisés.
- **Sécurité basique** : copies bornées et secrets non renvoyés par le statut.
- **Testabilité** : CRC, conversion et validation sont isolés en fonctions, même si aucun test n’est fourni.

---

## ❌ Points techniques négatifs

### Exactitude de compilation / API ESP8266

- **[mineur]** La syntaxe et les signatures bas niveau correspondent au core ESP8266 actuel, mais aucun `platformio.ini`, FQBN, numéro de core ou CI ne rend le build reproductible. `arduino-cli` et PlatformIO n’étaient pas installés localement, donc aucune compilation binaire n’a pu être exécutée. Impact : une version de core masquant `ETS_FRC_TIMER1_NMI_INTR_ATTACH`, `T1L`, `GPI`, `GPOS` ou `GPOC` peut casser compilation ou comportement.
- **[majeur]** `computeHalfPeriodCyclesIRAM()` effectue une multiplication 64 bits depuis l’ISR GPIO. L’annotation IRAM de la fonction ne prouve pas que le helper de toolchain éventuellement généré pour cette opération réside en ROM/IRAM. Impact : pendant `EEPROM.commit()` ou une opération flash avec cache indisponible, un appel transitif placé en flash peut provoquer un crash ; seule une map de liens du build ciblé permet de lever ce risque.

### Temps-réel / ISR

- **[majeur]** Le « single-writer » invoqué ne garantit pas un snapshot cohérent : la NMI peut préempter l’ISR GPIO entre les écritures de `lastEdgeCycle`, `measuredPeriodCycles` et `targetHalfCycles`. Impact : un réveil peut momentanément combiner fraîcheur nouvelle et période ancienne ; ce défaut devient fonctionnellement grave lors de la reprise après stall.
- **[majeur]** La promesse d’une conversion « moins de 15 cycles » est incompatible avec les accès `volatile`, branches, buffer circulaire, somme et multiplication 64 bits exécutés sur le chemin complet. Impact : budget de pire cas et jitter annoncés non démontrés, sans capture oscilloscope ni mesure de durée ISR.
- **[mineur]** La NMI se réveille au moins toutes les 5 ms même sans signal, soit 200 interruptions/s à vide. Impact : coût faible mais permanent ; l’assertion d’« isolation totale » reste trop absolue sur un SoC monocœur.
- **[mineur]** L’ISR GPIO lit puis réécrit directement `T1L` pour accélérer le réveil, tandis que la NMI possède aussi ce registre. Impact : couplage fragile à l’implémentation FRC1 ; `T1V` serait nécessaire pour raisonner sur le temps restant, et aucune instrumentation ne quantifie les réveils anticipés.

### Démarrage / Boot readiness

- **[majeur]** Initialiser Timer1 avant WiFi ne produit aucun tach au boot : la sortie reste relâchée jusqu’au spin-up du ventilateur et à deux fronts réels. Impact : si le Deye contrôle le tach avant cette mesure, il peut lever une alarme ; aucun tach nominal provisoire ni délai de grâce mesuré ne couvre cette fenêtre.
- **[majeur]** Le Wemos dépend des rails ventilateur. Si les deux rails sont coupés pendant l’autotest, le contrôleur ne démarre pas et ne peut fournir aucun signal anticipé. Impact : dépendance circulaire possible avec la logique de validation Deye.

### Génération sortie / Timer1

- **[mineur]** Lors d’un changement brusque de période, la demi-période déjà engagée n’est pas terminée selon l’ancienne consigne : la NMI lit immédiatement `targetHalfCycles`, puis additionne la nouvelle valeur à l’ancienne échéance. Impact : une impulsion transitoire peut être raccourcie ou allongée ; aucune adoption à frontière de front ni compteur de resynchronisation n’est prévu.
- **[mineur]** La compatibilité réelle du signal carré 50 %, de sa gigue et de la limite 30 000 RPM avec l’entrée Deye n’est étayée par aucune capture. Impact : fonctionnement protocolaire seulement déduit, pas mesuré sous trafic WiFi et écriture flash.

### Fidélité au protocole tachymétrique / Fail-safe

- **[critique]** Après un timeout, la NMI coupe la sortie mais ne remet jamais `hasSignal`, `measuredPeriodCycles` ni `targetHalfCycles` à zéro. Au premier front suivant, `processTachEdgeIRAM()` rafraîchit `lastEdgeCycle`, vide seulement l’index du filtre et retourne ; la vieille consigne reste active :

```cpp
if (!c.hasSignal || (dt > g_fanTimeoutCycles)) {
  c.lastEdgeCycle = now;
  c.ringIndex     = 0;
  c.ringCount     = 0;
  c.hasSignal     = 1;
  c.totalPulses   = c.totalPulses + 1;
  return;
}
```

  Impact : un seul front de reprise — ou un parasite isolé après plus de 500 ms — relance immédiatement l’ancienne fréquence pendant encore 500 ms, sans période mesurée. Des fronts isolés espacés d’un peu plus de 500 ms peuvent ainsi presque maintenir un faux tach et masquer un ventilateur toujours bloqué.
- **[majeur]** Même après une reprise légitime, une seule nouvelle période suffit à remplacer l’ancienne consigne ; aucune validation de plusieurs périodes cohérentes n’est exigée. Impact : deux fronts parasites peuvent produire un tach plausible pendant 500 ms.
- **[majeur]** Un seul ventilateur est surveillé dans chaque paire. Impact : le second Noctua peut être bloqué ou débranché, avec perte potentielle de la moitié du débit d’air, tandis que le Deye reçoit encore un tach normal.
- **[mineur]** La borne d’entrée ne filtre que les périodes trop courtes ; toute période jusqu’au timeout de 500 ms est acceptée, soit jusqu’à environ 60 RPM. Impact : un front manqué ou un outlier lent peut faire chuter brutalement la sortie.

### Filtres / Signal

- **[majeur]** La moyenne sur quatre périodes n’a ni médiane, ni rejet relatif d’outlier, ni borne plausible propre aux NF-A6/NF-A9. Impact : une période longue mais inférieure à 500 ms contamine la moyenne pendant quatre échantillons et réduit fortement le tach simulé.
- **[mineur]** Après le premier amorçage ou une reprise, la sortie utilise immédiatement une seule période avant que la moyenne annoncée sur deux tours soit disponible. Impact : le niveau de confiance du signal varie sans être exposé.
- **[mineur]** La résistance série 2,2 kΩ protège le GPIO mais ne constitue pas à elle seule le filtre CEM revendiqué ; aucun condensateur, Schmitt externe ou clamp n’est documenté. Impact : immunité aux fronts induits dans l’environnement IGBT non démontrée.

### WiFi / Réseau

- **[critique]** Aucun endpoint n’est authentifié et le mot de passe AP par défaut `deye12345` est fixe et publié. Tout client AP ou STA peut modifier les ratios, remplacer les identifiants WiFi ou réinitialiser la configuration. Impact : falsification distante du tach, perte d’accès et déclenchement répété d’écritures flash.
- **[majeur]** Il n’y a ni protection CSRF/origine ni TLS. Impact : une page ouverte par un client du même réseau peut soumettre les POST, et les identifiants WiFi saisis transitent en clair.
- **[mineur]** L’échappement JSON ne traite que `"` et `\`, pas les caractères de contrôle autorisés dans un SSID. Impact : `/api/status` peut devenir un JSON invalide.
- **[mineur]** `handleApiRatios()` accepte une mise à jour partielle : si un ratio est valide et l’autre invalide, il sauvegarde le premier et répond 200. Impact : configuration atomique attendue par l’interface non garantie.
- **[mineur]** Les résultats de `softAP()`, `EEPROM.commit()` et de la reconnexion ne sont pas reflétés dans les réponses. Impact : l’interface peut annoncer un succès alors que la persistance ou l’AP a échoué.

### Gestion mémoire / HEAP

- **[mineur]** `g_server.arg()` et les objets `String` des handlers WiFi allouent sur le heap à chaque POST. Impact : sous requêtes répétées, fragmentation possible du petit heap ESP8266 ; aucun minimum historique ni seuil d’alerte n’est exposé.
- **[mineur]** Le buffer JSON est borné, mais le retour de `snprintf` n’est pas contrôlé. Impact : une évolution du schéma de réponse pourrait tronquer silencieusement le JSON.

### Interface Web

- **[mineur]** Le polling ne protège pas les champs ratio contre une édition en cours : avant `initialLoaded`, une réponse tardive peut remplacer une saisie ; ensuite les valeurs ne sont plus resynchronisées depuis un autre client. Impact : affichage local potentiellement divergent de la configuration réelle.
- **[mineur]** Aucun âge de mesure, compteur de rejets, état de saturation, cause de fail-safe, heap ou charge NMI n’est exposé. Impact : impossible de distinguer stall, reprise sur vieille période, parasite et problème de timer sans oscilloscope.

### Persistance / Stockage

- **[majeur]** Chaque POST ratio/WiFi valide appelle `EEPROM.commit()`, même sans changement, et l’API est anonyme. Impact : l’usure de la flash peut être accélérée à distance ; l’EEPROM émulée ne fournit pas le wear-leveling de LittleFS.
- **[mineur]** Il n’existe ni double slot ni compteur de génération. Impact : une coupure pendant commit invalide le CRC et fait perdre toute la configuration au boot suivant.
- **[mineur]** Les valeurs par défaut après EEPROM invalide ne sont pas automatiquement persistées. Impact : le CRC est recalculé et les défauts sont rechargés à chaque boot jusqu’à la première sauvegarde, sans diagnostic de corruption.

### Documentation hardware/code et validité électrique

- **[critique]** L’alimentation annoncée pour 7–14 V ne peut pas fournir le courant ESP8266 sur toute cette plage. Avec 33 Ω en série, 7 V d’entrée et environ 5,1 V requis en sortie, le courant maximal théorique avant même marge transistor vaut seulement `(7 − 5,1 − 0,2) / 33 ≈ 52 mA`, inférieur au fonctionnement WiFi moyen et très inférieur aux pointes. Impact : chute du rail 5 V, brownout et reboot précisément quand le rail ventilateur est bas.
- **[critique]** À 12 V et 170 mA, les 33 Ω dissipent `I²R ≈ 0,95 W`, soit environ 0,32 W dans chacune des trois résistances 100 Ω 1/4 W en parallèle. Impact : surcharge thermique lors des pointes prolongées ; sans condensateur réservoir documenté, le montage ne garantit ni tension ni température sûres.
- **[majeur]** Le calcul du régulateur est faux : le pont 6,8 kΩ/1 kΩ mesure l’émetteur commun et le fixe vers `0,65 × 7,8 ≈ 5,07 V`; il ne fixe pas la base du ballast à 5,07 V pour obtenir 4,40 V. Impact : la broche 5V reçoit nominalement environ 5,1 V, avec dérive de `VBE` et des résistances ; la marge face à la limite 6 V du LDO est plus faible qu’annoncé.
- **[majeur]** L’« OR-ing sans diode » impose environ 5,1 V inverse entre émetteur et base du BC337 associé au rail éteint, au voisinage ou au-delà de `VEBO(max)=5 V`. La jonction base-émetteur peut entrer en avalanche et renvoyer du courant via 1,5 kΩ vers le canal mort. Impact : isolation non garantie, vieillissement du transistor et réinjection dans une sortie Deye arrêtée.
- **[majeur]** Aucun condensateur de découplage/bulk, PTC/fusible, TVS ou protection d’inversion n’est spécifié autour de l’alimentation. Impact : pointes WiFi, transitoires de câblage et environnement de puissance peuvent provoquer reset ou destruction.
- **[mineur]** Le document affirme qu’une résistance collecteur de 100 Ω garantit moins de 0,25 V pour toute pull-up Deye 3,3/5/12 V, sans connaître sa valeur ni son courant. Impact : le niveau bas est plausible pour quelques milliampères, mais pas universellement démontré.
- **[mineur]** Les masses des deux canaux sont reliées sans étape préalable imposant de mesurer leur différence de potentiel. Impact : si le Deye ne partage pas réellement ces retours, courant de boucle ou dommage possible.

### Qualité de code générale (négatif)

- **Structure / organisation — [mineur]** : le sketch de 1 200 lignes mélange moteur NMI, stockage, HTML, CSS et JavaScript — impact : revue, compilation incrémentale et tests plus coûteux.
- **Lisibilité / nommage — [mineur]** : `pinHigh` désigne la base NPN haute, donc une tach Deye basse — impact : confusion possible lors d’une modification de polarité.
- **Commentaires / documentation inline — [majeur]** : les promesses « isolation totale », « moins de 15 cycles » et OR-ing sûr sont présentées comme des faits malgré des contre-exemples mesurables — impact : confiance injustifiée dans les propriétés centrales.
- **Gestion d’erreurs / robustesse — [critique]** : l’état de période n’est pas invalidé au stall et l’alimentation est sous-dimensionnée — impact : faux tach après panne et brownouts possibles.
- **Duplication / DRY — [mineur]** : bonne factorisation logicielle ; le frontend volumineux reste toutefois embarqué dans l’unique unité de compilation.
- **Respect des conventions C++/Arduino — [majeur]** : dépendance directe à des hooks internes NMI et à un helper 64 bits non audité — impact : portabilité et sûreté IRAM non garanties.
- **Complexité / maintenabilité — [majeur]** : trois contextes concurrents partagent plusieurs champs sans protocole transactionnel — impact : les invariants de reprise sont difficiles à préserver.
- **Sécurité basique — [critique]** : API de commande anonyme, AP prévisible et absence de CSRF — impact : contrôle réseau des paramètres fonctionnels.
- **Testabilité — [majeur]** : aucun test hôte, build figé, capture de jitter, test de reprise, mesure thermique ou essai à 7 V — impact : les défauts déterminants n’ont pas été détectés avant livraison.

---

## ⭐ Note globale : 6,0/10 — Excellent ordonnanceur, mais fail-safe contournable et alimentation non qualifiée

Le moteur NMI, la formule 2 PPR, le filtrage et l’interface placent cette proposition au-dessus des firmwares non compilables ou à timing faux. Elle reste sous `composer 2.5 prompt from claude` et nettement sous les leaders : un unique front après stall réactive une vieille fréquence, tandis que le pré-régulateur ne peut tenir la plage 7–14 V annoncée et surcharge ses résistances aux pointes.

## ⭐ Note qualité de code : 7,8/10 — Très lisible et ambitieux, mais invariants critiques non tenus

La factorisation, les types fixes, `PROGMEM`, le CRC32 et le chemin IRAM sont de bon niveau, proche des propositions notées autour de 8/10. Le monolithe, les garanties non mesurées, l’état partagé non transactionnel, la sécurité HTTP et l’absence de tests empêchent d’atteindre les trois meilleures qualités de code.
