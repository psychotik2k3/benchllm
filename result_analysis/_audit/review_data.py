DATA = {}
def add(name, score, quality, verdict, pos, neg, structure, robustness, dry, testability='Aucun test fourni ; les calculs sont mêlés au firmware.'):
    DATA[name] = dict(score=score, quality=quality, verdict=verdict, pos=pos, neg=neg, structure=structure, robustness=robustness, dry=dry, testability=testability)

add('gpt-free', 6.1, 6.4, 'Moteur waveform pertinent, configuration fragile', {
'Temps-réel / ISR':'ISR d’entrée IRAM et copie atomique newPeriod/period ; somme du filtre réalisée en uint64_t hors interruptions.',
'Démarrage / Boot readiness':'Sorties basses et pause explicite de 100 ms avant EEPROM, capture puis réseau. Premier signal après deux fronts et premier processChannel ; aucune vitesse nominale avant mesure, durée power-on non mesurée.',
'Génération sortie / Timer1':'startWaveformClockCycles (479–487) utilise le moteur NMI du cœur, temps haut/bas égaux, deux sorties indépendantes.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2 ; fréquence de sortie proportionnelle au ratio, timeout logiciel de 500 ms avec drapeau signalPresent.',
'Filtres / Signal':'Périodes bornées et EMA 1/8 sur nouvelles mesures.',
'WiFi / Réseau':'AP+STA sans attente de connexion ; SSID/identifiants configurables.',
'Gestion mémoire / HEAP':'Aucune allocation dans les ISR, réservation explicite des String HTML/JSON.',
'Interface Web':'Dark theme et polling 500 ms des seules mesures ; formulaire chargé séparément.',
'Persistance / Stockage':'EEPROM structurée et signature, copies HTTP via toCharArray borné.',
'Documentation hardware/code':'Les commentaires du firmware expliquent les voies, les formules et les contraintes waveform ; pas de schéma annexe livré.'},[
('Persistance / Stockage','majeur','Seul magic est vérifié (225–253), sans CRC ni version ni fin de chaîne. constrain ne rejette pas NaN : des ratios EEPROM corrompus peuvent parvenir aux calculs et conversions numériques.'),
('WiFi / Réseau','majeur','handleGetConfig renvoie staPassword/apPassword (1065–1074), sans authentification et sans échappement JSON. Lecture des secrets et configuration possibles depuis le réseau ; guillemets/backslashes cassent la réponse.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','lastValidEdgeUs est daté lors de processChannel (579), pas lors du front. Un traitement HTTP lent peut rendre fraîche une ancienne mesure puis prolonger la sortie de 500 ms supplémentaires. Le timeout ne s’exécute que dans loop.'),
('Filtres / Signal','mineur','Le filtre filteredPeriodCycles n’est pas remis à zéro lors de l’arrêt (609–621). La reprise mélange une nouvelle vitesse avec l’ancien régime et nécessite plusieurs périodes pour converger.'),
('Interface Web','majeur','Les paramètres WiFi sont sauvegardés (1181) mais startWiFi n’est pas rappelé et aucun reboot n’est planifié. La nouvelle configuration radio n’est effective qu’après redémarrage manuel.'),
('Qualité de code générale','mineur','Retour de startWaveformClockCycles ignoré, page construite en RAM avec reserve(9000), deux grosses implémentations parallèles de capture. Le statut actif ne prouve pas que l’onde a démarré.')],
'Fonctions lisibles et sections explicatives, mais fichier de 1 575 lignes et état global.', 'Sections critiques et copies bornées présentes ; CRC, finitude, résultat waveform et reconfiguration manquent.', 'processChannel partagé, mais les deux ISR et l’interface sont très répétitifs.')

add('grok 4.6 medium', 6.0, 6.7, 'NMI simple, surveillance de perte incomplète', {
'Temps-réel / ISR':'Sortie NMI en IRAM, opérations entières et GPOS/GPOC ; données de consigne en volatile.',
'Démarrage / Boot readiness':'EEPROM puis NMI/capture avant WiFi. Sortie inactive sans mesure et premier front ignoré. Deux fronts puis loop et première demi-période nécessaires, hors setup non chronométré.',
'Génération sortie / Timer1':'DIV16, reload=100 ticks : NMI à 50 kHz et pas de 20 µs. La formule periodToNmiHalf correspond bien au ratio exprimé ×100 à 80 MHz.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2 et ratio 0,5–8 ; délai logiciel de 400 ms ; saturation de demi-périodes.',
'Filtres / Signal':'Intervalle minimal d’entrée 200 µs à 80 MHz.',
'WiFi / Réseau':'AP+STA et reconnexion toutes les 15 s.',
'Gestion mémoire / HEAP':'HTML PROGMEM et String status réservé.',
'Interface Web':'Polling à 500 ms, formulaires et mesures séparés.',
'Persistance / Stockage':'CRC32 par offsetof et bornage des ratios chargés.',
'Documentation hardware/code':'Schéma OU Schottky + buck 5 V, entrées tirées à 3,3 V, NPN de sortie et un seul tach par paire.'},[
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Au timeout, nmiEnabled est effacé mais havePeriod et lastEdgeUs restent valides (268–270). Après le wrap de micros (≈71 min 35 s), l’ancien timestamp redevient récent et l’ancienne fréquence peut être réémise pendant 400 ms sans ventilation.'),
('Filtres / Signal','majeur','tachCapture actualise lastCycle/lastEdgeUs avant le test anti-parasite (136–145). Des fronts tous <200 µs peuvent entretenir une ancienne période et empêcher l’arrêt ; aucun lissage ou borne maximale de période ne complète la capture.'),
('Temps-réel / ISR','majeur','NMI 50 kHz en permanence, mais timeout et nouvelles consignes dépendent de loop. La capture GPIO reste masquable ; le calendrier à ticks n’utilise pas un temps réel absolu pour compenser les retards de NMI.'),
('Qualité de code générale','mineur','parseRatioX100 (418–423) ne rejette pas explicitement NaN avant conversion vers uint16_t ; les SSID sont concaténés sans échappement JSON (454–455).'),
('WiFi / Réseau','majeur','Aucune authentification ; les réglages reconfigurent immédiatement la radio avant la réponse HTTP (479–481), ce qui peut interrompre la requête. Le résultat commit est ignoré.'),
('Documentation hardware/code','mineur','Le schéma propose NF-A9 ≈2 000 RPM (212) alors que la référence NF-A9 FLX nominale est 1 600 RPM. Le ratio doit provenir d’une mesure, pas de cette estimation.')],
'Structures par canal, conversions et stockage bien délimités.', 'CRC et ratios bornés ; état de capture non invalidé à la perte et validation numérique incomplète.', 'Calcul générique par canal, mais NMI écrite deux fois à la main.')

add('Grok_4.7_medium', 5.0, 6.6, 'Architecture cohérente, constantes de temps débordantes', {
'Temps-réel / ISR':'NMI échantillonne les entrées, génère les sorties et gère le timeout ; helpers IRAM et calculs entiers.',
'Démarrage / Boot readiness':'CPU forcé à 80 MHz, GPIO relâchés et EEPROM puis startTach avant radio. Pas de valeur nominale avant deux fronts valides ; polling 32 µs.',
'Génération sortie / Timer1':'DIV1 et cycles CPU à 80 MHz ; GPOS/GPOC, échéances accumulées et resynchronisation après retard.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2, ratios Q10 et arrêt qui efface le filtre et réamorce la capture (133–139).',
'Filtres / Signal':'Bornes de période et EMA entière 1/8 ; propriété du moteur confinée à la NMI.',
'WiFi / Réseau':'AP+STA et sommeil désactivé, connexion sans attente explicite.',
'Gestion mémoire / HEAP':'HTML PROGMEM, buffers status/config fixes de 320/192 octets.',
'Interface Web':'Polling 500 ms et chargement des réglages séparé ; états distincts selon canaux.',
'Persistance / Stockage':'Signature, version et CRC32 par offsetof ; chaînes terminées et ratios clampés.',
'Documentation hardware/code':'Schéma détaillé, un tach par paire, entrées/sorties NPN et calculs d’alimentation.'},[
('Fidélité au protocole tachymétrique / Fail-safe','critique','MAX_IN_CYC=300UL*80 000 000UL/1000 et LOST_CYC=350UL*80 000 000UL/1000 (66–67) débordent avant division. Ils valent respectivement 2 525 163 et 2 230 196 cycles, soit 31,565 ms et 27,877 ms, au lieu de 300/350 ms. Sous ≈1 076 RPM le timeout intervient avant le front suivant et empêche la mesure stable.'),
('Filtres / Signal','majeur','Échantillonnage sans validation de largeur : lastLevel puis lastEdge sont actualisés sur chaque transition (210–223). Un parasite capturé entre deux polls peut fausser la période ; seule sa durée entre fronts descendants est filtrée.'),
('Documentation hardware/code','majeur','Le régulateur discret proposé dépend de VBE et du partage thermique entre TO-92 (95–111). Le document reconnaît >80 °C sans flux d’air mais ne fournit pas de qualification au pire cas chaud ni de protection thermique. Risque de brownout, à tester sous charge WiFi et à température maximale.'),
('Qualité de code générale','mineur','appendJsonStr n’échappe pas les caractères de contrôle, parseRatio s’appuie sur toFloat et ne teste pas isfinite. commit est ignoré ; aucun test ne couvre les constantes en millisecondes.'),
('WiFi / Réseau','majeur','Configuration sans authentification. Les ratios ne sont recalculés qu’à la prochaine période acceptée ; après perte, la modification seule ne relance pas une sortie, comportement à documenter.')],
'Chan moteur, Config persistante et Snap de lecture séparent correctement les responsabilités.', 'CRC et réamorçage propres ; les constantes intermédiaires 32 bits ne sont pas protégées.', 'Un moteur commun et helpers partagés ; peu de duplication entre canaux.')

