#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <WebServer.h>
#include "WifiManager.h"

class WebManager {
private:
    WebServer server{8080};
    WifiManager& wifiManager;

    const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Smart Switch</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: Arial, sans-serif;
      background: #1a1a2e;
      color: #eee;
      min-height: 100vh;
      display: flex;
      flex-direction: column;
      align-items: center;
      padding: 20px;
    }
    h1 { margin: 20px 0 10px; font-size: 1.4em; color: #a0c4ff; }
    #status-bar {
      font-size: 0.85em;
      color: #aaa;
      margin-bottom: 20px;
    }
    #status-bar span { margin: 0 8px; }
    .grid {
      display: grid;
      grid-template-columns: repeat(2, 1fr);
      gap: 16px;
      width: 100%;
      max-width: 480px;
    }
    .card {
      background: #16213e;
      border-radius: 12px;
      padding: 20px;
      text-align: center;
      border: 2px solid #0f3460;
      transition: border-color 0.3s;
    }
    .card.on { border-color: #4ade80; }
    .card h3 { font-size: 1em; margin-bottom: 12px; color: #ccc; }
    .toggle {
      width: 70px;
      height: 36px;
      border-radius: 18px;
      border: none;
      cursor: pointer;
      font-size: 0.85em;
      font-weight: bold;
      letter-spacing: 1px;
      transition: background 0.3s;
      background: #374151;
      color: #9ca3af;
    }
    .toggle.on {
      background: #4ade80;
      color: #14532d;
    }
    .dot {
      display: inline-block;
      width: 10px; height: 10px;
      border-radius: 50%;
      background: #4ade80;
      margin-right: 5px;
      animation: pulse 1.5s infinite;
    }
    @keyframes pulse {
      0%, 100% { opacity: 1; }
      50% { opacity: 0.3; }
    }
    #ws-status { color: #f87171; }
    #ws-status.connected { color: #4ade80; }
  </style>
</head>
<body>
  <h1>ESP32 Smart Switch</h1>
  <div id="status-bar">
    <span id="ws-status">● Kapcsolódás...</span>
    <span id="wifi-rssi"></span>
    <span id="wifi-ip"></span>
  </div>
  <div class="grid" id="relay-grid">
    <!-- JS tölti fel -->
  </div>

  <script>
    const RELAY_COUNT = 4;
    const states = {};

    const grid = document.getElementById('relay-grid');
    const wsStatus = document.getElementById('ws-status');

    // Kártyák generálása
    for (let i = 1; i <= RELAY_COUNT; i++) {
      states[i] = false;
      grid.innerHTML += `
        <div class="card" id="card-${i}">
          <h3 id="name-${i}">Relé ${i}</h3>
          <button class="toggle" id="btn-${i}" onclick="toggle(${i})">KI</button>
        </div>`;
    }

    function toggle(id) {
      socket.send(JSON.stringify({action: 'toggle', id: id}));
    }

    function updateRelay(id, state, name) {
      states[id] = state;
      const btn  = document.getElementById('btn-' + id);
      const card = document.getElementById('card-' + id);
      const nm   = document.getElementById('name-' + id);
      if (!btn) return;
      btn.textContent = state ? 'BE' : 'KI';
      btn.className   = 'toggle' + (state ? ' on' : '');
      card.className  = 'card' + (state ? ' on' : '');
      if (name) nm.textContent = name;
    }

    const socket = new WebSocket(`ws://${location.hostname}:81`);

    socket.onopen = () => {
      wsStatus.textContent = '● Csatlakozva';
      wsStatus.className = 'connected';
    };

    socket.onclose = () => {
      wsStatus.textContent = '● Kapcsolat megszakadt';
      wsStatus.className = '';
      setTimeout(() => location.reload(), 3000);
    };

    socket.onerror = () => {
      wsStatus.textContent = '● Hiba';
    };

    socket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'relay') {
          updateRelay(data.id, data.state, data.name);
        } else if (data.type === 'wifi') {
          document.getElementById('wifi-rssi').textContent = data.rssi + ' dBm';
          document.getElementById('wifi-ip').textContent   = data.ip;
        }
      } catch(e) {
        console.error('JSON hiba:', e);
      }
    };
  </script>
</body>
</html>
)rawliteral";

    void setupRoutes() {
        server.on("/", HTTP_GET, [this]() {
            server.send(200, "text/html", INDEX_HTML);
        });
        server.onNotFound([this]() {
            server.send(404, "text/plain", "404: Nem található");
        });
    }

public:
    WebManager(WifiManager& wm) : wifiManager(wm) {}

    void begin() {
        setupRoutes();
        server.begin();
        Serial.println("[WebManager] HTTP szerver elindult a 8080-as porton");
    }

    void handle() {
        server.handleClient();
    }
};

#endif // WEB_MANAGER_H
