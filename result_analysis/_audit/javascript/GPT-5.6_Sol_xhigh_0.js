
let initialized = false;
function channel(prefix, data) {
  document.getElementById('in' + prefix).textContent = data.rpmIn.toFixed(0);
  document.getElementById('out' + prefix).textContent = data.rpmOut.toFixed(0);
  document.getElementById('showRatio' + prefix).textContent = data.ratio.toFixed(2);
  const state = document.getElementById('state' + prefix);
  state.textContent = data.active ? 'Simulation active' : 'En attente du signal';
  state.className = data.active ? 'active' : 'waiting';
}
async function refresh() {
  try {
    const response = await fetch('/api/status', {cache: 'no-store'});
    const data = await response.json();
    channel('6', data.ch6);
    channel('9', data.ch9);
    document.getElementById('ap').textContent = data.wifi.apSsid;
    document.getElementById('apIp').textContent = data.wifi.apIp;
    document.getElementById('staState').textContent = data.wifi.staState;
    document.getElementById('staIp').textContent = data.wifi.staIp || '—';
    if (!initialized) {
      document.getElementById('ratio6').value = data.ch6.ratio.toFixed(2);
      document.getElementById('ratio9').value = data.ch9.ratio.toFixed(2);
      document.getElementById('ssid').value = data.wifi.staSsid;
      initialized = true;
    }
  } catch (_) {}
}
refresh();
setInterval(refresh, 1000);
