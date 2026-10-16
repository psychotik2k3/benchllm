# Analyse — Grok_4.7_medium

**Sources analysées** :

- `competitors/Grok_4.7_medium/deye_fan/deye_fan.ino`
- `competitors/Grok_4.7_medium/schema_electronique.md`

---

## ✅ Points techniques positifs

### Temps-réel / ISR

- Le chemin temps-réel est isolé dans une NMI Timer1 et ses fonctions portent `IRAM_ATTR`. Il évite `String`, flottants, réseau, EEPROM et `digitalWrite()`.
- `CCOUNT`, `GPI`, `GPOS` et `GPOC` donnent des timestamps et accès GPIO directs. La soustraction modulo 32 bits reste correcte malgré le wrap de `CCOUNT` toutes les 53,7 s, car toutes les échéances sont très inférieures à 2³¹ cycles.
- Les ratios `loop()` → NMI sont des mots 32 bits alignés et bornés. Le sens NMI → `loop()` emploie un seqlock pair/impair cohérent :

```cpp
do {
  a = g_seq[i];
  s.period = g_period[i];
  s.half = g_half[i];
  s.run = g_run[i];
  b = g_seq[i];
} while (a != b || (a & 1u));
```

- Par rapport à `grok 4.6 medium`, le partage multi-champs est transactionnel et la fraîcheur n'est plus renouvelée par tout parasite : `lastGood` n'est mis à jour qu'après validation d'une période.

### Démarrage / Boot readiness

- Les bases des NPN de sortie sont mises à LOW avant `pinMode(..., OUTPUT)` ; les résistances de 47 kΩ documentées maintiennent aussi les transistors bloqués pendant le reset.
- Le moteur tach est lancé avant la radio et aucune attente de connexion STA ne bloque `setup()`.
- Le premier front ne devient pas une fausse période : il initialise seulement `lastEdge`; le second front descendant valide peut démarrer la synthèse.

### Génération sortie / Timer1

- Timer1 est utilisé en `TIM_SINGLE` et réarmé vers la plus proche échéance entre le prochain polling d'entrée et les fronts de sortie. Ce n'est ni un `Ticker`, ni une génération dans `loop()`.
- Les échéances sont absolues (`nextToggle += half`) ; un réveil tardif ne crée ni dérive cumulative normale ni rafale de fronts fictifs.
- La fréquence de sortie est beaucoup plus fine que chez Grok 4.6 : les fronts sont programmés en cycles CPU plutôt que sur une grille fixe de 20 µs.
- Le plancher de demi-période à 100 000 cycles borne la sortie à 12 000 RPM, plage prudente au regard des ventilateurs visés.

### Fidélité au protocole tachymétrique / Fail-safe

- La formule 2 PPR est correcte. Pour une période entre fronts identiques, `80 MHz × 60 / 2 = 2 400 000 000`, exactement la constante de `rpmFromPeriod()`. Pour une demi-période de sortie, la constante correcte est 1 200 000 000.
- Le calcul `half = période_entrée × 512 / ratio_q10` correspond bien à `Tentrée/(2 × ratio)`, sans débordement dans la plage déclarée.
- Une période n'est acceptée qu'entre 2 et 300 ms, soit 15 000 à 100 RPM à 2 PPR. Après plus de 350 ms sans période valide, la sortie est immédiatement relâchée.
- Le fail-safe résiste au défaut majeur de Grok 4.6 : des glitches de moins de 2 ms peuvent déplacer `lastEdge`, mais ne déplacent jamais `lastGood` et ne peuvent donc maintenir indéfiniment l'ancienne simulation.
- Après un stall, `releaseOutput()` remet à zéro filtre, période, état de première arête et sortie ; la reprise exige de nouveau deux fronts.

### Filtres / Signal

- L'IIR entier de coefficient 1/8 lisse la période sans flottant dans la NMI et il est réinitialisé après perte de signal.
- Les bornes basse et haute de période rejettent aussi bien les glitches rapides que les fréquences trop lentes hors cible.
- L'adaptation d'entrée par BC547 maintient le GPIO entre 0 et 3,3 V et inverse proprement le signal ; le firmware mesure le front descendant correspondant.

