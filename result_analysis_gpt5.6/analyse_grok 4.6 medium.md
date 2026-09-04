# Analyse — grok 4.6 medium

Sources analysées (liste exhaustive du dossier) :

- `competitors/grok 4.6 medium/deye_fan/deye_fan.ino`
- `competitors/grok 4.6 medium/SCHEMA_ELECTRONIQUE.md`

## ✅ Points techniques positifs

### Temps-réel / ISR

- Les ISR de capture GPIO et l’ISR NMI Timer1 sont marquées `IRAM_ATTR`. La NMI n’appelle ni flash, ni réseau, ni allocation, ni flottant.
- La sortie commute directement `GPOS/GPOC`, jamais `digitalWrite()` dans le chemin temps-réel :

```cpp
if (lvl) {
  GPOS = MASK_OUT_9;
} else {
  GPOC = MASK_OUT_9;
}
```

- La capture par CCOUNT donne une résolution de 12,5 ns à 80 MHz ; la soustraction non signée supporte le wrap.
- Le calcul de conversion, le HTTP, l’EEPROM et le WiFi sont confinés à `loop()`. Les publications principales vers la NMI sont des écritures alignées simples.
- Les broches de boot problématiques GPIO0/GPIO15 et GPIO16 sans IRQ sont explicitement évitées.

### Démarrage / Boot readiness

- Les bases NPN sont mises LOW par registre avant le Timer1 et le WiFi ; les résistances de rappel 10 kΩ documentées maintiennent les transistors coupés pendant reset.
- Le Timer1 NMI est installé avant l’association WiFi, ce qui évite que le réseau retarde l’activation du moteur temps-réel.
- La configuration par défaut fournit immédiatement un AP identifiable et protégé.

### Génération sortie / Timer1

- Le Timer1 matériel est utilisé directement en mode périodique NMI (`TIM_LOOP`) à 50 kHz ; ce n’est ni `Ticker`, ni du polling dans `loop()`.
- La NMI possède ses compteurs indépendants pour les deux canaux. Le WiFi et HTTP ne produisent jamais les fronts.
- La demi-période est correctement dérivée de la période d’entrée et du ratio entier centième. La simplification utilisée est exacte à CPU 80 MHz et NMI 50 kHz :

```cpp
uint64_t num = (uint64_t)periodCycles * 5ULL;
uint64_t den = 160ULL * (uint64_t)ratioX100;
uint32_t half = (uint32_t)(num / den);
```

- L’arithmétique 64 bits protège le numérateur des débordements. La sortie est bornée à au moins deux ticks NMI, soit 40 µs par demi-période.
- Une fois armée, la fréquence ne dépend plus de la cadence du `loop()` ; il n’y a donc pas de drift directement cumulé par polling réseau.

### Fidélité protocole tachymétrique / Fail-safe

- `PULSES_PER_REV = 2` est explicite et la conversion `CPU_HZ × 60 / (période × 2)` est correcte.
- En absence de période ou après 400 ms sans front, `nmiEnabled` passe à zéro et la NMI force la base NPN LOW, donc relâche la tach Deye.
- Le premier front sert uniquement de référence et un seuil de 200 µs rejette les fronts trop proches.
- L’état web et la LED distinguent simulation active et attente de signal.

### Filtres / Signal

- Le seuil temporel de 200 µs constitue un anti-glitch numérique simple.
- Le schéma propose un condensateur 100 pF optionnel contre les parasites HF et un pull-up externe 4,7 kΩ qui produit des fronts plus francs que le seul pull-up interne.
- La résistance série 220 Ω protège l’entrée sans former un diviseur dangereux.

### WiFi / Réseau

- AP+STA est permanent, avec AP local disponible même sans station.
- `WiFi.persistent(false)` évite les écritures SDK redondantes ; une routine tente une reconnexion STA toutes les 15 s.
- Le nombre de clients AP est limité à quatre et le canal est fixé, ce qui rend le comportement plus prévisible.
- Les mots de passe ne sont ni renvoyés dans l’API statut ni préchargés dans la page.

