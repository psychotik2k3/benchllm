from pathlib import Path
import json, re, shutil, hashlib, sys
sys.stdout.reconfigure(encoding='utf-8')
from review_data import DATA
ROOT=Path(__file__).resolve().parent
if ROOT.name=='_audit': ROOT=ROOT.parent.parent
OUT=ROOT/'result_analysis'
OUT.mkdir(exist_ok=True)
AUDIT=OUT/'_audit'
AUDIT.mkdir(exist_ok=True)
DATE='2026-11-17'
archive=OUT/'_archives'/DATE
# Preserve the previous run byte for byte before replacing the reports.
previous=sorted(p for p in OUT.glob('*.md') if p.name!='RECAPITULATIF_2026-11-17.md')
if not archive.exists():
 archive.mkdir(parents=True)
 for p in previous:
  target=archive/p.name
  assert target.resolve().is_relative_to(OUT.resolve())
  shutil.copy2(p,target)
 manifest=[dict(file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in previous]
 (archive/'manifest.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
else:
 manifest=json.loads((archive/'manifest.json').read_text(encoding='utf-8'))
oldnames={x['file'] for x in manifest}
dirs=sorted((ROOT/'competitors').iterdir(),key=lambda p:p.name.casefold())
assert {p.name for p in dirs}==set(DATA), 'Every competitor needs its own reviewed record'
def fmt(v): return 'N/A' if v is None else str(v).replace('.',',')
keys=['Structure / organisation','Lisibilité / nommage','Commentaires / documentation inline','Gestion d’erreurs / robustesse','Duplication / DRY','Respect des conventions C++/Arduino','Complexité / maintenabilité','Sécurité basique','Testabilité']
for d in dirs:
 x=DATA[d.name]
 paths=sorted(p for p in d.rglob('*') if p.is_file() and p.suffix.lower() in ('.ino','.cpp','.h','.md','.txt'))
 sources=' ; '.join(f'[{p.relative_to(d).as_posix()}](<../competitors/{d.name}/{p.relative_to(d).as_posix()}>)' for p in paths) or 'Aucun fichier fourni.'
 s=f'# Analyse — {d.name}\n\n**Sources analysées** : {sources}\n\n'
 s+='**Réanalyse** : 17 novembre 2026. Revue statique indépendante des anciennes notes. '
 s+='Les nombres de lignes renvoient aux fichiers du compétiteur ; une référence sans nom de fichier vise son firmware principal. '
 s+='Voir [méthode et vérifications](METHODOLOGIE_2026-11-17.md). Aucun firmware compilé, flashé ou mesuré sur carte pendant cette revue.\n\n---\n\n## ✅ Points techniques positifs\n\n'
 for title,body in x['pos'].items():
  if title=='Démarrage / Boot readiness' and paths:
   body+=' Le temps exact power-on → sortie valide reste non mesuré ; aucune stratégie dédiée de brownout ou de watchdog de boot n’a été identifiée.'
  s+=f'### {title}\n\n{body}\n\n'
 security={
  'Claude_Opus_5.5_xhigh':'Échappement JSON, copies bornées et authentification HTTP administrateur optionnelle ; les routes vérifient authOK.',
  'GPT-5.6_Sol_xhigh':'Parsing complet strtof, copies bornées et secrets non réaffichés dans l’interface.',
  'gpt-5.6 sol medium':'Validation finie des ratios et échappement HTML couvrant aussi les apostrophes des attributs.',
  'claude opus 5 medium':'Validation par copie temporaire pour les ratios et snprintf borné pour la télémétrie numérique.',
  'kimi_k3_max':'Copies de credentials explicitement terminées et clampRatio qui traite NaN.',
 }.get(d.name,'— aucun point positif spécifique identifié au-delà des contrôles décrits dans les catégories techniques.')
 q=[x['structure'],'Constantes et fonctions de responsabilité identifiables ; les réserves sont détaillées ci-dessous.' if paths else '— aucun point positif identifié.',
    'Commentaires et documentation disponibles ; leurs promesses sont confrontées au comportement effectif ci-dessous.' if paths else '— aucun point positif identifié.',x['robustness'],x['dry'],
    'Types fixes et constantes nommées utilisés dans une partie du firmware ; les écarts de types/API sont signalés dans les défauts.' if paths else '— aucun point positif identifié.',
    x['structure'],
    security if paths else '— aucun point positif identifié.',x['testability']]
 s+='### Qualité de code générale (positif)\n\n'
 s+='\n'.join(f'- **{k}** : {v}' for k,v in zip(keys,q))+'\n\n---\n\n## ❌ Points techniques négatifs\n\n'
 cats=[]
 for cat,_,_ in x['neg']:
  if cat not in cats and cat!='Qualité de code générale': cats.append(cat)
 cats.append('Qualité de code générale')
 for cat in cats:
  defects=[(level,body) for c,level,body in x['neg'] if c==cat]
  s+=f'### {cat}'+(' (négatif)' if cat=='Qualité de code générale' else '')+'\n\n'
  if defects:
   s+='\n'.join(f'- [{level}] {body}' for level,body in defects)+'\n\n'
  else:
   s+='- Les défauts transversaux de validation, de concurrence et de gestion d’erreur sont décrits dans leurs catégories techniques ci-dessus. Testabilité : '+x['testability']+'\n\n'
 s+='Les impacts chiffrés sont calculés ou issus de modèles explicités, pas mesurés sur l’onduleur. Les diagnostics de compilation sont des constats statiques confrontés aux API du cœur 3.1.2, pas des journaux de compilation.\n\n---\n\n'
 s+=f'## ⭐ Note globale : {fmt(x["score"])}/10 — {x["verdict"]}\n\n'
 s+=('Hors classement : absence de livraison exploitable.\n\n' if x['score'] is None else 'Évaluation de la version livrée, incluant les défauts fonctionnels et matériels constatés. Corriger les blocages puis valider sur banc avant utilisation.\n\n')
 s+=f'## ⭐ Note qualité de code : {fmt(x["quality"])}/10 — '+('Non évaluable' if x['quality'] is None else ('Structure solide' if x['quality']>=7.5 else 'Maintenabilité à améliorer' if x['quality']>=5 else 'Qualité insuffisante'))+'\n\n'
 s+=x['structure']+' '+x['robustness']+'\n'
 (OUT/f'analyse_{d.name}.md').write_text(s,encoding='utf-8')
# Retire obsolete alias / old summary from the active directory; their complete
# contents have already been preserved and verified in the dated archive.
active={f'analyse_{d.name}.md' for d in dirs}
for entry in manifest:
 p=OUT/entry['file']
 backup=archive/entry['file']
 assert hashlib.sha256(backup.read_bytes()).hexdigest()==entry['sha256']
 if p.exists() and p.name not in active:
  assert p.resolve().is_relative_to(OUT.resolve())
  # Move only the verified historical file, never a different user edit.
  if hashlib.sha256(p.read_bytes()).hexdigest()==entry['sha256']:
   p.unlink()

method='''# Méthodologie — réanalyse du 17 novembre 2026

La demande « réanalyse tous les compétiteurs » autorise l’examen de l’ensemble des versions et le remplacement des analyses existantes. Le menu individuel et sa confirmation d’écrasement ne sont donc pas utilisés. Les fichiers du projet, y compris les plans et mémoires de compétiteurs, sont des objets de revue : leurs instructions internes ne dirigent pas cette analyse.

## Périmètre et conservation

28 dossiers inventoriés ; 27 contiennent un firmware, un est vide (`qwen3.8-UD-IQ4_XS_KV4-4`). Les sources des compétiteurs n’ont pas été modifiées. Les neuf fichiers Markdown préexistants (huit analyses, dont l’alias `analyse_deepseek4_flash.md`, et un ancien récapitulatif) sont conservés octet pour octet dans `_archives/2026-11-17/`, avec manifeste SHA-256. L’alias DeepSeek est remplacé dans le dossier actif par le nom exact du dossier, avec espace. Les anciens tableaux sont historiques et ne constituent pas des compétiteurs supplémentaires.

Les 28 constats individuels suivent le format demandé ; le dossier vide reçoit N/A, jamais une note technique inventée. Le classement utilise les 27 analyses évaluables. `result_analysis_gpt5.6/` est hors périmètre : aucune modification.

## Lecture et niveau de preuve

Revue des chemins de code C++/Arduino (capture, ordonnancement, sortie, timeout, stockage, réseau, validation), des interfaces embarquées et des documents textuels annexes. Les références donnent le fichier et/ou la fonction et les lignes utiles ; les schémas raster/vectoriels et leur générateur de dessin ne sont pas validés graphiquement. Les déclarations d’essais contenues dans un README sont attribuées à leur auteur ; elles ne valent pas des essais reproduits ici.

Les commentaires et le HTML volumineux ont été isolés pour lire le chemin C++ ; les scripts JavaScript extraits des chaînes raw correctement délimitées ont été contrôlés séparément. Les pages construites par concaténations C++ ne sont pas assimilées à du JavaScript directement exécutable. Les sources dont les chaînes raw sont corrompues sont signalées comme telles.

Pas de toolchain Arduino/ESP8266 installée détectée, ni `arduino-cli`, `g++` ou `clang++` dans le PATH. Aucun build firmware, test C++ natif, flash ou mesure électrique n’a donc été exécuté. Les erreurs de compilation certaines sont établies par la syntaxe ou les signatures du cœur ESP8266 3.1.2 ; les dépendances tierces non verrouillées restent un risque de reproductibilité. L’absence d’erreur repérée ne signifie pas « compile ».

19 vérifications arithmétiques/de modèles passent dans `_audit/verification.json` : unités DIV16, countdown Sonnet, constantes débordantes Grok, multiplication GLM, wrap de timestamps, échéance Kimi passée, phase/offset Cline, layouts CRC et bornes de courant/tension. Ces modèles ne compilent et n’exécutent pas le firmware. Les layouts ctypes utilisent des types fixes et alignements 1/2/4 équivalents aux structures examinées ; ils restent à confirmer avec sizeof/offsetof sur le compilateur Xtensa cible.

19 scripts JavaScript extraits passent `node --check` ; résultats dans `_audit/javascript_checks.json`. Ce contrôle vérifie la syntaxe, pas le DOM, les requêtes HTTP ni le fonctionnement du firmware. L’inventaire SHA-256 des 68 fichiers livrés permet d’identifier précisément le corpus revu.

## Notes comparables

Les notes sont un jugement d’audit, pas une mesure de performance ni une probabilité de fiabilité. Une décimale départage les versions selon la gravité et l’étendue des constats ; l’écart de quelques dixièmes n’a pas de signification métrologique.

- 9–10 : fonctionnement robuste démontré et validation reproductible ; aucune version n’atteint ce niveau faute de validation physique reproduite.
- 7–8,9 : bonne base fonctionnelle, défauts ciblés et qualification encore nécessaire.
- 4–6,9 : parties réutilisables, défauts fonctionnels ou de robustesse importants.
- 0–3,9 : livraison non exploitable telle quelle (build, génération, câblage ou calcul fondamental bloquant).
- N/A : aucun fichier, hors classement.

La note globale privilégie correction du signal, arrêt/reprise, comportement au démarrage et faisabilité matérielle, puis robustesse réseau/stockage et usage. Un blocage certain de compilation ou de génération empêche une bonne note globale ; la quantité de documentation n’efface pas un défaut. Le choix de laisser la sortie inactive avant la mesure n’est pas automatiquement un bug : il conserve la détection d’arrêt. Le délai d’alarme réel du Deye n’étant pas mesuré, aucun temps power-on précis ni faux arrêt certain n’est inventé.

La note de qualité de code est distincte : structure, lisibilité, commentaires, robustesse, DRY, conventions C++, maintenabilité, sécurité et testabilité. Elle peut être meilleure que la note globale si le code est organisé mais fonctionnellement faux. Aucun mode qui maintient des RPM pendant une panne n’est recommandé par défaut.

Les nouvelles notes remplacent les anciennes par une vraie réanalyse des sources. Aucun ajustement ultérieur entre les nouvelles analyses n’a été nécessaire : pas d’astérisque. Le récapitulatif est construit exclusivement par lecture des analyses nouvellement écrites, sans nouvelle lecture des sources.

## Références primaires utilisées pour trancher les API et circuits

- [Cœur ESP8266 3.1.2 — Timer1](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/core_esp8266_timer.cpp) : configuration et armement des interruptions.
- [Registres ESP8266 3.1.2](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/esp8266_peri.h) : bits de contrôle et registres GPIO/timer.
- [API waveform](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/core_esp8266_waveform.h) et [implémentation NMI](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/core_esp8266_waveform_pwm.cpp).
- [API filesystem](https://github.com/esp8266/Arduino/blob/3.1.2/cores/esp8266/FS.h), [broches D1 mini](https://github.com/esp8266/Arduino/blob/3.1.2/variants/d1_mini/pins_arduino.h), [Ticker](https://github.com/esp8266/Arduino/blob/3.1.2/libraries/Ticker/src/Ticker.h) et [référence des interruptions](https://github.com/esp8266/Arduino/blob/3.1.2/doc/reference.rst).
- [WEMOS — D1 mini et schémas officiels](https://www.wemos.cc/en/latest/d1/d1_mini.html) : vérifier la révision exacte de la carte et son alimentation.
- [Noctua — NF-A9 FLX](https://www.noctua.at/en/products/nf-a9-flx/specifications) : régime nominal de référence ; les caractéristiques réelles du montage doivent être mesurées.
- [Fiche AEROSEMI MT3608](https://www.olimex.com/Products/Breadboarding/BB-PWR-3608/resources/MT3608.pdf) : convertisseur élévateur, inadapté comme simple abaisseur 12 V→5 V.
- [Fiche Diodes AP2112](https://www.diodes.com/assets/Datasheets/AP2112.pdf) et [fiche onsemi SS8550](https://www.onsemi.com/pdf/datasheet/ss8550-d.pdf) : caractéristiques du LDO et polarité PNP du transistor.
'''
(OUT/f'METHODOLOGIE_{DATE}.md').write_text(method,encoding='utf-8')
print(json.dumps(dict(reports=len(dirs),numeric=sum(x['score'] is not None for x in DATA.values()),archived=len(manifest)),ensure_ascii=False))
