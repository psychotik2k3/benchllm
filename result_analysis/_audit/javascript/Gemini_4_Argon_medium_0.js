
    let initialLoaded = false;

    function showToast(msg, isErr) {
      const t = document.getElementById('toast');
      t.textContent = msg;
      t.style.background = isErr ? '#ef4444' : '#0284c7';
      t.style.display = 'block';
      setTimeout(() => { t.style.display = 'none'; }, 3000);
    }

    function setPreset(ch, val) {
      document.getElementById('ratio' + ch).value = val.toFixed(2);
      saveRatios();
    }

    async function fetchStatus() {
      try {
        const res = await fetch('/api/status');
        if (!res.ok) return;
        const d = await res.json();

        const dot = document.getElementById('sysDot');
        const st  = document.getElementById('sysStateText');
        if (d.simulating) {
          dot.className = 'dot active';
          st.textContent = 'Simulation RPM Active (LED Fixe)';
        } else {
          dot.className = 'dot';
          st.textContent = 'En attente de signal ventilateur';
        }

        for (let i = 0; i < 2; i++) {
          const ch = d.channels[i];
          document.getElementById('ch' + i + 'InRpm').textContent  = Math.round(ch.in_rpm);
          document.getElementById('ch' + i + 'InHz').textContent   = ch.in_hz.toFixed(1) + ' Hz';
          document.getElementById('ch' + i + 'OutRpm').textContent = Math.round(ch.out_rpm);
          document.getElementById('ch' + i + 'OutHz').textContent  = ch.out_hz.toFixed(1) + ' Hz';
          const b = document.getElementById('ch' + i + 'Badge');
          b.textContent = ch.active ? ('Actif (×' + ch.ratio.toFixed(2) + ')') : 'Arrêté / Attente';
          b.style.borderColor = ch.active ? '#22c55e' : '#334155';

          if (!initialLoaded) {
            document.getElementById('ratio' + i).value = ch.ratio.toFixed(2);
          }
        }

        if (!initialLoaded) {
          document.getElementById('staSsid').value = d.wifi.sta_ssid || '';
          document.getElementById('apSsid').value  = d.wifi.ap_ssid  || '';
          initialLoaded = true;
        }

        document.getElementById('apIp').textContent    = d.wifi.ap_ip;
        document.getElementById('staIp').textContent   = d.wifi.sta_connected ? d.wifi.sta_ip : 'Non connecté';
        document.getElementById('staRssi').textContent = d.wifi.sta_connected ? (d.wifi.rssi + ' dBm') : '-';
        document.getElementById('uptime').textContent  = d.uptime_s + ' s';
      } catch (e) {
        document.getElementById('sysStateText').textContent = 'Hors ligne...';
      }
    }

    async function saveRatios() {
      const r0 = parseFloat(document.getElementById('ratio0').value);
      const r1 = parseFloat(document.getElementById('ratio1').value);
      const body = new URLSearchParams({ ratio0: r0, ratio1: r1 });
      const res = await fetch('/api/ratios', { method: 'POST', body });
      if (res.ok) {
        showToast('Ratios enregistrés et appliqués en temps réel !', false);
        fetchStatus();
      } else {
        showToast('Erreur lors de la sauvegarde des ratios', true);
      }
    }

    async function saveWifi() {
      const body = new URLSearchParams({
        sta_ssid: document.getElementById('staSsid').value.trim(),
        sta_pass: document.getElementById('staPass').value,
        ap_ssid:  document.getElementById('apSsid').value.trim(),
        ap_pass:  document.getElementById('apPass').value
      });
      const res = await fetch('/api/wifi', { method: 'POST', body });
      if (res.ok) {
        document.getElementById('staPass').value = '';
        document.getElementById('apPass').value  = '';
        showToast('Paramètres WiFi sauvegardés ! Reconnexion en cours...', false);
      } else {
        showToast('Paramètres WiFi invalides (mot de passe AP >= 8 car.)', true);
      }
    }

    async function resetFactory() {
      if (!confirm('Réinitialiser les ratios (2.33 / 3.50) et les paramètres WiFi par défaut ?')) return;
      const res = await fetch('/api/reset', { method: 'POST' });
      if (res.ok) {
        initialLoaded = false;
        showToast('Configuration d\'usine restaurée !', false);
        fetchStatus();
      }
    }

    fetchStatus();
    setInterval(fetchStatus, 500);
  