### Gestion mémoire / HEAP

- La page complète est une constante `PROGMEM` servie par `send_P()`, évitant la construction d’un buffer HTML de plusieurs kilo-octets dans le heap.
- Les réponses JSON réservent leur capacité et utilisent `F()` pour les fragments littéraux.
- Le temps-réel n’utilise ni `String`, ni allocation dynamique ; la configuration et les canaux ont des tailles fixes.

### Interface Web

- L’interface responsive expose les deux RPM lus/simulés, l’activité, les ratios et les statuts/IP AP/STA.
- Les ratios ont des bornes cohérentes côté HTML et serveur, avec conservation de l’ancienne valeur si la nouvelle est hors plage.
- Les champs mot de passe vides signifient « conserver », ce qui évite d’exposer les secrets existants.
- L’application d’une configuration relance le WiFi et persiste les ratios sans recompilation.

### Persistance / Stockage

- L’enregistrement EEPROM est protégé par `magic` et CRC32 ; une corruption détectée restaure un AP connu.
- Les ratios sont stockés comme centièmes entiers plutôt qu’en flottants, ce qui stabilise la représentation persistée et le partage avec la logique temps-réel.
- EEPROM est adaptée au petit enregistrement unique ; LittleFS n’est pas nécessaire tant que les écritures restent rares et contrôlées.

### Documentation hardware/code

- Le document fournit un OR-ing Schottky des deux rails 12 V, empêchant la rétro-alimentation d’un connecteur par l’autre, puis un buck réglé à 5 V pour le Wemos.
- Chaque paire de ventilateurs reste sur son propre rail, et un seul tach par taille est lu ; les fils tach ne sont pas mis en parallèle.
- L’entrée Noctua à collecteur ouvert est tirée uniquement vers 3,3 V, donc compatible ESP8266 sans exposer le GPIO au 5/12 V.
- Les sorties utilisent des NPN en collecteur ouvert avec résistance de base, pull-down de reset et résistance série. Le Vceo annoncé permet les pull-up Deye 3,3, 5 et 12 V.
- La documentation détaille brochage, nomenclature, polarités à vérifier et mise en service avec mesure à l’oscilloscope.

### Qualité de code générale (positif)

- **Structure :** sections fonctionnelles nettes, état par canal centralisé et séparation correcte entre NMI, capture, conversion, persistance et web.
- **Lisibilité :** constantes nommées avec unités, commentaires de formule et noms de broches explicites.
- **Commentaires :** les contraintes NMI, 2-PPR, collecteur ouvert et broches de boot sont expliquées au bon endroit.
- **Robustesse :** CRC, timeout, sortie relâchée, anti-glitch, reconnexion STA et bornes de ratio.
- **DRY :** conversion et mise à jour sont paramétrées par canal ; seule la NMI déroule volontairement les deux canaux pour minimiser les appels.
- **Conventions C++/Arduino :** types fixes, `static`, `const`, `IRAM_ATTR`, `PROGMEM` et arithmétique entière sont bien utilisés.
- **Maintenabilité :** paramètres matériels et temporels sont regroupés et le HTML est isolé dans une constante.
- **Sécurité basique :** AP protégé par défaut, secrets non renvoyés et longueurs de champs limitées.
- **Testabilité :** fonctions de CRC, conversion RPM et conversion période/reload sont isolées, et la documentation prescrit une vérification scope.

## ❌ Points techniques négatifs

### Temps-réel / ISR

