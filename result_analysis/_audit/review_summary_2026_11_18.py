from pathlib import Path
import re,json,hashlib,sys
sys.stdout.reconfigure(encoding='utf-8')
ROOT=Path(__file__).resolve().parent
if ROOT.name=='_audit': ROOT=ROOT.parent.parent
OUT=ROOT/'result_analysis'
archive=OUT/'_archives'/'2026-11-17'
manifest=json.loads((archive/'manifest.json').read_text(encoding='utf-8'))
records=[]
# Comparison stage: read analysis documents only, never competitor sources.
for p in sorted(OUT.glob('analyse_*.md')):
 t=p.read_text(encoding='utf-8')
 name=re.search(r'^# Analyse — (.+)$',t,re.M)[1]
 g=re.search(r'## ⭐ Note globale : (N/A|[0-9.,]+)/10 — (.+)',t)
 q=re.search(r'## ⭐ Note qualité de code : (N/A|[0-9.,]+)/10 — (.+)',t)
 value=lambda s: None if s=='N/A' else float(s.replace(',','.'))
 pos=t.split('## ✅ Points techniques positifs',1)[1].split('### Qualité de code générale',1)[0]
 sections=re.findall(r'### ([^\n]+)\n\n([\s\S]*?)(?=\n### |\Z)',pos)
 neg=t.split('## ❌ Points techniques négatifs',1)[1].split('Les impacts chiffrés',1)[0]
 defects=re.findall(r'^- \[(critique|majeur|mineur)\] (.+)$',neg,re.M)
 records.append(dict(name=name,path=p.name,global_=value(g[1]),quality=value(q[1]),verdict=g[2],positive=sections,negative=defects,text=t))
ranked=sorted((r for r in records if r['global_'] is not None),key=lambda r:(-r['global_'],-r['quality'],r['name'].casefold()))
quality_ranked=sorted(ranked,key=lambda r:(-r['quality'],-r['global_'],r['name'].casefold()))
na=[r for r in records if r['global_'] is None]
num=lambda v:str(v).replace('.',',')
link=lambda r:f'[{r["name"]}](<{r["path"]}>)'
def brief(s,n=190):
 s=s.replace('|','/').replace('\n',' ')
 if len(s)>n:s=s[:n].rsplit(' ',1)[0]+'…'
 return s
s='# Classement global — mise à jour du 18 novembre 2026\n\n'
s+='**30 dossiers : 29 versions évaluées et un dossier vide.** Nouvel ajout : `gpt6.2_astra_medium`, avec moteur FPGA autonome. Seules ses sources ont été lues ; les 29 rapports précédents et leurs notes restent inchangés. [Classement avant cet ajout](RECAPITULATIF_2026-11-17.md). Le choix de tête exige du matériel FPGA supplémentaire ; pour un **Wemos seul**, Claude_Opus_5.5_xhigh reste premier à 8,6/10.\n\n'
s+='**Meilleure base pour ce projet : '+link(ranked[0])+f' ({num(ranked[0]["global_"])}/10 ; code {num(ranked[0]["quality"])}/10).** Ce choix porte sur la version livrée après revue statique ; aucune qualification sur onduleur n’est affirmée. [Méthode, limites et références primaires](METHODOLOGIE_2026-11-18.md).\n\n'
s+='## 1. Inventaire des analyses\n\n| Dossier compétiteur | Analyse disponible | Situation avant ce run |\n|---|---|---|\n'
old={m['file'] for m in manifest if m['file'].startswith('analyse_')}
existing=[];new=[]
for r in sorted(records,key=lambda r:r['name'].casefold()):
 was=r['name']!='gpt6.2_astra_medium'
 (existing if was else new).append(r)
 s+=f'| {r["name"]} | ✅ [{r["path"]}](<{r["path"]}>)'+(' — constat N/A' if r in na else '')+' | '+('Existait — conservée' if was else 'Absente — créée')+' |\n'
