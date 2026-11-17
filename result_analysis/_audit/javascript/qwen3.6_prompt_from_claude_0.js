
async function loadReadings() {
  try {
    const r = await fetch("/api/readings");
    const d = await r.json();
    document.getElementById("inA").textContent   = d.input_rpm_a   >= 0 ? Math.round(d.input_rpm_a)  + " RPM" : "—";
    document.getElementById("inB").textContent   = d.input_rpm_b   >= 0 ? Math.round(d.input_rpm_b)  + " RPM" : "—";
    document.getElementById("outA").textContent  = d.output_rpm_a  >= 0 ? Math.round(d.output_rpm_a) + " RPM" : "—";
    document.getElementById("outB").textContent  = d.output_rpm_b  >= 0 ? Math.round(d.output_rpm_b) + " RPM" : "—";
    const st = document.getElementById("status");
    if (d.stalled) { st.textContent = "DECROCHAGE — ATTENTE"; st.className = "val stalled"; }
    else { st.textContent = "ACTIF"; st.className = "val"; }
  } catch(e) {}
}

async function loadConfig() {
  try {
    const r = await fetch("/api/config");
    const d = await r.json();
    if (d.ratios && d.ratios.length >= 2) {
      document.getElementById("ratioA").value = d.ratios[0];
      document.getElementById("ratioB").value = d.ratios[1];
    }
    if (d.ap_ssid)       document.getElementById("apSsid").value     = d.ap_ssid;
    if (d.ap_pass)       document.getElementById("apPass").value     = d.ap_pass;
  } catch(e) {}
}

async function saveAll() {
  const body = JSON.stringify({
    ratios: [parseFloat(document.getElementById("ratioA").value), 
             parseFloat(document.getElementById("ratioB").value)],
    wifi_ssid:   document.getElementById("staSsid").value,
    wifi_pass:   document.getElementById("staPass").value,
    ap_ssid:     document.getElementById("apSsid").value,
    ap_pass:     document.getElementById("apPass").value
  });
  try {
    await fetch("/api/config", { method:"POST", headers:{"Content-Type":"application/json"}, body:body });
    setTimeout(()=>location.reload(), 3000);
  } catch(e) { alert("Erreur sauvegarde"); }
}

loadConfig();
loadReadings();
setInterval(loadReadings, 1000);
