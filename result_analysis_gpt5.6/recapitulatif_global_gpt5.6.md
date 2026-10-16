# Récapitulatif global — GPT-5.6

## 1. Inventaire des analyses

Le disque contient **22 dossiers** sous `competitors`, tous couverts par une analyse. `kimi k3` désigne ici la **variante thinking high** analysée depuis `competitors/kimi k3`; `kimi_k3_max` est la **variante Max**, conservée comme concurrent distinct.

### ✅ Analyses existantes avant ce run (19)

- `claude opus 5 medium` — `analyse_claude opus 5 medium.md`
- `claude-sonnet5-free` — `analyse_claude-sonnet5-free.md`
- `composer 2.5` — `analyse_composer 2.5.md`
- `composer 2.5 prompt from claude` — `analyse_composer 2.5 prompt from claude.md`
- `deepseek4 flash` — `analyse_deepseek4 flash.md`
- `gemini 3.1 pro` — `analyse_gemini 3.1 pro.md`
- `gemini-free` — `analyse_gemini-free.md`
- `gemma4` — `analyse_gemma4.md`
- `glm5.3` — `analyse_glm5.3.md`
- `gpt-5.6 sol medium` — `analyse_gpt-5.6 sol medium.md`
- `gpt-free` — `analyse_gpt-free.md`
- `grok 4.6 medium` — `analyse_grok 4.6 medium.md`
- `Grok_4.7_medium` — `analyse_Grok_4.7_medium.md`
- `kimi k3` (**thinking high**) — `analyse_kimi k3.md`
- `kimi_k3_max` (**variante Max**) — `analyse_kimi_k3_max.md`
- `qwen3.6` — `analyse_qwen3.6.md`
- `qwen3.6-35b-fp8-freetoken` — `analyse_qwen3.6-35b-fp8-freetoken.md`
- `qwen3.6_prompt_from_claude` — `analyse_qwen3.6_prompt_from_claude.md`
- `qwen35` — `analyse_qwen35.md`

### 🆕 Nouvelles analyses produites lors de ce run (3)

- `Gemini_4_Argon_medium` — `analyse_Gemini_4_Argon_medium.md`
- `glm_5.2_high` — `analyse_glm_5.2_high.md`
- `muse_wonder_1.0_medium` — `analyse_muse_wonder_1.0_medium.md`

### ⚠️ Compétiteurs sans analyse disponible

- Aucun parmi les 22 compétiteurs du run.

## 2. 🔍 Notes révisées et justification

**Aucune révision postérieure n'est nécessaire.** La comparaison directe des 22 rapports ne démontre pas d'incohérence substantielle justifiant de modifier une note antérieure ou nouvelle. Les écarts restent expliqués par des défauts bloquants documentés :

- `muse_wonder_1.0_medium` mérite **7,7/10** au global et **8,1/10** en qualité grâce à son build ESP8266 3.1.2 réellement réussi, son Timer1/NMI événementiel, sa formule 2 PPR et son filtrage; il reste sous le trio à 8,0–8,4 car le calcul CRC inclut son propre champ à cause du padding et rejette presque toute configuration au reboot.
- `Gemini_4_Argon_medium` reste cohérent à **6,0/10** et **7,8/10** : sa NMI, ses échéances absolues et ses buffers bornés sont solides, mais un premier front après stall réactive l'ancienne période, tandis que l'alimentation discrète est sous-dimensionnée à 7 V et surcharge ses résistances aux pointes.
- `glm_5.2_high` reste cohérent à **1,8/10** malgré **4,8/10** en qualité apparente : il ne compile pas, son produit 32 bits ramène toutes les périodes calculées sous 1 000 µs et les fait rejeter, et son schéma relie contradictoirement les rails avant l'OR-ing tout en recommandant un LM2596 incapable de maintenir 5 V depuis 5 V.