add('kimi k3', 5.7, 6.1, 'Simple et exploitable après correction de l’amorçage', {
'Temps-réel / ISR':'ISR IRAM, capture micros et EMA entière 1/4 ; GPOS/GPOC en sortie.',
'Démarrage / Boot readiness':'Capture avant EEPROM, Timer1 avant WiFi ; sorties basses sans nominal artificiel. La première mesure utilise toutefois la durée depuis zéro. Le timer inactif peut dormir une seconde avant de voir la première consigne.',
'Génération sortie / Timer1':'Scheduler DIV16 au prochain front, facteur 5 ticks/µs et échéances accumulées ; récupération des retards.',
'Fidélité au protocole tachymétrique / Fail-safe':'2 PPR, ratio 0,5–10 et arrêt après 1,5 s dans updateChannels, contrôlé toutes les 100 ms.',
'Filtres / Signal':'Anti-parasites 300 µs et EMA 1/4.',
'WiFi / Réseau':'AP+STA, auto-reconnexion et mDNS.',
'Gestion mémoire / HEAP':'HTML PROGMEM, status dans un buffer de 320 octets.',
'Interface Web':'Actualisation des mesures toutes les secondes sans recharger le formulaire.',
'Persistance / Stockage':'CRC16, chaînes terminées, ratios vérifiés au chargement ; pour ce layout précis, sizeof(Config)−2 exclut bien le CRC.',
'Documentation hardware/code':'Buck 5 V, diodes Schottky, sorties NPN et un tach par paire explicités.'},[
('Démarrage / Boot readiness','majeur','onPulse calcule dt=micros−0 au premier front (157–162) et l’injecte dans l’EMA. Après un redémarrage ou une longue pause, cette durée n’est pas une période de rotation ; la consigne est fausse pendant plusieurs fronts.'),
('Génération sortie / Timer1','majeur','L’ISR inactive recharge jusqu’à 1 000 000 µs (174/198) et la boucle ne force pas de réveil lorsqu’une consigne apparaît (312). Le premier signal peut donc être retardé d’environ 1 s, puis son échéance nextToggle initialisée à zéro impose un rattrapage.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Le timeout dépend de loop après HTTP et les données de période/timestamp ne sont pas prises ensemble sous section critique (299–305). Un callback lent prolonge l’émission de l’ancienne vitesse au-delà de 1,5 s.'),
('Qualité de code générale','mineur','Validation EEPROM par comparaisons < et > seulement (134–135) : NaN peut passer. Pas de borne haute de période en capture ni contrôle du résultat commit.'),
('WiFi / Réseau','majeur','Routes de configuration accessibles sans authentification ; pas de remontée d’erreur de sauvegarde et pas de validation complète des credentials WiFi.')],
'Channel et Config compacts, fonctions courtes de capture/stockage/HTTP.', 'CRC et copies de chaînes bornées ; amorçage et validation de finitude incomplets.', 'Capture et traitement communs aux deux canaux.')

add('kimi_k3_max', 5.8, 6.5, 'Version structurée, calendrier et vieillissement à corriger', {
'Temps-réel / ISR':'Capture et Timer1 en IRAM, GPOS/GPOC et sections critiques lors des changements de sortie.',
'Démarrage / Boot readiness':'GPIO relâchés avant EEPROM ; réseau et HTTP initialisés avant capture (440–465). Timer démarré à la première consigne, pas de RPM nominal de secours ; loop traite toutes les 100 ms.',
'Génération sortie / Timer1':'Calendrier en cycles à échéances absolues, saturation de demi-période et rattrapage des échéances dépassées.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2, ratios 0,5–8, durée de perte 1,5 s et RPM affichés dérivés de la demi-période écrêtée.',
'Filtres / Signal':'Rejet des intervalles <400 µs et EMA 1/4 dans le traitement.',
'WiFi / Réseau':'AP+STA, WIFI_NONE_SLEEP et reprise de la station toutes les 15 s.',
'Gestion mémoire / HEAP':'HTML PROGMEM et buffer snprintf status de 360 octets.',
'Interface Web':'Polling 1 s avec protection explicite document.activeElement pour les ratios (360).',
'Persistance / Stockage':'CRC16 par offsetof, chaînes terminées et clampRatio résistant à NaN.',
'Documentation hardware/code':'BOM, buck 5 V, entrées à 3,3 V, NPN de sortie avec rappel base-émetteur et un tach par paire.'},[
('Fidélité au protocole tachymétrique / Fail-safe','majeur','captureEdge accepte le premier dt depuis zéro (196–202), sans maximum. Après une longue pause, la période inclut le temps d’arrêt et l’EMA peut générer une vitesse très basse avant convergence.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Au timeout, periodCycles et lastEdgeCycle ne sont pas invalidés (280–285). Après un tour du compteur de cycles, l’ancienne mesure redevient récente pendant 1,5 s : reprise possible sans nouveau front, tous les 53,687 s à 80 MHz (26,844 s à 160 MHz).'),
('Génération sortie / Timer1','majeur','scheduleNext utilise une différence non signée (218–224). Si une échéance est dépassée au moment d’une reprogrammation depuis loop, la différence devient énorme et le délai peut être plafonné à 1,678 s au lieu d’un réveil immédiat. Le facteur fixe 16 est également faux à CPU 160 MHz : il faut 32 cycles CPU par tick DIV16.'),
('Temps-réel / ISR','majeur','Timer1 normal et timeout dans loop ; le filtre est appliqué toutes les 100 ms même si aucune nouvelle mesure n’est arrivée. La charge HTTP influe sur le suivi et le délai de perte.'),
('WiFi / Réseau','majeur','Pas d’authentification et handleWifi n’appelle pas WiFi.disconnect lorsque staEnable est décoché (409–412) : la station peut rester connectée malgré la désactivation demandée.'),
('Qualité de code générale','mineur','Résultat de commit ignoré ; la précision d’un dixième dans la réponse status est inférieure à la précision de configuration float. Tests des débordements absents.')],
'FanChannel et ChannelState distincts ; fonctions de capture et programmation séparées.', 'CRC, chaînes terminées et NaN borné ; timestamps et calendrier insuffisamment invalidés.', 'Mêmes fonctions pour les deux voies, wrappers IRQ minimaux.')

add('muse_wonder_1.0_medium', 2.7, 5.5, 'Moteur NMI non armé au démarrage', {
'Temps-réel / ISR':'ISR de capture très courtes en IRAM, buffers de timestamps et sortie NMI entière.',
'Démarrage / Boot readiness':'GPIO bas et capture avant EEPROM, NMI prévue avant réseau. Aucun signal valide au démarrage à froid avec le code fourni : l’autorisation d’interruption Timer1 n’est pas armée.',
'Génération sortie / Timer1':'Deux compteurs de reste et GPOS/GPOC ; tick idle de 5 ms, demi-périodes bornées.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2 et ratio 0,5–5 ; perte demandée après 1,2 s et retour de sortie relâché dans le moteur.',
'Filtres / Signal':'Périodes bornées 100 µs–150 ms, EMA 1/4, rejet de variation isolée et limitation de variation de 2 % par appel.',
'WiFi / Réseau':'AP+STA, mDNS et reconnexion périodique sans attente.',
'Gestion mémoire / HEAP':'HTML PROGMEM, status snprintf de 512 octets et buffers d’entrée fixes.',
'Interface Web':'Polling à 500 ms, chargement des réglages séparé.',
'Persistance / Stockage':'Configuration structurée avec version et CRC16 dans l’EEPROM, mais calcul de taille erroné.',
'Documentation hardware/code':'Schéma détaillé avec OU Schottky, protections, buck et un tach par paire.'},[
('Génération sortie / Timer1','critique','setup fait timer1_enable puis écrit seulement T1L (580–585). L’activation du bit d’interruption de front TEIE1, réalisée par timer1_write dans le cœur, manque. Au démarrage à froid, le compteur peut tourner sans déclencher la NMI : sorties figées.'),
('Persistance / Stockage','majeur','Config possède crc à l’offset 212 et sizeof=216 sur ABI 32 bits ; sizeof−2=214 inclut les deux octets du CRC (191/205). La vérification est normalement fausse après reboot ; préférer offsetof(Config, crc). Modèle de layout reproduit.'),
('Temps-réel / ISR','majeur','Les buffers de 16 timestamps (235–243) n’ont pas de détection de débordement : après exactement 16 événements sans traitement, head rejoint tail et tous paraissent consommés. À 3 000 RPM, cela arrive après seulement 160 ms de blocage.'),
('Génération sortie / Timer1','majeur','nmiProgrammed décompte la durée programmée au lieu du temps réel (317–320), donc les durées ISR s’ajoutent ; CPU_MHZ est fixé à 80 pour la capture, sans détection de la fréquence réelle.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Timeout dans loop et simulatedRpm calcule RPM×ratio (392–394) au lieu de la période réellement appliquée après slew/écrêtage. L’interface peut annoncer une vitesse que le signal ne produit pas.'),
('WiFi / Réseau','majeur','Routes de sauvegarde et reboot non authentifiées ; commit non contrôlé, absence de protection contre les sauvegardes répétées.')],
'Structures entrée/config et moteur de sortie séparés, nombreuses fonctions spécialisées.', 'Bornes et filtres présents, mais armement matériel, CRC et dépassement de ring non vérifiés.', 'Boucles par canal, deux wrappers de capture essentiellement identiques.')

add('qwen3.6', 0.9, 2.8, 'Mauvaise API de timer et schéma dangereux', {
'Temps-réel / ISR':'Les fonctions de capture portent IRAM_ATTR ; l’intention de séparer capture et sortie est visible.',
'Démarrage / Boot readiness':'Sorties forcées hautes, attente WiFi jusqu’à 15 s + pause 500 ms avant capture/Timer (471–505/874–881). En l’état, aucun signal valide car les API timer ne ciblent pas l’ESP8266.',
'WiFi / Réseau':'AP+STA, identifiants configurables et API de lecture.',
'Interface Web':'Page de réglage et affichage des RPM ; paramètres enregistrables.',
'Persistance / Stockage':'Emplacements EEPROM explicites et bornes des ratios au chargement.',
'Documentation hardware/code':'Dossier détaillé fourni, avec schémas d’étages NPN, mais erreurs d’alimentation et de raccordement.'},[
('Qualité de code générale','critique','hw_timer_t, timerBegin, timerRead, timerWrite et timerAttachInterrupt (160/368/832–839) sont des API de timer ESP32, non disponibles dans le cœur ESP8266 visé. Le sketch ne compile pas tel quel.'),
('Génération sortie / Timer1','critique','La période simulée est multipliée par le ratio (320/344), puis utilisée comme demi-période sans division par deux (392/418). Même avec un timer corrigé, le signal aurait une fréquence ≈entrée/(2×ratio), à l’opposé de l’affichage RPM×ratio.'),
('Documentation hardware/code','critique','SCHÉMA_CIRCUIT.md (47–77, 315–341) relie le rail OR ≈12 V au Wemos en supposant un AMS1117 et réutilise le pull-up Deye pour le tach d’entrée. Il faut un vrai abaisseur et séparer la mesure Noctua du tach Deye simulé ; 12 V sur 5V peut détruire la carte.'),
('Démarrage / Boot readiness','majeur','WiFi bloquant jusqu’à 15 s avant capture ; un GPIO haut maintient le NPN de sortie conducteur. Aucun signal tach pendant cette phase, donc risque d’alarme au démarrage.'),
('Génération sortie / Timer1','majeur','Quand un canal inactif atteint son échéance, nextTick est mis à UINT32_MAX (378/407) ; un retour de signal ne réarme pas cette échéance. Comparaisons non résistantes au wrap.'),
('Temps-réel / ISR','majeur','Entrée GPIO15 sans pull-up garanti et broche de boot devant rester basse ; le code ne respecte pas clairement le circuit d’entrée. Les primitives prétendument isolées ne sont pas justifiées.'),
('Qualité de code générale','mineur','Fonctions EEPROM dupliquées, routes de mutation en GET, interface entière rechargée toutes les 2 s et mots de passe transmis dans l’URL.')],
'Sections nommées mais nombreuses variantes d’écriture EEPROM et duplication des deux canaux.', 'Quelques bornes de ratios, pas de vérification cohérente des API, temporisations ou circuits.', 'Duplication importante de capture, scheduling, persistance et construction HTML.')