### WiFi / Réseau

- AP+STA reste disponible sans attente bloquante, `WiFi.persistent(false)` évite les écritures SDK et `WIFI_NONE_SLEEP` réduit la variabilité radio.
- Les routes sont simples et cohérentes : `GET /`, `GET /api/config`, `GET /api/status` et `POST /api/save`.
- Les mots de passe ne sont pas renvoyés par les API. Le SSID configurable est échappé au moins pour `"` et `\`.

### Gestion mémoire / HEAP

- La page est en `PROGMEM` et servie par `send_P()`.
- Les JSON sont construits dans des buffers pile bornés par `snprintf`/`strlcat`; le chemin temps-réel n'alloue jamais.
- Les quelques temporaires `String` restent cantonnés au traitement HTTP et WiFi.

### Interface Web

- L'interface sombre et responsive affiche les RPM réels/simulés et l'état de chaque canal, avec polling toutes les 500 ms.
- Les ratios sont bornés côté serveur et appliqués immédiatement. Les mots de passe laissés vides conservent les secrets existants.
- Les mises à jour DOM utilisent `textContent`, ce qui évite une injection HTML via les données de statut.

### Persistance / Stockage

- La structure EEPROM contient magic, version et CRC32. Les chaînes sont explicitement terminées et les ratios sont re-bordés après chargement.
- Une configuration invalide revient à des valeurs par défaut utilisables, avec AP protégé par un mot de passe de huit caractères.

### Documentation hardware/code

- Les entrées et sorties tach sont bien décrites comme collecteurs ouverts, un seul tach par paire est lu et les sorties Deye utilisent des 2N2222 avec résistance de base et pull-down.
- La procédure impose une mesure de V5 avant raccordement du Wemos et rappelle que le ME6211 ne doit jamais recevoir 12 V.
- La nomenclature, les broches, l'inversion des entrées et les valeurs de résistances sont détaillées. Cette qualité documentaire rend aussi détectable l'erreur d'alimentation critique décrite plus bas.

### Qualité de code générale (positif)

- **Structure / organisation** : fonctions dédiées à l'acquisition, la publication, la persistance, la radio et chaque route HTTP.
- **Lisibilité / nommage** : constantes avec unités implicites explicables (`*_CYC`, `*_Q10`) et noms d'état courts mais cohérents.
- **Commentaires / documentation inline** : formules, contraintes NMI, polarités et choix de broches sont expliqués près du code.
- **Gestion d'erreurs / robustesse** : CRC, bornage, timeout fondé sur la dernière période valide, sortie relâchée et chaînes terminées.
- **Duplication / DRY** : état et traitement des deux canaux sont factorisés dans `Chan` et des boucles.
- **Respect des conventions C++/Arduino** : types fixes, `static`, `const`, `volatile`, `PROGMEM` et accès atomiques adaptés.
- **Complexité / maintenabilité** : les responsabilités sont séparées malgré le fichier unique et les paramètres sont centralisés.
- **Sécurité basique** : buffers bornés, secrets non relus et insertion DOM par `textContent`.
- **Testabilité** : CRC, conversion RPM, parsing et calculs sont isolés, même si aucun test automatisé n'est fourni.

---

## ❌ Points techniques négatifs

### Documentation hardware/code et alimentation

- **[critique]** Le « diode OU collecteur-base » Q1–Q4 est physiquement monté dans le sens bloquant. Dans un NPN, collecteur et émetteur sont de type N et la base de type P : la jonction C-B conduit en direct de la base P vers le collecteur N, pas du collecteur vers la base. Avec le collecteur à +12 V et la base sur VRAW plus bas, elle est polarisée en inverse :

```text
+12 V -- R1 -- collecteur Q1
               base Q1 -------- VRAW
               émetteur : flottant