Le trio de tête reste donc inchangé : équilibre global pour `gpt-5.6 sol medium`, qualité structurelle maximale pour `claude opus 5 medium`, et réveil Timer1 immédiat dans `kimi_k3_max`. Aucune note ne porte d'astérisque.

## 3. Tableau récapitulatif

Tableau trié par note globale décroissante. Chaque cellule contient au plus huit points fidèles au rapport individuel.

| Version | Note globale | Note qualité code | ✅ Points positifs (5–8) | ❌ Points négatifs (5–8) |
|---|---:|---:|---|---|
| `gpt-5.6 sol medium` | **8,4/10** | **8,2/10** | • Timer1 one-shot à prochaine échéance<br>• GPIO atomiques en ISR<br>• Formule 2 PPR exacte<br>• IIR réinitialisé au stall<br>• AP+STA non bloquant<br>• EEPROM versionnée avec CRC32<br>• OR-ing, buck-boost, PTC/TVS détaillés | • Aucun tach provisoire au boot<br>• Timeout stall de 2 s<br>• Une période suffit à réactiver<br>• Dépendance IRAM de `micros()` non figée<br>• HTTP sans auth/CSRF/TLS<br>• Gros `String` HTML sur le heap<br>• Saturation non reflétée dans l'UI |
| `kimi_k3_max` — Max | **8,1/10** | **8,3/10** | • Timer1 one-shot à échéances absolues<br>• Réveil immédiat depuis l'état idle<br>• Deux canaux réellement indépendants<br>• GPIO directs et ISR sobres<br>• EMA hors ISR réinitialisée<br>• PROGMEM et JSON borné<br>• CRC16 et reconnexion STA<br>• Hardware open-collector documenté | • Aucun tach provisoire au boot<br>• Premier intervalle calculé depuis zéro<br>• Pas de borne haute de période<br>• Une période suffit à activer<br>• Timeout stall de 1,5 s<br>• HTTP sans auth/CSRF<br>• Saturation 2 kHz peu observable<br>• Buck simple vulnérable à rail bas |
| `claude opus 5 medium` | **8,0/10** | **8,5/10** | • NMI/CCOUNT très rigoureux<br>• Seqlock NMI-vers-boucle<br>• Échéances absolues et rattrapage borné<br>• PPR entrée/sortie configurables<br>• Validation puis EMA entière<br>• PROGMEM et diagnostics riches<br>• EEPROM CRC32/version<br>• Hardware et tests détaillés | • API NMI privées non figées<br>• Charge NMI potentiellement agressive<br>• Aucun tach avant mesure réelle<br>• Un seul tach lu par paire<br>• `apPass` exposé par l'API<br>• HTTP non authentifié<br>• Chaînes réseau non échappées<br>• Valeurs RC contradictoires |
| `muse_wonder_1.0_medium` | **7,7/10** | **8,1/10** | • Build ESP8266 3.1.2 vérifié<br>• NMI événementielle placée en IRAM<br>• GPIO directs et deux phases indépendantes<br>• Formule 2 PPR exacte<br>• Rejet relatif, confirmation et EMA<br>• Glitches rejetés sans rafraîchir le timeout<br>• PROGMEM et JSON `snprintf`<br>• OR-ing, PTC/TVS et NPN détaillés | • CRC incluant son propre champ<br>• Réglages rejetés à chaque reboot<br>• Anneau ISR sans détection de saturation<br>• Aucun tach provisoire au boot<br>• Slew exécuté par tour de `loop()`<br>• Un seul tach surveillé par paire<br>• HTTP sans auth/CSRF<br>• Buck fragile sur rail bas/PWM |
| `kimi k3` — thinking high | **7,6/10** | **8,0/10** | • Timer1 one-shot à phase absolue<br>• Deux canaux et GPIO directs<br>• Formule 2 PPR correcte<br>• EMA entière et timeout<br>• PROGMEM et `snprintf` borné<br>• AP+STA, mDNS et CRC16<br>• OR-ing et buck 5 V corrects | • Réveil idle retardé jusqu'à 1 s<br>• Premier intervalle depuis le boot<br>• Reprise contaminée par la pause<br>• Pas de borne maximale de période<br>• Pas de validation multi-périodes<br>• Timeout de 1,5 s<br>• Pas d'auth/CSRF<br>• Pull-down NPN et protections absents |
| `glm5.3` | **7,2/10** | **8,0/10** | • Timer1 one-shot à échéances absolues<br>• Rattrapage borné<br>• Mesure par timestamps et EMA<br>• AP+STA, portail DNS et mDNS<br>• PROGMEM, buffers fixes, JSON échappé<br>• EEPROM CRC32/version<br>• Web et diagnostics complets<br>• Hardware et procédure de test détaillés | • Modèle de tach parallèles physiquement faux<br>• Panne d'un ventilateur masquée<br>• Longue section critique de snapshot<br>• Cache-panne avec trou d'environ 5 s<br>• EMA non réinitialisée après perte<br>• Tach ancien maintenu 2,5 s<br>• HTTP sans auth/CSRF<br>• Documentation contradictoire |
| `grok 4.6 medium` | **7,0/10** | **7,2/10** | • NMI Timer1 et GPIO directs<br>• Deux canaux indépendants<br>• Conversion 2 PPR exacte<br>• Timeout court de 400 ms<br>• PROGMEM et CRC32<br>• AP+STA avec reconnexion<br>• Un tach par paire sans mise en parallèle<br>• OR-ing et collecteurs ouverts cohérents | • NMI permanente à 50 kHz<br>• Quantification de 20 µs<br>• Fraîcheur mise à jour avant validation<br>• Aucun filtre numérique de période<br>• Publication multi-champs non transactionnelle<br>• CPU 80 MHz codé en dur<br>• Aucun tach de boot<br>• HTTP sans auth/CSRF |
| `composer 2.5 prompt from claude` | **6,2/10** | **6,3/10** | • Timer1 matériel et deux échéances<br>• Formule 2 PPR correcte<br>• Moyenne et limitation de pente prévues<br>• Serveur asynchrone AP+STA<br>• PROGMEM et documents JSON statiques<br>• LittleFS évolutif<br>• OR-ing, buck et NPN documentés | • ISR fixe 20 kHz avec `digitalWrite()`<br>• `now + half` accumule la dérive<br>• Quantification mesurable<br>• Front invalide repousse le timeout<br>• Filtre réutilisant la même mesure<br>• Boot retardé par LittleFS<br>• Pas d'auth/CSRF<br>• Persistance non atomique |
| `Gemini_4_Argon_medium` | **6,0/10** | **7,8/10** | • Bonne NMI FRC1 et GPIO directs<br>• Échéances absolues sans rafale<br>• Formule 2 PPR et Q16.16 correctes<br>• Timeout court de 500 ms<br>• Moyenne de quatre périodes<br>• PROGMEM et JSON borné<br>• EEPROM CRC32/version<br>• AP+STA et reconnexion non bloquants | • Reprise sur ancienne période au premier front<br>• État partagé non transactionnel<br>• Multiplication 64 bits ISR non auditée<br>• Aucun tach provisoire au boot<br>• API HTTP sans auth/CSRF<br>• Alimentation insuffisante à 7 V<br>• Résistances surchargées aux pointes<br>• OR-ing NPN proche du VEBO maximal |
| `claude-sonnet5-free` | **4,4/10** | **7,0/10** | • Timer1 matériel et GPIO directs<br>• Deux canaux factorisés<br>• Formule 2 PPR correcte<br>• PROGMEM et buffers `char`<br>• EEPROM avec contrôle d'intégrité<br>• AP+STA non bloquant<br>• Sorties open-collector bien conçues | • Entrée fondée sur une fausse pull-up 12 V<br>• Premier front pris comme période<br>• Compteur `N+1`<br>• Dérive cumulative<br>• Résolution de 100 µs<br>• Timeout de 3 s<br>• Aucun filtre de période<br>• HTTP non authentifié |
| `gpt-free` | **4,1/10** | **4,7/10** | • Capture CCOUNT en ISR courte<br>• Générateur waveform hors `loop()`<br>• Formule 2 PPR correcte<br>• IIR entier 7/8–1/8<br>• AP+STA et interface complète<br>• Structures statiques<br>• Code lisible et factorisé | • API waveform interne non versionnée<br>• Constantes dépendantes de 160 MHz<br>• Aucun tach anticipé au boot<br>• Hardware d'entrée/alimentation incomplet<br>• Secrets renvoyés en clair<br>• EEPROM sans CRC/version<br>• Page de 9 ko construite sur le heap<br>• Phase des réarmements non garantie |
| `Grok_4.7_medium` | **4,0/10** | **8,0/10** | • NMI/CCOUNT et GPIO directs<br>• Seqlock NMI-vers-boucle<br>• Échéances absolues en cycles<br>• Formule 2 PPR exacte<br>• IIR réinitialisé au stall<br>• Fraîcheur fondée sur période valide<br>• PROGMEM, JSON borné et CRC32<br>• Deux canaux factorisés | • Q1–Q4 C-B polarisés en inverse<br>• VRAW ne reçoit aucun courant utile<br>• Régulateur linéaire discret fragile<br>• NMI permanente à 31,25 kHz<br>• Divisions NMI non auditées IRAM<br>• Aucun tach provisoire au boot<br>• Une période suffit à activer<br>• HTTP sans auth/CSRF |
| `deepseek4 flash` | **3,3/10** | **3,8/10** | • Event-calendar one-shot ambitieux<br>• NMI et GPIO atomiques<br>• Formule 2 PPR correcte<br>• Timeout adaptatif<br>• EMA entière<br>• PROGMEM et buffer JSON fixe<br>• EEPROM versionnée<br>• OR-ing, buck et NPN | • Copie de `volatile SnapT` probablement non compilable<br>• `noInterrupts()` inefficace contre NMI<br>• Réveil idle désynchronisé<br>• Course sur registres FRC1<br>• Premier front accepté comme période<br>• Parasites repoussent le timeout<br>• Secrets exposés<br>• API privées non verrouillées |
| `gemini 3.1 pro` | **2,8/10** | **2,8/10** | • Timer1 matériel et GPIO directs<br>• Intention 2 PPR correcte<br>• Deux sorties indépendantes<br>• AP+STA<br>• HTML en PROGMEM<br>• OR-ing, buck et NPN décrits | • `0xDEYE0001` empêche le build<br>• Comptage suppose une fenêtre exacte de 1 s<br>• Tick 20 kHz et dérive cumulative<br>• Ratios sans maximum<br>• `strcpy` HTTP non borné<br>• Mot de passe STA exposé<br>• Aucun vrai filtre<br>• Conseil électrique dangereux |
| `gemini-free` | **2,5/10** | **3,5/10** | • Timer1 matériel visé<br>• Formule 2 PPR correcte<br>• Deux consignes indépendantes<br>• AP+STA asynchrone<br>• PROGMEM et JSON statique<br>• `strlcpy` pour les identifiants<br>• Interface simple | • Timer DIV16 environ 16 fois trop lent<br>• `.cpp` sans `Arduino.h` explicite<br>• Premier front accepté comme période<br>• Ratio invalide conserve l'ancienne sortie<br>• Plage d'entrée incohérente<br>• Aucun filtre robuste<br>• Aucun schéma matériel complet<br>• API sans auth |
| `qwen3.6_prompt_from_claude` | **2,5/10** | **3,5/10** | • Bonne formule 2 PPR<br>• GPIO directs prévus<br>• Moyenne et lissage envisagés<br>• PROGMEM et buffers bornés<br>• AP+STA, OTA et interface riches<br>• LittleFS évolutif<br>• Schéma fonctionnel assez complet | • Plusieurs erreurs de compilation<br>• `Ticker` local détruit après `setup()`<br>• `Ticker` n'est pas Timer1<br>• Polarité/fail-safe NPN inversés<br>• Sortie non relâchée au stall<br>• MT3608 n'est pas un buck<br>• AP/OTA fixes et HTTP ouvert<br>• LittleFS non atomique |
| `composer 2.5` | **2,0/10** | **4,0/10** | • Intention Timer1 à deux canaux<br>• Formule 2 PPR correcte<br>• EEPROM avec CRC16<br>• AP+STA non bloquant<br>• Pull-up d'entrée 3,3 V correct<br>• Sorties collecteur ouvert prévues<br>• Code compact et lisible | • `0xDEYE` bloque la compilation<br>• Symbole WiFi probablement invalide<br>• Timer1 environ cinq fois trop rapide<br>• Flottants dans ISR<br>• Timeout repoussé par parasites<br>• Secrets STA exposés<br>• 12 V conseillé sur pin 5 V<br>• Pas de pull-down ni tests |
| `glm_5.2_high` | **1,8/10** | **4,8/10** | • API waveform adaptée au core 3.1.2<br>• Deux voies prévues séparément<br>• Intention 2 PPR correcte<br>• Moyenne de quatre périodes<br>• AP+STA non bloquant<br>• LittleFS et copies réseau bornées<br>• Sorties NPN collecteur ouvert | • Parenthèses surnuméraires : build cassé<br>• Symboles `GPIOxx` inexistants<br>• Overflow 32 bits rejetant toute période<br>• Aucune sortie ne peut démarrer<br>• Deux tach parallèles recommandés<br>• Secrets WiFi affichés et API ouverte<br>• Rails reliés avant les diodes<br>• LM2596 incapable de réguler 5 V depuis 5 V |
| `qwen3.6` | **1,5/10** | **2,5/10** | • Intention de deux canaux<br>• Formule d'affichage 2 PPR correcte<br>• Timeout prévu<br>• Ratios bornés<br>• AP+STA visé<br>• Documentation et BOM présentes<br>• NPN de sortie pertinent en principe | • API ESP32 sur ESP8266<br>• Erreur syntaxique certaine<br>• Ratio multiplié au lieu d'être divisé<br>• Demi-période confondue avec période<br>• Canal incapable de redémarrer<br>• Polarité NPN inversée<br>• GPIO15 de strap risqué<br>• AP ouvert et 12 V direct |
| `qwen3.6-35b-fp8-freetoken` | **1,0/10** | **2,0/10** | • Structure lisible<br>• Formule d'affichage 2 PPR correcte<br>• ISR marquées IRAM<br>• Ratios HTTP bornés<br>• AP+STA et portail captif<br>• Nomenclature et schémas fournis | • Ne compile pas : EEPROM/API Timer1<br>• Timer jamais configuré<br>• Canal 2 jamais généré<br>• Sorties stoppées entre fronts<br>• Quatre tach parallélisés<br>• Fail-safe NPN inversé<br>• 3,3 V injecté sur pin 5 V<br>• Entrée pouvant surtensionner le GPIO |
| `qwen35` | **0,5/10** | **1,0/10** | • Documentation abondante<br>• Interface responsive<br>• AP protégé<br>• Ratios bornés côté serveur<br>• Avertissements haute tension | • Erreurs de syntaxe et d'API<br>• GPIO10/11 liés à la flash<br>• Aucun Timer1 conforme<br>• `Ticker` en secondes<br>• RPM issu de compteurs cumulatifs<br>• Une sortie moyenne<br>• Aucune persistance utile<br>• Schéma incohérent |
| `gemma4` | **0,3/10** | **0,8/10** | • Interface compacte<br>• AP protégé au minimum<br>• NPN open-collector mentionné<br>• Petit code facile à lire<br>• Endpoint d'observation simple | • Ne compile pas<br>• GPIO10/11 réservés à la flash<br>• Aucune génération tach/Timer1<br>• Une sortie pour deux canaux<br>• Formule RPM fausse<br>• Aucun fail-safe<br>• Aucune persistance/STA réelle<br>• Aucun OR-ing/buck complet |

