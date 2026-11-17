
        // Variables globales pour la synchronisation
        let lastR9 = 0;
        let lastR6 = 0;
        let lastRS = 0;
        let updateCount = 0;

        // Fonction pour afficher le message de statut
        function showStatus(message, type) {
            const msg = document.getElementById('statusMessage');
            msg.textContent = message;
            msg.className = 'status-message ' + type;

            // Masquer après 3 secondes
            setTimeout(() => {
                msg.style.display = 'none';
            }, 3000);
        }

        // Fonction pour actualiser manuellement
        function forceRefresh() {
            fetch('/data?force=true')
                .then(r => r.json())
                .then(d => {
                    lastR9 = d.r9;
                    lastR6 = d.r6;
                    lastRS = d.rs;
                    updateDisplay();
                    showStatus('Données actualisées', 'success');
                })
                .catch(e => {
                    showStatus('Erreur de connexion', 'error');
                });
        }

        // Fonction pour mettre à jour l'affichage
        function updateDisplay() {
            document.getElementById('r9').textContent = lastR9;
            document.getElementById('r6').textContent = lastR6;
            document.getElementById('rs9').textContent = lastR9 * 2.0;
            document.getElementById('rs6').textContent = lastR6 * 2.5;
            document.getElementById('rs').textContent = lastRS;

            // Ajouter une classe active si les données sont valides
            const r9El = document.getElementById('r9');
            const r6El = document.getElementById('r6');

            if (lastR9 > 0) {
                r9El.classList.add('active');
            } else {
                r9El.classList.remove('active');
            }

            if (lastR6 > 0) {
                r6El.classList.add('active');
            } else {
                r6El.classList.remove('active');
            }
        }

        // Mise à jour automatique toutes les 100ms
        setInterval(function() {
            fetch('/data')
                .then(r => r.json())
                .then(d => {
                    lastR9 = d.r9;
                    lastR6 = d.r6;
                    lastRS = d.rs;
                    updateDisplay();
                })
                .catch(e => {
                    // Ignorer les erreurs de connexion
                });
        }, 100);

        // Vérifier l'état du WiFi au chargement
        function checkWifiStatus() {
            const statusEl = document.getElementById('wifiStatus');
            const textEl = document.getElementById('wifiText');

            if (navigator.onLine) {
                statusEl.classList.add('connected');
                textEl.textContent = 'Connecté';
            } else {
                statusEl.classList.remove('connected');
                textEl.textContent = 'Déconnecté';
            }
        }

        // Vérifier le statut du WiFi
        setInterval(checkWifiStatus, 2000);
        checkWifiStatus();

        // Écouteur de changement des inputs pour validation
        document.getElementById('v9').addEventListener('change', function() {
            const value = parseFloat(this.value);
            if (value < 0) this.value = 0;
            if (value > 4.67) this.value = 4.67;

            // Envoyer la nouvelle valeur au serveur sans recharger
            fetch('/config?channel=9&ratio=' + this.value);
        });

        document.getElementById('v6').addEventListener('change', function() {
            const value = parseFloat(this.value);
            if (value < 0) this.value = 0;
            if (value > 4.67) this.value = 4.67;

            // Envoyer la nouvelle valeur au serveur sans recharger
            fetch('/config?channel=6&ratio=' + this.value);
        });
    