add('qwen3.6-35b-fp8-freetoken', 0.6, 1.9, 'Une seule entrée, sorties non implémentées correctement', {
'Temps-réel / ISR':'Les deux fonctions ISR déclarées portent IRAM_ATTR ; l’intention est de mesurer une période puis programmer une sortie.',
'Démarrage / Boot readiness':'GPIO initialisés et AP configuré ; pas de vitesse de secours. La compilation bloquée empêche toute conclusion opérationnelle.',
'Interface Web':'Page status/configuration et projet de DNS captif.',
'Persistance / Stockage':'Sauvegarde EEPROM des ratios et credentials ; une fonction auxiliaire évite de réécrire des octets identiques.',
'Documentation hardware/code':'README et circuit.h décrivent le câblage et une nomenclature, mais plusieurs connexions sont incorrectes.'},[
('Qualité de code générale','critique','EEPROM est utilisé sans include EEPROM.h ; timer1Write/timer1AttachInterrupt (341/380–381) ne sont pas les API timer1_write/timer1_attachInterrupt ; DNSServer.processNextDNSRequest (764) ne correspond pas à processNextRequest. Dépendances et appels incompatibles avec le cœur ciblé.'),
('Génération sortie / Timer1','critique','loadVal est calculé mais jamais chargé (368–381), aucune activation Timer1 n’est effectuée et le reload est zéro. currentOutputPin ne sélectionne qu’une sortie ; seul CH1 est configuré en génération (667–670), CH2 n’a pas de moteur indépendant.'),
('Fidélité au protocole tachymétrique / Fail-safe','critique','Une seule entrée GPIO13 remplace les deux canaux, les quatre tach étant mélangés selon README 106–117. rawRPM1 et rawRPM2 reçoivent la même mesure ; calculateRPMS coupe les sorties dès qu’aucune nouvelle période n’arrive dans une loop (652–689), sans timeout adapté.'),
('Documentation hardware/code','critique','README 114 et circuit.h 69 relient les cathodes des diodes OR au GND alors qu’elles constituent le rail positif : court-circuit si suivi littéralement. Le README 118–121 ajoute aussi un pull-up 10 kΩ de +12 V vers le nœud GPIO déjà tiré à GND par 10 kΩ : tension proche de 6 V, dangereuse pour l’ESP8266.'),
('Persistance / Stockage','majeur','EEPROM vierge vaut 0xFF, pas 0 : le test (224) ne reconnaît pas l’état vierge. Des float NaN deviennent les ratios. L’octet de passe maximal à 98 chevauche le premier ratio à l’offset 98.'),
('Qualité de code générale','majeur','L’interruption est attachée avec LOW (730) au lieu d’un front FALLING/RISING ; pas de validation robuste des ratios ou erreur de sauvegarde. Le README prétend à tort qu’une coupure de 30 s efface l’EEPROM.')],
'Prototypes et sections existent, mais les inclusions arrivent après des usages et le modèle des canaux est incohérent.', 'Copies bornées à plusieurs endroits, absence de validation fiable des données EEPROM.', 'Une voie est artificiellement dupliquée à l’affichage ; fonctions EEPROM partiellement inutilisées.')

add('qwen3.6_prompt_from_claude', 1.0, 2.9, 'Architecture annoncée non exécutable', {
'Temps-réel / ISR':'ISR capture IRAM, buffers de périodes et volonté de séparation réseau/génération.',
'Démarrage / Boot readiness':'LittleFS avant GPIO ; sortie initiale haute et aucun signal avant mesure. Aucun fonctionnement démontrable car erreurs de compilation et scheduler local.',
'Filtres / Signal':'Moyenne de quatre périodes, bornes de 3 à 100 Hz et IIR ; seuil anti-parasites 500 µs.',
'WiFi / Réseau':'ESPAsyncWebServer, AP/STA, mDNS et ArduinoOTA protégée par un mot de passe configuré dans le code.',
'Gestion mémoire / HEAP':'HTML PROGMEM ; réponses JSON en buffers, mais nombreuses incompatibilités de types.',
'Interface Web':'API de réglage et lectures séparées.',
'Persistance / Stockage':'LittleFS JSON et demande de sauvegarde différée.',
'Documentation hardware/code':'BOM, entrées 3,3 V et sorties NPN fournies dans SCHEMATIC.md.'},[
('Qualité de code générale','critique','Conversions de pointeurs volatile vers non-volatile dans updateTargetPeriods/checkStall (377–381/428–429), buffer const donné à une fonction non-const (386), static_cast de char* vers uint8_t* (658/767), handler HTTP_POST avec deux arguments (672/785) et restartTime utilisé hors de sa portée (1073) : plusieurs erreurs C++ certaines.'),
('Qualité de code générale','critique','LittleFS.begin(true) (453) ne correspond pas à FS::begin() du cœur ESP8266 3.1.2 ; request->getString n’est pas le mécanisme de réception de corps de l’API AsyncWebServer habituelle. Dépendances non verrouillées.'),
('Génération sortie / Timer1','critique','Ticker scheduler est local à setup (941–942) ; sa destruction détache le callback à la sortie de setup. Ticker est un timer logiciel SDK, pas Timer1/NMI. Aucun moteur de génération durable n’est établi.'),
('Documentation hardware/code','critique','SCHEMATIC.md 54/123–128 propose MT3608 comme abaisseur 12 V→5 V. La fiche AEROSEMI décrit un élévateur : ce composant ne peut pas réaliser cette alimentation, la sortie peut exposer le Wemos au rail d’entrée.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','À la perte, target est mis à zéro sans forcer la broche à l’état relâché (432–433). updateTargetPeriods recalcule la cible depuis un historique jamais vidé avant checkStall ; le scheduler peut voir des consignes transitoires périmées.'),
('Persistance / Stockage','majeur','Reboot différé après 200 ms (1066) mais sauvegarde interdite avant cooldown de 5 s (1051). Une modification faite dans les cinq premières secondes peut provoquer le reboot avant sa sauvegarde.')],
'Sections nombreuses, mais contrats de fonctions et durée de vie des objets incohérents.', 'Erreurs de JSON reconnues mais paramètres chargés sans défauts complets/validation ; nombreux types incompatibles.', 'Boucle commune au traitement des voies, ISR dupliquées et variables de contrôle redondantes.')

add('qwen3.8-UD-IQ4_XS_KV4-4', None, None, 'Non évaluable : dossier vide', {
'Démarrage / Boot readiness':'Aucun fichier de firmware ou de documentation présent dans ce dossier lors de l’inventaire du 17 novembre 2026. Aucun comportement ne peut être attribué à ce compétiteur.'},[
('Livraison','critique','Dossier présent, mais aucun fichier à analyser. Impossible d’évaluer compilation, ISR, signal, stockage, matériel ou qualité de code. Exclusion des classements chiffrés ; ne pas assimiler absence de livraison et note technique zéro.')],
'Aucun code fourni.', 'Non évaluable.', 'Non évaluable.', 'Aucun fichier ou test fourni.')

add('qwen35', 0.4, 1.2, 'Source corrompue, génération hors sujet', {
'Temps-réel / ISR':'Les deux compteurs d’entrée sont volatile et leurs handlers portent IRAM_ATTR ; seuil de 1 ms prévu.',
'Démarrage / Boot readiness':'Le texte contient un setup, mais sa portée est absorbée par un raw string non fermé au bon endroit. Aucun premier signal valide ne peut être garanti.',
'Filtres / Signal':'Intention de moyenne glissante de quatre valeurs, sans conversion correcte des compteurs en fréquence.',
'Interface Web':'Page et nombreuses fonctions de configuration présentes dans le texte fourni, sans compilation opérationnelle.',
'Documentation hardware/code':'README, index, guide et schéma sont fournis ; ils contiennent aussi des ajouts Home Assistant sans rapport avec la génération tach.'},[
('Qualité de code générale','critique','samples9[`] (142) contient un caractère parasite ; GPOS est appelé comme une fonction (170/184/194) alors que c’est un registre. WiFi.transmitting n’est pas une API du cœur ; plusieurs erreurs empêchent la compilation.'),
('Qualité de code générale','critique','Le raw string html_page ouvert à 227 n’est pas fermé avant les fonctions de setup/HTTP ; il ne trouve une fermeture correspondante qu’au bloc YAML vers 812. Des fonctions présentées comme du C++ font donc partie d’une chaîne ; le setup utile manque à l’édition de liens.'),
('Temps-réel / ISR','critique','GPIO10/11 utilisés comme entrées (33–34) sont des broches de la flash ESP-12S, pas D1/D2 ; leur usage pour le tach compromet l’exécution et n’est pas câblable comme indiqué.'),
('Fidélité au protocole tachymétrique / Fail-safe','critique','rpm_raw reçoit count cumulatif (119–132), jamais remis à zéro ni normalisé par le temps/2 PPR. La vitesse affichée augmente à rotation constante et reste positive après arrêt.'),
('Génération sortie / Timer1','critique','Un unique PIN_OUTPUT et une bascule après 500 ms dans updateSmoothedRPM (169–172), indépendante de simRPM : train d’environ 1 Hz au mieux si cet appel était périodique. Les Ticker textuels utilisent attach(250), en secondes, pas en millisecondes.'),
('Documentation hardware/code','majeur','Le schéma décrit SS8550 comme NPN (schematic.md vers 38–40), alors qu’il s’agit d’un PNP ; il ne fournit pas une topologie de sortie collecteur ouvert NPN valide. Contradictions entre documentation et broches effectives.')],
'Sections et fonctions nommées mais mélange C++, HTML et YAML corrompu.', 'Quelques bornes textuelles de ratios, aucune livraison compilable ni gestion correcte des erreurs.', 'Calculs copiés et fusionnés en une sortie ; contenu Home Assistant étranger au besoin.')