s+='\n### ✅ Analyses existantes avant ce run\n\n'+ '\n'.join('- '+link(r) for r in existing)+'\n\n'
s+='Ces 29 rapports existaient avant cet ajout et sont conservés sans relecture des sources. Les anciens résultats restent disponibles dans le récapitulatif du 17 novembre et les archives de la réanalyse initiale.\n\n'
s+='### 🆕 Analyses produites lors de ce run\n\n'
s+='Un seul nouveau rapport a été créé lors de cet ajout :\n\n'+'\n'.join('- '+link(r)+(' — constat sans code, N/A' if r in na else '') for r in new)+'\n\n'
s+='### ⚠️ Compétiteurs sans analyse de code disponible\n\n'
s+='- '+link(na[0])+': dossier vide ; un constat est écrit, mais aucune note n’est inventée. Exclu des tableaux et classements chiffrés.\n\n'
s+='Les 30 analyses individuelles alimentent le classement ; la méthodologie courante et ce récapitulatif expliquent le périmètre. Les documents datés du 17 novembre sont historiques, pas des concurrents supplémentaires. Aucun score historique ne se mélange aux notes actives.\n\n'
s+='## 🔍 Notes révisées et justification\n\n'
s+='Aucune note existante modifiée, aucun astérisque. La comparaison ne lit que les rapports. [gpt6.2_astra_medium](analyse_gpt6.2_astra_medium.md) obtient 8,8/10 au global et 8,1/10 en code : premier au global, quatrième en qualité de code. Son rapport établit « Capture et génération matérielles indépendantes du Wemos », tandis que [Claude_Opus_5.5_xhigh](analyse_Claude_Opus_5.5_xhigh.md) décrit une chaîne en NMI ESP8266. Le nouveau permet une sauvegarde pendant la ventilation ; [gpt6.1sol_medium](analyse_gpt6.1sol_medium.md) conserve sa réserve de sauvegarde indéfiniment différée. Ces progrès fonctionnels justifient le rang global, mais ne remplacent pas les preuves physiques. Le rapport nouveau signale l’absence de testbench/timing et les limites des optocoupleurs : il ne dépasse donc pas les meilleures structures du classement code. Ce classement autorise les ajouts matériels ; une contrainte Wemos seul exclut la variante FPGA du choix final.\n\n'
s+='## 2. Tableau récapitulatif\n\n'
s+='Tri par note globale décroissante ; les puces résument les rapports liés et ne remplacent pas leurs réserves.\n\n'
s+='| Version | Note globale | Note qualité code | ✅ Points positifs | ❌ Points négatifs |\n|---|---:|---:|---|---|\n'
for r in ranked:
 pp=r['positive'][:5]
 positive='<br>'.join('• '+brief(title+' : '+body,155) for title,body in pp)
 # Preserve the original order of importance used in the report sections.
 order={'critique':0,'majeur':1,'mineur':2}
 nn=sorted(r['negative'],key=lambda p:order[p[0]])[:5]
 negative='<br>'.join('• ['+level+'] '+brief(body,175) for level,body in nn)
 s+=f'| {link(r)} | {num(r["global_"])}/10 | {num(r["quality"])}/10 | {positive} | {negative} |\n'
