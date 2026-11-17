import re,sys,pathlib
sys.stdout.reconfigure(encoding='utf-8')
base=pathlib.Path(__file__).resolve().parent
if base.name=='_audit': base=base.parent.parent
root=base/'competitors'
dirs=sorted(root.iterdir(),key=lambda p:p.name.casefold())
mode=sys.argv[1]
for n in map(int,sys.argv[2:]):
 d=dirs[n-1]; print(f'=== {n} {d.name} ===')
 files=sorted(p for p in d.rglob('*') if p.is_file() and p.suffix.lower() in (('.ino','.cpp','.h') if mode in ('code','ui') else ('.md','.txt')))
 for p in files:
  print('FILE',p.relative_to(root))
  s=p.read_text(encoding='utf-8-sig'); lines=s.splitlines()
  if mode=='code':
   # Preserve strings and line locations; remove comments only.
   pat=r'R"([^ ()\\\t\r\n]*)\([\s\S]*?\)\1"|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|/\*[\s\S]*?\*/|//[^\n]*'
   def sub(m):
    t=m.group()
    if t.startswith(('/*','//')): return '\n'*t.count('\n')
    if t.startswith('R"'):
     return '"<RAW HTML: inspected separately>"'+'\n'*t.count('\n')
    return t
   s=re.sub(pat,sub,s)
   for i,l in enumerate(s.splitlines(),1):
    if l.strip(): print(f'{i}:{l.strip()}')
  elif mode=='docs':
   fence=False
   for i,l in enumerate(lines,1):
    if l.strip().startswith('```'): fence=not fence; continue
    if l.strip() and not fence and not re.match(r'^\s*[|+│├└┌┐┘─┬┴┤╔╚═]',l): print(f'{i}:{l}')
  elif mode=='ui':
   for i,l in enumerate(lines,1):
    if re.search(r'fetch\(|activeElement|focus|innerHTML|textContent|setInterval|PROGMEM|send_P|authenticate|requestAuthentication',l): print(f'{i}:{l}')
