#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <WebServer.h>
#include "WifiManager.h"
#include "AuthManager.h"

class WebManager {
private:
    WebServer    server{8080};
    WifiManager& wifiManager;
    AuthManager* auth = nullptr;

    // Cookie értékének kinyerése a header stringből
    String getCookieValue(const String& header, const String& name) {
        String search = name + "=";
        int s = header.indexOf(search);
        if (s < 0) return "";
        s += search.length();
        int e = header.indexOf(';', s);
        if (e < 0) e = header.length();
        return header.substring(s, e);
    }

    // HTTP kérés hitelesítése: cookie token -> AuthManager ellenőrzés
    bool isAuthenticated() {
        if (auth == nullptr) return false;
        String token = getCookieValue(server.header("Cookie"), "token");
        return auth->isAuthenticated(token);
    }

    // ----------------------------------------------------------------
    const char* LOGIN_HTML = R"rawliteral(
<!DOCTYPE html><html><head><meta charset="UTF-8"><title>Bejelentkezes</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{font-family:Arial,sans-serif;background:#f0f2f5;display:flex;justify-content:center;align-items:center;height:100vh;margin:0}
.card{background:#fff;padding:30px;border-radius:8px;box-shadow:0 4px 15px rgba(0,0,0,.1);width:100%;max-width:360px}
h2{margin-top:0;color:#333;text-align:center}
input[type=password]{width:100%;padding:10px;margin:15px 0;border:1px solid #ccc;border-radius:4px;box-sizing:border-box}
button{width:100%;padding:10px;background:#007bff;color:#fff;border:none;border-radius:4px;font-size:16px;cursor:pointer}
</style></head><body>
<div class="card"><h2>Vezerlo Belepes</h2>
<form action="/login" method="POST">
<input type="password" name="password" placeholder="Jelszo" required autofocus>
<button type="submit">Bejelentkezes</button>
</form></div></body></html>
)rawliteral";

    // ----------------------------------------------------------------
    const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html><html><head>
<meta charset="UTF-8"><title>Smart Relay Dashboard</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
*{box-sizing:border-box}
body{font-family:'Segoe UI',sans-serif;background:#eef2f7;margin:0;padding:16px;color:#333}
.container{max-width:960px;margin:0 auto}
.header{background:#1e293b;color:#fff;padding:16px 20px;border-radius:8px;margin-bottom:16px;display:flex;justify-content:space-between;align-items:center}
.header h1{margin:0;font-size:22px}
.info-bar{background:#fff;padding:10px 20px;border-radius:8px;margin-bottom:16px;display:flex;justify-content:space-between;font-size:14px;flex-wrap:wrap;gap:6px}
.grid{display:grid;grid-template-columns:repeat(auto-fit,minmax(200px,1fr));gap:16px;margin-bottom:20px}
.card{background:#fff;padding:18px;border-radius:8px;box-shadow:0 2px 6px rgba(0,0,0,.07)}
.relay-name{font-weight:700;font-size:17px;margin-bottom:6px;display:inline-block}
.rename-btn{background:none;border:none;color:#007bff;cursor:pointer;font-size:12px;margin-left:4px}
.relay-status{font-size:13px;color:#666;margin-bottom:12px;min-height:34px}
.switch-btn{width:100%;padding:11px;border:none;border-radius:6px;color:#fff;font-weight:700;font-size:15px;cursor:pointer;transition:.15s}
.switch-btn.on{background:#ef4444}.switch-btn.off{background:#10b981}
.logout-btn{background:#64748b;color:#fff;border:none;padding:7px 14px;border-radius:4px;cursor:pointer}
.chpass-btn{background:#475569;color:#fff;border:none;padding:7px 14px;border-radius:4px;cursor:pointer;margin-right:6px}

/* ---- Jelszócsere modal ---- */
.modal-overlay{display:none;position:fixed;inset:0;background:rgba(0,0,0,.5);z-index:1000;justify-content:center;align-items:center}
.modal-overlay.open{display:flex}
.modal-box{background:#fff;border-radius:10px;padding:28px 24px;width:100%;max-width:360px;box-shadow:0 8px 32px rgba(0,0,0,.2)}
.modal-box h3{margin:0 0 20px;color:#1e293b;font-size:18px}
.modal-field{display:flex;flex-direction:column;gap:5px;margin-bottom:14px}
.modal-field label{font-size:12px;font-weight:700;color:#475569}
.modal-field input{padding:9px 12px;border:1px solid #cbd5e1;border-radius:5px;font-size:14px;width:100%}
.modal-field input:focus{outline:none;border-color:#2563eb;box-shadow:0 0 0 2px rgba(37,99,235,.15)}
.modal-actions{display:flex;gap:8px;margin-top:8px}
.modal-save{flex:1;background:#2563eb;color:#fff;border:none;padding:10px;border-radius:5px;font-weight:700;cursor:pointer;font-size:14px}
.modal-save:hover{background:#1d4ed8}
.modal-cancel{flex:1;background:#f1f5f9;color:#475569;border:1px solid #cbd5e1;padding:10px;border-radius:5px;font-weight:700;cursor:pointer;font-size:14px}
.modal-cancel:hover{background:#e2e8f0}
.modal-msg{font-size:13px;margin-top:10px;min-height:18px;text-align:center;font-weight:600}
.modal-msg.ok{color:#10b981}.modal-msg.err{color:#ef4444}

/* ---- Időzítő szekció ---- */
.sched-section{background:#fff;padding:20px;border-radius:8px;margin-top:4px}
.sched-section h2{margin-top:0;font-size:18px;color:#1e293b}
.tabs{display:flex;gap:8px;margin-bottom:16px}
.tab-btn{padding:8px 18px;border:2px solid #cbd5e1;border-radius:6px;background:#f8fafc;cursor:pointer;font-weight:600;color:#475569;transition:.15s}
.tab-btn.active{border-color:#2563eb;background:#2563eb;color:#fff}
.tab-pane{display:none}.tab-pane.active{display:block}

/* form */
.form-row{display:flex;flex-wrap:wrap;gap:12px;align-items:flex-end;background:#f8fafc;padding:14px;border-radius:6px;margin-bottom:16px}
.fg{display:flex;flex-direction:column;gap:4px}
.fg label{font-size:12px;font-weight:700;color:#475569}
.fg select,.fg input{padding:8px 10px;border:1px solid #cbd5e1;border-radius:4px;font-size:14px}
.save-btn{background:#2563eb;color:#fff;border:none;padding:9px 20px;border-radius:4px;font-weight:700;cursor:pointer;height:38px;align-self:flex-end}
.save-btn:hover{background:#1d4ed8}

/* nap választó */
.day-picker{display:flex;gap:6px;flex-wrap:wrap}
.day-btn{width:38px;height:38px;border:2px solid #cbd5e1;border-radius:50%;background:#f8fafc;cursor:pointer;font-weight:700;font-size:13px;color:#475569;transition:.15s}
.day-btn.sel{border-color:#2563eb;background:#2563eb;color:#fff}

/* táblázat */
.sched-table{width:100%;border-collapse:collapse;font-size:14px}
.sched-table th{background:#f1f5f9;padding:9px 10px;color:#475569;border-bottom:2px solid #e2e8f0;text-align:left}
.sched-table td{padding:10px;border-bottom:1px solid #edf2f7;vertical-align:middle}
.del-btn{background:#ef4444;color:#fff;border:none;padding:4px 10px;border-radius:4px;cursor:pointer}
.badge-on{color:#10b981;font-weight:700}.badge-off{color:#ef4444;font-weight:700}
.badge-week{display:inline-flex;gap:3px;flex-wrap:wrap}
.day-tag{background:#dbeafe;color:#1d4ed8;border-radius:3px;padding:1px 5px;font-size:12px;font-weight:600}

/* ---- Relay config modal ---- */
.relcfg-btn{background:#334155;color:#fff;border:none;padding:7px 14px;border-radius:4px;cursor:pointer;margin-right:6px}
.modal-box-wide{background:#fff;border-radius:10px;padding:24px;width:100%;max-width:580px;box-shadow:0 8px 32px rgba(0,0,0,.2);max-height:90vh;overflow-y:auto}
.modal-box-wide h3{margin:0 0 14px;color:#1e293b;font-size:18px}
.rcfg-head{display:grid;grid-template-columns:60px 1fr 72px 110px 36px;gap:8px;padding:0 0 6px;border-bottom:2px solid #e2e8f0;margin-bottom:4px}
.rcfg-head span{font-size:11px;font-weight:700;color:#475569}
.rcfg-row{display:grid;grid-template-columns:60px 1fr 72px 110px 36px;gap:8px;align-items:center;padding:7px 0;border-bottom:1px solid #f1f5f9}
.rcfg-row:last-child{border-bottom:none}
.rcfg-row input[type=text]{padding:6px 8px;border:1px solid #cbd5e1;border-radius:4px;font-size:13px;width:100%}
.rcfg-row input[type=number]{padding:6px 8px;border:1px solid #cbd5e1;border-radius:4px;font-size:13px;width:100%;text-align:center}
.rcfg-row select{padding:6px 4px;border:1px solid #cbd5e1;border-radius:4px;font-size:12px;width:100%}
.rcfg-add{background:#10b981;color:#fff;border:none;padding:8px 16px;border-radius:4px;font-weight:700;cursor:pointer;font-size:13px;margin-top:10px}
.rcfg-add:hover{background:#059669}
.rcfg-del{background:#ef4444;color:#fff;border:none;width:28px;height:28px;border-radius:4px;cursor:pointer;font-size:14px;line-height:1}

@media(max-width:480px){
body{padding:10px}
.grid{grid-template-columns:repeat(2,1fr);gap:10px}
.card{padding:12px 10px}
.relay-name{font-size:13px;white-space:nowrap;overflow:hidden;text-overflow:ellipsis;max-width:100%;display:block}
.rename-btn{font-size:10px}
.relay-status{font-size:11px;min-height:28px;margin-bottom:8px}
.switch-btn{padding:8px;font-size:13px}
.header{flex-wrap:wrap;gap:8px;padding:12px 14px}
.header h1{font-size:18px}
.info-bar{font-size:12px;padding:8px 12px}
}

/* Szenzor jelvények a relé kártyán */
.sensor-badge {
    display: inline-block;
    background: #e2e8f0;
    padding: 2px 8px;
    border-radius: 12px;
    font-size: 12px;
    margin: 2px;
    color: #475569;
    font-weight: bold;
}

/* Szenzor gomb a kártyán */
.sensor-btn {
    background: #f1f5f9;
    border: 1px solid #cbd5e1;
    cursor: pointer;
    padding: 5px;
    border-radius: 4px;
    margin-top: 5px;
    font-size: 11px;
}
.sensor-btn:hover { background: #e2e8f0; }

/* Modal stílus (ha még nincs) */
.modal { display: none; position: fixed; top: 0; left: 0; width: 100%; height: 100%; background: rgba(0,0,0,0.5); }
.modal.open { display: flex; justify-content: center; align-items: center; }
.modal-content { background: white; padding: 20px; border-radius: 8px; width: 300px; }
</style></head><body>
<div class="container">

<!-- Fejléc -->
<div class="header">
  <h1>&#x26A1; Smart Relay System</h1>
  <div>
    <button class="relcfg-btn" onclick="openRelayModal()">&#x2699; Relek</button>
    <button class="chpass-btn" onclick="openPassModal()">&#x1F512; Jelszocsere</button>
    <button class="logout-btn" onclick="logout()">Kijelentkezes</button>
  </div>
</div>

<!-- Jelszócsere modal -->
<div class="modal-overlay" id="passModal">
  <div class="modal-box">
    <h3>&#x1F512; Jelszo megvaltoztatasa</h3>
    <div class="modal-field">
      <label>Jelenlegi jelszo</label>
      <input type="password" id="passOld" placeholder="Jelenlegi jelszo">
    </div>
    <div class="modal-field">
      <label>Uj jelszo</label>
      <input type="password" id="passNew" placeholder="Legalabb 4 karakter">
    </div>
    <div class="modal-field">
      <label>Uj jelszo megegyszer</label>
      <input type="password" id="passNew2" placeholder="Uj jelszo megegyszer">
    </div>
    <div class="modal-actions">
      <button class="modal-cancel" onclick="closePassModal()">Megse</button>
      <button class="modal-save" onclick="changePassword()">Mentés</button>
    </div>
    <div class="modal-msg" id="passMsg"></div>
  </div>
</div>

<!-- Relay konfiguráció modal -->
<div class="modal-overlay" id="relayModal">
  <div class="modal-box-wide">
    <h3>&#x2699; Relek konfigurálása</h3>
    <div class="rcfg-head">
      <span>UUID</span><span>Nev</span><span>GPIO pin</span><span>Logika</span><span></span>
    </div>
    <div id="rcfgList"></div>
    <button class="rcfg-add" onclick="addRcfgRow()">+ Rele hozzaadasa</button>
    <div class="modal-actions" style="margin-top:14px">
      <button class="modal-cancel" onclick="closeRelayModal()">Megse</button>
      <button class="modal-save"   onclick="saveRelayCfg()">Mentes</button>
    </div>
    <div class="modal-msg" id="relayMsg"></div>
  </div>
</div>

<!-- Sensor konfigurációs modal -->
<div id="sensorModal" class="modal">
  <div class="modal-content">
    <h3>Szenzor hozzáadása</h3>
    <input type="hidden" id="sensorUuid">
    <select id="sensorType">
      <option value="CURRENT">Árammérő (CURRENT)</option>
    </select>
    <input type="number" id="sensorPin" placeholder="GPIO Pin">
    <button onclick="saveSensor()">Mentés</button>
    <button onclick="closeSensorModal()">Mégse</button>
  </div>
</div>

<!-- Idő / WiFi / Memória sáv -->
<div class="info-bar">
  <span id="time-display">Ido: Szinkronizalas...</span>
  <span id="wifi-display">WiFi: --</span>
  <span id="boot-display" style="font-weight:bold; color:#64748b;">Indulások: 0</span>
  <span id="mem-display" style="display:flex;align-items:center;gap:8px;font-size:13px">
    <span>RAM:</span>
    <span style="position:relative;display:inline-block;width:100px;height:12px;background:#e2e8f0;border-radius:6px;overflow:hidden">
      <span id="mem-bar" style="position:absolute;left:0;top:0;height:100%;background:#10b981;border-radius:6px;transition:width .5s"></span>
    </span>
    <span id="mem-text">-- KB</span>
  </span>
</div>

<!-- Relé kártyák -->
<div class="grid" id="relay-grid"></div>

<!-- Időzítő szekció -->
<div class="sched-section">
  <h2>&#x23F0; Automatikus Idozitesek</h2>

  <!-- Fülek -->
  <div class="tabs">
    <button class="tab-btn active" onclick="switchTab('once',this)">&#x1F4C5; Egyszeri</button>
    <button class="tab-btn"       onclick="switchTab('weekly',this)">&#x1F501; Heti ismetlodo</button>
  </div>

  <!-- Egyszeri form -->
  <div id="tab-once" class="tab-pane active">
    <div class="form-row">
      <div class="fg"><label>Cel eszkoz</label><select id="onceRelay"></select></div>
      <div class="fg">
        <label>Kezdes</label>
        <input type="datetime-local" id="onceFrom" step="1">
      </div>
      <div class="fg">
        <label>Vege</label>
        <input type="datetime-local" id="onceTo" step="1">
      </div>
      <div class="fg"><label>Muvelet</label>
        <select id="onceAction">
          <option value="1">Bekapcsolas (ON)</option>
          <option value="0">Kikapcsolas (OFF)</option>
        </select>
      </div>
      <div class="fg"><label>Lejarat utan</label>
        <select id="onceEndAction">
          <option value="0">Kikapcsolas (OFF)</option>
          <option value="1">Bekapcsolas (ON)</option>
          <option value="2">Marad (KEEP)</option>
        </select>
      </div>
      <button class="save-btn" onclick="addOnce()">Mentes</button>
    </div>
  </div>

  <!-- Heti form -->
  <div id="tab-weekly" class="tab-pane">
    <div class="form-row">
      <div class="fg"><label>Cel eszkoz</label><select id="weekRelay"></select></div>
      <div class="fg">
        <label>Napok</label>
        <div class="day-picker">
          <button class="day-btn" data-bit="0" onclick="toggleDay(this)">H</button>
          <button class="day-btn" data-bit="1" onclick="toggleDay(this)">K</button>
          <button class="day-btn" data-bit="2" onclick="toggleDay(this)">Sze</button>
          <button class="day-btn" data-bit="3" onclick="toggleDay(this)">Cs</button>
          <button class="day-btn" data-bit="4" onclick="toggleDay(this)">P</button>
          <button class="day-btn" data-bit="5" onclick="toggleDay(this)">Szo</button>
          <button class="day-btn" data-bit="6" onclick="toggleDay(this)">V</button>
        </div>
      </div>
      <div class="fg"><label>Kezdes (oo:pp:mm)</label><input type="time" id="weekStart" value="08:00:00" step="1"></div>
      <div class="fg"><label>Vege (oo:pp:mm)</label><input type="time" id="weekEnd" value="08:00:15" step="1"></div>
      <div class="fg"><label>Muvelet</label>
        <select id="weekAction">
          <option value="1">Bekapcsolas (ON)</option>
          <option value="0">Kikapcsolas (OFF)</option>
        </select>
      </div>
      <div class="fg"><label>Lejarat utan</label>
        <select id="weekEndAction">
          <option value="0">Kikapcsolas (OFF)</option>
          <option value="1">Bekapcsolas (ON)</option>
          <option value="2">Marad (KEEP)</option>
        </select>
      </div>
      <button class="save-btn" onclick="addWeekly()">Mentes</button>
    </div>
  </div>

  <!-- Táblázat -->
  <table class="sched-table">
    <thead>
      <tr><th>Rele</th><th>Tipusa</th><th>Idozites</th><th>Muvelet</th><th>Torles</th></tr>
    </thead>
    <tbody id="scheduleList">
      <tr><td colspan="5" style="text-align:center;color:#94a3b8;">Nincsenek aktiv idozitesek.</td></tr>
    </tbody>
  </table>
</div>

</div><!-- /container -->

<script>
// ================================================================
// Globális állapot
// ================================================================
let ws;
let relayNamesMap  = {};
let relayStates    = {};
let relayUptimes   = {};
let uptimeTimer    = null;
let lastRelayState = [];
let pendingReboot  = false;
let reconnectDelay = 1000; // Alapértelmezett 1 másodperc

const DAY_NAMES = ['H','K','Sze','Cs','P','Szo','V'];

// ================================================================
// WebSocket
// ================================================================
function initWebSocket() {
  ws = new WebSocket('ws://' + window.location.hostname + ':81');
  ws.onmessage = e => {
    const d = JSON.parse(e.data);
    if (d.type === 'full_state') {
      lastRelayState = d.relays || [];
      renderRelays(d.relays);
      renderSchedules(d.schedules);
      updateSelects(d.relays);
      startUptimeTimer();
      updateMemory(d.heap_free, d.heap_total);
        const bootEl = document.getElementById('boot-display');
        if (bootEl) {
            bootEl.innerText = 'Indulások száma: ' + d.boot_count;
        }
    } else if (d.type === 'time') {
      document.getElementById('time-display').innerText = 'Ido: ' + d.display;
      // Memória frissítése a time üzenetből – másodpercenként automatikusan
      updateMemory(d.heap_free, d.heap_total);
    } else if (d.type === 'wifi') {
      if (d.mode === 'AP') {
        document.getElementById('wifi-display').innerText =
          'WiFi: [Hotspot] ' + d.ip;
      } else {
        document.getElementById('wifi-display').innerText =
          'WiFi: ' + (d.connected ? d.rssi + ' dBm | ' + d.ip : 'Nincs kapcsolat');
      }
    } else if (d.type === 'reboot') {
      pendingReboot = true;
    }
  };
  ws.onclose = () => { 
    // Exponenciális növelés: duplázza az időt, de max 30 másodperc
    reconnectDelay = Math.min(reconnectDelay * 2, 30000); 
    setTimeout(initWebSocket, reconnectDelay); 
  };
  ws.onopen = () => { 
    reconnectDelay = 1000; // Sikeres csatlakozáskor reseteljük 1 másodpercre
    if (pendingReboot) { pendingReboot=false; location.reload(); } 
  };
}

// ================================================================
// Relék
// ================================================================
function startUptimeTimer() {
  if (uptimeTimer) clearInterval(uptimeTimer);
  uptimeTimer = setInterval(() => {
    for (const id in relayStates) {
      if (relayStates[id]) {
        relayUptimes[id] = (relayUptimes[id] || 0) + 1;
        const el = document.getElementById('status-' + id);
        if (el) el.innerHTML = 'Allapot: <b style="color:#10b981">BE</b><br>Futasi ido: ' + formatUptime(relayUptimes[id]);
      }
    }
  }, 1000);
}

function renderRelays(relays) {
  const grid = document.getElementById('relay-grid');
  grid.innerHTML = '';
  relays.forEach(r => {
    relayNamesMap[r.id] = r.name;
    relayStates[r.id]   = r.state;
    relayUptimes[r.id]  = r.uptime || 0;

    // --- ÚJ: Szenzorok megjelenítése ---
    let sensorsHtml = '';
    if (r.sensors && r.sensors.length > 0) {
      r.sensors.forEach(s => {
        sensorsHtml += `<div class="sensor-badge">⚡ ${s.type}: ${s.value}A</div>`;
      });
    }

    const c = document.createElement('div');
    c.className = 'card';
    c.innerHTML =
      '<span class="relay-name">' + r.name + '</span>' +
      '<button class="rename-btn" onclick="renameRelay(' + r.id + ',\'' + r.name + '\')">&#x270F;</button>' +
      '<div class="relay-status" id="status-' + r.id + '">' +
        'Allapot: <b style="color:' + (r.state ? '#10b981' : '#ef4444') + '">' + (r.state ? 'BE' : 'KI') + '</b><br>' +
        'Futasi ido: ' + formatUptime(r.uptime) +
      '</div>' +
      sensorsHtml + // Itt jelennek meg a szenzorok
      '<button class="sensor-btn" onclick="openSensorModal(\'' + (r.uuid || '') + '\')">⚙️ Szenzor</button>' +
      '<button class="switch-btn ' + (r.state ? 'on' : 'off') + '" onclick="toggleRelay(' + r.id + ')">' +
        (r.state ? 'Kikapcsolas' : 'Bekapcsolas') +
      '</button>';
    grid.appendChild(c);
  });
}

function updateSelects(relays) {
  ['onceRelay','weekRelay'].forEach(selId => {
    const sel = document.getElementById(selId);
    const cur = sel.value;
    sel.innerHTML = '';
    relays.forEach(r => {
      const o = document.createElement('option');
      o.value = r.id; o.innerText = r.name;
      sel.appendChild(o);
    });
    if (cur) sel.value = cur;
  });
}

function toggleRelay(id)  { ws.send(JSON.stringify({type:'toggle',id})); }
function renameRelay(id, oldName) {
  const n = prompt('Uj nev:', oldName);
  if (n && n.trim()) ws.send(JSON.stringify({type:'rename',id,name:n.trim()}));
}

// ================================================================
// Tabs
// ================================================================
function switchTab(name, btn) {
  document.querySelectorAll('.tab-pane').forEach(p => p.classList.remove('active'));
  document.querySelectorAll('.tab-btn').forEach(b => b.classList.remove('active'));
  document.getElementById('tab-' + name).classList.add('active');
  btn.classList.add('active');
}

// ================================================================
// Nap kiválasztó
// ================================================================
function toggleDay(btn) {
  btn.classList.toggle('sel');
}
function getDayMask() {
  let mask = 0;
  document.querySelectorAll('.day-btn.sel').forEach(b => {
    mask |= (1 << parseInt(b.dataset.bit));
  });
  return mask;
}

// ================================================================
// Egyszeri időzítés
// ================================================================
function addOnce() {
  // 1. Mezők beolvasása (az 'onceFromSec' és 'onceToSec' már nem kell!)
  const relay     = parseInt(document.getElementById('onceRelay').value);
  const fromV     = document.getElementById('onceFrom').value;
  const toV       = document.getElementById('onceTo').value;
  const action    = parseInt(document.getElementById('onceAction').value);
  const endAction = parseInt(document.getElementById('onceEndAction').value);

  // 2. Ellenőrzés
  if (!fromV || !toV) { 
    alert('Adj meg dátumot!'); 
    return; 
  }

  // 3. UNIX időbélyeg számítása
  // A 'new Date(fromV).getTime()' a step="1" miatt már tartalmazza a másodpercet is!
  const fromEpoch = Math.floor(new Date(fromV).getTime() / 1000);
  const toEpoch   = Math.floor(new Date(toV).getTime() / 1000);

  if (fromEpoch >= toEpoch) { 
    alert('A végnek a kezdés után kell lennie!'); 
    return; 
  }

  // 4. Küldés a szervernek
  ws.send(JSON.stringify({
    type: 'add_schedule_once',
    id:   Math.floor(Date.now() / 1000),
    relay: relay, 
    from: fromEpoch, 
    to: toEpoch, 
    action: action, 
    endAction: endAction
  }));

  // 5. HTML mezők alaphelyzetbe állítása (kizárólag a meglévők)
  document.getElementById('onceFrom').value = '';
  document.getElementById('onceTo').value   = '';
}

// ================================================================
// Heti ismétlődő időzítés
// ================================================================
function addWeekly() {
  const relay    = parseInt(document.getElementById('weekRelay').value);
  const startVal = document.getElementById('weekStart').value;
  const endVal   = document.getElementById('weekEnd').value;
  const [sh2, sm2, ss2] = startVal.split(':').map(Number);
  const [eh2, em2, es2] = endVal.split(':').map(Number);
  const startSec = sh2 * 3600 + sm2 * 60 + (ss2 || 0);
  const endSec   = eh2 * 3600 + em2 * 60 + (es2 || 0);
  const action   = parseInt(document.getElementById('weekAction').value);
  const dayMask  = getDayMask();

  if (!startVal || !endVal) { alert('Adj meg idopontot!'); return; }
  if (dayMask === 0)         { alert('Valassz legalabb egy napot!'); return; }

  const endActionW = parseInt(document.getElementById('weekEndAction').value);
  ws.send(JSON.stringify({
    type: 'add_schedule_weekly',
    id:   Math.floor(Date.now() / 1000),
    relay, dayMask, startSec, endSec, action, endAction: endActionW
  }));

  // Reset
  document.querySelectorAll('.day-btn').forEach(b => b.classList.remove('sel'));
  document.getElementById('weekStart').value = '08:00:00';
  document.getElementById('weekEnd').value   = '08:00:15';
}

// ================================================================
// Törlés
// ================================================================
function deleteSchedule(relay, id) {
  if (confirm('Torlod ezt az idozitesi szabalyt?'))
    ws.send(JSON.stringify({type:'delete_schedule', relay, id}));
}

// ================================================================
// Lista renderelése
// ================================================================
function renderSchedules(schedules) {
  const tbody = document.getElementById('scheduleList');
  tbody.innerHTML = '';
  if (!schedules || schedules.length === 0) {
    tbody.innerHTML = '<tr><td colspan="5" style="text-align:center;color:#94a3b8;">Nincsenek aktiv idozitesek.</td></tr>';
    return;
  }
  schedules.forEach(s => {
    const name   = relayNamesMap[s.relay] || (s.relay + '. Rele');
    const actStr = s.action === 1
      ? '<span class="badge-on">&#x25CF; BE</span>'
      : '<span class="badge-off">&#x25CF; KI</span>';
    const endActStr = s.endAction === 2
      ? '<span style="color:#64748b">&#x25CF; Marad</span>'
      : s.endAction === 1
        ? '<span class="badge-on">&#x25CF; BE</span>'
        : '<span class="badge-off">&#x25CF; KI</span>';

    let typeStr, schedStr;

    if (s.type === 0) {
      // ONE_TIME - sajat formatozas, toLocaleString nem mutat masodpercet
      typeStr = '&#x1F4C5; Egyszeri';
      const fDate = new Date(s.from * 1000);
      const tDate = new Date(s.to   * 1000);
      const fmt = d =>
        d.getFullYear() + '.' +
        String(d.getMonth() + 1).padStart(2,'0') + '.' +
        String(d.getDate()).padStart(2,'0') + '. ' +
        String(d.getHours()).padStart(2,'0') + ':' +
        String(d.getMinutes()).padStart(2,'0') + ':' +
        String(d.getSeconds()).padStart(2,'0');
      schedStr = fmt(fDate) + ' &rarr;<br><small style="color:#64748b">' + fmt(tDate) + '</small>';
    } else {
      // WEEKLY - nap badge + oo:pp:mm
      typeStr = '&#x1F501; Heti';
      const dayBadges = DAY_NAMES
        .filter((_, i) => (s.dayMask >> i) & 1)
        .map(d => '<span class="day-tag">' + d + '</span>')
        .join('');
      const sH = String(Math.floor(s.startSec / 3600)).padStart(2,'0');
      const sM = String(Math.floor((s.startSec % 3600) / 60)).padStart(2,'0');
      const sS = String(s.startSec % 60).padStart(2,'0');
      const eH = String(Math.floor(s.endSec / 3600)).padStart(2,'0');
      const eM = String(Math.floor((s.endSec % 3600) / 60)).padStart(2,'0');
      const eS = String(s.endSec % 60).padStart(2,'0');
      schedStr = '<div class="badge-week">' + dayBadges + '</div>' +
                 '<small>' + sH + ':' + sM + ':' + sS + ' &rarr; ' + eH + ':' + eM + ':' + eS + '</small>';
    }

    const tr = document.createElement('tr');
    tr.innerHTML =
      '<td><b>' + name + '</b></td>' +
      '<td>' + typeStr + '</td>' +
      '<td>' + schedStr + '</td>' +
      '<td>' + actStr + ' &rarr; ' + endActStr + '</td>' +
      '<td><button class="del-btn" onclick="deleteSchedule(' + s.relay + ',' + s.id + ')">&#x2716;</button></td>';
    tbody.appendChild(tr);
  });
}

// ================================================================
// Segédfüggvények
// ================================================================
function formatUptime(s) {
  if (!s) return '0p';
  let m = Math.floor(s / 60), h = Math.floor(m / 60);
  s %= 60; m %= 60;
  return h > 0 ? h + 'o ' + m + 'p' : m + 'p ' + s + 'm';
}

function updateMemory(free, total) {
  if (!free || !total) return;
  const usedPct = Math.round((1 - free / total) * 100);
  const freeKB  = Math.round(free  / 1024);
  const totalKB = Math.round(total / 1024);
  // Szín: zöld < 60%, sárga 60-80%, piros > 80%
  const color = usedPct < 60 ? '#10b981' : usedPct < 80 ? '#f59e0b' : '#ef4444';
  const bar  = document.getElementById('mem-bar');
  const text = document.getElementById('mem-text');
  if (bar)  { bar.style.width = usedPct + '%'; bar.style.background = color; }
  if (text) text.innerText = freeKB + ' / ' + totalKB + ' KB szabad (' + usedPct + '%)';
}
function logout() { fetch('/logout',{method:'POST'}).then(()=>location.reload()); }

// ================================================================
// Relay konfiguráció modal
// ================================================================
let rcfgData      = [];   // [{id, name, pin, activeLow}, ...]
//let pendingReboot = false;

function openRelayModal() {
  rcfgData = [];
  document.getElementById('rcfgList').innerHTML = '';
  document.getElementById('relayMsg').textContent = '';
  document.getElementById('relayMsg').className   = 'modal-msg';

  // Jelenlegi relék betöltése lastRelayState-ből
  if (lastRelayState && lastRelayState.length > 0) {
    lastRelayState.forEach(r => {
      rcfgData.push({
        id:        r.id,
        uuid:      r.uuid      || '',
        name:      r.name,
        pin:       r.pin       || 0,
        activeLow: r.activeLow !== false
      });
    });
  }
  rcfgData.forEach((_, i) => renderRcfgRow(i));
  document.getElementById('relayModal').classList.add('open');
}

function closeRelayModal() {
  document.getElementById('relayModal').classList.remove('open');
}

function renderRcfgRow(i) {
  const r   = rcfgData[i];
  const row = document.createElement('div');
  row.className = 'rcfg-row';
  row.id = 'rcfgRow-' + i;
  // UUID: meglévő reléknél megőrizzük, új reléknél "NEW" jelzés amíg ESP nem generál
  const uuidDisplay = r.uuid && r.uuid !== '000000'
    ? '<span style="font-family:monospace;font-size:11px;color:#475569;background:#f1f5f9;padding:2px 4px;border-radius:3px">' + r.uuid + '</span>'
    : '<span style="font-size:10px;color:#94a3b8;font-style:italic">auto</span>';

  row.innerHTML =
    // UUID mező – csak olvasható, rejtett input tárolja az értéket
    '<div style="display:flex;align-items:center">' + uuidDisplay +
      '<input type="hidden" id="rcUUID-' + i + '" value="' + (r.uuid || '') + '"></div>' +
    '<input type="text"   id="rcName-' + i + '" value="' + r.name + '" placeholder="Nev" maxlength="31">' +
    '<input type="number" id="rcPin-'  + i + '" value="' + r.pin  + '" min="0" max="39" placeholder="GPIO">' +
    '<select id="rcLow-' + i + '">' +
      '<option value="1"' + ( r.activeLow ? ' selected' : '') + '>ActiveLOW</option>' +
      '<option value="0"' + (!r.activeLow ? ' selected' : '') + '>ActiveHIGH</option>' +
    '</select>' +
    '<button class="rcfg-del" onclick="removeRcfgRow(' + i + ')">&#x2716;</button>';
  document.getElementById('rcfgList').appendChild(row);
}

function addRcfgRow() {
  const newId = rcfgData.length + 1;
  rcfgData.push({ id: newId, uuid: '', name: 'Rele ' + newId, pin: 0, activeLow: true });
  renderRcfgRow(rcfgData.length - 1);
}

function removeRcfgRow(i) {
  rcfgData.splice(i, 1);
  document.getElementById('rcfgList').innerHTML = '';
  rcfgData.forEach((_, idx) => renderRcfgRow(idx));
}

function saveRelayCfg() {
  const relays = [];
  for (let i = 0; i < rcfgData.length; i++) {
    const name      = document.getElementById('rcName-' + i).value.trim();
    const pin       = parseInt(document.getElementById('rcPin-'  + i).value);
    const activeLow = document.getElementById('rcLow-'  + i).value === '1';
    if (!name)                        { alert('A nev nem lehet ures!');            return; }
    if (isNaN(pin) || pin < 0 || pin > 39) { alert('Ervenytelen GPIO: ' + pin);   return; }
    const uuid = document.getElementById('rcUUID-' + i).value;
    relays.push({ id: i + 1, uuid, name, pin, activeLow });
  }
  ws.send(JSON.stringify({ type: 'save_relay_config', relays }));
  const msg = document.getElementById('relayMsg');
  msg.textContent = 'Mentés sikeres! Az ESP32 ujraindul...';
  msg.className   = 'modal-msg ok';
  pendingReboot   = true;
}

// ================================================================
// Jelszócsere modal
// ================================================================
function openPassModal() {
  document.getElementById('passOld').value  = '';
  document.getElementById('passNew').value  = '';
  document.getElementById('passNew2').value = '';
  setPassMsg('', '');
  document.getElementById('passModal').classList.add('open');
  setTimeout(() => document.getElementById('passOld').focus(), 50);
}

function closePassModal() {
  document.getElementById('passModal').classList.remove('open');
}

function setPassMsg(text, type) {
  const el = document.getElementById('passMsg');
  el.textContent = text;
  el.className = 'modal-msg' + (type ? ' ' + type : '');
}

// Egységes event listener – ESC, háttér kattintás, Enter
document.addEventListener('keydown', e => {
  if (e.key === 'Escape') { closeRelayModal(); closePassModal(); closeSensorModal(); }
  if (e.key === 'Enter' && document.getElementById('passModal').classList.contains('open'))
    changePassword();
});
document.addEventListener('click', e => {
  if (e.target === document.getElementById('relayModal')) closeRelayModal();
  if (e.target === document.getElementById('passModal'))  closePassModal();
  if (e.target === document.getElementById('sensorModal')) closeSensorModal();
});

function changePassword() {
  const oldP  = document.getElementById('passOld').value.trim();
  const newP  = document.getElementById('passNew').value;
  const newP2 = document.getElementById('passNew2').value;

  // Kliens oldali validáció
  if (!oldP || !newP || !newP2) {
    setPassMsg('Minden mezo kitoltese kotelezo!', 'err'); return;
  }
  if (newP.length < 4) {
    setPassMsg('Az uj jelszo legalabb 4 karakter legyen!', 'err'); return;
  }
  if (newP !== newP2) {
    setPassMsg('A ket uj jelszo nem egyezik!', 'err'); return;
  }

  setPassMsg('Mentés...', '');

  fetch('/change_password', {
    method: 'POST',
    headers: {'Content-Type': 'application/x-www-form-urlencoded'},
    body: 'old=' + encodeURIComponent(oldP) + '&new=' + encodeURIComponent(newP)
  })
  .then(r => r.text())
  .then(t => {
    if (t === 'OK') {
      setPassMsg('Jelszo sikeresen megvaltoztatva!', 'ok');
      setTimeout(closePassModal, 1500);
    } else if (t === 'WRONG') {
      setPassMsg('A jelenlegi jelszo nem helyes!', 'err');
      document.getElementById('passOld').focus();
    } else {
      setPassMsg('Hiba tortent, probald ujra!', 'err');
    }
  })
  .catch(() => setPassMsg('Kapcsolati hiba!', 'err'));
}

function openSensorModal(uuid) {
  document.getElementById('sensorUuid').value = uuid;
  document.getElementById('sensorModal').classList.add('open');
}

function closeSensorModal() {
  document.getElementById('sensorModal').classList.remove('open');
}

function saveSensor() {
  const uuid = document.getElementById('sensorUuid').value;
  const type = document.getElementById('sensorType').value;
  const pin  = parseInt(document.getElementById('sensorPin').value);

  if (isNaN(pin)) { alert('Érvénytelen Pin!'); return; }

  // Küldés az ESP32 felé a már megírt WebSocket handlernek
  ws.send(JSON.stringify({
    type: 'add_sensor',
    uuid: uuid,
    sensorType: type,
    pin: pin
  }));
  
  closeSensorModal();
}

window.onload = initWebSocket;
</script>
</body></html>
)rawliteral";

public:
    WebManager(WifiManager& wm) : wifiManager(wm) {}

    void setAuth(AuthManager& am) { auth = &am; }
    void begin();
    void handle() { server.handleClient(); }
};

inline void WebManager::begin() {
    const char* headers[] = {"Cookie"};
    server.collectHeaders(headers, 1);
    server.enableCORS(false);
    server.enableDelay(false);

    server.on("/", HTTP_GET, [this]() {
        if (!isAuthenticated()) {
            server.sendHeader("Location", "/login");
            server.sendHeader("Connection", "close");
            server.send(302, "text/plain", "");
            return;
        }
        server.sendHeader("Connection", "close");
        server.sendHeader("Cache-Control", "no-store");
        server.send(200, "text/html; charset=UTF-8", INDEX_HTML);
    });

    server.on("/login", HTTP_GET, [this]() {
        if (isAuthenticated()) {
            server.sendHeader("Location", "/");
            server.sendHeader("Connection", "close");
            server.send(302, "text/plain", "");
            return;
        }
        server.sendHeader("Connection", "close");
        server.send(200, "text/html; charset=UTF-8", LOGIN_HTML);
    });

    server.on("/login", HTTP_POST, [this]() {
        if (auth == nullptr) { server.send(500, "text/plain", "ERR"); return; }
        if (auth->login(server.arg("password"))) {
            server.sendHeader("Set-Cookie", "token=" + auth->getToken() + "; Max-Age=2592000; Path=/; HttpOnly");
            server.sendHeader("Location", "/");
            server.sendHeader("Connection", "close");
            server.send(302, "text/plain", "");
        } else {
            server.sendHeader("Connection", "close");
            server.send(401, "text/plain", "Hibas jelszo");
        }
    });

    server.on("/logout", HTTP_POST, [this]() {
        if (auth) auth->logout();
        server.sendHeader("Set-Cookie", "token=; Max-Age=0; Path=/");
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", "OK");
    });

    server.on("/change_password", HTTP_POST, [this]() {
        server.sendHeader("Connection", "close");
        if (!isAuthenticated()) {
            server.send(403, "text/plain", "DENIED");
            return;
        }
        if (auth == nullptr) { server.send(500, "text/plain", "ERR"); return; }
        auto result = auth->changePassword(server.arg("old"), server.arg("new"));
        switch (result) {
            case AuthManager::ChangeResult::OK:        server.send(200, "text/plain", "OK");    break;
            case AuthManager::ChangeResult::WRONG_OLD: server.send(200, "text/plain", "WRONG"); break;
            case AuthManager::ChangeResult::TOO_SHORT: server.send(200, "text/plain", "SHORT"); break;
        }
    });

    server.onNotFound([this]() {
        server.sendHeader("Connection", "close");
        server.send(404, "text/plain", "404");
    });

    server.begin();
    Serial.println("[Web] HTTP szerver elindult (8080)");
}

#endif // WEB_MANAGER_H
