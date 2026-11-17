
let token='',initialized=false,busy=false;
const $=id=>document.getElementById(id);
async function poll(){if(busy)return;busy=true;try{const r=await fetch('/api/status',{cache:'no-store'});if(!r.ok)throw Error('HTTP '+r.status);const s=await r.json();token=s.token;
if(!initialized){$('r9').value=s.ratio[0];$('r6').value=s.ratio[1];$('ap').value=s.ap;$('sta').value=s.sta;initialized=true;}
$('net').textContent='AP '+s.ap+' · STA '+s.ip+' · '+(s.connected?'connectée':'déconnectée');
$('rows').replaceChildren();s.channels.forEach((c,i)=>{let tr=document.createElement('tr');[i?'6 cm':'9 cm',c.rpm.toFixed(0),c.output.toFixed(0),c.active?(c.limited?'active / plafonnée':'active'):'attente'].forEach(x=>{let td=document.createElement('td');td.textContent=x;tr.append(td)});$('rows').append(tr)});
$('diag').textContent=s.channels.map((c,i)=>(i?'6':'9')+' cm : retard ISR max observé '+c.late+' µs ; resynchronisations '+c.missed).join(' · ');
$('storage').textContent=s.saveFailed?'Échec de sauvegarde : paramètres uniquement en RAM.':s.pending?'Sauvegarde en attente d’arrêt des deux entrées tach.':'Paramètres persistants / configuration chargée.';
}catch(e){$('net').textContent='Communication interrompue : '+e.message}finally{busy=false}}
async function submit(e,path){e.preventDefault();try{const r=await fetch(path,{method:'POST',headers:{'X-CSRF-Token':token},body:new URLSearchParams(new FormData(e.target))});const t=await r.text();if(!r.ok)throw Error(t);$('msg').textContent=t}catch(x){$('msg').textContent=x.message}}
$('ratios').onsubmit=e=>submit(e,'/api/ratios');$('wifi').onsubmit=e=>submit(e,'/api/wifi');poll();setInterval(poll,1000);
