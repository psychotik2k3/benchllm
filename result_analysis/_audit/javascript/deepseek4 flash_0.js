
function fmtUs(u){ if(u>1000){ return (u/1000).toFixed(1)+' ms'; } return u+' \u00b5s'; }
function clampVal(v,a,b){ return Math.max(a, Math.min(b, v)); }
function sendR(ck,v){ v=clampVal(v,40,2000); fetch('/api/set?c='+ck+'&r='+v); document.getElementById('rv'+ck).textContent=(v/100).toFixed(2)+' \u00d7'; }
function esc(s){ return String(s).replace(/[<>&"]/g, function(c){ return {'<':'&lt;','>':'&gt;','&':'&amp;','"':'&quot;'}[c]; }); }
async function refresh(){
 try{
  const j = await (await fetch('/api/state')).json();
  const now = Date.now();
  for(let k=0;k<2;k++){
   const c=j.ch[k];
   document.getElementById('st'+k).textContent = c.live? 'en cours' : 'attente';
   document.getElementById('st'+k).className = c.live? 'live' : 'dead';
   document.getElementById('iu'+k).textContent = c.inUs? fmtUs(c.inUs) : '\u2014';
   document.getElementById('ri'+k).textContent = c.rpm? c.rpm+' RPM' : '\u2014';
   document.getElementById('ro'+k).textContent = c.sim? c.sim+' RPM' : '\u2014';
   document.getElementById('ou'+k).textContent = c.outUs? fmtUs(c.outUs) : '\u2014';
   const rv=(c.r/100).toFixed(2);
   document.getElementById('rv'+k).textContent = rv+' \u00d7';
   document.getElementById('r'+k).value = Math.round(c.r/5)*5;
  }
  document.getElementById('fr0').value = (j.ch[0].r/100).toFixed(2);
  document.getElementById('fr1').value = (j.ch[1].r/100).toFixed(2);
  document.getElementById('fs').value = j.sta.ssid;
  document.getElementById('fp').value = j.sta.pass;
  document.getElementById('fa').value = j.sta.ssidAp;
  document.getElementById('fap').value = j.sta.passAp;
  let w  = '<tr><td>STA</td><td>'+(j.sta.conn? 'connect\u00e9 \u00e0 <b>'+esc(j.sta.ssid)+'</b> \u00b7 IP '+esc(j.sta.ip) : 'pas de liaison')+'</td></tr>';
  w    += '<tr><td>AP</td><td><b>'+esc(j.sta.ssidAp)+'</b> \u00b7 192.168.4.1 \u00b7 '+j.ap.clients+' client(s)</td></tr>';
  w    += '<tr><td>Uptime</td><td>'+Math.round(now/1000)+' s</td></tr>';
  document.getElementById('wt').innerHTML = w;
 }catch(e){ /* silencieux : tentative au prochain polling */ }
}
setInterval(refresh,1500); refresh();
