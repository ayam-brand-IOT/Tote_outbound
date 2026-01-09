const char* INDEX_HTML = R"rawliteral(
    <!DOCTYPE html>
    <html lang="es">
    <head>
      <meta charset="UTF-8" />
      <meta name="viewport" content="width=device-width,initial-scale=1.0"/>
      <title>Revisión de Palet - Loadcell</title>
      <style>
        body {
          margin: 0;
          padding: 0;
          font-family: 'Segoe UI', 'Arial', sans-serif;
          background: #f4f6fa;
          min-height: 100vh;
          display: flex;
        }
        .sidebar {
          width: 56px;
          background: #fff;
          box-shadow: 2px 0 6px #0001;
          display: flex;
          flex-direction: column;
          align-items: center;
          padding: 30px 0;
        }
        .sidebar .icon {
          font-size: 2rem;
          color: #2563eb;
          margin-bottom: 1rem;
        }
        .container {
          flex: 1;
          display: flex;
          align-items: center;
          justify-content: center;
        }
        .card {
          background: #fff;
          border-radius: 20px;
          box-shadow: 0 4px 24px #0001;
          padding: 2rem 2.5rem 2.5rem 2.5rem;
          min-width: 320px;
          max-width: 380px;
          width: 100%;
        }
        .title {
          font-size: 2rem;
          color: #1a202c;
          text-align: center;
          margin-bottom: 1.5rem;
          font-weight: 600;
        }
        .weight-label {
          color: #64748b;
          text-align: center;
          margin-bottom: 0.5rem;
          font-size: 1rem;
        }
        .weight-value {
          text-align: center;
          font-size: 3.5rem;
          font-weight: bold;
          color: #2563eb;
          margin-bottom: 1.5rem;
          letter-spacing: -2px;
        }
        .form-label {
          color: #1e293b;
          font-weight: 500;
          margin-bottom: 0.4rem;
          display: block;
        }
        .input {
          width: 100%;
          font-size: 1.15rem;
          border-radius: 8px;
          border: 1px solid #d1d5db;
          padding: 0.6rem 1rem;
          margin-bottom: 1.2rem;
          outline: none;
          box-sizing: border-box;
          transition: border 0.2s;
        }
        .input:focus {
          border: 1.7px solid #2563eb;
        }
        .button {
          width: 100%;
          background: #2563eb;
          color: #fff;
          border: none;
          border-radius: 8px;
          font-size: 1.15rem;
          font-weight: 600;
          padding: 0.75rem 0;
          cursor: pointer;
          transition: background 0.18s;
          box-shadow: 0 2px 8px #2563eb22;
        }
        .button:active {
          background: #1740b6;
        }
        .status {
          margin-top: 1rem;
          padding: 0.7rem 0.5rem;
          text-align: center;
          border-radius: 8px;
          font-weight: 500;
          font-size: 1.05rem;
        }
        .status-success {
          background: #bbf7d0;
          color: #166534;
          border: 1.2px solid #16653444;
        }
        .status-error {
          background: #fecaca;
          color: #991b1b;
          border: 1.2px solid #991b1b44;
        }
        .socket-bar {
          position: fixed;
          bottom: 0;
          left: 0;
          width: 100%;
          text-align: center;
          padding: 0.5rem 0;
          font-weight: 500;
        }
        .socket-bar.connected {
          background: #bbf7d0;
          color: #166534;
        }
        .socket-bar.disconnected {
          background: #fecaca;
          color: #991b1b;
        }
      </style>
    </head>
    <body>
      <div class="sidebar">
        <div class="icon">⚖️</div>
        <!-- Puedes agregar más iconos aquí -->
      </div>
      <div class="container">
        <div class="card">
          <div class="title">Ice and Water</div>
          <div class="weight-label">Actual weight (kg)</div>
          <div id="weight" class="weight-value">{{WEIGHT}}</div>
          <form id="palletForm" autocomplete="off">
            <label class="form-label" for="palletId">Tote ID</label>
            <input id="palletId" class="input" type="text" required maxlength="32" placeholder="Ex. 124A09" />
            <button type="submit" class="button">Enter Tote ID</button>
          </form>
          <div id="statusMsg"></div>
          <div id="recentContainer">
            <div class="weight-label" style="margin-top:1rem;">Last IDs</div>
            <ul id="recentIds" style="list-style:none;padding-left:0;margin-top:0.5rem;"></ul>
          </div>
        </div>
      </div>
      <div id="socketStatus" class="socket-bar disconnected">Socket: disconnected</div>
      <script>
        // === WebSocket para peso en tiempo real con reconexión ===
        let ws;
        let reconnectDelay = 1000;
        function updateSocketStatus(isConnected) {
          const el = document.getElementById('socketStatus');
          if (isConnected) {
            el.textContent = 'Socket: connected';
            el.className = 'socket-bar connected';
          } else {
            el.textContent = 'Socket: disconnected';
            el.className = 'socket-bar disconnected';
          }
        }
        function connectWS() {
          ws = new WebSocket(`ws://${location.host}/ws`);
          ws.onopen = () => {
            reconnectDelay = 1000;
            updateSocketStatus(true);
          };
          ws.onmessage = (event) => {
            document.getElementById('weight').textContent = event.data || '-';
            console.log("Weight updated:", event.data);
          };
          ws.onclose = () => {
            document.getElementById('weight').textContent = '-';
            updateSocketStatus(false);
            setTimeout(connectWS, reconnectDelay);
            reconnectDelay = Math.min(reconnectDelay * 2, 10000);
          };
          ws.onerror = () => {
            ws.close();
          };
        }
        connectWS();
    
        // === Formulario Tote ID ===
        document.getElementById('palletForm').addEventListener('submit', function(e) {
          e.preventDefault();
          const palletId = document.getElementById('palletId').value.trim();
          const statusMsg = document.getElementById('statusMsg');
          statusMsg.textContent = '';
          statusMsg.className = '';
          fetch('/register_pallet', {
          method: 'POST',
            headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
            body: new URLSearchParams({ id: palletId })
          })
          .then(async res => {
            const text = await res.text();
            if (!res.ok) throw new Error(text || 'Error registering Tote.');
            return text || 'Tote registered successfully.';
          })
          .then(msg => {
            statusMsg.textContent = msg;
            statusMsg.className = 'status status-success';
            document.getElementById('palletForm').reset();
            addRecentId(palletId);
          })
          .catch(err => {
            statusMsg.textContent = err.message || 'Error registering Tote.';
            statusMsg.className = 'status status-error';
          });
        });

        const recentIds = [];
        const maxIds = 5;
        function addRecentId(id) {
          recentIds.unshift(id);
          if (recentIds.length > maxIds) recentIds.pop();
          const list = document.getElementById('recentIds');
          list.innerHTML = '';
          recentIds.forEach(i => {
            const li = document.createElement('li');
            li.textContent = i;
            list.appendChild(li);
          });
        }
      </script>
    </body>
    </html>
    )rawliteral";
    