add('strata-qwen3.8-flash-next-iq3-s_medium', 0.8, 3.1, 'Timer arrêté dans son ISR et sortie jamais amorcée', {
'Temps-réel / ISR':'Volonté d’échantillonner entrées et sorties dans la même ISR IRAM, tableaux de deux canaux et données volatile.',
'Démarrage / Boot readiness':'Montage LittleFS, radio, serveur puis timer (817–849) ; aucun nominal de secours. En l’état, aucun train tach valide ne peut démarrer.',
'Filtres / Signal':'Médiane de huit valeurs et EMA entière prévues ; paramètres de filtrage nommés.',
'WiFi / Réseau':'AP+STA, DNS captif et mDNS ; WiFi.persistent(false) et sommeil désactivé.',
'Gestion mémoire / HEAP':'HTML PROGMEM et buffer status de 640 octets.',
'Interface Web':'Contrôles des deux canaux, mode simulation/bypass et lecture status.',
'Persistance / Stockage':'LittleFS avec deux emplacements et sauvegarde différée.',
'Documentation hardware/code':'README très détaillé avec alimentation OU Schottky/buck, nomenclature et intention de chemin de secours.'},[
('Qualité de code générale','critique','GPO_SET_SET/GPO_SET_CLEAR, GPIO_INPUT_REG(pin) et timer1_init (305/394/846) ne sont pas les primitives définies par le cœur ESP8266 standard ; aucune définition locale ne les fournit. Compilation bloquée.'),
('Génération sortie / Timer1','critique','tickISR appelle timer1_disable puis timer1_write sans réactiver T1C (374–375). Après la première interruption, le compteur reste désactivé. De plus, 800 ticks DIV16 =160 µs, pas TICK_US=50 : facteur 3,2 sur la base de temps.'),
('Génération sortie / Timer1','critique','En mode SIM, outNext reste à zéro à l’initialisation ; outService ne produit un front que si outNext vaut 1 après décrément (341–359). Aucun code ne lance la première échéance en mode SIM : sortie figée.'),
('Fidélité au protocole tachymétrique / Fail-safe','critique','Toutes les périodes <6 000 ticks sont classées « trappe » (428–433), soit les vitesses >100 RPM avec un tick de 50 µs. Les Noctua nominaux sont donc rejetés après huit périodes ; le flag trapped n’est pas réarmé.'),
('Génération sortie / Timer1','majeur','La cible (487) multiplie e, déjà en ticks, par TICK_US puis la traite encore comme ticks. Le facteur 50 et l’addition de largeur de pulse faussent la fréquence, indépendamment du ratio demandé.'),
('Persistance / Stockage','majeur','Config : crc à 220 et sizeof=224, donc sizeof−2=222 inclut le CRC (226/234/251). Les deux fichiers reçoivent la même valeur sans version temporelle, rename transactionnel ni retour d’écriture contrôlé. Modèle de layout reproduit.'),
('WiFi / Réseau','majeur','AP ouvert par défaut et routes de contrôle/sauvegarde sans authentification. inputLost est seulement indiqué mais n’interdit pas outService : maintien indéfini d’une ancienne fréquence si le moteur était réparé.')],
'Structures de configuration et état par canal, mais moteur complexe et unités mélangées.', 'Bornes de ratios prévues ; tests de trappe, armement et tailles CRC non vérifiés.', 'Boucles par canal et médiane commune, nombreux modes aux interactions non testées.')

add('strata-qwen3.8-flash-next-iq3-s_medium-cline', 1.7, 4.1, 'Défauts de compilation et fréquence NCO incorrecte', {
'Temps-réel / ISR':'Entrée échantillonnée et sorties dans la même ISR ; données partagées volatile et état runtime séparé.',
'Démarrage / Boot readiness':'GPIO puis pause 60 ms, LittleFS et moteur avant WiFi (926–986). Intention de plancher 600 RPM avant mesure, mais code non compilable et pas de validation physique de ce mode.',
'Génération sortie / Timer1':'DIV16 chargé à 125 ticks : pas de 25 µs, 40 kHz ; accumulateur de phase entier et limite RPM_MAX=20 000.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR entrée/sortie configurables, ratios, offsets, mode test et maintien optionnel après perte ; valeurs bornées dans les handlers.',
'Filtres / Signal':'Quatre échantillons concordants (100 µs) et moyenne des périodes sur fenêtre 400 ms.',
'WiFi / Réseau':'AP+STA, sommeil désactivé, mDNS et reprise station sans attendre la connexion.',
'Gestion mémoire / HEAP':'HTML PROGMEM, données temps réel fixes et String status réservé.',
'Interface Web':'Status détaillé, réglages de canaux et mode test séparés.',
'Persistance / Stockage':'LittleFS texte et valeurs par défaut déjà initialisées ; retour booléen de sauvegarde exploité.',
'Documentation hardware/code':'Commentaires embarqués détaillés ; aucun schéma annexe autonome fourni.'},[
('Qualité de code générale','critique','TICK_US est utilisé dans handleStatus/setup (781/945–947) sans définition. LittleFS.begin(true) (953) ne correspond pas à FS::begin() du cœur 3.1.2. Deux erreurs certaines bloquent le build standard.'),
('Génération sortie / Timer1','critique','La phase ne progresse pas tant que pulseLeft est non nul (346–365). La période effective = durée d’accumulation + largeur d’impulsion, alors que outConfigure ne soustrait pas cette largeur. Pour 7 500 RPM, 2 PPR : inc=409, 161 ticks d’accumulation +53 ticks de pulse donnent ≈5 607 RPM, soit −25,2 %.'),
('Fidélité au protocole tachymétrique / Fail-safe','critique','Le test `if (!alive)` (344) regarde le masque global, pas le bit du canal. Si le canal 0 tourne, un canal 1 perdu peut continuer à émettre ; le bloc de perte (418–421) ne fait pas outKill, ce qui entretient la dernière fréquence.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Soustraction d’offset en uint32_t (398) : 1 000−3 000 sous-déborde puis est plafonné à 20 000 RPM (425), au lieu de zéro/plancher. Au démarrage age=0 rend aussi des entrées sans front temporairement « vivantes » et peut inventer 600 RPM.'),
('Temps-réel / ISR','majeur','Calculs de division uint64_t et génération à 40 kHz dans une IRQ normale. Pas de budget d’exécution/qualification IRAM des helpers produits par le compilateur ; les attributs IRAM sur des tableaux de données (289–290) exigent aussi vérification de section.'),
('WiFi / Réseau','majeur','Mutations via GET sans authentification (989–992), notamment factory reset. Le mode test est sauvegardé partiellement : testMode n’est pas écrit/rechargé dans config.txt.')],
'AppConfig, commandes et état st distincts ; helpers de parsing et handlers regroupés.', 'Bornage et retours FS présents ; calcul signé/non signé et sélection de canal erronés.', 'Moteur commun par canal ; logique de modes dense et publication de télémétrie redondante.')

add('Swift_1.5_Qwen3.8-27B_medium-IQ3S', 0.8, 2.5, 'API réseau et timer incompatibles', {
'Démarrage / Boot readiness':'GPIO au repos et chargement de paramètres prévus avant le réseau ; l’absence de build valide empêche de mesurer un premier tach exploitable.',
'Filtres / Signal':'Intention d’EMA de vitesse 1/10 et traitement toutes les 10 ms.',
'WiFi / Réseau':'Intention AP+STA et paramètres configurables dans une page HTTP.',
'Interface Web':'Page de mesures et réglages de ratios/WiFi ; statut JSON prévu.',
'Persistance / Stockage':'Intention de stockage Preferences avec défauts de ratios.',
'Documentation hardware/code':'Schéma texte fourni avec OU de diodes et sorties NPN, mais alimentation erronée.'},[
('Qualité de code générale','critique','server est déclaré WiFiServer (149) puis utilisé comme ESP8266WebServer : send/on/hasArg/arg/handleClient (216–302) ne sont pas ses méthodes. ESP8266HTTPServer.h et Preferences.h ne sont pas des dépendances du cœur ESP8266 standard annoncé.'),
('Génération sortie / Timer1','critique','Timer0 est programmé une seule fois avec 100 (267–269), sans réarmement dans onTick, et timer0_enable n’est pas l’API exposée par le cœur. Le code suppose une base en microsecondes alors que Timer0 compare des cycles absolus.'),
('Temps-réel / ISR','critique','onTick/onIn1/onIn2 ne sont pas IRAM_ATTR, Channel n’est pas volatile malgré des champs partagés entre ISR et loop. digitalWriteFast et WiFi.connect ne sont pas fournis par la livraison.'),
('Fidélité au protocole tachymétrique / Fail-safe','critique','Le timeout est dans un else-if déclenché seulement si p est hors plage (312–318). Après une mesure valide, p reste valide pour toujours : un ventilateur arrêté continue à être annoncé actif et sa fréquence reste émise.'),
('Génération sortie / Timer1','majeur','Le scheduler ajoute pulseWidth à une attente égale à la période complète, avec deux ticks supplémentaires de countdown. La fréquence réelle serait inférieure à la consigne même avec le timer réparé.'),
('Documentation hardware/code','critique','schema_electronique.txt 28–34 place 100 kΩ en série avant le LDO depuis 12 V, limitant le courant à <120 µA ; impossible d’alimenter un ESP8266 consommant des dizaines de mA. Le schéma propose aussi AP2112 directement sur 12 V, hors de sa plage d’entrée.'),
('WiFi / Réseau','majeur','Modifications en GET, mots de passe dans l’URL, absence d’authentification et rechargement complet chaque seconde, qui gêne la saisie.')],
'Channel regroupe les données mais mélange capture, commande et génération sans contrat de partage.', 'Quelques bornes HTTP, aucun contrôle cohérent de disponibilité des API ou de vieillissement.', 'Deux ISR similaires ; boucle commune pour les calculs et la sortie.')

