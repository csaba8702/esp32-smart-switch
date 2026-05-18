#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <WebServer.h>
#include "WifiManager.h"
#include "EepromManager.h"

class WebManager {
private:
    WebServer server{8080};
    WifiManager& wifiManager;
    EepromManager* eeprom = nullptr;
    String sessionToken = "";

    String generateToken() {
        String token = "";
        const char chars[] = "0123456789abcdef";
        for (int i = 0; i < 32; i++) token += chars[esp_random() % 16];
        return token;
    }

    String getCookieValue(const String& cookieHeader, const String& name) {
        String search = name + "=";
        int start = cookieHeader.indexOf(search);
        if (start < 0) return "";
        start += search.length();
        int end = cookieHeader.indexOf(';', start);
        if (end < 0) end = cookieHeader.length();
        return cookieHeader.substring(start, end);
    }

    bool isAuthenticated() {
        if (sessionToken.isEmpty()) return false;
        String cookieHeader = server.header("Cookie");
        String token = getCookieValue(cookieHeader, "token");
        return token == sessionToken;
    }

    const char* LOGIN_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Bejelentkezés</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:Arial,sans-serif;background:#0f172a;color:#eee;min-height:100vh;display:flex;align-items:center;justify-content:center}
    .box{background:#1e293b;border-radius:14px;padding:40px 32px;width:100%;max-width:320px;border:1px solid #334155;text-align:center}
    h2{color:#a0c4ff;margin-bottom:24px;font-size:1.2em}
    input{width:100%;padding:10px 14px;border-radius:8px;border:1px solid #334155;background:#0f172a;color:#eee;font-size:1em;margin-bottom:16px;outline:none}
    input:focus{border-color:#a0c4ff}
    button{width:100%;padding:10px;border-radius:8px;border:none;background:#22c55e;color:#052e16;font-size:1em;font-weight:bold;cursor:pointer}
    button:hover{background:#16a34a}
    .error{color:#f87171;margin-top:12px;font-size:0.9em}
  </style>
</head>
<body>
  <div class="box">
    <h2>ESP32 Smart Switch</h2>
    <input type="password" id="pass" placeholder="Jelszó" onkeydown="if(event.key==='Enter')login()">
    <button onclick="login()">Bejelentkezés</button>
    <div class="error" id="err"></div>
  </div>
  <script>
    function login() {
      const pass = document.getElementById('pass').value;
      fetch('/login', {method:'POST',headers:{'Content-Type':'application/x-www-form-urlencoded'},body:'pass='+encodeURIComponent(pass)})
        .then(r => { if(r.ok) window.location.href='/'; else document.getElementById('err').textContent='Hibás jelszó!'; });
    }
  </script>
</body>
</html>
)rawliteral";

    const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>ESP32 Smart Switch</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:Arial,sans-serif;background:#0f172a;color:#eee;min-height:100vh;display:flex;flex-direction:column;align-items:center;padding:16px}
    .topbar{width:100%;max-width:480px;display:flex;align-items:center;justify-content:space-between;margin-bottom:6px;padding:0 2px}
    .topbar-title{font-size:1.1em;font-weight:600;color:#a0c4ff}
    .topbar-right{display:flex;align-items:center;gap:10px}
    .ws-dot{width:8px;height:8px;border-radius:50%;background:#ef4444}
    .ws-dot.on{background:#22c55e}
    #datetime{font-size:0.85em;color:#64748b;margin-bottom:4px;width:100%;max-width:480px;text-align:right;padding:0 2px}
    .statusbar{width:100%;max-width:480px;display:flex;gap:10px;font-size:0.78em;color:#475569;margin-bottom:14px;padding:0 2px}
    .logout-btn{background:transparent;border:1px solid #334155;border-radius:6px;color:#64748b;padding:4px 10px;font-size:0.78em;cursor:pointer}
    .logout-btn:hover{border-color:#ef4444;color:#f87171}
    .grid{display:grid;grid-template-columns:1fr 1fr;gap:12px;width:100%;max-width:480px}

    /* Kártya */
    .card{background:#1e293b;border-radius:12px;border:2px solid #334155;padding:14px;transition:border-color .25s}
    .card.on{border-color:#22c55e}
    .card-header{display:flex;align-items:flex-start;justify-content:space-between;margin-bottom:10px}
    .card-icon{width:38px;height:38px;border-radius:9px;background:#0f172a;display:flex;align-items:center;justify-content:center;font-size:20px;flex-shrink:0}
    .card.on .card-icon{background:#052e16}
    .gear{background:transparent;border:none;cursor:pointer;color:#475569;font-size:15px;padding:2px 4px;border-radius:4px;line-height:1}
    .gear:hover{color:#94a3b8;background:#334155}
    .card-name{font-size:0.9em;font-weight:500;color:#e2e8f0;margin-bottom:3px;overflow:hidden;text-overflow:ellipsis;white-space:nowrap}
    .card-meta{font-size:0.75em;color:#475569}
    .card.on .card-meta{color:#4ade80}
    .card-footer{display:flex;align-items:center;justify-content:space-between;margin-top:10px}
    .toggle{width:58px;height:30px;border-radius:15px;border:none;cursor:pointer;font-size:0.78em;font-weight:600;letter-spacing:.5px;transition:background .25s;background:#334155;color:#64748b}
    .toggle.on{background:#22c55e;color:#052e16}
    .runtime{font-size:0.72em;color:#475569}
    .runtime.on{color:#86efac}

    /* Konfig panel */
    .cfg-panel{background:#1e293b;border-top:1px solid #334155;padding:14px;width:100%;max-width:480px;border-radius:0 0 12px 12px;margin-top:-4px}
    .cfg-title{font-size:0.82em;color:#64748b;margin-bottom:10px;display:flex;align-items:center;gap:6px}
    .cfg-row{display:flex;align-items:center;justify-content:space-between;padding:8px 0;border-bottom:1px solid #1e293b}
    .cfg-label{font-size:0.83em;color:#94a3b8}
    .cfg-val{font-size:0.83em;color:#64748b}
    .badge{font-size:0.75em;padding:2px 8px;border-radius:4px;background:#0f172a;border:1px solid #334155;color:#64748b}
    .inp-row{display:flex;gap:8px;margin-top:10px}
    .inp{flex:1;background:#0f172a;border:1px solid #334155;border-radius:6px;padding:6px 10px;color:#e2e8f0;font-size:0.83em;outline:none}
    .inp:focus{border-color:#3b82f6}
    .save-btn{background:#3b82f6;color:#fff;border:none;border-radius:6px;padding:6px 14px;font-size:0.83em;cursor:pointer;font-weight:500;white-space:nowrap}
    .save-btn:hover{background:#2563eb}

    /* Toast */
    .toast{position:fixed;bottom:20px;left:50%;transform:translateX(-50%);background:#1e293b;border:1px solid #22c55e;border-radius:8px;padding:10px 18px;font-size:0.85em;color:#4ade80;opacity:0;transition:opacity .3s;pointer-events:none;white-space:nowrap;z-index:999}
    .toast.show{opacity:1}
  </style>
</head>
<body>

  <div class="topbar">
    <span class="topbar-title">ESP32 Smart Switch</span>
    <div class="topbar-right">
      <span id="wifi-rssi" style="font-size:0.78em;color:#475569"></span>
      <span id="wifi-ip" style="font-size:0.78em;color:#475569"></span>
      <div class="ws-dot" id="ws-dot"></div>
      <button class="logout-btn" onclick="logout()">Ki</button>
    </div>
  </div>
  <div id="datetime" style="text-align:right">Szinkronizálás...</div>
  <div class="statusbar" id="statusbar"></div>

  <div class="grid" id="relay-grid"></div>
  <div class="cfg-panel" id="cfg-panel" style="display:none"></div>
  <div class="toast" id="toast"></div>

  <script>
    const RELAY_COUNT = 4;
    const ICONS = {GENERIC:'⚡', WATER_PUMP:'💧', GATE:'🚪', LIGHT:'💡', MOTOR:'⚙'};
    const states = {};
    const names  = {};
    const startTimes = {};
    let activeConfig = null;
    let runtimeInterval = null;

    const grid     = document.getElementById('relay-grid');
    const cfgPanel = document.getElementById('cfg-panel');
    const wsDot    = document.getElementById('ws-dot');

    // Kártyák generálása
    for (let i = 1; i <= RELAY_COUNT; i++) {
      states[i] = false;
      names[i]  = 'Relé ' + i;
      startTimes[i] = 0;
      grid.innerHTML += `
        <div class="card" id="card-${i}">
          <div class="card-header">
            <div class="card-icon" id="icon-${i}">⚡</div>
            <button class="gear" onclick="openConfig(${i})" title="Beállítások">⚙</button>
          </div>
          <div class="card-name" id="name-${i}">Relé ${i}</div>
          <div class="card-meta" id="meta-${i}">—</div>
          <div class="card-footer">
            <button class="toggle" id="btn-${i}" onclick="doToggle(${i})">KI</button>
            <span class="runtime" id="runtime-${i}"></span>
          </div>
        </div>`;
    }

    function doToggle(id) {
      socket.send(JSON.stringify({action:'toggle', id:id}));
    }

    function updateRelay(id, state, name) {
      states[id] = state;
      if (name) names[id] = name;
      const card = document.getElementById('card-'+id);
      const btn  = document.getElementById('btn-'+id);
      const nm   = document.getElementById('name-'+id);
      const meta = document.getElementById('meta-'+id);
      const rt   = document.getElementById('runtime-'+id);
      if (!btn) return;
      card.className = 'card' + (state ? ' on' : '');
      btn.className  = 'toggle' + (state ? ' on' : '');
      btn.textContent = state ? 'BE' : 'KI';
      nm.textContent  = names[id];
      if (state) {
        startTimes[id] = Date.now();
        meta.textContent = 'Fut...';
        rt.className = 'runtime on';
        showToast(names[id] + ' bekapcsolva');
      } else {
        meta.textContent = 'Utoljára: most';
        rt.textContent = '';
        rt.className = 'runtime';
        showToast(names[id] + ' kikapcsolva');
      }
    }

    // Futási idő frissítés
    setInterval(() => {
      for (let i = 1; i <= RELAY_COUNT; i++) {
        if (states[i]) {
          const sec = Math.floor((Date.now() - startTimes[i]) / 1000);
          const m = Math.floor(sec/60), s = sec%60;
          const rt = document.getElementById('runtime-'+i);
          if (rt) rt.textContent = m + 'p ' + s + 'mp';
        }
      }
    }, 1000);

    // Konfig panel
    function openConfig(id) {
      if (activeConfig === id) {
        cfgPanel.style.display = 'none';
        activeConfig = null;
        return;
      }
      activeConfig = id;
      cfgPanel.style.display = 'block';
      cfgPanel.innerHTML = `
        <div class="cfg-title">⚙ Beállítások – ${names[id]}</div>
        <div class="cfg-row">
          <span class="cfg-label">GPIO pin</span>
          <span class="badge">GPIO ${[16,17,18,19][id-1]}</span>
        </div>
        <div class="cfg-row">
          <span class="cfg-label">Állapot</span>
          <span class="cfg-val">${states[id] ? '🟢 BE' : '⚫ KI'}</span>
        </div>
        <div style="margin-top:10px;font-size:0.78em;color:#64748b;margin-bottom:6px">Átnevezés</div>
        <div class="inp-row">
          <input class="inp" id="cfg-inp" value="${names[id]}" maxlength="31">
          <button class="save-btn" onclick="saveName(${id})">Mentés</button>
        </div>`;
    }

    function saveName(id) {
      const val = document.getElementById('cfg-inp').value.trim();
      if (!val) return;
      names[id] = val;
      document.getElementById('name-'+id).textContent = val;
      socket.send(JSON.stringify({action:'rename', id:id, name:val}));
      cfgPanel.style.display = 'none';
      activeConfig = null;
      showToast('Név elmentve: ' + val);
    }

    function logout() {
      fetch('/logout', {method:'POST'}).then(() => window.location.href='/login');
    }

    function showToast(msg) {
      const t = document.getElementById('toast');
      t.textContent = '✓ ' + msg;
      t.classList.add('show');
      setTimeout(() => t.classList.remove('show'), 2500);
    }

    // WebSocket
    const socket = new WebSocket(`ws://${location.hostname}:81`);

    socket.onopen = () => {
      wsDot.className = 'ws-dot on';
    };
    socket.onclose = () => {
      wsDot.className = 'ws-dot';
      setTimeout(() => location.reload(), 3000);
    };
    socket.onerror = () => {
      wsDot.className = 'ws-dot';
    };

    socket.onmessage = (event) => {
      try {
        const data = JSON.parse(event.data);
        if (data.type === 'relay') {
          updateRelay(data.id, data.state, data.name);
        } else if (data.type === 'wifi') {
          document.getElementById('wifi-rssi').textContent = data.rssi + ' dBm';
          document.getElementById('wifi-ip').textContent   = data.ip;
        } else if (data.type === 'time') {
          const el = document.getElementById('datetime');
          if (el) el.textContent = data.display;
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
            if (!isAuthenticated()) {
                server.sendHeader("Location", "/login");
                server.send(302, "text/plain", "");
                return;
            }
            server.send(200, "text/html", INDEX_HTML);
        });

        server.on("/login", HTTP_GET, [this]() {
            if (isAuthenticated()) {
                server.sendHeader("Location", "/");
                server.send(302, "text/plain", "");
                return;
            }
            server.send(200, "text/html", LOGIN_HTML);
        });

        server.on("/login", HTTP_POST, [this]() {
            String pass = server.arg("pass");
            String storedPass = (eeprom != nullptr) ? eeprom->loadWebPassword() : "admin";
            if (pass == storedPass) {
                sessionToken = generateToken();
                if (eeprom != nullptr) eeprom->saveToken(sessionToken);
                String cookie = "token=" + sessionToken + "; Max-Age=2592000; Path=/; HttpOnly";
                server.sendHeader("Set-Cookie", cookie);
                server.sendHeader("Location", "/");
                server.send(302, "text/plain", "");
                Serial.println("[Auth] Sikeres bejelentkezés.");
            } else {
                server.send(401, "text/plain", "Hibás jelszó");
                Serial.println("[Auth] Hibás jelszó kísérlet.");
            }
        });

        server.on("/logout", HTTP_POST, [this]() {
            server.sendHeader("Set-Cookie", "token=; Max-Age=0; Path=/");
            server.send(200, "text/plain", "OK");
            Serial.println("[Auth] Kijelentkezés.");
        });

        server.onNotFound([this]() {
            server.send(404, "text/plain", "404: Nem található");
        });
    }

public:
    WebManager(WifiManager& wm) : wifiManager(wm) {}

    void setEeprom(EepromManager& em) {
        eeprom = &em;
        sessionToken = eeprom->loadToken();
        if (sessionToken.length() > 0) {
            Serial.println("[Auth] Token betöltve EEPROM-ból.");
        }
    }

    void begin() {
        const char* headers[] = {"Cookie"};
        server.collectHeaders(headers, 1);
        setupRoutes();
        server.begin();
        Serial.println("[WebManager] HTTP szerver elindult a 8080-as porton");
    }

    void handle() {
        server.handleClient();
    }
};

#endif // WEB_MANAGER_H