## 4. Classements séparés

Les égalités sont départagées par l'autre note, puis alphabétiquement.

### Classement par note globale — candidat IoT

1. `gpt-5.6 sol medium` — **8,4/10** — meilleur équilibre
2. `kimi_k3_max` (Max) — **8,1/10** — temps-réel très solide
3. `claude opus 5 medium` — **8,0/10** — moteur le plus sophistiqué
4. `muse_wonder_1.0_medium` — **7,7/10** — excellent moteur, persistance cassée
5. `kimi k3` (thinking high) — **7,6/10** — très bonne base
6. `glm5.3` — **7,2/10** — avancé, hypothèse tach critique
7. `grok 4.6 medium` — **7,0/10** — NMI crédible, fail-safe fragile
8. `composer 2.5 prompt from claude` — **6,2/10** — complet mais non déterministe
9. `Gemini_4_Argon_medium` — **6,0/10** — bonne NMI, reprise et alimentation fragiles
10. `claude-sonnet5-free` — **4,4/10** — firmware propre, entrée invalide
11. `gpt-free` — **4,1/10** — logiciel prometteur, hardware incomplet
12. `Grok_4.7_medium` — **4,0/10** — firmware solide, alimentation bloquée
13. `deepseek4 flash` — **3,3/10** — architecture ambitieuse cassée
14. `gemini 3.1 pro` — **2,8/10** — non compilable
15. `gemini-free` — **2,5/10** — timing central faux
16. `qwen3.6_prompt_from_claude` — **2,5/10** — ambitieux mais inexécutable
17. `composer 2.5` — **2,0/10** — dangereux et non compilable
18. `glm_5.2_high` — **1,8/10** — non compilable, aucune période valide
19. `qwen3.6` — **1,5/10** — refonte nécessaire
20. `qwen3.6-35b-fp8-freetoken` — **1,0/10** — non fonctionnel
21. `qwen35` — **0,5/10** — non exploitable
22. `gemma4` — **0,3/10** — fonctionnalité absente

