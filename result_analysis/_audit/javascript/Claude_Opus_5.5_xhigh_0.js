
const $=i=>document.getElementById(i);
const f0=v=>Math.round(v).toLocaleString('fr-FR');
const esc=s=>String(s).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
let busy=false;
const msg=t=>{const m=$('msg');m.textContent=t;m.style.display='block';clearTimeout(m._t);m._t=setTimeout(()=>m.style.display='none',4000);};
const post=async(u,d)=>{const r=await fetch(u,{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:new URLSearchParams(d)});
 const j=await r.json().catch(()=>({ok:false,msg:'réponse invalide'}));if(!r.ok||!j.ok)throw new Error(j.msg||('HTTP '+r.status));return j;};
const card=c=>`<div class="row"><h2>Ventilateur ${c.name}</h2><span class="badge ${c.active?'on':'off'}">${c.active?'SIMULATION':'ATTENTE SIGNAL'}</span></div>
<div class="row"><div><div class="mut">RPM lus (Noctua)</div><div class="big">${f0(c.rpm_in)}</div><div class="mut">${c.f_in.toFixed(2)} Hz</div></div>
<div style="text-align:right"><div class="mut">RPM simulés (vers Deye)</div><div class="big">${f0(c.rpm_out)}</div><div class="mut">${c.f_out.toFixed(2)} Hz</div></div></div>
<div class="mut">ratio ×${c.ratio.toFixed(4)} · impulsions lues ${c.edges} · émises ${c.pulses} · parasites ${c.glitches}</div>`;
const tr=(k,v)=>`<tr><td>${k}</td><td>${v}</td></tr>`;
const render=s=>{
 s.ch.forEach((c,i)=>{$('c'+i).innerHTML=card(c);const r=parseFloat($('r'+i).value);
  $('h'+i).textContent=(c.rpm_in>0&&r>0)?`→ ${f0(c.rpm_in*r)} tr/min simulés avec ce ratio`:'';});
 const w=s.wifi,t=s.timing;
 $('sys').innerHTML=tr('Firmware',s.fw)+tr('Uptime',Math.floor(s.uptime/3600)+' h '+Math.floor(s.uptime%3600/60)+' min')
  +tr('Mémoire libre',s.heap+' o')+tr('CPU',s.cpu+' MHz')+tr('LED',esc(s.led))
  +tr('STA',esc(w.sta_ssid||'—')+' · '+esc(w.sta_state)+(w.sta_ip?' · '+w.sta_ip+' · '+w.rssi+' dBm':''))
  +tr('AP',esc(w.ap_ssid)+' · '+w.ap_ip+' · '+w.ap_clients+' client(s) · canal '+w.channel)
  +tr('Nom d\'hôte',esc(w.hostname)+'.local')
  +tr('Échantillonnage entrées',t.poll_us+' µs')+tr('NMI / s',t.nmi_rate)
  +tr('Durée max d\'une NMI',t.nmi_max_us.toFixed(2)+' µs')
  +tr('Retard max d\'un front de sortie',t.late_max_us.toFixed(3)+' µs')
  +tr('Fronts en retard (> 1 µs)',t.late_count);
};
const poll=async()=>{if(busy)return;busy=true;
 try{const r=await fetch('/api/status',{cache:'no-store'});render(await r.json());$('conn').textContent='en ligne';}
 catch(e){$('conn').textContent='hors ligne';}busy=false;};
const loadCfg=async()=>{try{const j=await (await fetch('/api/config',{cache:'no-store'})).json();
 $('r0').value=j.ratio[0].toFixed(4);$('r1').value=j.ratio[1].toFixed(4);$('ss').value=j.sta_ssid;$('as').value=j.ap_ssid;$('hn').value=j.hostname;
 $('sp').placeholder=j.sta_pass_set?'(inchangé)':'(aucun)';$('ad').placeholder=j.admin_pass_set?'(inchangé)':'(aucun)';}catch(e){}};
const saveRatios=async()=>{try{await post('/api/ratios',{r0:$('r0').value,r1:$('r1').value});msg('Ratios appliqués et enregistrés');}catch(e){msg('Erreur : '+e.message);}};
const defRatios=()=>{$('r0').value=(4500/1600).toFixed(4);$('r1').value=(7000/3000).toFixed(4);};
const scan=async()=>{msg('Scan WiFi en cours…');for(let k=0;k<20;k++){try{const j=await (await fetch('/api/scan',{cache:'no-store'})).json();
 if(j.status==='done'){$('nets').innerHTML=j.networks.map(n=>`<option value="${esc(n.ssid)}">${n.rssi} dBm${n.open?' (ouvert)':''}</option>`).join('');msg(j.networks.length+' réseau(x) trouvé(s)');return;}}catch(e){}
 await new Promise(r=>setTimeout(r,1000));}msg('Scan : délai dépassé');};
const saveWifi=async()=>{const d={sta_ssid:$('ss').value,ap_ssid:$('as').value,hostname:$('hn').value};
 if($('so').checked)d.sta_open='1';else if($('sp').value)d.sta_pass=$('sp').value;
 if($('ap').value)d.ap_pass=$('ap').value;if($('adc').checked)d.admin_clear='1';else if($('ad').value)d.admin_pass=$('ad').value;
 try{const j=await post('/api/wifi',d);msg(j.msg||'Enregistré');}catch(e){msg('Erreur : '+e.message);}};
const act=async(u,c)=>{if(c&&!confirm(c))return;try{const j=await post(u,{});msg(j.msg||'OK');}catch(e){msg('Erreur : '+e.message);}};
loadCfg();poll();setInterval(poll,500);
