from pathlib import Path
import shutil,hashlib,json
root=Path(__file__).resolve().parents[2]
out=root/'result_analysis'
archive=out/'_archives'/'2026-11-17'/'avant_gpt6.1sol_medium'
archive.mkdir(parents=True,exist_ok=True)
manifest=[]
for name in ['RECAPITULATIF_2026-11-17.md','METHODOLOGIE_2026-11-17.md','_audit/report_validation.json']:
 p=out/name;dest=archive/p.name
 if not dest.exists():shutil.copy2(p,dest)
 manifest.append(dict(file=dest.name,sha256=hashlib.sha256(dest.read_bytes()).hexdigest()))
(archive/'manifest.json').write_text(json.dumps(manifest,indent=2),encoding='utf-8')
p=out/'_audit'/'review_summary.py'
s=p.read_text(encoding='utf-8')
s=s.replace("s+='**28 dossiers examinés : 27 firmwares évalués, 1 dossier vide.** Les 28 rapports individuels remplacent les anciens constats ; huit analyses antérieures et un ancien tableau sont [archivés](./_archives/2026-11-17/). Aucun code compétiteur n’a été modifié.\\n\\n'", "s+='**29 dossiers examinés : 28 firmwares évalués, 1 dossier vide.** Ajout de `gpt6.1sol_medium` après la réanalyse initiale. Ses sources seules ont été lues ; le classement compare les 29 rapports, sans relire les autres firmwares. Les 28 rapports précédents gardent leurs notes. [État avant cet ajout](./_archives/2026-11-17/avant_gpt6.1sol_medium/) et [archives historiques](./_archives/2026-11-17/). Aucun code compétiteur modifié.\\n\\n'")
s=s.replace("was=r['path'] in old or r['name']=='deepseek4 flash' and 'analyse_deepseek4_flash.md' in old", "was=r['name']!='gpt6.1sol_medium'")
s=s.replace("('Existait — réécrite' if was else 'Absente — créée')", "('Existait — conservée' if was else 'Absente — créée')")
s=s.replace("s+='Huit analyses antérieures sont conservées dans l’archive. Pour DeepSeek, l’ancien nom utilisait un underscore (`analyse_deepseek4_flash.md`) ; le nouveau respecte exactement le nom du dossier.\\n\\n'", "s+='Ces 28 rapports existaient avant cet ajout et sont conservés sans réanalyse des sources. Les huit analyses historiques d’avant la réanalyse initiale restent archivées ; DeepSeek utilise désormais le nom exact du dossier avec espace.\\n\\n'")
s=s.replace("s+='Les huit analyses ci-dessus ont été entièrement refaites ; vingt fichiers supplémentaires ont été créés :\\n\\n'", "s+='Un seul nouveau rapport a été créé lors de cet ajout :\\n\\n'")
s=s.replace('28 rapports individuels, la méthodologie','29 rapports individuels, la méthodologie')
s=s.replace("s+='Il s’agit de nouvelles analyses des sources, pas d’un recalage à partir des anciennes notes. La comparaison finale relit exclusivement les 28 rapports écrits. Aucun ajustement postérieur de ces nouvelles notes n’a été nécessaire ; aucun astérisque n’est donc ajouté. Une différence avec le classement historique s’explique par les constats documentés, notamment les erreurs Timer1, débordements, arrêts/reprises et circuits d’alimentation. Les tests annoncés par un auteur ne sont pas comptés comme reproduits.\\n\\n'", "s+='Aucune note existante n’a été modifiée ; pas d’astérisque. La comparaison finale relit uniquement les 29 analyses. Le nouveau [gpt6.1sol_medium](analyse_gpt6.1sol_medium.md), noté 8,0/10 et 8,4/10 en code, gagne la deuxième place dans les deux classements. Son rapport décrit « Authentification HTTP obligatoire et protection CSRF » et un timeout dans l’ISR : deux avantages face aux limites HTTP/arrêt en loop décrites pour [GPT-5.6_Sol_xhigh](analyse_GPT-5.6_Sol_xhigh.md). Il reste derrière [Claude_Opus_5.5_xhigh](analyse_Claude_Opus_5.5_xhigh.md), dont le rapport place « la génération et le timeout » dans la NMI. La sauvegarde indéfiniment différée et l’IRQ masquable du nouveau justifient cet écart. Ces différences de notes restent un jugement d’audit, sans précision métrologique au dixième.\\n\\n'")
s=s.replace("recommendations=[", "recommendations=[\n('gpt6.1sol_medium','Authentification et protection des mutations','« protection CSRF »','Reprendre authorized, les mutations POST avec jeton de session et les secrets absents du status. Remplacer la clé initiale déductible du SSID ; HTTP Basic ne chiffre pas le trafic.'),\n('gpt6.1sol_medium','Traçabilité et lecture des erreurs','« Le résultat commit est contrôlé »','Conserver le retour de sauvegarde, les états RAM/en attente/échec et les empreintes de source/binaire. Sa politique de report jusqu’à l’arrêt des deux voies ne satisfait pas une persistance fiable en ventilation continue : ne pas la copier sans autre stockage ou moteur autonome.'),")
s=s.replace("practices=[", "practices=[\n('gpt6.1sol_medium','« parsing strtof complet »','Réutiliser les validations centralisées, les chaînes bornées, le snapshot et l’invalidation du signal. Ajouter des tests automatisés du scheduler, distincts des journaux de compilation et contrôles de sommes.'),")
s=s.replace("s+='Aucune compilation ESP8266 ni mesure sur carte/onduleur réalisée : outil Arduino absent de l’environnement détecté.", "s+='Pour le nouveau compétiteur, [14 contrôles supplémentaires](_audit/gpt6.1sol_medium_verification.json) passent : deux SHA-256, onze vérifications numériques/de transitions et une syntaxe JavaScript. Son journal annonce une compilation core 3.1.2 réussie ; les empreintes source/binaire livrées concordent. Cette compilation n’a pas été reproduite par l’auditeur. Aucune compilation ESP8266 ni mesure sur carte/onduleur réalisée par l’auditeur : outil Arduino absent de l’environnement détecté.")
p.write_text(s,encoding='utf-8')
method=out/'METHODOLOGIE_2026-11-17.md'
t=method.read_text(encoding='utf-8')
t+='''

## Ajout ultérieur le 17 novembre 2026 — gpt6.1sol_medium

Le corpus comporte désormais 29 dossiers, 28 firmwares et un dossier vide. Seul le nouveau compétiteur a été analysé ; les 28 rapports précédents et leurs notes sont conservés. L’ancien récapitulatif et l’ancienne méthodologie sont sauvegardés dans `_archives/2026-11-17/avant_gpt6.1sol_medium/`. La comparaison du nouveau avec les autres s’appuie exclusivement sur les rapports.

Le nouveau livre dix fichiers, dont journal, symboles IRAM, instructions de reproduction, binaire et manifeste. Deux empreintes SHA-256 concordent avec le sketch et le binaire présents. Cela vérifie l’intégrité par rapport au manifeste, pas une compilation indépendante ni une correspondance binaire/source démontrée par rebuild. Le journal rapporte un build ESP8266 3.1.2 réussi ; l’auditeur ne l’a pas reproduit. Quatorze contrôles supplémentaires passent : deux empreintes, onze calculs/modèles et un script JavaScript. Voir `_audit/gpt6.1sol_medium_verification.json`. Les 19 contrôles arithmétiques et 19 scripts de la revue initiale restent inchangés.

L’alimentation proposée par le nouveau a été confrontée à la [fiche Pololu S13V10F5](https://www.pololu.com/product/4083). La nature masquable de Timer1 est confrontée au cœur 3.1.2 déjà référencé. Pas d’essai physique. Les 68 empreintes du corpus initial et les dix fichiers du nouvel ajout sont vérifiés séparément.
'''
if '## Ajout ultérieur le 17 novembre 2026' not in method.read_text(encoding='utf-8'):method.write_text(t,encoding='utf-8')
print('Summary script and methodology updated; previous output preserved.')