add('claude opus 5 medium', 7.4, 7.7, 'Moteur NMI convaincant, supervision à renforcer', {
'Temps-réel / ISR':'`fanEngineIsr` (373–425) échantillonne les deux entrées toutes les 40 µs en NMI ; les fonctions critiques sont en IRAM et les échanges utilisent des mots atomiques et un seqlock.',
'Démarrage / Boot readiness':'`setup` (1087–1119) charge la configuration avant `engineBegin`, puis démarre le réseau sans attente de connexion. Sorties relâchées, sans RPM de remplacement. Deux fronts valides sont nécessaires : à 1 600 RPM, environ 18,75–37,5 ms après activation de la capture, plus le calcul périodique de 20 ms et le setup non chronométré.',
'Génération sortie / Timer1':'Échéances absolues en cycles, GPOS/GPOC et conversion explicite cycles/ticks à 80 ou 160 MHz. Rattrapage borné à 16 fronts par passage (340–367).',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR entrée/sortie séparés, défaut 2 ; ratio entier au millième ; plafonnement configurable. `updateChannel` (683–739) applique RPM × ratio et arrête sur absence de signal.',
'Filtres / Signal':'Trois échantillons concordants (120 µs), périodes de 2 à 200 ms, EMA 1/8 et compteur de rejets.',
'WiFi / Réseau':'AP+STA sans sommeil, reconnexion à 30 s et mDNS. Routes HTTP de lecture et de configuration séparées.',
'Gestion mémoire / HEAP':'HTML en PROGMEM, petits blocs `snprintf` et réservation du String JSON ; aucune allocation dans le moteur.',
'Interface Web':'Polling séparé des formulaires ; télémétrie des fronts, rejets, écrêtage et activité NMI.',
'Persistance / Stockage':'Signature, version et CRC32 ; bornage des ratios/PPR au chargement (631–660).',
'Documentation hardware/code':'SCHEMA.md décrit OU Schottky + buck 5 V, entrées filtrées, sorties NPN et un seul tach par paire ; calculs et procédure de mesure fournis.'},[
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Le timeout de 1 200 ms est évalué dans `updateChannel`, appelé après HTTP dans `loop` (1122–1135). Une requête bloquante retarde l’arrêt d’autant ; le moteur NMI continue la dernière consigne sans contrôler lui-même la perte d’entrée.'),
('WiFi / Réseau','majeur','Aucune authentification des routes (1030–1039) et `/api/status` renvoie `apPass` (927). Un client réseau peut lire le secret AP, modifier les ratios ou redémarrer le module.'),
('Qualité de code générale','majeur','Les SSID et mots de passe sont concaténés sans échappement JSON (926–931). Un SSID contenant un guillemet casse `/api/status` et son polling.'),
('Documentation hardware/code','mineur','SCHEMA.md (270–302) déduit la saturation jusqu’à 85–120 mA d’un hFE minimal. Avec Ib ≈ 1,16 mA, un gain forcé de 10 ne garantit qu’environ 11,6 mA ; le dimensionnement doit être vérifié avec le vrai pull-up.'),
('Persistance / Stockage','mineur','`configSave` ignore le résultat de `EEPROM.commit` (627), tout en annonçant un enregistrement réussi. Aucun fichier de secours ni limitation des sauvegardes répétées.')],
'Types InputChannel/OutputChannel/Config distincts et fonctions courtes pour le moteur, le stockage et le réseau.', 'Bornage défensif des paramètres et CRC32, mais erreurs de sauvegarde non remontées.', 'Boucles à deux canaux et helpers GPIO partagés.')

add('claude-sonnet5-free', 4.1, 6.0, 'Firmware lisible, entrée matérielle incomplète', {
'Temps-réel / ISR':'Capture GPIO et Timer1 annotés ICACHE_RAM_ATTR ; accès GPOS/GPOC, instantané protégé par noInterrupts (188–257).',
'Démarrage / Boot readiness':'Timer1 activé après EEPROM et avant WiFi ; aucune attente STA. Sortie inactive jusqu’au traitement, toutes les 150 ms. La première période utilise toutefois un horodatage initial nul.',
'Génération sortie / Timer1':'Timer DIV16 chargé à 500 : cadence de 100 µs, deux compteurs indépendants.',
'Fidélité au protocole tachymétrique / Fail-safe':'Formule RPM = 30 000 000 / période et consigne divisée par le ratio ; arrêt logiciel après 3 s sans front.',
'Filtres / Signal':'Rejet des intervalles inférieurs à 2 ms ; instantané atomique des timestamps.',
'WiFi / Réseau':'AP+STA, WIFI_NONE_SLEEP et CPU réglé à 160 MHz ; connexion STA non bloquante.',
'Gestion mémoire / HEAP':'HTML PROGMEM, réponses status/config dans des buffers de 220/256 octets.',
'Interface Web':'Actualisation des seules mesures toutes les secondes, formulaires chargés séparément.',
'Persistance / Stockage':'Configuration structurée, signature et checksum pondérée ; défauts en cas d’invalidité.',
'Documentation hardware/code':'Schéma des deux voies, sorties NPN et alimentation OU Schottky + buck.'},[
('Documentation hardware/code','critique','Le circuit d’entrée (schema_electronique.md 108–124) suppose un pull-up 12 V interne au Noctua mais n’en fournit aucun sur son tach : la résistance 4,7 kΩ est au collecteur du transistor tampon. Avec une sortie Noctua collecteur ouvert sans pull-up, la base ne reçoit aucun niveau haut garanti et la capture peut rester à zéro.'),
('Génération sortie / Timer1','majeur','Le compteur est rechargé à N puis décrémenté N fois avant le prochain toggle (220–230) : demi-période réelle (N+1)×100 µs. À N=10, plafond affiché 15 000 RPM mais réel 13 636 RPM (−9,09 %).'),
('Temps-réel / ISR','majeur','`timer1_attachInterrupt` fournit une interruption masquable, pas une NMI ; les ticks perdus pendant les sections critiques ne sont pas compensés. Le timeout de 3 s dépend également de loop.'),
('Fidélité au protocole tachymétrique / Fail-safe','mineur','Le premier front est interprété comme une période depuis le démarrage (190–195). La première valeur est fausse et le délai d’arrêt atteint 3 s + 150 ms hors blocage.'),
('Qualité de code générale','majeur','JSON des SSID sans échappement (432–433), routes sans authentification et `EEPROM.commit` ignoré. Un guillemet dans le SSID invalide la réponse ; une erreur de stockage reste invisible.')],
'Fonctions capture, sortie, stockage et HTTP clairement séparées.', 'Validation des ratios web, mais pas de contrôle de finitude/chaînes après chargement.', 'Tableaux communs pour les deux voies, seuls les wrappers ISR sont dupliqués.')

add('Claude_Opus_5.5_xhigh', 8.6, 8.5, 'Meilleure base temps réel, validation physique requise', {
'Temps-réel / ISR':'`tachNmi` (433–470) porte l’échantillonnage, le calcul de cible, la génération et le timeout. Helpers forcés inline, arithmétique Q16 et données séparées E/S ; pas de flottant ni appel réseau dans la NMI.',
'Démarrage / Boot readiness':'GPIO relâchés dès 1233–1237, EEPROM puis `engineStart` avant WiFi (1247–1260). Aucune vitesse inventée avant deux fronts. À 1 600 RPM, acquisition typique 18,75–37,5 ms après activation, avec validation numérique de 150 µs et amorçage de sortie d’environ 10 µs ; durée power-on non mesurée.',
'Génération sortie / Timer1':'Échéances absolues en cycles, anticipation de 8 µs et attente active bornée avant le front ; les rapports 80/160 MHz sont pris en compte. Diagnostics de durée NMI et retard maximum.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2 des deux côtés, ratio 0,25–8 ; délai de perte 500 ms géré dans le moteur, arrêt à une frontière de sortie (368–428). Reprise avec capture réamorcée.',
'Filtres / Signal':'Trois échantillons à 50 µs, rejet des fronts <2 ms, moyenne adaptative de 1/2/4/8 périodes puis IIR 1/4.',
'WiFi / Réseau':'AP permanent, machine d’état STA, sommeil désactivé, scan asynchrone, DNS captif, mDNS et mise à jour HTTP avec mot de passe. Authentification administrateur configurable.',
'Gestion mémoire / HEAP':'HTML PROGMEM ; moteur sans allocation ; String status réservé à 1 400 octets, échappement JSON explicite.',
'Interface Web':'Polling 500 ms avec garde busy, configuration chargée séparément, aperçu du ratio, diagnostics et boutons explicites.',
'Persistance / Stockage':'Signature, version, longueur, CRC32 par offsetof, contrôle des bornes et terminaison des chaînes ; static_assert sur la taille EEPROM.',
'Documentation hardware/code':'README/SCHEMA distinguent hypothèses et essais déclarés, détaillent alimentation, sorties NPN, masses communes et un seul tach par paire. Les essais annoncés par le compétiteur n’ont pas été reproduits ici.'},[
('Filtres / Signal','mineur','Le cumul moyenne de huit périodes + IIR 1/4 (334–345) ralentit les transitions. À 1 600 RPM, la fenêtre couvre 150 ms et la constante IIR est de l’ordre de 65 ms ; l’établissement prend plusieurs centaines de ms.'),
('WiFi / Réseau','majeur','`authOK` retourne true quand adminPass est vide (850–854), état par défaut. Les clients du LAN peuvent alors configurer, réinitialiser ou redémarrer le contrôleur. L’authentification optionnelle doit être activée avant déploiement.'),
('Persistance / Stockage','mineur','`EEPROM.commit` n’est pas contrôlé (591–597) ; pas de copie transactionnelle ni de réduction des sauvegardes répétées.'),
('Qualité de code générale','mineur','`parseRatio` utilise String.toFloat (963–970), qui accepte un préfixe numérique suivi de texte ; par exemple « 2abc » est traité comme 2. La plage est validée, pas la syntaxe complète.'),
('Temps-réel / ISR','mineur','Architecture NMI complexe et polling 20 kHz ; les compteurs internes mesurent le retard logiciel, pas la forme électrique au collecteur. Aucun binaire, map IRAM ou essai oscilloscope reproduit dans cette réanalyse : les chiffres métrologiques du README restent des déclarations.')],
'Engine, Shared, Config et machine d’état STA séparent clairement responsabilités et propriété des données.', 'CRC, chaînes terminées et mises à jour de configuration par candidat temporaire ; limites sur commit et parsing.', 'Canaux génériques, helpers numériques partagés et fonctions de service séparées.',
'mulQ16, CRC et conversion de ratios sont isolables. Le README annonce des simulations, mais leurs scripts de test ne sont pas livrés ; le script Python fourni génère le schéma, pas un banc du moteur.')

