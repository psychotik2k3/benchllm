"""Behavioral checks of the reviewed tick-level equations, not an HDL simulation."""
from pathlib import Path
import hashlib,json,re,subprocess,sys
sys.stdout.reconfigure(encoding='utf-8')
root=Path(__file__).resolve().parents[2]
folder=root/'competitors'/'gpt6.2_astra_medium'
audit=Path(__file__).resolve().parent
checks=[]
def check(name,ok,detail):checks.append(dict(name=name,pass_=bool(ok),detail=detail))
class Channel:
 def __init__(self):
  self.samples=7;self.level=1;self.armed=0;self.seen_out=0
  self.age=self.out_age=self.phase=self.limit=self.drive=self.period=self.generated=0
 def step(self,tach,ratio):
  old=self.__dict__.copy();n=old.copy()
  n['samples']=((old['samples']<<1)|tach)&7
  if old['samples']==7:n['level']=1
  if old['samples']==0:n['level']=0
  if old['age']<62500:n['age']=old['age']+1
  if old['level'] and old['samples']==0 and (not old['armed'] or old['age']>=249):
   n['period']=old['age']+1 if old['armed'] and old['age']<62499 else 0
   n['armed']=1;n['age']=0
  elif old['age']>=62499:n['armed']=0;n['period']=0
  if old['period']==0 or ratio==0:
   for k in ('drive','phase','limit','out_age','seen_out','generated'):n[k]=0
  else:
   if old['out_age']!=65535:n['out_age']=old['out_age']+1
   if old['limit']==0:n['limit']=old['period']*100
   elif old['phase']+ratio*2>=old['limit']:
    n['phase']=old['phase']+ratio*2-old['limit'];n['limit']=old['period']*100
    n['drive']=1-old['drive']
    if not old['drive']:
     if old['seen_out']:n['generated']=old['out_age']+1
     n['seen_out']=1;n['out_age']=0
   else:n['phase']=old['phase']+ratio*2
  self.__dict__.update(n)
  return self.drive!=old['drive']
def run(period,ratio,nticks=150000):
 c=Channel();edges=[];rises=[]
 for t in range(nticks):
  tach=int((t%period)<period//2)
  if c.step(tach,ratio) and t>nticks//2:
   edges.append(t)
   if c.drive:rises.append(t)
 halves=[b-a for a,b in zip(edges,edges[1:])]
 return c,halves,rises
a,ha,ra=run(2500,250)
b,hb,rb=run(1250,175)
check('50 Hz, ratio 2.50 : périodes numériques',a.period==2500 and set(ha)=={500} and a.generated==1000,dict(period=a.period,half_ticks=sorted(set(ha)),generated=a.generated,rpm=3750000/a.generated))
check('100 Hz, ratio 1.75 : accumulateur fractionnaire',b.period==1250 and set(hb)=={357,358} and max(hb)-min(hb)==1,dict(period=b.period,half_ticks=sorted(set(hb)),generated=b.generated))
for level in (0,1):
 c,_,_=run(1250,250,10000)
 for _ in range(62502):c.step(level,250)
 check(f'Perte entrée figée à {level}',c.period==0 and c.drive==0 and c.generated==0,dict(age=c.age,period=c.period,drive=c.drive))
for ticks in (1,2):
 c=Channel()
 for _ in range(20):c.step(1,250)
 for _ in range(ticks):c.step(0,250)
 for _ in range(20):c.step(1,250)
 check(f'Glitch bas de {ticks} ticks rejeté',not c.armed and c.period==0,dict(armed=c.armed,period=c.period))
check('Largeur arithmétique : seuil maximal',62499*100<2**23,dict(target_max=62499*100,bits=23))
check('Plage RPM entrée / synthèse théorique',3750000/250==15000 and 3750000/250*8==120000,dict(max_input_rpm=15000,max_output_rpm=120000,min_half_us=125))
def packet(a,b):
 p=[0xA5,a>>8,a&255,b>>8,b&255]
 p+=[p[0]^p[1]^p[2]^p[3]^p[4],0,0,0]
 return p
def accepted(p):
 a=(p[1]<<8)|p[2];b=(p[3]<<8)|p[4]
 return len(p)==9 and p[0]==0xa5 and p[5]==(p[0]^p[1]^p[2]^p[3]^p[4]) and p[6:]==[0,0,0] and 100<=a<=800 and 100<=b<=800
p=packet(250,175)
check('Trame commande nominale acceptée',accepted(p),p)
bad=p.copy();bad[2]^=1
check('Erreur de bit isolée détectée par XOR',not accepted(bad),bad)
bad2=p.copy();bad2[2]^=1;bad2[4]^=1
check('Limite du XOR : deux erreurs peuvent passer',accepted(bad2),dict(packet=bad2,ratios=[(bad2[1]<<8)|bad2[2],(bad2[3]<<8)|bad2[4]]))
# Bus signals are absent from channel.step: a paused command transfer does not
# enter the tick datapath. This is structural/model evidence, not electrical QA.
check('Moteur séparé des signaux SPI dans le modèle',set(Channel(). __dict__).isdisjoint({'cs','sck','mosi'}),'Le RTL channel ne reçoit que clk, tick, tach et ratio ; r9/r6 sont mémorisés dans top.')
source=(folder/'deye_fan/deye_fan.ino').read_text(encoding='utf-8-sig')
html=re.search(r'R"HTML\(([\s\S]*?)\)HTML"',source)[1]
js=re.search(r'<script>([\s\S]*?)</script>',html)[1]
dest=audit/'javascript'/'gpt6.2_astra_medium_0.js';dest.write_text(js,encoding='utf-8')
node=r'C:\Users\sebas\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe'
r=subprocess.run([node,'--check',str(dest)],capture_output=True,text=True,encoding='utf-8',errors='replace')
check('Syntaxe JavaScript embarqué',r.returncode==0,r.stdout+r.stderr)
pins={line.split()[1]:int(line.split()[2]) for line in (folder/'fpga/icebreaker.pcf').read_text().splitlines()}
reference=dict(clk=35,tach9=4,tach6=2,q9=47,q6=45,cs=43,sck=38,mosi=34,miso=31,led_n=37)
check('Brochage PCF confronté à la référence iCEBreaker',pins==reference,pins)
inventory=[dict(path=p.relative_to(root).as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()) for p in sorted(folder.rglob('*')) if p.is_file()]
result=dict(scope='Modèle Python des transitions tick/NBA, contrôles de protocole, brochage, syntaxe JS et empreintes. Aucun compilateur Verilog ni build bitstream/Arduino exécuté. Synchroniseurs 12 MHz et délais physiques non simulés.',checks=checks,sources=inventory)
(audit/'gpt6.2_astra_medium_verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
assert all(c['pass_'] for c in checks),[c for c in checks if not c['pass_']]
print(json.dumps(dict(checks=len(checks),passed=len(checks),source_files=len(inventory)),ensure_ascii=False))