s+='\n## 3. Classements séparés\n\n### Classement par note globale\n\n'
for i,r in enumerate(ranked,1):s+=f'{i}. {link(r)} — {num(r["global_"])}/10 — {r["verdict"]}.\n'
s+='\n### Classement par qualité de code\n\n'
for i,r in enumerate(quality_ranked,1):s+=f'{i}. {link(r)} — {num(r["quality"])}/10 — '+('structure solide' if r['quality']>=7.5 else 'maintenabilité à améliorer' if r['quality']>=5 else 'qualité insuffisante')+'.\n'
s+='\n## 4. Recommandation de fusion\n\n'
s+='### Fonctionnalités IoT à emprunter\n\n'
recommendations=[
('gpt6.2_astra_medium','Conversion matériellement autonome','« Capture et génération matérielles indépendantes du Wemos »','Choisir ce moteur FPGA si le matériel supplémentaire est accepté : synchroniseurs, qualification, deux accumulateurs, expiration saturante et registres de ratios. La sauvegarde ESP ne cadence plus les fronts. Vérifier le RTL par simulation, reconstruire le bitstream, qualifier timing et optocoupleurs avant déploiement.'),
('gpt6.1sol_medium','Authentification et protection des mutations','« protection CSRF »','Reprendre authorized, les mutations POST avec jeton de session et les secrets absents du status. Remplacer la clé initiale déductible du SSID ; HTTP Basic ne chiffre pas le trafic.'),
('gpt6.1sol_medium','Traçabilité et lecture des erreurs','« Le résultat commit est contrôlé »','Conserver le retour de sauvegarde, les états RAM/en attente/échec et les empreintes de source/binaire. Sa politique de report jusqu’à l’arrêt des deux voies ne satisfait pas une persistance fiable en ventilation continue : ne pas la copier sans autre stockage ou moteur autonome.'),
('Claude_Opus_5.5_xhigh','Alternative Wemos seul : temps réel et perte de signal','« la génération et le timeout »','Le rapport décrit une chaîne entière dans la NMI, amorcée avant WiFi, et un arrêt à 500 ms avec capture réamorcée. C’est la base prioritaire parmi les variantes Wemos seul ; le moteur FPGA du nouvel ajout est l’alternative avec matériel supplémentaire. Conserver les échéances absolues et les diagnostics, puis mesurer le collecteur sous trafic et pendant sauvegarde.'),
('GPT-5.6_Sol_xhigh','Protocole et moteur de sortie','« moteur NMI waveform du cœur réutilisé »','Si la maintenabilité prime sur la NMI personnalisée, ce moteur est l’alternative : il faut choisir un seul propriétaire Timer1. Ne pas brancher ensemble deux moteurs concurrents. Conserver les deux voies et les fonctions PPR/ratio, corriger l’amorçage à très basse vitesse et rendre l’arrêt indépendant du serveur.'),
('claude opus 5 medium','PPR, limites et diagnostics','« PPR entrée/sortie séparés, défaut 2 »','Reprendre les paramètres PPR, l’écrêtage de RPM et les compteurs de rejets. Déplacer l’expiration du signal dans le moteur, supprimer les secrets du status et échapper le JSON avant intégration.'),
('Claude_Opus_5.5_xhigh','WiFi et mises à jour','« machine d’état STA »','Emprunter les tentatives STA bornées, AP de secours, scan asynchrone et reconfiguration différée. Activer une authentification administrateur par défaut et fournir une stratégie d’identifiants propre à chaque carte.'),
('GPT-5.6_Sol_xhigh','Alimentation et interfaces électriques','« PTC, TVS, OR-ing, filtrage »','Le schéma explicite les protections, le budget courant, le buck/buck-boost selon la tension disponible et un seul tach par paire. Dimensionner sur les caractéristiques réellement mesurées, la révision de la carte et la température ambiante ; ne pas reprendre les schémas qui appliquent 12 V au 5V ou proposent un MT3608 abaisseur.'),
('composer 2.5 prompt from claude','Format de persistance','« LittleFS et JSON »','Le format est facile à inspecter, mais son implémentation n’est pas à copier telle quelle : terminer les chaînes, vérifier les écritures et enregistrer via fichier temporaire puis rename avec secours. Pour l’EEPROM, conserver plutôt le CRC par offsetof du leader et remonter le résultat commit.')]
byname={r['name']:r for r in records}
for name,title,quote,body in recommendations:
 r=byname[name]
 # Validate that every quoted excerpt really exists in the saved analysis.
 excerpt=quote[2:-2]
 assert excerpt in r['text'],(name,excerpt)
 s+=f'- **{title} — {link(r)}** : {quote}. {body}\n'
