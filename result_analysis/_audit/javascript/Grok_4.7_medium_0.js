
const fmt = q => (q / 1024).toFixed(2).replace('.', ',');
const $ = id => document.getElementById(id);
async function loadCfg() {
  const c = await (await fetch('/api/config')).json();
  $('r6').value = fmt(c.r6);
  $('r9').value = fmt(c.r9);
  $('ssid').value = c.ssid;
}
async function poll() {
  try {
    const s = await (await fetch('/api/status')).json();
    $('net').textContent = s.sta
      ? ('point d\'accès ' + s.ap + '  ·  maison ' + s.ip)
      : ('point d\'accès ' + s.ap + '  ·  maison non connectée');
    for (let i = 0; i < 2; i++) {
      const ch = s.c[i];
      document.getElementById('in' + i).textContent = ch.run ? ch.in : '–';
      document.getElementById('out' + i).textContent = ch.run ? ch.out : '–';
      const st = document.getElementById('st' + i);
      st.textContent = ch.run ? 'simulation active' : 'attente de signal';
      st.className = ch.run ? 'ok' : 'wait';
    }
  } catch (e) {
    $('net').textContent = 'page injoignable';
  }
}
$('f').addEventListener('submit', async (ev) => {
  ev.preventDefault();
  $('msg').textContent = 'enregistrement…';
  const body = new URLSearchParams(new FormData($('f')));
  const r = await fetch('/api/save', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body
  });
  $('msg').textContent = r.ok ? 'enregistré, ratios appliqués' : 'refus';
  $('pass').value = '';
  $('appass').value = '';
});
loadCfg().catch(() => { $('msg').textContent = 'réglages illisibles'; });
poll();
setInterval(poll, 500);
