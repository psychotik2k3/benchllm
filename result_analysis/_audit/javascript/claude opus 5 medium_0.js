
const $=s=>document.querySelector(s);
let cfgBuilt=false,wifiBuilt=false;
const f1=(v,d=0)=>Number(v).toFixed(d);

function buildCfg(d){
  $('#cfgch').innerHTML=d.channels.map((c,i)=>`<div>
    <div style="font-weight:600;margin-bottom:6px">Canal ${i+1} &mdash; ${c.name}</div>
    <label>Ratio multiplicateur (0,10 &ndash; 10,00)</label>
    <input name="ratio${i}" type="number" step="0.01" min="0.1" max="10" value="${(c.ratio).toFixed(2)}" required>
    <label>Impulsions par tour &mdash; entree Noctua</label>
    <input name="ppin${i}" type="number" step="1" min="1" max="8" value="${c.ppIn}" required>
    <label>Impulsions par tour &mdash; sortie Deye</label>
    <input name="ppout${i}" type="number" step="1" min="1" max="8" value="${c.ppOut}" required>
    <label>Ecretage de securite (RPM simules max)</label>
    <input name="maxrpm${i}" type="number" step="100" min="500" max="30000" value="${c.maxSimRpm}" required>
  </div>`).join('');
  cfgBuilt=true;
}

function render(d){
  if(!cfgBuilt)buildCfg(d);
  if(!wifiBuilt){const f=$('#fwifi');f.apSsid.value=d.wifi.apSsid;f.apPass.value=d.wifi.apPass;
    f.staSsid.value=d.wifi.staSsid;f.staEnabled.value=d.wifi.staEnabled?'1':'0';wifiBuilt=true;}
  $('#chans').innerHTML=d.channels.map((c,i)=>`<div class="card">
    <h2>Canal ${i+1} &mdash; ${c.name}</h2>
    <div style="margin-bottom:8px"><span class="pill ${c.active?'on':'off'}">${c.active?'SIMULATION ACTIVE':'ATTENTE DE SIGNAL'}</span>
    ${c.clamped?' <span class="pill off">ECRETE</span>':''}</div>
    <div class="metric"><span>RPM lus (Noctua)</span><b>${f1(c.inRpm)}</b></div>
    <div class="metric"><span>RPM simules (vus par le Deye)</span><b>${f1(c.simRpm)}</b></div>
    <div class="metric"><span>Ratio applique</span><b>${c.ratio.toFixed(2)}&times;</b></div>
    <div class="metric"><span>Frequence de sortie</span><b>${f1(c.outHz,2)} Hz</b></div>
    <div class="metric"><span>Fronts lus / impulsions emises</span><b>${c.edges} / ${c.pulses}</b></div>
    <div class="metric"><span>Fronts rejetes (bruit)</span><b>${c.rejects}</b></div>
  </div>`).join('');
  $('#sys').innerHTML=`
    <tr><td>Moteur de signal</td><td>${d.sys.engine}</td></tr>
    <tr><td>Frequence CPU</td><td>${d.sys.cpuMHz} MHz</td></tr>
    <tr><td>Reveils NMI / s</td><td>${d.sys.nmiRate}</td></tr>
    <tr><td>Point d'acces</td><td>${d.wifi.apSsid} &mdash; ${d.wifi.apIp} (${d.wifi.apClients} client(s))</td></tr>
    <tr><td>Station</td><td>${d.wifi.staEnabled?(d.wifi.staConnected?d.wifi.staSsid+' &mdash; '+d.wifi.staIp:'connexion...'):'desactivee'}</td></tr>
    <tr><td>Nom mDNS</td><td>${d.sys.host}.local</td></tr>
    <tr><td>Duree de fonctionnement</td><td>${d.sys.uptime}</td></tr>
    <tr><td>Memoire libre</td><td>${d.sys.heap} octets</td></tr>`;
}

async function poll(){
  try{render(await(await fetch('/api/status')).json());}catch(e){}
  setTimeout(poll,600);
}

async function post(url,form,box){
  const m=$(box);m.className='msg';m.textContent='Envoi...';
  try{
    const r=await fetch(url,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},
      body:new URLSearchParams(new FormData(form))});
    const t=await r.text();
    m.className='msg '+(r.ok?'ok':'err');m.textContent=t;
  }catch(e){m.className='msg err';m.textContent='Erreur reseau';}
}

$('#fcfg').addEventListener('submit',e=>{e.preventDefault();post('/api/config',e.target,'#mcfg');});
$('#fwifi').addEventListener('submit',e=>{e.preventDefault();post('/api/wifi',e.target,'#mwifi');});
$('#brb').addEventListener('click',async()=>{await fetch('/api/reboot',{method:'POST'});
  $('#mwifi').className='msg ok';$('#mwifi').textContent='Redemarrage en cours...';});
poll();
