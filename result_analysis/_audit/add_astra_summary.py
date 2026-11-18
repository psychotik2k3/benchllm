from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[2]
out=root/'result_analysis';audit=out/'_audit'
manifest_path=audit/'before_gpt6.2_astra_medium.json'
if not manifest_path.exists():
 manifest=[dict(file=p.name,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in out.glob('analyse_*.md') if p.name!='analyse_gpt6.2_astra_medium.md']
 manifest_path.write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
s=(audit/'review_summary.py').read_text(encoding='utf-8')
lines=s.splitlines()
for i,line in enumerate(lines):
 if line.startswith("s='# Réanalyse globale"):
  lines[i]="s='# Classement global — mise à jour du 18 novembre 2026\\n\\n'"
 elif line.startswith("s+='**29 dossiers examinés"):
  lines[i]="s+='**30 dossiers : 29 versions évaluées et un dossier vide.** Nouvel ajout : `gpt6.2_astra_medium`, avec moteur FPGA autonome. Seules ses sources ont été lues ; les 29 rapports précédents et leurs notes restent inchangés. [Classement avant cet ajout](RECAPITULATIF_2026-11-17.md). Le choix de tête exige du matériel FPGA supplémentaire ; pour un **Wemos seul**, Claude_Opus_5.5_xhigh reste premier à 8,6/10.\\n\\n'"
 elif line.startswith("s+='Ces 28 rapports existaient"):
  lines[i]="s+='Ces 29 rapports existaient avant cet ajout et sont conservés sans relecture des sources. Les anciens résultats restent disponibles dans le récapitulatif du 17 novembre et les archives de la réanalyse initiale.\\n\\n'"
 elif line.startswith("s+='Tous les fichiers Markdown actifs"):
  lines[i]="s+='Les 30 analyses individuelles alimentent le classement ; la méthodologie courante et ce récapitulatif expliquent le périmètre. Les documents datés du 17 novembre sont historiques, pas des concurrents supplémentaires. Aucun score historique ne se mélange aux notes actives.\\n\\n'"
 elif line.startswith("s+='Aucune note existante n’a été modifiée"):
  lines[i]="s+='Aucune note existante modifiée, aucun astérisque. La comparaison ne lit que les rapports. [gpt6.2_astra_medium](analyse_gpt6.2_astra_medium.md) obtient 8,8/10 au global et 8,1/10 en code : premier au global, quatrième en qualité de code. Son rapport établit « Capture et génération matérielles indépendantes du Wemos », tandis que [Claude_Opus_5.5_xhigh](analyse_Claude_Opus_5.5_xhigh.md) décrit une chaîne en NMI ESP8266. Le nouveau permet une sauvegarde pendant la ventilation ; [gpt6.1sol_medium](analyse_gpt6.1sol_medium.md) conserve sa réserve de sauvegarde indéfiniment différée. Ces progrès fonctionnels justifient le rang global, mais ne remplacent pas les preuves physiques. Le rapport nouveau signale l’absence de testbench/timing et les limites des optocoupleurs : il ne dépasse donc pas les meilleures structures du classement code. Ce classement autorise les ajouts matériels ; une contrainte Wemos seul exclut la variante FPGA du choix final.\\n\\n'"
s='\n'.join(lines)+'\n'
s=s.replace("was=r['name']!='gpt6.1sol_medium'", "was=r['name']!='gpt6.2_astra_medium'")
s=s.replace('METHODOLOGIE_2026-11-17.md','METHODOLOGIE_2026-11-18.md')
s=s.replace("(OUT/'RECAPITULATIF_2026-11-17.md').write_text", "(OUT/'RECAPITULATIF_2026-11-18.md').write_text")
s=s.replace("recommendations=[", "recommendations=[\n('gpt6.2_astra_medium','Conversion matériellement autonome','« Capture et génération matérielles indépendantes du Wemos »','Choisir ce moteur FPGA si le matériel supplémentaire est accepté : synchroniseurs, qualification, deux accumulateurs, expiration saturante et registres de ratios. La sauvegarde ESP ne cadence plus les fronts. Vérifier le RTL par simulation, reconstruire le bitstream, qualifier timing et optocoupleurs avant déploiement.'),")
# Keep recommendations readable: all source excerpts below are asserted by the
# existing comparison script against the saved reports, not competitor code.
s=s.replace("('Claude_Opus_5.5_xhigh','Temps réel, démarrage et perte de signal'", "('Claude_Opus_5.5_xhigh','Alternative Wemos seul : temps réel et perte de signal'")
s=s.replace("C’est la base prioritaire pour éviter que HTTP ou une sauvegarde prolonge une fausse vitesse.", "C’est la base prioritaire parmi les variantes Wemos seul ; le moteur FPGA du nouvel ajout est l’alternative avec matériel supplémentaire.")
s=s.replace("practices=[", "practices=[\n('gpt6.2_astra_medium','« module HDL channel instancié deux fois »','Conserver la séparation protocole/moteur et la validation de commande avant publication atomique. Paramétrer les constantes temporelles, ajouter CRC/compteur de séquence et testbench ; valider également les réponses SPI et les chaînes chargées.'),")
insert="s+='Le nouvel ajout [gpt6.2_astra_medium](analyse_gpt6.2_astra_medium.md) passe [14 contrôles](_audit/gpt6.2_astra_medium_verification.json) : modèles numériques, pertes/glitches, intégrité de trames, brochage et syntaxe JavaScript. Ce sont des modèles Python, pas une simulation Verilog. Le bitstream fourni est seulement inventorié et haché ; ni synthèse/place-and-route ni build Arduino reproduits. Les mesures électriques et thermiques restent absentes.\\n\\n'\n"
s=s.replace("s+='Pour le nouveau compétiteur, [14 contrôles supplémentaires]", "s+='Pour l’ajout du 17 novembre gpt6.1sol_medium, [14 contrôles supplémentaires]")
s=s.replace("(OUT/'RECAPITULATIF_2026-11-18.md').write_text", insert+"(OUT/'RECAPITULATIF_2026-11-18.md').write_text")
(audit/'review_summary_2026_11_18.py').write_text(s,encoding='utf-8')
method=(out/'METHODOLOGIE_2026-11-17.md').read_text(encoding='utf-8')
method=method.replace('# Méthodologie — réanalyse du 17 novembre 2026','# Méthodologie — revue initiale du 17 et ajout du 18 novembre 2026',1)
method+='''

## Ajout du 18 novembre 2026 — gpt6.2_astra_medium

30 dossiers, 29 livraisons évaluées, un dossier vide. Seul ce nouveau dossier a été examiné (Arduino, Verilog, contraintes PCF et schéma ; bitstream haché, non désassemblé). Les 29 rapports précédents restent inchangés et le classement du 17 novembre est conservé. La comparaison ne lit que les rapports. Notes : 8,8/10 global, 8,1/10 code ; aucun ajustement des autres notes.

Cette architecture ajoute un FPGA autonome, deux DC/DC isolés, quatre optocoupleurs et un multiplexeur de sources : elle n’est pas une substitution de firmware Wemos seul. Le premier rang global est conditionné à l’acceptation de ce matériel ; Claude_Opus_5.5_xhigh reste le meilleur candidat Wemos seul. La présence d’un matériel autonome n’est pas une preuve de qualification électrique.

Quatorze contrôles supplémentaires passent dans `_audit/gpt6.2_astra_medium_verification.json` : équations du canal au pas de 8 µs, timeout, glitches courts, ratios/fractionnement, trames XOR, affectations PCF et syntaxe JS. Le modèle Python suit les valeurs d’état avant/après affectations non bloquantes, mais ne simule ni le RTL compilé, ni les synchroniseurs à 12 MHz, ni les optocoupleurs. Les outils iverilog/yosys/verilator/arduino-cli n’ont pas été trouvés dans le PATH lors de cet ajout. Aucun bitstream reconstruit, rapport de timing indépendant ou essai physique. Le contrôle XOR à deux erreurs accepté est un défaut reproduit, pas une validation de robustesse.

Références primaires consultées : [PCF officiel iCEBreaker](https://github.com/icebreaker-fpga/icebreaker-examples/blob/main/icebreaker/icebreaker.pcf), [TI LM66200](https://www.ti.com/lit/ds/symlink/lm66200.pdf), [Vishay VO617A](https://www.vishay.com/docs/83430/vo617a.pdf), [Traco TDN5WI](https://www.tracopower.com/sites/default/files/products/datasheets/tdn5wi_datasheet.pdf). Le PDF Traco a renvoyé 403 en ouverture directe ; les données indexées et le catalogue officiel ont permis de vérifier gamme et brochage. Tous les chiffres du moteur restent des calculs numériques, distincts de mesures au connecteur.
'''
(out/'METHODOLOGIE_2026-11-18.md').write_text(method,encoding='utf-8')
print('Prepared dated summary; older reports preserved.')