s+='\n### Bonnes pratiques qualité code à intégrer\n\n'
practices=[
('gpt6.2_astra_medium','« module HDL channel instancié deux fois »','Conserver la séparation protocole/moteur et la validation de commande avant publication atomique. Paramétrer les constantes temporelles, ajouter CRC/compteur de séquence et testbench ; valider également les réponses SPI et les chaînes chargées.'),
('gpt6.1sol_medium','« parsing strtof complet »','Réutiliser les validations centralisées, les chaînes bornées, le snapshot et l’invalidation du signal. Ajouter des tests automatisés du scheduler, distincts des journaux de compilation et contrôles de sommes.'),
('GPT-5.6_Sol_xhigh','« tach_math.h isole les fonctions pures »','Réutiliser ce découpage et le parsing strict strtof. Les assertions fournies constituent un point de départ ; ajouter les scénarios de wrap, perte/reprise, fréquence CPU, limites d’offset et armement du timer. Aucun test C++ natif n’a été exécuté dans ce run.'),
('gpt-5.6 sol medium','« mise à jour transactionnelle des paramètres HTTP »','Valider tous les champs dans une copie avant de publier la configuration, et distinguer « appliqué en RAM » de « sauvegardé avec succès ». Ne pas reprendre son décompte de temps virtuel ni ses timestamps invalidés de façon incomplète.'),
('Claude_Opus_5.5_xhigh','« propriété des données »','Garder un seul écrivain par état du moteur et des commandes atomiques vers celui-ci ; isoler le code critique du réseau. Faire produire une map par le build pour confirmer IRAM/DRAM et le coût de la NMI.'),
('kimi_k3_max','« protection explicite document.activeElement »','Préserver le champ en cours de saisie lors du polling. Réutiliser ce comportement UI indépendamment des défauts de son calendrier.'),
('GPT-5.6_Sol_xhigh','« parseur strtof exigeant toute la chaîne »','Appliquer le même principe aux entiers, ratios et offsets ; refuser NaN, infini, suffixes et paramètres hors limites au lieu de les convertir silencieusement.')]
for name,quote,body in practices:
 r=byname[name];excerpt=quote[2:-2];assert excerpt in r['text'],(name,excerpt)
 s+=f'- **{link(r)}** : {quote}. {body}\n'
s+='\n## 5. Vérifications et limites\n\n'
s+='Les [19 reproductions arithmétiques et modèles](_audit/verification.json) passent, ainsi que les [19 contrôles de syntaxe JavaScript](_audit/javascript_checks.json). Les rapports couvrent notamment les constantes Grok débordantes (timeout 27,877 ms au lieu de 350 ms), le calcul GLM qui rejette toutes les périodes, le facteur ×16 de Gemini-free et les CRC incluant leur propre champ.\n\n'
s+='Pour l’ajout du 17 novembre gpt6.1sol_medium, [14 contrôles supplémentaires](_audit/gpt6.1sol_medium_verification.json) passent : deux SHA-256, onze vérifications numériques/de transitions et une syntaxe JavaScript. Son journal annonce une compilation core 3.1.2 réussie ; les empreintes source/binaire livrées concordent. Cette compilation n’a pas été reproduite par l’auditeur. Aucune compilation ESP8266 ni mesure sur carte/onduleur réalisée par l’auditeur : outil Arduino absent de l’environnement détecté. L’absence de défaut statique repéré ne certifie pas un build. Avant toute utilisation, les correctifs doivent être compilés pour la carte exacte, puis validés au banc : premier signal, deux canaux, arrêt/reprise, débordement des horloges, sauvegarde flash et charge réseau. Les seuils réels de détection Deye restent à mesurer.\n'
s+='Le nouvel ajout [gpt6.2_astra_medium](analyse_gpt6.2_astra_medium.md) passe [14 contrôles](_audit/gpt6.2_astra_medium_verification.json) : modèles numériques, pertes/glitches, intégrité de trames, brochage et syntaxe JavaScript. Ce sont des modèles Python, pas une simulation Verilog. Le bitstream fourni est seulement inventorié et haché ; ni synthèse/place-and-route ni build Arduino reproduits. Les mesures électriques et thermiques restent absentes.\n\n'
(OUT/'RECAPITULATIF_2026-11-18.md').write_text(s,encoding='utf-8')
print(json.dumps(dict(ranked=len(ranked),unscored=len(na),top=[{k:r[k] for k in ('name','global_','quality')} for r in ranked[:3]],bytes=len(s.encode('utf-8'))),ensure_ascii=False))
