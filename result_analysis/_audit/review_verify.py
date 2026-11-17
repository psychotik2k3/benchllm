import ctypes as c, json, pathlib, math, hashlib, sys
sys.stdout.reconfigure(encoding='utf-8')
ROOT=pathlib.Path(__file__).resolve().parent
if ROOT.name=='_audit': ROOT=ROOT.parent.parent
OUT=ROOT/'result_analysis'/'_audit'
OUT.mkdir(parents=True,exist_ok=True)
checks=[]
def check(name, value, expected, evidence):
    ok=abs(value-expected)<1e-6 if isinstance(expected,(int,float)) else value==expected
    checks.append(dict(name=name,observed=value,expected=expected,pass_=ok,evidence=evidence))
u32=lambda x:x&0xffffffff
check('DIV16 : 10 ticks = 2 us',10/5,2,'composer 2.5 : setupTimer1')
check('DIV16 : 4000 ticks = 800 us',4000/5,800,'gemini-free : onTimerISR')
check('Sonnet : N=10, 11 ticks par demi-période',15_000_000/(11*100),13636.363636363636,'claude-sonnet5-free : onTimer1ISR')
check('Grok MAX_IN_CYC en uint32',u32(300*80_000_000)//1000,2525163,'Grok_4.7_medium : constante MAX_IN_CYC')
check('Grok LOST_CYC en uint32',u32(350*80_000_000)//1000,2230196,'Grok_4.7_medium : constante LOST_CYC')
check('GLM : overflow à 1600 RPM',u32(1_500_000*1_000_000)//80_000_000,13,'glm_5.2_high : traiterCanal')
check('GLM : quotient maximal après overflow',0xffffffff//80_000_000,53,'glm_5.2_high : période minimale exigée =1000 us')
check('Argon : fenêtre périmée redevient récente',u32((1<<32)+100_000),100000,'Gemini_4_Argon_medium : timeout non invalidant')
check('Kimi max : échéance passée devient très lointaine',min(u32(1000-1100)//16,8388607),8388607,'kimi_k3_max : scheduleNext')
inc=(7500*2*65536)//(60*40000)
period=65536//inc
pulse=min(period//3,200)
ticks=math.ceil(65536/inc)+pulse
check('Cline : 7500 RPM, pas NCO',inc,409,'strata cline : outConfigure')
check('Cline : période avec phase suspendue',ticks,214,'strata cline : isrTimer1')
check('Cline : RPM physiques avec largeur additionnelle',30*40000/ticks,5607.476635514019,'modèle déterministe du chemin nominal, hors IRQ perdues')
check('Cline : offset négatif devient plafond',min(u32(1000-3000),20000),20000,'strata cline : target -= offset puis clamp')
class Deep(c.Structure):
 _fields_=[('magic',c.c_uint8*4),('ver',c.c_uint8),('staSsid',c.c_char*33),('staPw',c.c_char*65),('apSsid',c.c_char*33),('apPw',c.c_char*65),('ratio',c.c_uint16*2),('crc',c.c_uint8)]
class Muse(c.Structure):
 _fields_=[('magic',c.c_uint32),('version',c.c_uint16),('ratio',c.c_float*2),('staSsid',c.c_char*33),('staPass',c.c_char*65),('apSsid',c.c_char*33),('apPass',c.c_char*65),('crc',c.c_uint16)]
class Ch(c.Structure):
 _fields_=[('ratio',c.c_uint16),('mode',c.c_uint8),('enabled',c.c_uint8)]
class Strata(c.Structure):
 _fields_=[('magic',c.c_uint32),('version',c.c_uint32),('len',c.c_uint32),('ch',Ch*2),('ppr',c.c_uint8),('master',c.c_uint8),('direct',c.c_uint8),('rfu',c.c_uint8),('ssid',c.c_char*33),('pass_',c.c_char*65),('apssid',c.c_char*33),('appass',c.c_char*65),('crc',c.c_uint16)]
for cls, expected in [(Deep,(206,208)),(Muse,(212,216)),(Strata,(220,224))]:
 check('Layout ABI 32 bits : '+cls.__name__,[cls.crc.offset,c.sizeof(cls)],list(expected),'types fixes, alignements 1/2/4 ; modèle ctypes, pas sizeof exécuté sur Xtensa')
check('Swift : courant max après résistance 100k',12/100000,0.00012,'schema_electronique.txt : limite supérieure même sans chute LDO')
check('Argon : tension disponible après 33 ohms à 300mA',12-33*0.3,2.1,'SCHEMA_ET_ARCHITECTURE.md : borne statique, transitoire non simulé')
checks.append(dict(name='Outils de compilation',observed='arduino-cli, g++, clang++ absents du PATH ; aucune compilation firmware effectuée',expected='limite explicitée',pass_=True,evidence='inventaire outils de la session'))
source_inventory=[]
for p in sorted((ROOT/'competitors').rglob('*')):
 if p.is_file():
  source_inventory.append(dict(path=p.relative_to(ROOT).as_posix(),bytes=p.stat().st_size,sha256=hashlib.sha256(p.read_bytes()).hexdigest()))
result=dict(scope='Reproductions arithmétiques et modèles de transitions ; ne sont pas des tests du firmware compilé ni du matériel.',checks=checks,sources=source_inventory)
(OUT/'verification.json').write_text(json.dumps(result,ensure_ascii=False,indent=2),encoding='utf-8')
assert all(x['pass_'] for x in checks),[x for x in checks if not x['pass_']]
print(json.dumps(dict(checks=len(checks),passed=len(checks),source_files=len(source_inventory),layouts={cls.__name__:[cls.crc.offset,c.sizeof(cls)] for cls in (Deep,Muse,Strata)}),ensure_ascii=False))
