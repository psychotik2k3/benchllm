from pathlib import Path
import hashlib,json,re,sys
sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).resolve().parents[2];out=root/'result_analysis';audit=out/'_audit'
reports=sorted(out.glob('analyse_*.md'))
dirs=sorted(p.name for p in (root/'competitors').iterdir() if p.is_dir())
assert sorted(p.stem.removeprefix('analyse_') for p in reports)==dirs
assert len(reports)==30
required=['**Sources analysées**','## ✅ Points techniques positifs','### Qualité de code générale (positif)','## ❌ Points techniques négatifs','### Qualité de code générale (négatif)','## ⭐ Note globale','## ⭐ Note qualité de code']
for p in out.glob('*.md'):
 s=p.read_text(encoding='utf-8')
 if p in reports:
  for title in required:assert title in s,(p.name,title)
  assert len(re.findall(r'^- \*\*',s,re.M))==9,p.name
  assert '[...]' not in s,p.name
 for a,b in re.findall(r'\]\((?:<([^>]+)>|([^)]*))\)',s):
  target=a or b
  if re.match(r'^(https?://|#)',target):continue
  assert (p.parent/target.split('#')[0]).resolve().exists(),(p.name,target)
previous=json.loads((audit/'before_gpt6.2_astra_medium.json').read_text(encoding='utf-8'))
assert len(previous)==29
for x in previous:assert hashlib.sha256((out/x['file']).read_bytes()).hexdigest()==x['sha256'],x['file']
source_count=0
for fname in ['verification.json','gpt6.1sol_medium_verification.json','gpt6.2_astra_medium_verification.json']:
 v=json.loads((audit/fname).read_text(encoding='utf-8'))
 assert all(x['pass_'] for x in v['checks']),fname
 for x in v['sources']:
  assert hashlib.sha256((root/x['path']).read_bytes()).hexdigest()==x['sha256'],x['path']
  source_count+=1
s=(out/'RECAPITULATIF_2026-11-18.md').read_text(encoding='utf-8')
table=s.split('## 2. Tableau récapitulatif',1)[1].split('## 3.',1)[0]
rows=[l for l in table.splitlines() if l.startswith('| [')]
assert len(rows)==29
scores=[float(l.split('|')[2].strip().removesuffix('/10').replace(',','.')) for l in rows]
assert scores==sorted(scores,reverse=True)
assert 'gpt6.2_astra_medium' in rows[0]
quality=s.split('### Classement par qualité de code',1)[1].split('## 4.',1)[0]
assert next(l for l in quality.splitlines() if 'gpt6.2_astra_medium' in l).startswith('4.')
result=dict(reports=30,ranked=29,unscored=1,previous_reports_unchanged=29,source_hashes_verified=source_count,local_links='all resolve',new_checks=14,new_global_rank=1,new_code_rank=4)
(audit/'report_validation_2026-11-18.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(result,ensure_ascii=False))
