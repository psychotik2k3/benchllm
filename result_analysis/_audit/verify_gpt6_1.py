from pathlib import Path
import json, hashlib, re, subprocess, sys, math
sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).resolve().parents[2]
folder=root/'competitors'/'gpt6.1sol_medium'
out=Path(__file__).resolve().parent
checks=[]
expected={'deye_fan/deye_fan.ino':'9A5B816624CB6346A71F644D95DD963D75544639389A565CAE0D83BB4675CEED','verification/deye_fan.ino.bin':'B1E0DFDA018A96AD46C5B989CABEDA9F336DAC9B30F7E750761AB538339D647D'}
for name,claimed in expected.items():
 actual=hashlib.sha256((folder/name).read_bytes()).hexdigest().upper()
 checks.append(dict(name='SHA256 fourni : '+name,pass_=actual==claimed,actual=actual,claimed=claimed))
def check(name,actual,expected):
 checks.append(dict(name=name,actual=actual,expected=expected,pass_=abs(actual-expected)<1e-8))
check('Produit Q12 maximal + arrondi reste en uint32',500000*4096+4096,2048004096)
check('Timer DIV16 : réveil maximum en microsecondes',2000*5/5,2000)
check('Acquisition 4 fronts à 1600 RPM : délai minimal après premier front',3*30000000/1600/1000,56.25)
check('Acquisition 4 fronts à 3000 RPM : délai minimal après premier front',3*30000000/3000/1000,30)
check('Timeout nominal 1600 RPM',max(300000,3*18750),300000)
check('Timeout maximal',max(300000,3*500000),1500000)
check('Plafond de sortie à demi-période 250 us',15000000/250,60000)
# The no-new-input path invalidates the capture at 1.5 s, before a micros wrap.
seen=True;good=3;period=18750
age=1500001
if seen and age>1500000: seen=False;good=0;period=0
check('Capture invalidée avant wrap, sans nouveau front',int(seen or good or period),0)
for p,ratio,target in [(18750,2,4688),(10000,2,2500),(10000,8,625)]:
 inv=int(4096/ratio+0.5); half=max(250,(p*inv+4096)>>13)
 check(f'Demi-période p={p}, ratio={ratio}',half,target)
source=(folder/'deye_fan/deye_fan.ino').read_text(encoding='utf-8-sig')
html=re.search(r'R"HTML\(([\s\S]*?)\)HTML"',source)[1]
js=re.search(r'<script>([\s\S]*?)</script>',html)[1]
dest=out/'javascript'/'gpt6.1sol_medium_0.js'
dest.write_text(js,encoding='utf-8')
node=r'C:\Users\sebas\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe'
r=subprocess.run([node,'--check',str(dest)],capture_output=True,text=True,encoding='utf-8',errors='replace')
checks.append(dict(name='Syntaxe JavaScript embarqué',pass_=r.returncode==0,exit_code=r.returncode,output=r.stdout+r.stderr))
inventory=[dict(path=p.relative_to(root).as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(folder.rglob('*')) if p.is_file()]
result=dict(scope='Empreintes des livrables, modèles arithmétiques et contrôle de syntaxe JS ; aucune recompilation ni exécution firmware.',checks=checks,sources=inventory)
(out/'gpt6.1sol_medium_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
assert all(c['pass_'] for c in checks),[c for c in checks if not c['pass_']]
print(json.dumps(dict(checks=len(checks),passed=len(checks),source_files=len(inventory)),ensure_ascii=False))