add('composer 2.5', 1.4, 3.8, 'Non compilable et alimentation à corriger', {
'Temps-réel / ISR':'ISR marquées IRAM_ATTR, tableaux TachChannel et bornes de période explicites.',
'Démarrage / Boot readiness':'Intention : sorties basses, capture puis EEPROM, Timer1 avant WiFi sans attente STA. En l’état, aucun premier signal exploitable puisque la compilation est bloquée.',
'Génération sortie / Timer1':'Architecture à compteurs par canal et fonctions GPIO dédiées.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2 et division de période par ratio 1–6 ; arrêt demandé après 3 s dans loop.',
'WiFi / Réseau':'AP+STA avec connexion non bloquante, WiFi.persistent(false).',
'Persistance / Stockage':'Structure Config et CRC16, paramètres conservés dans EEPROM.',
'Documentation hardware/code':'Schémas et BOM fournis ; sorties NPN collecteur ouvert correctement identifiées.'},[
('Qualité de code générale','critique','`EEPROM_MAGIC 0xDEYE` (51) contient Y, interdit dans un entier hexadécimal. `loadConfig` l’utilise : le firmware ne peut pas être compilé tel quel.'),
('Documentation hardware/code','critique','schema_electronique.md (62–68) propose le rail ≈12 V directement sur la broche 5V et invoque un AMS1117. Le D1 mini visé exige une alimentation 5 V régulée ; ce câblage peut détruire la carte. Ajouter un abaisseur est obligatoire, pas optionnel au-delà de 12 V.'),
('Génération sortie / Timer1','critique','`timer1_write(TIMER_TICK_US)` (411) charge 10 ticks DIV16, donc 2 µs au lieu de 10 µs. Après correction des erreurs de compilation, la fréquence serait multipliée par cinq et la charge ISR atteindrait 500 kHz.'),
('Temps-réel / ISR','majeur','`handleTachEdge` (167–175) divise en flottant dans l’ISR ; le ratio partagé n’est pas volatile. Les délais ISR et la visibilité des mises à jour demandent correction et vérification du binaire.'),
('Interface Web','mineur','Rechargement complet toutes les 2 s (291), donc perte du focus et des valeurs en cours de saisie.'),
('Qualité de code générale','majeur','SSID et mot de passe sont insérés sans échappement dans des attributs HTML (332–337), sans authentification. Un apostrophe peut sortir de l’attribut et injecter du HTML.')],
'TachChannel et Config identifiables, fonctions dédiées mais page construite par concaténations.', 'Ratios web bornés ; compilation, unités et alimentation non vérifiées.', 'Traitement générique de deux canaux, wrappers ISR minimaux.')

add('composer 2.5 prompt from claude', 5.5, 5.8, 'Base fonctionnelle, robustesse réseau et flash insuffisante', {
'Temps-réel / ISR':'ISR de capture en IRAM et instantanés sous noInterrupts ; calculs flottants réalisés hors ISR.',
'Démarrage / Boot readiness':'GPIO inactifs, montage LittleFS puis capture/Timer1 avant WiFi (461–493). Pas de nominal artificiel ; deux fronts puis premier traitement loop nécessaires. Formatage automatique éventuel avant capture : délai non borné par le code.',
'Génération sortie / Timer1':'Tick DIV16 de 250 ticks = 50 µs ; sorties indépendantes, nouvelle échéance après chaque toggle.',
'Fidélité au protocole tachymétrique / Fail-safe':'RPM=30 000 000/période, ratio 1–4 et arrêt après 1,5 s sans front, ligne relâchée dans le traitement (241–250).',
'Filtres / Signal':'Périodes 2,5–500 ms, moyenne de quatre valeurs et limitation de variation de 15 % par appel.',
'WiFi / Réseau':'ESPAsyncWebServer, AP+STA et machine de connexion sans attente bloquante.',
'Gestion mémoire / HEAP':'HTML PROGMEM, documents JSON statiques ; réponse sérialisée dans un String.',
'Interface Web':'Polling status séparé des formulaires, sauvegarde de ratios et WiFi par POST.',
'Persistance / Stockage':'LittleFS et JSON avec retour aux défauts après erreur de lecture/désérialisation.',
'Documentation hardware/code':'OU Schottky + buck 5 V, pull-up d’entrée à 3,3 V et sorties BC547 avec résistance de base dimensionnée.'},[
('Qualité de code générale','majeur','Les strncpy (150–153 et 440–449) utilisent toute la capacité, sans forcer le NUL final. Un SSID de 33 octets ou un mot de passe de 65 octets produit une chaîne non terminée et une lecture hors tampon au stockage/WiFi.'),
('Temps-réel / ISR','majeur','Timer1 masquable : les écritures LittleFS peuvent retarder les fronts et les entrées. DigitalWrite et échéances `now + half` (112–113) ne compensent ni le retard ni l’arrondi au tick ; biais de période jusqu’à presque 100 µs, hors latences supplémentaires.'),
('Filtres / Signal','mineur','La moyenne et le slew sont recalculés à chaque loop, pas à chaque nouvel échantillon ; g_edgeCount n’est jamais consommé. Les quatre cases peuvent contenir quatre copies de la même mesure, la vitesse du filtre dépend de la charge réseau.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','L’horodatage g_lastEdgeUs est actualisé avant validation (79–87). Un bruit continu hors plage peut conserver une ancienne mesure active et repousser le timeout.'),
('WiFi / Réseau','majeur','Pas d’authentification ; handler asynchrone WiFi avec delay(300) puis ESP.restart (451–454), sans report dans loop. Risque de réponse interrompue ou de blocage du contexte réseau.'),
('Persistance / Stockage','mineur','Réécriture directe de config.json, sans rename ni sauvegarde ; les retours saveConfig ne sont pas propagés. Une coupure pendant sauvegarde peut perdre toute la configuration.')],
'Séparation capture, filtrage, stockage et handlers HTTP ; globals nombreuses.', 'JSON invalide géré, mais chaînes, erreurs d’écriture et données non finies à mieux contrôler.', 'Boucles par canal ; prise d’instantané explicitement dupliquée pour les deux voies.')

add('deepseek4 flash', 1.8, 4.0, 'Accès registres erronés, aucune génération fiable', {
'Temps-réel / ISR':'Intention NMI en IRAM ; calculs Q16 hors ISR et écritures GPIO W1TS/W1TC.',
'Démarrage / Boot readiness':'Initialisation annoncée avant WiFi ; GPIO bas et consignes nulles. Aucun délai de premier signal valide ne peut être garanti à cause des défauts Timer1.',
'Fidélité au protocole tachymétrique / Fail-safe':'Formules à deux impulsions par tour, ratios entiers et délai de perte adaptatif max(2 s, 4 périodes filtrées).',
'Filtres / Signal':'EMA 1/8 et bornes de périodes, mais la limite haute de 120 s est beaucoup trop large pour un ventilateur nominal.',
'WiFi / Réseau':'AP+STA, mDNS et reconnexion toutes les 20 s.',
'Gestion mémoire / HEAP':'HTML PROGMEM, buffer status global de 640 octets et snprintf borné.',
'Interface Web':'Télémétrie et réglages séparés, polling annoncé à 1,5 s.',
'Persistance / Stockage':'Signature et stockage structuré, chaînes explicitement terminées au chargement.',
'Documentation hardware/code':'README et schéma détaillés, buck et sorties collecteur ouvert.'},[
('Génération sortie / Timer1','critique','FRC1_CTRL_DIV_16 est défini bit 8 et ENABLE bit 0 (105–108), alors que le cœur ESP8266 utilise TCPD=2 et TCTE=7. `initTimer` écrit donc une configuration qui n’active pas le compteur ; il écrit aussi le registre COUNT en lecture seule (509–519).'),
('Qualité de code générale','critique','`g_snap` est un volatile SnapT (141), mais `handleState` fait `s = g_snap` (416). L’opérateur de copie implicite d’une structure C++ ne prend pas volatile const SnapT& : conversion interdite dans un build C++ standard.'),
('Génération sortie / Timer1','majeur','Le temps virtuel avance seulement de g_lastDelta (155) et ignore le temps réel de traitement/retard. La précision de 0,2 µs annoncée n’est donc pas démontrée, même après correction du registre.'),
('Persistance / Stockage','majeur','CfgT est aligné sur deux octets : crc à 206, sizeof=208. La somme sur sizeof−1=207 inclut le checksum lui-même (256/278) ; la relecture ne valide pas normalement les réglages sauvegardés. Calcul reproduit par modèle de layout.'),
('WiFi / Réseau','majeur','`/api/state` publie les deux mots de passe en clair (438–439), sans authentification et sans échappement JSON. Secrets lisibles par tout client joignable et JSON invalide avec guillemets.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Mettre g_halfTicks à zéro au timeout (572) ne relâche pas nécessairement le GPIO dans la NMI : son niveau peut rester haut, donc transistor conducteur. La perte est traitée dans loop et peut être retardée par HTTP.')],
'Types Config/Snap et fonctions réseau distincts ; bas niveau développé indépendamment des définitions du cœur.', 'Bornage de ratios et de buffers, mais erreurs de registres et copie volatile bloquantes.', 'Deux ISR d’entrée copiées ; sortie et calculs regroupés par canal.')

add('gemini 3.1 pro', 2.0, 3.5, 'Compilation bloquée, sécurité mémoire insuffisante', {
'Temps-réel / ISR':'Compteurs d’entrée et ISR Timer1 en IRAM, instantané des compteurs sous section critique.',
'Démarrage / Boot readiness':'Timer1 prévu avant WiFi, sorties au repos ; premier calcul après 1 s de loop. En l’état, pas de binaire compilable.',
'Génération sortie / Timer1':'DIV16 chargé à 250 = 50 µs, compteurs de demi-période par canal ; conversion `300000/rpmOut` cohérente avec 2 PPR.',
'WiFi / Réseau':'AP+STA et démarrage non bloquant de la station.',
'Gestion mémoire / HEAP':'HTML source en PROGMEM, mais copie dans String à chaque page.',
'Interface Web':'Routes status et save distinctes ; mesures mises à jour sans recompiler.',
'Persistance / Stockage':'Paramètres regroupés en Config et signature EEPROM.',
'Documentation hardware/code':'Schéma d’entrées/sorties et proposition d’un buck 5 V.'},[
('Qualité de code générale','critique','EEPROM_MAGIC=0xDEYE0001 (8) contient Y : entier hexadécimal invalide. Toute comparaison/affectation de cette macro empêche la compilation.'),
('Qualité de code générale','critique','`saveConfig` copie les arguments HTTP non bornés par strcpy dans ssid[32] et pass[64] (161–163). Une requête de plus de 31/63 caractères écrase les champs adjacents en RAM.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Le comptage par fenêtre utilise p×30 sans diviser par la durée réelle écoulée (261–275). Si HTTP retarde une fenêtre à 1,5 s, la vitesse est surestimée de 50 % ; aucune protection contre le bruit et aucune borne supérieure des ratios côté serveur.'),
('Temps-réel / ISR','majeur','La documentation parle de NMI-like et de zéro jitter, mais le code utilise une interruption Timer1 ordinaire. Les ticks perdus ne sont pas compensés et la sortie attend le calcul à 1 Hz.'),
('Persistance / Stockage','majeur','Seule la signature protège l’EEPROM ; ratios et chaînes ne sont pas validés au chargement. Une configuration partiellement corrompue peut déclencher lectures hors tampon ou conversions invalides.'),
('WiFi / Réseau','majeur','Aucune authentification ; la page HTML réinsère SSID et mot de passe sans échappement (174–176), exposant le secret et une injection HTML.')],
'Petites fonctions dédiées mais état global et traitements dupliqués par voie.', 'Validation serveur limitée à une borne basse des ratios ; aucune protection strcpy.', 'Compteurs, sorties et calculs copiés entre les deux voies.')

