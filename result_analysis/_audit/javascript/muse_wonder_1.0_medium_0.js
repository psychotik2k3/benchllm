
async function refresh(){
 try{
  let s=await (await fetch('/api/status')).json();
  for(let i=0;i<2;i++){document.getElementById('r'+i).textContent=Math.round(s.rpm_in[i]);
   document.getElementById('s'+i).textContent=Math.round(s.rpm_out[i]);
   document.getElementById('x'+i).textContent=s.ratio[i].toFixed(2);
   let e=document.getElementById('e'+i);e.textContent=s.active[i]?'Simulation active':'En attente de signal';
   e.className=s.active[i]?'ok':'warn';}
  document.getElementById('wifi').textContent='STA : '+(s.sta_connected?('connecté, IP '+s.sta_ip):'non connecté')+' — AP : '+s.ap_ip;
 }catch(e){}
}
async function loadCfg(){
 try{let c=await (await fetch('/api/config')).json();
  ratio0.value=c.ratio[0].toFixed(2);ratio1.value=c.ratio[1].toFixed(2);
  sta_ssid.value=c.sta_ssid;ap_ssid.value=c.ap_ssid;}catch(e){}
}
loadCfg();refresh();setInterval(refresh,500);
