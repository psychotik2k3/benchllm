
function q(id){return document.getElementById(id)}
function tick(){
  fetch('/api/status').then(r=>r.json()).then(d=>{
    q('r9').textContent=d.ch9.rpm; q('s9').textContent=d.ch9.sim;
    q('r6').textContent=d.ch6.rpm; q('s6').textContent=d.ch6.sim;
    q('a9').innerHTML=d.ch9.alive?'<span class="ok">simulation active</span>':'<span class="wait">attente signal</span>';
    q('a6').innerHTML=d.ch6.alive?'<span class="ok">simulation active</span>':'<span class="wait">attente signal</span>';
    q('net').textContent='AP '+d.wifi.apIp+'  ·  STA '+(d.wifi.sta?'connecté '+d.wifi.staIp:'hors ligne');
    if(!q('ratio9').dataset.init){
      q('ratio9').value=d.ch9.ratio; q('ratio6').value=d.ch6.ratio;
      q('staSsid').value=d.wifi.staSsid; q('apSsid').value=d.wifi.apSsid;
      q('ratio9').dataset.init='1';
    }
  }).catch(()=>{});
}
setInterval(tick, 500); tick();
q('f').onsubmit=function(e){
  e.preventDefault();
  const b=new URLSearchParams(new FormData(q('f')));
  fetch('/api/config',{method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:b})
    .then(r=>r.text()).then(t=>{q('msg').textContent=t; setTimeout(()=>location.reload(),800);});
};
