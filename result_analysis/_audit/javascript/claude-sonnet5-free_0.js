
function refreshStatus(){
  fetch('/status').then(r=>r.json()).then(d=>{
    document.getElementById('ria').textContent = d.rpm_in_a.toFixed(0);
    document.getElementById('roa').textContent = d.rpm_out_a.toFixed(0);
    document.getElementById('sa').innerHTML = d.active_a ? '<span class="badge ok">simulation</span>' : '<span class="badge wait">attente</span>';
    document.getElementById('rib').textContent = d.rpm_in_b.toFixed(0);
    document.getElementById('rob').textContent = d.rpm_out_b.toFixed(0);
    document.getElementById('sb').innerHTML = d.active_b ? '<span class="badge ok">simulation</span>' : '<span class="badge wait">attente</span>';
  }).catch(()=>{});
}
setInterval(refreshStatus, 1000);
refreshStatus();

function loadConfig(){
  fetch('/config').then(r=>r.json()).then(d=>{
    document.getElementById('ratio_a').value = d.ratio_a;
    document.getElementById('ratio_b').value = d.ratio_b;
    document.getElementById('sta_ssid').value = d.sta_ssid;
    document.getElementById('ap_ssid').value = d.ap_ssid;
  });
}
loadConfig();

document.getElementById('cfgform').addEventListener('submit', function(e){
  e.preventDefault();
  const data = new URLSearchParams(new FormData(this));
  fetch('/save', {method:'POST', body:data}).then(r=>r.text()).then(t=>{
    document.getElementById('msg').textContent = t;
  });
});