- **[majeur]** La compilation probable repose sur des API bas niveau spécifiques au core/SDK (`ETS_FRC_TIMER1_NMI_INTR_ATTACH`, `GPOS`, `GPOC`, `ets_sys.h`) sans version ESP8266 Arduino fixée. Une version où ces macros ne sont plus exposées peut échouer à compiler.
- **[majeur]** `tachCapture()` marqué IRAM appelle `ESP.getCycleCount()` et `micros()`. La sûreté IRAM transitive dépend de la version du core ; aucune map mémoire ou version testée ne la garantit.
- **[majeur]** La NMI tourne en permanence à 50 kHz, même sans signal, et traite les deux canaux à chaque tick. Cela consomme inutilement une part CPU significative et augmente les interactions avec le WiFi, contrairement à un Timer1 one-shot armé uniquement à la prochaine échéance.
- **[majeur]** `nmiReload`, `nmiCount` et `nmiEnabled` sont modifiés depuis `loop()` pendant que la NMI les lit, sans section critique NMI. Les écritures 32 bits sont atomiques, mais la transition multi-champs n’est pas transactionnelle ; un premier intervalle peut utiliser un ancien compteur/reload.
- **[mineur]** Le code suppose strictement 80 MHz via `CPU_HZ`, alors qu’aucun garde de compilation ne refuse un build à 160 MHz. À 160 MHz, RPM et fréquence de sortie seraient faux d’un facteur deux.

### Démarrage / Boot readiness

- **[critique]** Malgré le commentaire « Timer1 NMI AVANT le Wi-Fi », aucune sortie n’est activée avant une période valide. Il faut deux fronts puis un passage de `loop()` ; si le Deye contrôle le tach avant la montée du ventilateur, une alarme boot reste probable.
- **[majeur]** Le contrôleur est alimenté par les rails ventilateur eux-mêmes. Si les deux sont coupés au boot ou pilotés après validation tach, le montage est dans une dépendance circulaire et ne peut produire le signal attendu.
- **[mineur]** Aucun mode temporaire « boot RPM » borné ni délai de grâce configurable n’est proposé pour couvrir la première mesure.

### Génération sortie / Timer1

- **[majeur]** La résolution de sortie n’est que de 20 µs par tick NMI. À une demi-période de 250 µs, l’arrondi par troncature peut atteindre presque 8 % ; le tach simulé peut être sensiblement trop rapide. Le code tronque toujours vers le bas au lieu d’arrondir, introduisant un biais systématique.
- **[majeur]** La NMI à pas fixe réduit le drift dû à `loop()`, mais la fréquence résultante est quantifiée. Aucun accumulateur fractionnaire ne compense l’erreur moyenne, donc une erreur de période constante persiste indéfiniment.
- **[majeur]** Le minimum de deux ticks autorise une demi-période de 40 µs, mais le commentaire SDK ne démontre que la période NMI minimale de 100 ticks Timer1. Les limites électriques et protocolaires de la sortie ne sont pas reliées aux RPM réalistes.
- **[mineur]** L’activation charge `nmiCount`, mais une modification de ratio pendant fonctionnement change `nmiReload` sans recaler la phase ; la première période après changement peut mêler ancienne et nouvelle durée.
- **[mineur]** L’UI affiche le ratio mathématique, pas la fréquence réellement quantifiée par NMI.

### Fidélité protocole tachymétrique / Fail-safe

- **[critique]** `lastEdgeUs` est mis à jour avant validation anti-glitch. Une rafale de parasites de moins de 200 µs peut donc maintenir le canal « frais » indéfiniment tout en conservant l’ancienne `periodCycles`; le Deye continue alors de voir un tach valide après un vrai stall :

```cpp
ch->lastEdgeUs = micros();
ch->pulseCount = ch->pulseCount + 1;
if (prev == 0) {
  return;
}
const uint32_t dt = now - prev;
if (dt < MIN_EDGE_CYCLES) {
  return;
}
```

