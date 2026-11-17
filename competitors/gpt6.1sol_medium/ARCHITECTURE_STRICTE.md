# Satisfaire réellement l'indépendance vis-à-vis du WiFi

## Pourquoi le sketch ESP8266 seul ne la satisfait pas

Le core ESP8266 3.1.2 exécute le callback Timer1 depuis un gestionnaire d'interruption. L'horloge du timer est matérielle mais la commutation de GPIO dans le callback est logicielle. Un front tach d'entrée est horodaté lorsque le CPU traite son interruption, et non par une capture matérielle autonome dédiée à chaque canal.

La pile WiFi, le SDK, les sections critiques et les opérations flash peuvent modifier ces latences. Déplacer les traitements de `loop()` vers une ISR évite une dépendance directe aux requêtes HTTP ; cela ne prouve pas leur indépendance temporelle. Une baisse du jitter observé ou l'absence d'erreur sur un essai n'est pas une garantie pour toute charge réseau.

## Architecture nécessaire

```text
Noctua A9 -- adaptation 3,3 V -- capture matérielle 1 -- moteur temps réel -- sortie 1 -- NPN -- Deye 9
Noctua A6 -- adaptation 3,3 V -- capture matérielle 2 -- moteur temps réel -- sortie 2 -- NPN -- Deye 6
                                                        |
                                             registres double tampon
                                                        |
                                               liaison à CRC / limites
                                                        |
                                              ESP8266 : AP + STA + Web
```

Un petit FPGA/CPLD correctement dimensionné permet de réaliser deux compteurs de capture, un filtre entier, le calcul des périodes, deux générateurs à accumulateur de phase, deux timeouts et le pilotage LED **en logique synchrone dédiée**. Une horloge locale et une alimentation découplée distinctes définissent le budget de précision. Les mises à jour web écrivent seulement des registres double tampon ; transfert atomique aux frontières de cycle, sans pause de la sortie. La capture et la synthèse ne dépendent ni des transactions réseau ni de la réception des paramètres. Aucun flux logiciel ne fournit chaque front.

Un second microcontrôleur doté de capture et compare matériels autonomes, ou de machines d'état de type PIO, est aussi envisageable. Il faut vérifier **les deux voies de capture, la synthèse, les rechargements et les timeouts**, y compris la dépendance éventuelle à des ISR, DMA ou FIFO. Ajouter un second CPU sans cette analyse ne suffit pas à promettre l'absence de jitter. Le présent livrable ne fournit pas un firmware de coprocesseur ni une conception HDL.

## Règles du moteur autonome

- Mesure des deux périodes par une horloge stable, capture/synchronisation matérielle des signaux asynchrones ; quantification bornée et métastabilité traitée.
- Synthèse de fréquence avec granularité d'horloge définie, sorties indépendantes, rapport cyclique maîtrisé. « Aucun jitter » doit devenir une limite chiffrée, par exemple erreur de front <= une période d'horloge plus incertitude du synchroniseur, hors changement volontaire de ratio.
- Ratios bornés et vérifiés dans le moteur, défaut 1, CRC et numéro de séquence sur commandes. Le réseau peut demander une modification de fréquence ; il ne peut pas suspendre le moteur.
- Compteurs de timeout matériels : disparition de tach => libération de la sortie après une durée bornée, même si l'ESP se bloque.
- Télémetrie par instantané cohérent ; lecture des registres sans suspendre les compteurs. Débordements de liaison rejetés, aucun back-pressure sur le trajet tach.
- Sauvegarde persistante gérée côté ESP sans écriture flash du moteur pendant son fonctionnement. Au démarrage, moteur inactif tant qu'aucun tach réel valide n'est acquis.
- Reset du Wemos et forte charge HTTP sans effet sur les trains d'impulsions en cours. Le comportement lors d'une perte prolongée de liaison doit être défini : maintien du dernier ratio autorisé tant que le tach réel est valide, ou arrêt volontaire via watchdog de communication ; ne pas confondre ce choix avec une perturbation de fréquence.

## Limite physique à préciser

Une isolation **logicielle** totale du réseau n'est pas la promesse d'une absence absolue de couplage électrique ou RF mesurable. Pour cette dernière, il faut spécifier tolérances d'horloge, alimentations, masses, filtrage, routage et mesures CEM. L'activité radio peut injecter du bruit dans les alimentations même lorsque le calcul tach est séparé. Une alimentation dédiée filtrée du moteur et un bon découplage sont requis ; une isolation galvanique ne se justifie que si les références électriques l'exigent.

Avec seulement le Wemos, les résistances et les NPN proposés, l'ensemble des exigences initiales n'est donc pas réalisable. Le sketch livré est un prototype fonctionnel à tester, pas une substitution silencieuse à cette architecture stricte.