add('gemini-free', 3.4, 4.4, 'Erreur Timer1 ×16, validation des ratios absente', {
'Temps-réel / ISR':'ISR annotées IRAM et horodatage par micros ; callbacks réseau asynchrones.',
'Démarrage / Boot readiness':'GPIO bas, LittleFS et WiFi avant la capture et Timer1 (193–222). Aucune attente explicite STA ; premier signal après une mesure et jusqu’à 50 ms de loop. Pas de valeur nominale de secours.',
'Fidélité au protocole tachymétrique / Fail-safe':'Mesure RPM=30 000 000/période et demi-période demandée=15 000 000/RPM ; timeout logiciel de 1 s.',
'Filtres / Signal':'Rejet des intervalles ≤5 ms, sans lissage.',
'WiFi / Réseau':'ESPAsyncTCP/ESPAsyncWebServer, AP+STA permanent.',
'Gestion mémoire / HEAP':'HTML déclaré PROGMEM et JSON statique sérialisé vers String.',
'Interface Web':'Routes data/config séparées des réglages.',
'Persistance / Stockage':'LittleFS JSON, tailles de copies bornées par strlcpy.'},[
('Génération sortie / Timer1','critique','onTimerISR annonce un pas de 50 µs mais recharge 80×50=4 000 ticks en DIV16 (91/221), soit 800 µs. La fréquence réelle est environ 16 fois trop faible : une consigne de 7 500 RPM donne ≈468,75 RPM hors surcoût ISR.'),
('Qualité de code générale','majeur','Les ratios HTTP sont acceptés sans bornes ni isfinite (251–252), idem au chargement. Un ratio nul/négatif ne réinitialise pas la précédente target si le test rpmOut>0 échoue (281–284), laissant potentiellement une ancienne fréquence active.'),
('Temps-réel / ISR','majeur','Interruption Timer1 masquable et digitalWrite à chaque bascule ; aucune compensation des pauses flash. Le timeout est géré dans loop avec delay(50).'),
('Fidélité au protocole tachymétrique / Fail-safe','mineur','Premier front mesuré depuis micros=0 ; les paires timestamp/période sont lues et parfois écrasées dans loop sans instantané atomique cohérent.'),
('Persistance / Stockage','majeur','Montage LittleFS non vérifié, sauvegarde directe sans backup ni résultat propagé. Un filesystem indisponible rend la configuration non persistante sans erreur web.'),
('WiFi / Réseau','majeur','Routes sans authentification ; delay(1000) et reboot dans le callback asynchrone WiFi (261–263). Le contexte réseau peut être bloqué et la réponse tronquée.')],
'Fonctions stockage, capture et handlers séparées, mais globals dispersées.', 'Copies de chaînes bornées ; ratios et montage FS non sécurisés.', 'Traitement intégralement dupliqué pour les deux voies.')

add('Gemini_4_Argon_medium', 4.9, 6.8, 'Moteur avancé, perte de signal et alimentation fragiles', {
'Temps-réel / ISR':'Sorties pilotées par NMI, GPOS/GPOC et calendrier en cycles ; capture GPIO IRAM, timeout contrôlé dans la NMI (222–307).',
'Démarrage / Boot readiness':'Sorties basses, EEPROM puis init moteur avant WiFi (1155–1177). Capture initialisée par un premier front puis mesure au second ; délai d’acquisition de 10–20 ms à 3 000 RPM et 18,75–37,5 ms à 1 600 RPM, hors setup non mesuré.',
'Génération sortie / Timer1':'DIV1 et prise en compte du mode CPU turbo ; anticipation de 2 µs, échéances accumulées et correction de grand retard.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2, ratios 0,1–10 ; demi-période minimale 500 µs, soit plafond 30 000 RPM ; perte nominale après 500 ms.',
'Filtres / Signal':'Rejet des fronts <2,5 ms et moyenne de quatre périodes, sans flottant dans la NMI.',
'WiFi / Réseau':'AP+STA, sommeil désactivé et reconnexion à 15 s, paramètres modifiables sans reboot.',
'Gestion mémoire / HEAP':'HTML PROGMEM, status construit dans un buffer de 768 octets, chaînes JSON partiellement échappées.',
'Interface Web':'SPA et polling à 500 ms, état des deux canaux et paramètres WiFi.',
'Persistance / Stockage':'Signature, version, CRC32 par offsetof ; ratios/chaînes contrôlés au chargement.',
'Documentation hardware/code':'Documentation détaillée des étages NPN et séparation des tach des deux ventilateurs d’une paire.'},[
('Fidélité au protocole tachymétrique / Fail-safe','critique','La NMI stoppe la sortie au timeout mais ne remet pas hasSignal ni targetHalfCycles à zéro (243–249). Sans nouvel événement, l’âge en cycles déborde : après 53,687 s à 80 MHz ou 26,844 s à 160 MHz, il redevient inférieur à 500 ms et réactive une ancienne consigne pendant une fenêtre d’environ 500 ms.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Au premier front après un arrêt, processTachEdgeIRAM réinitialise le ring mais conserve measuredPeriodCycles/targetHalfCycles (328–334). L’ancienne vitesse peut être émise avant la seconde mesure valide.'),
('Documentation hardware/code','majeur','Alimentation maison à BC337 et résistance de 33 Ω (SCHEMA_ET_ARCHITECTURE.md 25–43). À 300 mA, la résistance chute de 9,9 V : il ne reste que 2,1 V depuis 12 V, insuffisant pour 4,4 V. Sans dimensionnement transitoire probant, les pics WiFi peuvent faire redémarrer le module.'),
('Temps-réel / ISR','majeur','La capture reste une IRQ GPIO masquable et peut être retardée pendant la flash. De plus, GPIO ISR et NMI modifient T1L (370–371 et 307), ce qui exige un protocole de programmation vérifié sous préemption.'),
('Qualité de code générale','mineur','escapeJsonString ne traite pas les caractères de contrôle (938–947), donc un SSID avec retour ligne invalide le JSON ; les mutations WiFi sont partielles avant validation complète.'),
('WiFi / Réseau','majeur','Routes de réglage/reset sans authentification et retour de sauvegarde ignoré dans les handlers.')],
'Structures temps réel séparées, helpers de conversion et stockage dédiés.', 'CRC et bornage corrects, mais réamorçage du signal incomplet et mutations WiFi non transactionnelles.', 'Traitements génériques des canaux, petits wrappers ISR.')

add('gemma4', 0.5, 1.3, 'Livraison inutilisable pour le projet', {
'Démarrage / Boot readiness':'Le texte prévoit AP+STA et des Ticker, mais aucune sortie tach valide ne peut être produite en l’état.',
'Interface Web':'Page de visualisation avec fetch /data ; le polling ne remplace que les mesures.',
'Documentation hardware/code':'README mentionne un tampon NPN, mais son implémentation et les promesses réseau ne concordent pas avec le code.'},[
('Qualité de code générale','critique','Le raw string commence par R"=====( (53) et se termine avec six signes = au lieu de cinq (92) ; PIN_TACH_6m (113) ne correspond pas à PIN_TACH_6cm (24), et le fichier finit par un résidu Markdown (135). Erreurs de syntaxe/identifiant bloquantes.'),
('Temps-réel / ISR','critique','Les entrées sont GPIO10/11 (23–24), utilisées par la flash sur le module ESP-12S ; elles ne correspondent pas aux broches D1/D2 annoncées. Les ISR n’ont pas IRAM_ATTR.'),
('Fidélité au protocole tachymétrique / Fail-safe','critique','processData multiplie des compteurs cumulés jamais remis à zéro (45–48), sans durée ni 2 PPR. Une rotation constante produit une valeur croissante ; après arrêt elle ne revient pas à zéro.'),
('Génération sortie / Timer1','critique','PIN_OUTPUT est seulement configuré en sortie ; aucun générateur ni toggle n’est appelé. Un unique résultat moyen remplace deux canaux : zéro train tach envoyé à l’onduleur.'),
('Interface Web','majeur','Les champs ratio ne sont reliés à aucun handler de modification ; aucun paramètre WiFi STA ou stockage persistant implémenté malgré le README.')],
'Quelques petites fonctions nommées ; la chaîne HTML corrompue rend leur portée incertaine.', 'Aucune validation ou stratégie d’erreur utilisable.', 'Canaux copiés séparément et fusionnés en une seule valeur.')

add('glm5.3', 2.2, 5.2, 'Non compilable, mélange des tach incorrect', {
'Temps-réel / ISR':'ISR IRAM, buffer de 128 horodatages, génération à échéances absolues et GPOS/GPOC.',
'Démarrage / Boot readiness':'Intention : EEPROM, sorties relâchées, capture/Timer1 puis WiFi sans attente. Quatre fronts et un tick de contrôle 100 ms sont nécessaires ; aucune sortie réelle en l’état non compilable.',
'Génération sortie / Timer1':'Timer1 DIV1, rattrapage limité à trois fronts et commandes pendantes par canal.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2 et mode cache-panne optionnel explicitement désactivé par défaut ; perte 2,5 s, secours nominal seulement après 7,5 s depuis la dernière mesure valide.',
'Filtres / Signal':'Rejet des fronts <400 µs, fenêtre d’historique 2 s, EMA de fréquence 1/4.',
'WiFi / Réseau':'AP+STA sans sommeil, DNS captif, mDNS, scan asynchrone et reprise STA.',
'Gestion mémoire / HEAP':'HTML PROGMEM et buffers snprintf pour status/config.',
'Interface Web':'Status et formulaires séparés, scan et contrôle des canaux.',
'Persistance / Stockage':'Signature/version/taille et CRC32 avec résultat EEPROM.commit contrôlé par certains handlers.',
'Documentation hardware/code':'Schéma complet avec OU Schottky + buck et procédures de calibration.'},[
('Qualité de code générale','critique','SETTINGS_MAGIC=0xDEYEF001UL (114) contient Y. settingsLoad/Save ne peuvent pas compiler avec cette constante.'),
('Documentation hardware/code','critique','README 17 et schema_electronique.md 47–48 imposent les deux tach d’une paire en parallèle puis une division par deux. Deux collecteurs ouverts réalisent une combinaison de niveaux, pas une addition de fronts : à fréquence/phase identiques, on ne reçoit qu’une fréquence et le calcul divise la vitesse réelle par deux ; à d’autres phases, des impulsions se masquent.'),
('Temps-réel / ISR','majeur','Timer1 ordinaire et copie de 128 timestamps sous noInterrupts (213–217), soit jusqu’à 512 octets copiés sans IRQ. L’isolation totale et la latence maximale annoncées dans le README ne sont pas établies.'),
('Qualité de code générale','majeur','handleConfigPost et handleWifiPost modifient S avant la validation globale (532–585). Une requête finalement refusée peut avoir déjà changé certains champs en RAM ; aucune transaction par copie.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Le mode cache-panne maintient volontairement des RPM après perte réelle ; la détection et la bascule sont traitées dans loop. La mesure nécessite quatre fronts : amorçage de 30–40 ms à 3 000 RPM puis jusqu’à 100 ms de contrôle, hors setup.'),
('WiFi / Réseau','majeur','Routes config, reboot et factory reset sans authentification. Les paramètres chargés avec CRC ne sont pas systématiquement assainis (chaînes, ratios) avant usage.')],
'InChannel/OutChannel/Settings et fonctions de mesure/contrôle séparés.', 'CRC et erreurs EEPROM vérifiés à plusieurs endroits, mais mises à jour en place non transactionnelles.', 'Boucles à deux voies et helpers de validation ; séparation acceptable malgré un gros fichier.')