- **[majeur]** Un seul intervalle valide active la sortie. Il n’y a ni confirmation sur plusieurs pulses, ni plage maximale de période, ni cohérence relative ; un parasite isolé peut simuler un ventilateur pendant 400 ms.
- **[majeur]** Le timeout de 400 ms coupe correctement un fan normal, mais exclut mécaniquement toute vitesse inférieure à 75 RPM à 2 PPR et n’est pas dérivé d’une plage explicitement validée.
- **[mineur]** `pulseCount` est incrémenté mais jamais exploité ; aucune détection de pertes, compteur diagnostique ou validation multi-pulses n’en bénéficie.
- **[mineur]** Le RPM web repose sur la dernière période brute, sans indiquer son âge ni la cause de désactivation.

### Filtres / Signal

- **[majeur]** Aucun filtre numérique de période n’est implémenté. Toute mesure valide devient immédiatement le reload NMI ; bruit et jitter du tach réel sont donc reproduits, puis aggravés par la quantification 20 µs.
- **[majeur]** Le filtre matériel 100 pF est optionnel et très faible ; il traite la HF mais pas des rebonds/parasites plus lents. Il n’y a ni Schmitt trigger ni hystérésis.
- **[majeur]** L’anti-glitch n’a qu’une borne minimale et sa mise à jour prématurée de fraîcheur neutralise partiellement le fail-safe.
- **[mineur]** Le pull-up interne activé en parallèle du 4,7 kΩ externe change la résistance effective et n’est pas pris en compte dans les calculs de courant, même si cela reste dans la capacité Noctua.

### WiFi / Réseau

- **[critique]** Aucune authentification applicative ne protège `/api/config`. Toute personne sur l’AP ou le LAN STA peut modifier les ratios et identifiants, ce qui touche directement le signal de sécurité perçu par l’onduleur.
- **[majeur]** HTTP est en clair et il n’y a aucune défense CSRF/origine.
- **[majeur]** `startWifi()` est rappelé après sauvegarde sans arrêter proprement l’ancien AP/STA. Le résultat lors d’un changement simultané d’identifiants dépend du core et peut laisser une session incohérente ; un reboot contrôlé serait plus déterministe.
- **[majeur]** SSID/AP sont concaténés dans le JSON sans échappement. Une quote ou un backslash casse `/api/status`; des caractères HTML dans les valeurs peuvent ensuite atteindre `textContent` sans injection HTML, mais l’API reste invalide.
- **[mineur]** L’AP est permanent, augmentant la surface d’attaque même lorsque STA fonctionne.

### Gestion mémoire / HEAP

- **[mineur]** Le choix `PROGMEM/send_P()` est bon, mais la réponse statut crée encore des temporaires `String` pour IP et ratio à chaque poll de 500 ms.
- **[mineur]** Aucun suivi de heap libre/minimum ni comportement en cas d’allocation impossible n’est fourni.
- **[mineur]** Les appels répétés à `g_http.arg()` créent/récupèrent plusieurs `String`; l’impact reste hors NMI mais pourrait être réduit en copiant chaque argument une fois.

### Interface Web

- **[majeur]** L’UI affiche le texte de réponse sans vérifier `response.ok`; un refus ou une erreur réseau peut être présenté de manière ambiguë, puis le rechargement est tout de même planifié.
- **[majeur]** `parseRatioX100()` utilise `toFloat()`, donc une chaîne invalide devient 0 et conserve silencieusement l’ancien ratio. Le serveur renvoie néanmoins « OK », empêchant l’utilisateur de savoir que la saisie a été rejetée.
- **[majeur]** Les validations de longueur et format reposent en partie sur les attributs HTML. Un client direct peut envoyer des valeurs longues ; les copies sont bornées, mais le serveur ne renvoie aucune erreur explicite de troncature.
- **[mineur]** Le statut ne montre ni âge de la dernière mesure, ni erreur de quantification, ni raison de l’arrêt.

### Persistance / Stockage

