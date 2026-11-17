import re, pathlib, subprocess, json
root=pathlib.Path(__file__).resolve().parent
if root.name=='_audit': root=root.parent.parent
out=root/'result_analysis'/'_audit'/'javascript'
out.mkdir(parents=True,exist_ok=True)
node=pathlib.Path(r'C:\Users\sebas\.cache\codex-runtimes\codex-primary-runtime\dependencies\node\bin\node.exe')
results=[]
for p in sorted((root/'competitors').rglob('*')):
 if p.suffix not in ('.ino','.cpp'): continue
 text=p.read_text(encoding='utf-8-sig')
 # Only syntactically delimited C++ raw strings. Ordinary String-built pages
 # need a C++ renderer and are intentionally not treated as JavaScript input.
 matches=re.finditer(r'R"([^ ()\\\t\r\n]*)\(([\s\S]*?)\)\1"',text)
 html=''.join(m.group(2) for m in matches)
 for i,m in enumerate(re.finditer(r'<script[^>]*>([\s\S]*?)</script>',html,re.I)):
  dest=out/(p.relative_to(root/'competitors').parts[0]+f'_{i}.js')
  dest.write_text(m.group(1),encoding='utf-8')
  r=subprocess.run([str(node),'--check',str(dest)],capture_output=True,text=True,encoding='utf-8',errors='replace')
  results.append(dict(source=p.relative_to(root).as_posix(),script=i,exit_code=r.returncode,output=r.stdout+r.stderr))
(out.parent/'javascript_checks.json').write_text(json.dumps(results,ensure_ascii=False,indent=2),encoding='utf-8')
print(json.dumps(dict(scripts=len(results),passed=sum(x['exit_code']==0 for x in results),failed=[x for x in results if x['exit_code']]),ensure_ascii=False))