add('glm_5.2_high', 1.8, 4.4, 'Conversion de cycles inutilisable', {
'Temps-réel / ISR':'Capture des cycles dans des ISR IRAM et instantané sous section critique.',
'Démarrage / Boot readiness':'GPIO au repos ; LittleFS, WiFi et serveur précèdent l’attachement des ISR (551–568). Pas de vitesse de secours et pas d’attente STA explicite ; en l’état, les mesures sont rejetées par le calcul de période.',
'Génération sortie / Timer1':'Réutilisation du moteur waveform du cœur ; état appliqué distinct de la consigne.',
'Fidélité au protocole tachymétrique / Fail-safe':'Formule PPR=2 et timeout logiciel 1,5 s ; sortie relâchée à l’arrêt.',
'Filtres / Signal':'Moyenne de quatre périodes non nulles et seuil de mise à jour pour limiter les reprogrammations.',
'WiFi / Réseau':'AP+STA et paramètres configurables depuis HTTP.',
'Interface Web':'Polling des cellules du tableau sans réécrire les champs du formulaire.',
'Persistance / Stockage':'LittleFS, taille de structure et signature vérifiées ; ratios assainis.',
'Documentation hardware/code':'BOM, buck et sorties NPN détaillés.'},[
('Qualité de code générale','critique','GPIO12/GPIO13/GPIO14/GPIO4/GPIO5 sont utilisés comme constantes (76–81) sans définition fournie. Le cœur standard attend des numéros GPIO ou des alias Dn ; ces identifiants ne sont pas ceux des broches D1 mini.'),
('Fidélité au protocole tachymétrique / Fail-safe','critique','`deltaCycles * 1000000UL` (270) déborde en 32 bits avant division par 80 000 000. Le quotient est toujours ≤53 µs, alors que le filtre exige ≥1 000 µs (271) : aucune période ne peut être acceptée. Par exemple 1 500 000 cycles (1 600 RPM) deviennent 13 µs.'),
('Documentation hardware/code','majeur','Le schéma recommande les tach de deux ventilateurs en parallèle (114–119) : ils se masquent. Le buck annoncé 7–35 V est ensuite présenté comme fonctionnant depuis 5 V (72–77), sans marge pour fournir 5 V après OR-ing.'),
('Filtres / Signal','mineur','Le seuil (307) vaut 20 % de la fréquence appliquée +0,05 Hz, et non 0,2 Hz : un changement de 100 à 115 Hz n’est pas appliqué. La comparaison c1>c0 rejette également la période traversant le wrap du compteur.'),
('Qualité de code générale','majeur','escapeHtml traite les guillemets doubles mais pas l’apostrophe, alors que les attributs utilisent des apostrophes (344–354/395–406). Mots de passe affichés, routes sans authentification et stockage sans CRC/backup.')],
'CanalMap, Config et CanalState séparés ; fonctions regroupées par responsabilité.', 'Taille/magic du fichier vérifiés, mais erreurs d’unités et chaînes chargées non terminées explicitement.', 'Boucles et table de brochage communes aux canaux.')

add('gpt-5.6 sol medium', 6.8, 7.5, 'Base claire, ordonnanceur et arrêt à consolider', {
'Temps-réel / ISR':'Capture IRAM et sortie par registres ; calculs et allocations hors ISR ; instantanés atomiques.',
'Démarrage / Boot readiness':'EEPROM puis setupSignalEngine avant réseau ; sorties basses et deux fronts exigés. À 1 600 RPM, acquisition 18,75–37,5 ms, puis jusqu’à 10 ms de timer idle, hors setup.',
'Génération sortie / Timer1':'Timer1 DIV16, conversion explicite 5 ticks/µs et ordonnanceur commun à deux sorties ; saturation des demi-périodes.',
'Fidélité au protocole tachymétrique / Fail-safe':'PPR=2, ratios 0,1–20 et timeout 2 s ; filtre réamorcé à la perte de signal.',
'Filtres / Signal':'Périodes 500 µs–1,5 s ; EMA 1/4 appliquée seulement si le compteur de mesures change (514–523).',
'WiFi / Réseau':'AP+STA et connexion non bloquante ; WiFi.persistent(false).',
'Gestion mémoire / HEAP':'Aucune allocation ISR ; réservations des String HTML/JSON.',
'Interface Web':'Dark theme, polling et formulaires séparés ; mots de passe non réaffichés.',
'Persistance / Stockage':'Version, taille, CRC32 et contrôle de finitude des ratios ; configuration HTTP validée dans une copie temporaire.',
'Documentation hardware/code':'BOM, niveaux électriques, buck/buck-boost selon plage d’entrée, un tach par paire et limites métrologiques explicites.'},[
('Génération sortie / Timer1','majeur','outputTimerIsr décompte `armedTimerTicks` au lieu du temps réel écoulé (205–243), puis recharge le timer après son traitement. Les temps ISR et latences s’ajoutent à chaque intervalle : dérive négative de fréquence. Aucun plafond de jitter mesuré ne peut être déduit.'),
('Temps-réel / ISR','majeur','Timer1 et GPIO restent masquables pendant les opérations critiques/flash. Le timeout (500–510) n’est contrôlé que dans loop ; HTTP peut prolonger l’émission d’une ancienne consigne au-delà de 2 s.'),
('Fidélité au protocole tachymétrique / Fail-safe','majeur','À la perte, inputLastValidEdgeUs/inputPeriodUs ne sont pas invalidés (504–511). Au wrap de micros après ≈71 min 35 s, l’ancien timestamp redevient récent ; l’EMA est alors à zéro et aucun nouveau compteur n’arrive pour la réamorcer : division par zéro et conversion non finie possibles (514–533).'),
('Persistance / Stockage','mineur','configIsValid ne vérifie pas un NUL dans toutes les chaînes ; les copies HTTP sont sûres, mais une configuration CRC-valide forgée peut être non terminée. Le résultat commit est ignoré.'),
('WiFi / Réseau','majeur','Pas d’authentification sur la configuration ; toute machine joignable peut modifier la fréquence ou programmer un reboot via changement WiFi.')],
'Namespace interne, types persistants/runtime distincts et fonctions d’instantané bien délimitées.', 'Validation finie, CRC, copies bornées et mise à jour transactionnelle des paramètres HTTP.', 'Moteur et conversion génériques par canal, constantes partagées.')

add('GPT-5.6_Sol_xhigh', 7.7, 8.2, 'Bonne base maintenable, quelques limites de capture', {
'Temps-réel / ISR':'Capture IRAM limitée aux timestamps et sommes ; accumulation protégée contre overflow ; moteur NMI waveform du cœur réutilisé.',
'Démarrage / Boot readiness':'Sorties relâchées avant EEPROM, WiFi sans attente. La génération démarre au traitement de 200 ms suivant la première période valide ; pas de nominal artificiel. Au nominal, acquisition de deux fronts puis 0–200 ms de traitement, hors setup.',
'Génération sortie / Timer1':'startWaveform avec demi-périodes égales ; retour de démarrage contrôlé et état appliqué mémorisé pour éviter les mises à jour de moins de 2 µs.',
'Fidélité au protocole tachymétrique / Fail-safe':'Deux voies indépendantes, 2 PPR et ratios 0,1–10 ; arrêt après 2 s et remise à zéro de la capture pour la reprise.',
'Filtres / Signal':'Moyenne des périodes reçues sur fenêtre 200 ms, puis IIR 1/4 ; rejet des fronts <1 ms et périodes >2 s.',
'WiFi / Réseau':'AP+STA permanent, auto-reconnexion, WIFI_NONE_SLEEP ; reconfiguration STA différée sans reboot.',
'Gestion mémoire / HEAP':'HTML PROGMEM, String status réservé et aucune allocation dans les ISR.',
'Interface Web':'Formulaire et télémétrie séparés, états des deux canaux ; mots de passe non divulgués.',
'Persistance / Stockage':'Signature, version, CRC32, static_assert et contrôle de terminaison NUL des chaînes ; parseur strtof exigeant toute la chaîne.',
'Documentation hardware/code':'DESIGN et SCHEMA_ET_MONTAGE exposent PTC, TVS, OR-ing, filtrage, courant disponible et essais au banc. Le plan annexe est traité comme document, pas comme instruction à exécuter.'},[
('Fidélité au protocole tachymétrique / Fail-safe','majeur','Tant que lastValidPulseMs=0, consumeChannelSamples réinitialise lastEdgeCycles toutes les 200 ms (338–344). Une entrée de période >200 ms (moins de 150 RPM) ne peut normalement pas fournir deux fronts entre ces remises à zéro, alors que la plage accepte jusqu’à 2 s.'),
('Temps-réel / ISR','majeur','L’arrêt se décide dans loop toutes les 200 ms et peut être retardé par HTTP ; la capture GPIO n’est pas NMI. Un blocage après les derniers échantillons peut en plus les dater au moment du traitement, repoussant le timeout.'),
('Filtres / Signal','mineur','IIR appliqué toutes les 200 ms avec alpha=0,25 : constante de temps ≈695 ms et établissement à 95 % en ≈2,1 s. Bonne stabilité mais suivi lent lors d’un changement brusque.'),
('WiFi / Réseau','majeur','Configuration et ratios modifiables sans authentification. Le mot de passe AP fixe est partagé par tous les exemplaires.'),
('Persistance / Stockage','mineur','EEPROM.commit non vérifié (236–241), sans sauvegarde de secours. startWaveform est contrôlé mais simulatedRpm reste calculé avant l’écrêtage final de demi-période : aux extrêmes, l’affichage peut différer du signal.')],
'tach_math.h isole les fonctions pures ; types de capture, état de canal et configuration sont distincts.', 'CRC, chaînes vérifiées, parsing strict, accumulation bornée et retour waveform contrôlé.', 'Fonctions communes consumeChannelSamples/stopChannelOutput ; deux petites instances explicites.',
'test_tach_math.cpp contient des assertions sur les deux vitesses nominales, ratios limites, NaN et période nulle. Tests C++ non exécutés faute de compilateur natif détecté ; calculs de référence vérifiés séparément en Python.')
