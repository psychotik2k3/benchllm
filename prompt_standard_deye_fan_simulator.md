sans regarder aucun autre fichier dans competitors ou ses sous dossiers, 
dans ce dossier XXXXX tu vas creer le code demande par le prompt suivant:

Tu es un expert en électronique embarquée et en programmation Arduino/ESP8266.

Contexte

J'ai un onduleur solaire Deye SUN-8K-SG05LP1-EU-AM2-P dont je remplace les ventilateurs d'origine bruyants par des modèles Noctua silencieux. Les ventilateurs d'origine sont un NMB 06025VE-12N-CL (6 cm) et un NMB 09225VE-12N-CU (9 cm), tous deux alimentés en 12 V. Je les remplace par deux Noctua NF-A6x25-FLX et deux Noctua NF-A9-FLX respectivement — je double le nombre de ventilateurs pour compenser leur débit d'air inférieur.

Les Noctua tournent significativement moins vite que les NMB d'origine. L'onduleur lit les signaux RPM (tach) de ses ventilateurs et risque de lever une alarme ou de modifier son comportement si les RPM lui semblent trop bas. Il faut donc lui faire croire que les ventilateurs tournent plus vite qu'ils ne le font réellement.

Les ventilateurs sont des modèles 3 broches (pas de PWM) : GND, +12V, tach.

Matériel disponible
Microcontrôleur : Wemos D1 Mini V2.3.0 (ESP-12S / ESP8266)
Composants discrets : résistances de toutes valeurs, transistors NPN parmi : 2N2222, 2N3904, BC547, BC548, S9013, S9014, S8050, SS8050, C1815, C945, 2N5551, BC337, S9018
Ce que le système doit faire
Lire le signal tach d'un ventilateur Noctua 9 cm et d'un ventilateur Noctua 6 cm, chacun sur son propre canal
Appliquer un ratio multiplicateur configurable par canal et réémettre vers l'onduleur un signal tach simulé correspondant aux RPM multipliés
Le ratio de chaque canal doit être réglable sans recompiler ni modifier le câblage
Le Wemos doit exposer une interface web permettant de configurer les ratios, configurer le WiFi, et afficher en temps réel les RPM lus et les RPM simulés pour chaque canal
Le WiFi doit fonctionner en mode AP et STA simultanément, avec les paramètres persistants entre les redémarrages
Une LED doit indiquer visuellement si le système est en train de simuler activement des RPM ou s'il attend un signal
Contraintes importantes
La tension que l'onduleur applique sur sa broche tach est inconnue — elle peut être 3,3 V, 5 V ou 12 V. La solution doit fonctionner dans tous les cas sans modification matérielle
Le Wemos doit être alimenté depuis les connecteurs ventilateur (+12V), sachant que les deux connecteurs ne sont pas forcément alimentés en même temps ni à la même tension. La solution d'alimentation doit gérer cette situation proprement
Le signal de sortie doit être le plus stable et exempt de jitter possible, y compris lorsque le WiFi est actif
La conversion du signal tach — aussi bien la lecture des entrées que la génération des sorties — doit être totalement isolée des perturbations liées au WiFi, au serveur web et à toute autre tâche logicielle. Une activité réseau intense ou une requête HTTP ne doit avoir aucun impact mesurable sur la précision ou la régularité du signal produit vers l'onduleur. Réfléchis soigneusement à l'architecture logicielle pour garantir cette isolation.
Ce que tu dois produire
Le code Arduino complet (deye_fan.ino), commenté, prêt à compiler pour la carte LOLIN(WEMOS) D1 mini sous Arduino IDE
Le schéma électronique complet de toute l'installation : alimentation, circuits d'entrée (Noctua → ESP8266) et circuits de sortie (ESP8266 → Deye)

Tu ne dois pas regarder dans d'autres dossiers parent ou autre.
tu as le droit de faire des recherches web pour mettre a jour tes donnees