### Classement par note qualité de code

1. `claude opus 5 medium` — **8,5/10** — rigueur structurelle
2. `kimi_k3_max` (Max) — **8,3/10** — rigoureux et factorisé
3. `gpt-5.6 sol medium` — **8,2/10** — structuré et défensif
4. `muse_wonder_1.0_medium` — **8,1/10** — rigoureux, CRC non testé
5. `kimi k3` (thinking high) — **8,0/10** — clair et compact
6. `glm5.3` — **8,0/10** — maintenable mais métier erroné
7. `Grok_4.7_medium` — **8,0/10** — rigoureux, hardware invalidant
8. `Gemini_4_Argon_medium` — **7,8/10** — ambitieux, invariants de reprise fragiles
9. `grok 4.6 medium` — **7,2/10** — clair, bas niveau fragile
10. `claude-sonnet5-free` — **7,0/10** — lisible, insuffisamment validé
11. `composer 2.5 prompt from claude` — **6,3/10** — clair, invariants faibles
12. `glm_5.2_high` — **4,8/10** — structuré, vérification élémentaire absente
13. `gpt-free` — **4,7/10** — lisible mais fragile
14. `composer 2.5` — **4,0/10** — ordonné, erreurs fondamentales
15. `deepseek4 flash` — **3,8/10** — documenté, concurrence incorrecte
16. `gemini-free` — **3,5/10** — lisible, non validé
17. `qwen3.6_prompt_from_claude` — **3,5/10** — structuré, non fiable
18. `gemini 3.1 pro` — **2,8/10** — clair, non livrable
19. `qwen3.6` — **2,5/10** — lisible, incohérent
20. `qwen3.6-35b-fp8-freetoken` — **2,0/10** — moteur incomplet
21. `qwen35` — **1,0/10** — erreurs multiples
22. `gemma4` — **0,8/10** — prototype incohérent

