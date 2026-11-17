
async function poll(){
 try{
  const d=await (await fetch('/api/status')).json();
  for(let i=0;i<2;i++){
   document.getElementById('r'+i).textContent=d.ch[i].rin;
   document.getElementById('o'+i).textContent=d.ch[i].rout;
   document.getElementById('k'+i).textContent=d.ch[i].ratio;
   const a=document.getElementById('a'+i);
   a.textContent=d.ch[i].act?'ACTIF':'attente';
   a.className=d.ch[i].act?'on':'off';
   const f=document.getElementById('f'+(i+1));
   if(document.activeElement!==f)f.value=d.ch[i].ratio;
  }
  document.getElementById('ap').textContent=d.ap;
  document.getElementById('sta').textContent=d.sta_ok?d.sta:'non connect\u00e9';
  document.getElementById('up').textContent=d.up;
 }catch(e){}
}
setInterval(poll,1000);poll();
