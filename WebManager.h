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

    // --- Token generálás ---
    String generateToken() {
        String token = "";
        const char chars[] = "0123456789abcdef";
        for (int i = 0; i < 32; i++) {
            token += chars[esp_random() % 16];
        }
        return token;
    }

    // --- Cookie kiolvasás ---
    String getCookieValue(const String& cookieHeader, const String& name) {
        String search = name + "=";
        int start = cookieHeader.indexOf(search);
        if (start < 0) return "";
        start += search.length();
        int end = cookieHeader.indexOf(';', start);
        if (end < 0) end = cookieHeader.length();
        return cookieHeader.substring(start, end);
    }

    // --- Auth ellenőrzés ---
    bool isAuthenticated() {
        if (sessionToken.isEmpty()) return false;
        String cookieHeader = server.header("Cookie");
        String token = getCookieValue(cookieHeader, "token");
        return token == sessionToken;
    }

    // --- Login HTML ---
    const char* LOGIN_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta charset="UTF-8">
  <title>Bejelentkezés</title>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    * { box-sizing: border-box; margin: 0; padding: 0; }
    body {
      font-family: Arial, sans-serif;
      background: #1a1a2e;
      color: #eee;
      min-height: 100vh;
      display: flex;
      align-items: center;
      justify-content: center;
    }
    .box {
      background: #16213e;
      border-radius: 12px;
      padding: 40px 32px;
      width: 100%;
      max-width: 320px;
      border: 1px solid #0f3460;
      text-align: center;
    }
    h2 { color: #a0c4ff; margin-bottom: 24px; font-size: 1.2em; }
    input {
      width: 100%;
      padding: 10px 14px;
      border-radius: 8px;
      border: 1px solid #0f3460;
      background: #0f3460;
      color: #eee;
      font-size: 1em;
      margin-bottom: 16px;
      outline: none;
    }
    input:focus { border-color: #a0c4ff; }
    button {
      width: 100%;
      padding: 10px;
      border-radius: 8px;
      border: none;
      background: #4ade80;
      color: #14532d;
      font-size: 1em;
      font-weight: bold;
      cursor: pointer;
    }
    button:hover { background: #22c55e; }
    .error { color: #f87171; margin-top: 12px; font-size: 0.9em; }
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
      fetch('/login', {
        method: 'POST',
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
        body: 'pass=' + encodeURIComponent(pass)
      }).then(r => {
        if (r.ok) {
          window.location.href = '/';
        } else {
          document.getElementById('err').textContent = 'Hibás jelszó!';
        }
      });
    }
  </script>
</body>
</html>
)rawliteral";

    // --- Főoldal HTML ---
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
      width: 70px; height: 36px;
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
    .toggle.on { background: #4ade80; color: #14532d; }
    #ws-status { color: #f87171; }
    #ws-status.connected { color: #4ade80; }
    #logout-btn {
      margin-top: 24px;
      padding: 8px 20px;
      border-radius: 8px;
      border: 1px solid #374151;
      background: transparent;
      color: #aaa;
      cursor: pointer;
      font-size: 0.85em;
    }
    #logout-btn:hover { border-color: #f87171; color: #f87171; }
  </style>
</head>
<body>
  <h1>ESP32 Smart Switch</h1>
  <div id="status-bar">
    <span id="ws-status">● Kapcsolódás...</span>
    <span id="wifi-rssi"></span>
    <span id="wifi-ip"></span>
  </div>
  <div class="grid" id="relay-grid"></div>
  <button id="logout-btn" onclick="logout()">Kijelentkezés</button>

  <script>
    const RELAY_COUNT = 4;
    const states = {};
    const grid = document.getElementById('relay-grid');
    const wsStatus = document.getElementById('ws-status');

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

    function logout() {
      fetch('/logout', { method: 'POST' }).then(() => {
        window.location.href = '/login';
      });
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
        // Főoldal – auth szükséges
        server.on("/", HTTP_GET, [this]() {
            if (!isAuthenticated()) {
                server.sendHeader("Location", "/login");
                server.send(302, "text/plain", "");
                return;
            }
            server.send(200, "text/html", INDEX_HTML);
        });

        // Login oldal
        server.on("/login", HTTP_GET, [this]() {
            if (isAuthenticated()) {
                server.sendHeader("Location", "/");
                server.send(302, "text/plain", "");
                return;
            }
            server.send(200, "text/html", LOGIN_HTML);
        });

        // Login POST
        server.on("/login", HTTP_POST, [this]() {
            String pass = server.arg("pass");
            String storedPass = (eeprom != nullptr) ? eeprom->loadWebPassword() : "admin";
            if (pass == storedPass) {
                // Token generálás és mentés
                sessionToken = generateToken();
                if (eeprom != nullptr) eeprom->saveToken(sessionToken);
                // Cookie beállítás – 30 napos
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

        // Logout
        server.on("/logout", HTTP_POST, [this]() {
            // Cookie törlés
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
        // Token betöltés EEPROM-ból
        sessionToken = eeprom->loadToken();
        if (sessionToken.length() > 0) {
            Serial.println("[Auth] Token betöltve EEPROM-ból.");
        }
    }

    void begin() {
        // Cookie header olvasáshoz szükséges
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
