from pathlib import Path
import hashlib,json,re,sys
sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).resolve().parents[2]
out=root/'result_analysis'
reports=sorted(out.glob('analyse_*.md'))
dirs=sorted(p.name for p in (root/'competitors').iterdir() if p.is_dir())
assert sorted(p.stem.removeprefix('analyse_') for p in reports)==dirs
required=['**Sources analysées**','## ✅ Points techniques positifs','### Qualité de code générale (positif)','## ❌ Points techniques négatifs','### Qualité de code générale (négatif)','## ⭐ Note globale','## ⭐ Note qualité de code']
missing=[]
for p in out.glob('*.md'):
 s=p.read_text(encoding='utf-8')
 assert '[...]' not in s,p
 if p in reports:
  for title in required: assert title in s,(p,title)
  assert len(re.findall(r'^- \*\*',s,re.M))==9,p
 for a,b in re.findall(r'\]\((?:<([^>]+)>|([^)]*))\)',s):
  target=a or b
  if re.match(r'^(https?://|#)',target):continue
  destination=(p.parent/target.split('#')[0]).resolve()
  if not destination.exists():missing.append(dict(file=p.name,target=target))
assert not missing,missing
archive=out/'_archives'/'2026-11-17'
manifest=json.loads((archive/'manifest.json').read_text(encoding='utf-8'))
for x in manifest:assert hashlib.sha256((archive/x['file']).read_bytes()).hexdigest()==x['sha256'],x
verification=json.loads((out/'_audit'/'verification.json').read_text(encoding='utf-8'))
for x in verification['sources']:assert hashlib.sha256((root/x['path']).read_bytes()).hexdigest()==x['sha256'],x['path']
summary=(out/'RECAPITULATIF_2026-11-17.md').read_text(encoding='utf-8')
table=summary.split('## 2. Tableau récapitulatif',1)[1].split('## 3.',1)[0]
rows=[x for x in table.splitlines() if x.startswith('| [')]
assert len(rows)==27,len(rows)
scores=[float(x.split('|')[2].strip().removesuffix('/10').replace(',','.')) for x in rows]
assert scores==sorted(scores,reverse=True),scores
assert 'qwen3.8-UD-IQ4_XS_KV4-4' not in table
js=json.loads((out/'_audit'/'javascript_checks.json').read_text(encoding='utf-8'))
assert len(js)==19 and all(x['exit_code']==0 for x in js)
result=dict(individual_reports=len(reports),ranked_reports=len(rows),unscored_reports=1,archive_hashes_verified=len(manifest),unchanged_source_hashes=len(verification['sources']),local_links='all resolve',arithmetic_checks=len(verification['checks']),javascript_checks=len(js))
(out/'_audit'/'report_validation.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