- **[majeur]** La structure n’a ni version ni taille. Le CRC détecte la corruption mais pas une migration de layout avec le même `magic`; une évolution firmware peut restaurer les défauts ou interpréter un ancien format.
- **[majeur]** Les appels `strncpy(..., size - 1)` ne forcent pas explicitement le dernier octet à zéro. L’état initial le rend souvent sûr, mais une donnée persistée inhabituelle ou une évolution peut laisser une chaîne non terminée et provoquer une lecture hors limites.
- **[mineur]** Une coupure pendant `EEPROM.commit()` invalide le CRC et perd toute configuration au boot suivant ; aucun double-slot transactionnel n’est prévu.
- **[mineur]** Toute soumission écrit la flash, même sans changement, ce qui permet une usure accélérée via l’API non authentifiée.

### Documentation hardware/code

- **[majeur]** Les modules buck proposés (MP1584/Mini360) ne sont pas des buck-boost et n’ont pas nécessairement d’UVLO propre. Si les rails ventilateur sont PWM, chutent ou basculent, le Wemos peut brown-out ; aucune plage réelle ni tenue aux transitoires n’est spécifiée.
- **[majeur]** Contrairement à un montage robuste d’onduleur, il n’y a ni PTC/fusible par connecteur, ni TVS, ni condensateurs de découplage obligatoires. Le seul 100 µF est optionnel.
- **[majeur]** La documentation réunit les masses Deye sans exiger explicitement une mesure préalable de leur équipotentialité/isolation. Si les retours ne sont pas communs, le montage peut créer un chemin de courant indésirable.
- **[mineur]** Le choix générique de nombreux NPN ne vérifie pas brochage, Vceo et gain pour chaque référence ; remplacer « tous conviennent » par des composants qualifiés éviterait les erreurs.
- **[mineur]** La mise en service recommande l’oscilloscope mais ne demande pas de mesurer d’abord la tension de pull-up Deye, le niveau bas saturé et les transitoires des rails.
- **[mineur]** L’affirmation « jitter indépendant du trafic HTTP » est trop absolue sans mesures ; la NMI réduit fortement l’influence mais partage toujours le silicium et introduit sa propre quantification.

### Qualité de code générale (négatif)

- **Structure : [mineur]** le fichier reste monolithique et embarque HTML/CSS/JS avec le firmware, malgré une bonne séparation interne.
- **Lisibilité : [mineur]** le commentaire de simplification de `periodToNmiHalf()` contient une hésitation (« wait »), ce qui réduit la confiance dans une formule pourtant correcte.
- **Commentaires : [majeur]** certaines affirmations de boot et d’indépendance du jitter sont plus fortes que le comportement réel.
- **Robustesse : [critique]** la fraîcheur mise à jour par des glitches peut masquer un stall, défaut le plus grave du code.
- **DRY : [mineur]** la NMI duplique le traitement des deux canaux ; ce choix peut se justifier par la prédictibilité, mais augmente le risque de corrections divergentes.
- **Conventions C++/Arduino : [majeur]** APIs SDK internes non versionnées et constante CPU codée en dur sans `static_assert`/garde.
- **Maintenabilité : [majeur]** format EEPROM non versionné et chaînes pas explicitement terminées.
- **Sécurité basique : [majeur]** API de commande sans auth/CSRF sur AP permanent.
- **Testabilité : [majeur]** aucun test automatisé de la formule, du wrap, des glitches, du stall ou de la concurrence NMI ; aucune capture de jitter réel n’est fournie.

## ⭐ Note globale : 7,1/10 — Bonne architecture NMI et excellent schéma de principe, mais le fail-safe contournable par glitches et la quantification de sortie doivent être corrigés.

Le montage est crédible après durcissement, sans atteindre encore le niveau de sûreté requis autour d’un onduleur.

## ⭐ Note qualité de code : 7,3/10 — Implémentation claire et économe en heap, pénalisée par les API bas niveau fragiles, la concurrence NMI et l’absence de tests.

Le code montre une bonne maîtrise ESP8266, mais plusieurs invariants critiques sont affirmés plutôt que prouvés.
