
const $=id=>document.getElementById(id);
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
const fmt=(v,d=0)=>v==null?'—':(+v).toFixed(d);
let nets=[];

async function jget(u){const r=await fetch(u);return r.json();}
async function post(u,body){const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(body).toString()});return r.json();}

async function loadStatus(){
 try{
  const s=await jget('/api/status');
  $('sysinfo').innerHTML='STA : '+(s.sta.conn?esc(s.sta.ip)+' ('+s.sta.rssi+' dBm)':'non connecté')+' · AP : '+esc(s.ap.ip)+'<br>heap : '+Math.round(s.heap/1024)+' ko · marche : '+fmt(s.up/3600,1)+' h';
  for(let i=0;i<2;i++){
   const c=s.ch[i];
   $('rpmin'+i).textContent=c.valid?fmt(c.rpmIn)+' tr/min':'—';
   $('hzin'+i).textContent=c.valid?fmt(c.combHz,1)+' Hz':'—';
   $('rpmout'+i).textContent=c.run?fmt(c.rpmOut)+' tr/min':'—';
   $('hzout'+i).textContent=c.run?fmt(c.outHz,1)+' Hz':'—';
   const b=$('st'+i);
   b.className='badge '+(c.run?(c.blind?'b-blind':'b-run'):(c.en?'b-wait':'b-off'));
   b.textContent=c.run?(c.blind?'Simulation (cache-panne)':'Simulation'):(c.en?'Attente signal':'Désactivé');
  }
 }catch(e){$('sysinfo').textContent='Erreur de communication';}
}

async function loadCfg(){
 try{
  const c=await jget('/api/config');
  for(let i=0;i<2;i++){
   $('ratio'+i).value=c.ch[i].ratio;
   $('par'+i).value=c.ch[i].par;
   $('blindrpm'+i).value=c.ch[i].blindRpm;
   $('en'+i).checked=!!c.ch[i].en;
   $('blind'+i).checked=!!c.ch[i].blind;
  }
  $('sta_ssid').value=c.staSsid;
  $('ap_ssid').value=c.apSsid;
  $('hostname').value=c.hostname;
 }catch(e){}
}

async function saveCh(i){
 const b={};b['ch'+(i+1)+'_ratio']=$('ratio'+i).value;b['ch'+(i+1)+'_par']=$('par'+i).value;
 b['ch'+(i+1)+'_blindrpm']=$('blindrpm'+i).value;
 b['ch'+(i+1)+'_en']=$('en'+i).checked?1:0;b['ch'+(i+1)+'_blind']=$('blind'+i).checked?1:0;
 try{
  const r=await post('/api/config',b);
  $('m'+i).textContent=r.ok?'Enregistré ✓':(r.err||'Erreur');
  $('m'+i).className='msg '+(r.ok?'ok':'err');
 }catch(e){$('m'+i).textContent='Erreur réseau';$('m'+i).className='msg err';}
 setTimeout(()=>{$('m'+i).textContent='';},4000);
}

async function saveWifi(){
 try{
  const r=await post('/api/wifi',{sta_ssid:$('sta_ssid').value,sta_pass:$('sta_pass').value,ap_ssid:$('ap_ssid').value,ap_pass:$('ap_pass').value,hostname:$('hostname').value});
  $('mw').textContent=r.ok?'Enregistré — redémarrage…':(r.err||'Erreur');
  $('mw').className='msg '+(r.ok?'ok':'err');
 }catch(e){$('mw').textContent='Erreur réseau';$('mw').className='msg err';}
}

async function doScan(){
 $('nets').innerHTML='<span class="hint">Scan en cours…</span>';
 try{await jget('/api/scan');}catch(e){}
 for(let t=0;t<12;t++){
  await new Promise(r=>setTimeout(r,1500));
  try{
   const s=await jget('/api/scan.json');
   if(!s.busy){
    nets=s.nets||[];
    $('nets').innerHTML='<table><tr><th>SSID</th><th>Signal</th><th>Sécurité</th></tr>'+
      nets.map((n,i)=>'<tr onclick="pick('+i+')" style="cursor:pointer"><td>'+esc(n.s)+'</td><td>'+n.r+' dBm</td><td>'+(n.sec?'verrouillé':'ouvert')+'</td></tr>').join('')+'</table>';
    return;
   }
  }catch(e){}
 }
 $('nets').innerHTML='<span class="hint">Scan indisponible</span>';
}
function pick(i){if(nets[i])$('sta_ssid').value=nets[i].s;}

async function doReboot(){
 if(!confirm('Redémarrer le module ?'))return;
 await post('/api/reboot',{});
 $('ms').textContent='Redémarrage en cours…';$('ms').className='msg ok';
}
async function doReset(){
 if(!confirm("Restaurer les réglages d'usine (ratios et Wi-Fi) et redémarrer ?"))return;
 await post('/api/factoryreset',{});
 $('ms').textContent='Réinitialisation en cours…';$('ms').className='msg ok';
}

loadStatus();setInterval(loadStatus,1000);loadCfg();
