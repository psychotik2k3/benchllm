
async function refresh(){
  const r=await fetch('/api/status');const d=await r.json();
  document.getElementById('status').innerHTML=
    'Canal 0 — entrée: '+d.inputRpm[0]+' RPM, sortie: '+d.outputRpm[0]+' RPM, actif: '+d.active[0]+'<br>'+
    'Canal 1 — entrée: '+d.inputRpm[1]+' RPM, sortie: '+d.outputRpm[1]+' RPM, actif: '+d.active[1]+'<br>'+
    'AP: '+d.apIp+' | STA: '+(d.wifiSta||'—');
  document.getElementById('ratio0').value=d.ratio0;
  document.getElementById('ratio1').value=d.ratio1;
}
document.getElementById('ratioForm').onsubmit=async e=>{
  e.preventDefault();
  const body=new URLSearchParams({
    ratio0:document.getElementById('ratio0').value,
    ratio1:document.getElementById('ratio1').value
  });
  await fetch('/api/save-ratios',{method:'POST',body});
  refresh();
};
document.getElementById('wifiForm').onsubmit=async e=>{
  e.preventDefault();
  const body=new URLSearchParams({
    staSsid:document.getElementById('staSsid').value,
    staPass:document.getElementById('staPass').value,
    apSsid:document.getElementById('apSsid').value,
    apPass:document.getElementById('apPass').value
  });
  await fetch('/api/save-wifi',{method:'POST',body});
  alert('WiFi enregistré. Redémarrage…');
};
setInterval(refresh,1000);refresh();