## 5. Recommandation de fusion

La base recommandée reste `gpt-5.6 sol medium`, complétée par le réveil Timer1 immédiat et le frontend sobre de `kimi_k3_max`, puis par les invariants et diagnostics de `claude opus 5 medium`. Le build réellement validé de `muse_wonder_1.0_medium` confirme qu'une NMI FRC1 événementielle, `GPOS/GPOC` et un partage par mots atomiques sont viables sur le core ESP8266 3.1.2, mais son stockage CRC ne doit pas être repris. Il faut choisir **un seul** ordonnanceur, pas fusionner plusieurs moteurs concurrents.

### Fonctionnalités IoT à emprunter

- **Temps-réel et génération :** conserver l'ordonnanceur one-shot de `gpt-5.6 sol medium`, décrit comme « *le temps armé est soustrait aux deux compteurs, puis la prochaine échéance minimale est programmée* ». Y intégrer le réarmement de `kimi_k3_max`, où « *`setOutput()` réarme Timer1 dès l'activation d'un canal* », afin d'éviter la seconde d'attente observée dans `kimi k3`. `muse_wonder_1.0_medium` apporte une preuve de compilation sur ESP8266 3.1.2 et une résolution de 0,2 µs; reprendre son principe événementiel, mais ajouter un compteur de saturation à l'anneau de capture.
- **Phase et publication ISR :** reprendre de `claude opus 5 medium` l'adoption de fréquence « *à une frontière de front* », le `nextEdge += halfPeriod` et le seqlock. `Grok_4.7_medium` confirme l'intérêt d'un snapshot pair/impair et améliore Grok 4.6 en ne rafraîchissant le fail-safe qu'après une période valide. `Gemini_4_Argon_medium` confirme l'intérêt des échéances absolues, mais son partage de `lastEdgeCycle`, période et consigne n'est pas transactionnel. Figer le core, inspecter la map IRAM et interdire toute reprise sur une vieille consigne.
- **Boot readiness :** aucune proposition ne couvre complètement le démarrage. Ajouter les états `BOOT_WAITING`, `VALIDATING`, `RUNNING`, `STALE` et `FAULT`, une fenêtre de grâce mesurée, puis éventuellement un tach nominal strictement borné. Le constat commun est explicite : chez `kimi_k3_max`, « *aucun signal nominal provisoire n’est produit avant deux fronts réels* ».
- **Protocole et fail-safe :** garder le calcul de `gpt-5.6 sol medium`, « *`RPM = 30 000 000 / période_us`* », et la règle démontrée par `Grok_4.7_medium` : « *`lastGood` n'est mis à jour qu'après validation d'une période* ». Exiger trois périodes cohérentes avant activation, invalider période/consigne au stall et utiliser un timeout adaptatif court. Cette invalidation évite précisément le défaut de `Gemini_4_Argon_medium`, où le premier front de reprise relance l'ancienne fréquence. Ne jamais reprendre les tach parallèles de `glm5.3` ou `glm_5.2_high`.
- **Filtres :** combiner médiane courte, rejet relatif d'outlier et EMA réinitialisée. Le rejet relatif puis confirmation de `muse_wonder_1.0_medium` est intéressant, mais son slew de 2 % doit être déclenché par **nouvelle période**, pas par tour de `loop()`. L'EMA 75/25 hors ISR de `kimi_k3_max` reste préférable aux filtres de `composer 2.5 prompt from claude`, qui « *réinsère continuellement la même valeur* » à chaque tour de boucle.
- **WiFi et Web :** retenir AP+STA non bloquant, `WiFi.persistent(false)`, reconnexion, mDNS, page `PROGMEM` et statut par buffer borné. Ajouter authentification, CSRF, secret AP unique et débit limité. Ne jamais reproduire `claude opus 5 medium`, où « *`/api/status` renvoie en clair `apPass`* ».
- **Persistance :** utiliser `magic`, version, taille, CRC32 et validation `isfinite()` de `gpt-5.6 sol medium`, puis ajouter double-slot et compteur de génération. Calculer la couverture avec `offsetof(Config, crc)`, jamais avec `sizeof(Config)-sizeof(crc)` : `muse_wonder_1.0_medium` démontre qu'un padding terminal peut sinon inclure le CRC lui-même et invalider chaque reboot. N'écrire que lors d'un changement.
- **Hardware :** partir de l'OR-ing Schottky ou MOSFET, du buck-boost qualifié, des entrées 3,3 V et des sorties NPN des leaders. Ne jamais reprendre Q1–Q4 de `Grok_4.7_medium`, le pré-régulateur résistif de `Gemini_4_Argon_medium`, ni le dessin de `glm_5.2_high` qui relie les deux rails avant ses diodes et suppose qu'un LM2596 maintient 5 V depuis environ 5 V. Conserver aussi l'instruction de `gpt-5.6 sol medium` selon laquelle « *les deux fils tach ne doivent jamais être reliés* » et qualifier précisément PTC, TVS, transistor et convertisseur.