```

  Impact : à 12 V, bien sous le claquage `Vcbo` de 40 V, seul un courant de fuite passe. VRAW ne monte pas, le régulateur ne reçoit aucune énergie et le Wemos ne démarre pas. Mettre quatre jonctions identiques en parallèle ne corrige ni leur polarité ni l'absence de conduction. `Vcbo` est une tenue maximale en blocage, pas une tension directe ni une fonction de diode.
- **[critique]** La phrase « bloque vraiment le retour vers le rail éteint » confond précisément blocage inverse et acheminement direct : le montage bloque aussi chaque rail vivant. Impact : l'ensemble n'est fonctionnel que si le Wemos reçoit une alimentation externe non documentée; tel que câblé, aucun tach simulé n'est produit.
- **[majeur]** Même après remplacement de Q1–Q4 par un OR-ing réellement conducteur, le régulateur discret Q5–Q7 est un linéaire sans limitation de courant, protection thermique, référence stable ni qualification de boucle. La consigne suppose arbitrairement `Vbe(Q7)=0,65 V`; la dispersion et le coefficient thermique d'environ −15 mV/°C reporté sur V5 peuvent déplacer sensiblement la sortie. Impact : surtension possible à froid, sous-tension/brownout aux pointes WiFi et absence de protection en court-circuit.
- **[majeur]** À 300 mA et environ 12 V en entrée, R10 dissipe 0,9 W et les deux TO-92 dissipent ensemble approximativement 1,2 W après les autres chutes, avec partage dépendant des résistances de 1 Ω et des gains. Impact : jonctions très chaudes, marge transitoire faible et risque de dérive/arrêt thermique; « placer dans le flux d'air » ne remplace pas un dimensionnement garanti.
- **[majeur]** L'alimentation dépend des rails ventilateur. S'ils sont tous deux coupés ou hachés avant validation tach, le contrôleur ne peut démarrer et aucune sortie de secours n'est possible. Impact : dépendance circulaire potentielle avec la logique Deye.
- **[mineur]** Il n'y a ni fusible/PTC, ni TVS, ni protection contre inversion/transitoires 12 V. Impact : un défaut de câblage ou pic automobile/inductif peut détruire transistors et Wemos.

### Temps-réel / ISR

- **[majeur]** La NMI se réarme au plus tard toutes les 32 µs, soit 31 250 NMI/s même à l'arrêt, et parcourt deux fois les deux canaux avec publication seqlock à chaque passage. Impact : charge CPU/radio permanente, latence accrue du code masquable et risque de watchdog ou d'instabilité WiFi non quantifié. Les leaders à Timer1 one-shot événementiel n'imposent pas ce coût à vide.
- **[majeur]** `acceptPeriod()` exécute divisions et modulo 32 bits dans la NMI. Sur Xtensa LX106, leur implantation peut appeler un helper de toolchain; le commentaire ne prouve ni sa résidence ROM/IRAM ni celle de `timer1_write()`. Impact : si un helper ou trampoline réside en flash pendant `EEPROM.commit()`, crash possible cache désactivé. Une map de liens ou un build verrouillé est nécessaire.
- **[majeur]** Les affirmations « le WiFi ne peut pas la retarder » et « jitter bien en dessous de 1 µs » ne sont accompagnées d'aucune capture ni budget de pire cas. Une NMI réduit fortement la gigue masquable, mais partage toujours CPU, bus et timer. Impact : garantie temps-réel non démontrée en production.
- **[mineur]** La dépendance aux API internes `ETS_FRC_TIMER1_NMI_INTR_ATTACH`, registres GPIO et `timer1_write()` n'est associée à aucune version exacte du core. Impact : régression de compilation ou de placement IRAM lors d'une mise à jour. `arduino-cli` n'était pas disponible localement pour confirmer le build.

### Démarrage / Boot readiness

- **[majeur]** Le moteur démarre avant WiFi, mais la sortie reste arrêtée jusqu'à deux fronts valides, puis attend encore une demi-période synthétique avant son premier front. Impact : à 1 600 RPM, jusqu'à environ 37,5 ms sont nécessaires pour observer deux fronts selon la phase initiale; à la borne de 100 RPM, la fenêtre peut approcher 600 ms, hors spin-up. Le Deye peut déclarer un arrêt avant le premier tach.
- **[mineur]** Aucun tach nominal provisoire, état de grâce Deye mesuré ou valeur persistée bornée ne couvre le spin-up. Impact : disponibilité au boot dépend entièrement du comportement non documenté de l'onduleur.

### Génération sortie / Timer1

- **[majeur]** Le polling d'entrée à 32 µs quantifie chaque période mesurée. L'erreur de période peut atteindre environ 32 µs selon la phase d'échantillonnage, soit 1,6 % à la période minimale de 2 ms; toute impulsion basse plus courte que la grille peut être manquée. Impact : RPM et fréquence simulée bruités aux hautes vitesses malgré la résolution CCOUNT annoncée.
- **[mineur]** Si une échéance de sortie est déjà dépassée, le code émet un seul front puis recale à `now + half`; il évite correctement une rafale, mais introduit une discontinuité de phase. Impact : une surcharge NMI devient un pulse allongé et une erreur temporaire de fréquence, sans compteur diagnostic.
- **[mineur]** L'interface affiche le RPM issu de `half`, donc reflète le plafond de 12 000 RPM, mais n'indique ni saturation ni retard de front. Impact : diagnostic difficile sous surcharge.

### Fidélité au protocole tachymétrique / Fail-safe

- **[majeur]** Une seule période valide suffit à activer la simulation. Impact : deux fronts parasites espacés de 2 à 300 ms peuvent produire un tach crédible pendant jusqu'à 350 ms; une validation sur plusieurs périodes cohérentes serait plus sûre.
- **[majeur]** Le fail-safe honnête arrête la sortie au stall au lieu de masquer indéfiniment la panne, mais il continue l'ancienne fréquence pendant jusqu'à 350 ms après la dernière période valide. Impact : retard mesurable de détection d'un ventilateur bloqué.
- **[majeur]** Un seul ventilateur tach est surveillé dans chaque paire. Impact : le second peut être bloqué ou débranché, avec perte potentielle de la moitié du débit d'air, tandis que le Deye reçoit encore une simulation normale.
- **[mineur]** La formule et les 2 PPR sont corrects, mais aucune mesure réelle ne confirme que le Deye compte le même front, accepte 50 % de duty-cycle et tolère 12 000 RPM. Impact : compatibilité protocolaire encore théorique.

### Filtres / Signal

- **[mineur]** L'IIR 1/8 atténue le bruit mais n'a ni médiane, ni rejet relatif d'outlier, ni validation multi-périodes. Impact : une période aberrante pourtant comprise entre 2 et 300 ms déplace immédiatement le tach, puis met plusieurs mesures à disparaître.
- **[mineur]** L'entrée est échantillonnée et non capturée par interruption GPIO. Impact : des pulses ou parasites de largeur inférieure à 32 µs peuvent être invisibles ou dépendre de leur phase.

### WiFi / Réseau

- **[critique]** Aucun endpoint n'est authentifié et l'AP utilise le mot de passe usine prévisible `deyefan1`. Tout client AP ou STA peut appeler `/api/save`, modifier les ratios jusqu'à ×6 et remplacer les identifiants réseau. Impact : falsification distante du tach, perte d'accès ou usure flash.
- **[majeur]** Il n'y a ni protection CSRF/origine ni TLS. Impact : une page ouverte par un client du même réseau peut soumettre un POST à l'équipement et les secrets saisis circulent en clair.
- **[majeur]** `parseRatio()` transforme une chaîne invalide en `0.0`, puis la borne silencieusement à 0,50. Impact : une faute de saisie valide une valeur dangereusement différente au lieu de renvoyer HTTP 400.
- **[mineur]** L'échappement JSON du SSID ne traite pas les caractères de contrôle. Impact : certains SSID peuvent rendre `/api/config` invalide.
- **[mineur]** Toute route inconnue, y compris une faute d'API, reçoit une redirection 302 vers `/`. Impact : les clients automatisés distinguent mal endpoint absent et interface valide.

### Gestion mémoire / HEAP

- **[mineur]** `server.arg()`, `URLSearchParams` côté client et les conversions `IPAddress::toString()` créent encore des temporaires dynamiques. Impact : pression heap limitée mais répétée au polling, sans mesure du minimum de heap.

### Interface Web

- **[majeur]** Le formulaire ne valide pas `response.ok` avant d'afficher le résultat autrement que par un ternaire minimal et ne protège pas les champs en cours d'édition contre un futur rechargement de configuration. Impact : retour utilisateur sommaire et risque d'écrasement si l'interface évolue vers un polling de configuration.
- **[mineur]** Aucun âge de mesure, compteur de rejets, état de saturation, cause de fail-safe ou charge NMI n'est exposé. Impact : impossible de distinguer stall, parasites, pulse manqué et surcharge sans oscilloscope.

### Persistance / Stockage

- **[mineur]** Chaque POST valide appelle `EEPROM.commit()` même si les valeurs sont inchangées et l'API est anonyme. Impact : usure flash provoquable à distance.
- **[mineur]** Il n'existe ni double slot ni compteur de génération. Impact : une coupure pendant commit invalide le CRC et fait perdre toute la configuration au boot suivant.

### Qualité de code générale (négatif)

- **Structure / organisation — [mineur]** : le sketch de plus de 650 lignes mélange moteur NMI, persistance, HTML, CSS et JavaScript — impact : revue et évolution plus difficiles qu'avec des modules séparés.
- **Lisibilité / nommage — [mineur]** : `level=0` signifie GPIO bas et transistor de sortie bloqué, tandis que le niveau tach Deye est l'inverse — impact : risque de confusion lors d'une modification de polarité.
- **Commentaires / documentation inline — [majeur]** : les garanties de NMI et surtout l'explication de la diode C-B sont affirmées sans validation physique — impact : elles donnent une confiance injustifiée à un montage qui ne s'alimente pas.
- **Gestion d'erreurs / robustesse — [critique]** : alimentation impossible et API de commande anonyme dominent les protections logicielles — impact : produit non démarrable tel que documenté et reconfigurable par tout client réseau.
- **Duplication / DRY — [mineur]** : bonne factorisation générale; la duplication matérielle Q1–Q4 reproduit toutefois quatre fois la même erreur — impact : coût accru sans redondance fonctionnelle.
- **Respect des conventions C++/Arduino — [majeur]** : dépendance à des hooks NMI privés et divisions dont le placement IRAM n'est pas prouvé — impact : portabilité et sûreté de build faibles.
- **Complexité / maintenabilité — [majeur]** : NMI permanente, polling et ordonnanceur de sorties sont fortement couplés — impact : toute évolution d'acquisition modifie directement la charge temps-réel.
- **Sécurité basique — [critique]** : absence d'authentification, CSRF et validation stricte des nombres — impact : contrôle distant des ratios et de la connectivité.
- **Testabilité — [majeur]** : aucun test hôte, build reproductible, capture de jitter, test thermique ou mesure du régulateur — impact : les invariants les plus risqués restent non vérifiés.

---

## ⭐ Note globale : 4,0/10 — Excellent firmware compromis par une alimentation physiquement bloquée

Le logiciel corrige plusieurs défauts de Grok 4.6 et approche les leaders sur Timer1, seqlock, formule 2 PPR et fail-safe. Mais Q1–Q4 ne peuvent pas alimenter VRAW dans la polarité dessinée; le montage complet ne démarre donc pas sans refonte de l'OR-ing et qualification du régulateur.

## ⭐ Note qualité de code : 8,0/10 — Moteur rigoureux, mais garanties NMI et sécurité insuffisamment prouvées

Le code est clair, borné et nettement plus robuste que Grok 4.6, avec un seqlock pertinent et un fail-safe fondé sur les seules périodes valides. La NMI permanente à 31,25 kHz, les helpers de division non audités, l'API anonyme et l'absence de tests empêchent d'égaler les meilleures qualités de code du classement.