### Bonnes pratiques de qualité code à intégrer

- **Architecture modulaire :** reprendre la séparation logique de `claude opus 5 medium` entre « *primitives bas niveau, moteur, état partagé, application, stockage, Web et réseau* », puis sortir HTML/CSS/JS du fichier principal.
- **Concurrence explicite :** utiliser snapshots versionnés ou le seqlock pair/impair bien appliqué par `Grok_4.7_medium`; restaurer l'état d'interruption précédent et ne jamais supposer que `noInterrupts()` protège une NMI.
- **ISR minimales :** aucune allocation, chaîne, flottant, `digitalWrite()`, persistance ou opération réseau. Les accès transitifs `micros()` et `timer1_write()` doivent être validés sur la toolchain verrouillée.
- **Heap maîtrisé :** retenir `PROGMEM`/`send_P` et les buffers `char` bornés de `kimi_k3_max`, dont le statut est « *produit dans un buffer `char` borné avec `snprintf`* ». Exposer heap courant et minimum historique.
- **Frontières sûres :** validation serveur stricte, `isfinite()`, longueurs, terminaison NUL, échappement HTML/JSON, erreurs HTTP visibles, authentification et CSRF; aucun secret ne doit être relu par API.
- **Build reproductible :** ajouter `platformio.ini` ou une commande `arduino-cli`, verrouiller core et bibliothèques et compiler en CI. Le build concluant de `muse_wonder_1.0_medium` sur `esp8266:esp8266:d1_mini` 3.1.2 montre le niveau de preuve attendu; la même étape aurait bloqué les parenthèses et GPIO inexistants de `glm_5.2_high`, les littéraux `0xDEYE`, les API ESP32 et les appels Timer1 inexistants.
- **Tests :** couvrir PPR, demi-période, saturation, wraparound, overflow 32 bits, taille/padding de structure, CRC après sauvegarde-rechargement, premier front, reprise, glitches, anneau plein et stall. Un test de round-trip aurait détecté le CRC de Muse; un test de période nominale aurait montré que GLM 5.2 rejette tous ses échantillons. Mesurer fréquence et jitter au scope sous trafic WiFi et écriture flash.
- **Documentation vérifiable :** documenter unités, polarité NPN, limites et résultats mesurés; bannir les promesses « zéro jitter » ou « totalement isolé » sans captures ni protocole reproductible.
