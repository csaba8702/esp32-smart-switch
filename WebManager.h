// WebManager.h
#ifndef WEB_MANAGER_H
#define WEB_MANAGER_H

#include <WebServer.h>
#include "WifiManager.h"

class WebManager {
private:
    WebServer server{8080};
    WifiManager& wifiManager;
    
    // Az index.html tartalma konstansként tárolva
    const char* INDEX_HTML = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <title>Kapuvezérlő</title>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <style>
        body { 
            font-family: Arial, sans-serif; 
            margin: 20px;
            background-color: #f0f0f0;
        }
        .container {
            max-width: 600px;
            margin: 0 auto;
            background: white;
            padding: 20px;
            border-radius: 10px;
            box-shadow: 0 2px 4px rgba(0,0,0,0.1);
        }
        .status {
            display: grid;
            grid-template-columns: repeat(2, 1fr);
            gap: 10px;
            margin-bottom: 20px;
            padding: 15px;
            background: #f8f9fa;
            border-radius: 5px;
        }
        .status div {
            text-align: center;
        }
        .controls {
            display: grid;
            grid-template-columns: 1fr; /* Csak egy oszlop */
            gap: 10px;
        }
        button {
            padding: 15px;
            border: none;
            border-radius: 5px;
            background: #4CAF50;
            color: white;
            cursor: pointer;
            font-size: 16px;
        }
        button:disabled {
            background: #cccccc;
            cursor: not-allowed;
        }
        button.pause {
            background: #ff9800;
        }
        button.close {
            background: #f44336;
        }
        .gate-progress {
            width: 100%;
            background-color: #f3f3f3;
            border-radius: 5px;
            overflow: hidden;
            margin-top: 5px;
        }
        .progress-bar {
            width: 0%;
            height: 20px;
            background-color: #4CAF50;
            transition: width 0.3s ease;
            position: relative;
        }
        .progress-text {
            position: absolute;
            width: 100%;
            text-align: center;
            color: white;
            text-shadow: 1px 1px 1px rgba(0,0,0,0.5);
        }
    </style>
</head>
<body>
    <div class="container">
        <div class="status">
            <div>
                <h3>Bal Kapu</h3>
                <div class="gate-progress">
                    <div class="progress-bar" id="leftGateProgress">
                        <span class="progress-text">0%</span>
                    </div>
                </div>
            </div>
            <div>
                <h3>Jobb Kapu</h3>
                <div class="gate-progress">
                    <div class="progress-bar" id="rightGateProgress">
                        <span class="progress-text">0%</span>
                    </div>
                </div>
            </div>
            <div id="wifiStatus">WiFi: Csatlakozva</div>
            <div id="signalStrength">Jelerősség: --- dBm</div>
        </div>
        
        <div class="controls">
            <button id="openGatesBtn" onclick="toggleGates()" class="btn btn-primary">Kapuk Nyitasa</button>
            <div style="display: grid; grid-template-columns: 1fr 1fr; gap: 10px; margin: 10px 0;">
                <button id="gate1Btn" onclick="toggleGate1()" class="btn btn-primary">Kapu 1 Nyitasa</button>
                <button id="gate2Btn" onclick="toggleGate2()" class="btn btn-primary">Kapu 2 Nyitasa</button>
            </div>
        </div>
    </div>

    <script>
        let wsUrl = `ws://${window.location.hostname}:81`;
        const socket = new WebSocket(wsUrl);
        
        // Gombok állapotának kezelése
        function updateButtonStates(gateData) {
            console.log("Received gate data:", gateData);

            const button = document.getElementById(`gate${gateData.gate}Btn`);
            const toggleBtn = document.getElementById('openGatesBtn');
            
            if (!button) {
                console.error(`Button not found for gate ${gateData.gate}`);
                return;
            }

            // Egyedi kapu gomb frissítése - csak akkor tiltjuk le, ha ténylegesen mozog
            if (gateData.isMoving) {
                button.disabled = true;
            } else {
                button.disabled = false;  // Ha nem mozog, mindig engedélyezzük
            }

            // Gomb szövegének frissítése
            if (gateData.state === 2) { // OPENED állapot
                button.textContent = `Kapu ${gateData.gate} Zárása`;
            } else if (gateData.state === 0) { // CLOSED állapot
                button.textContent = `Kapu ${gateData.gate} Nyitasa`;
            }

            // Közös gomb frissítése
            const gate1Btn = document.getElementById('gate1Btn');
            const gate2Btn = document.getElementById('gate2Btn');
            
            const bothOpen = gate1Btn.textContent.includes('Zárása') && gate2Btn.textContent.includes('Zárása');
            const bothClosed = !gate1Btn.textContent.includes('Zárása') && !gate2Btn.textContent.includes('Zárása');
            const anyMoving = gateData.isMoving || 
                             (gateData.gate === 1 ? gate2Btn.disabled : gate1Btn.disabled);
            
            if (bothOpen) {
                toggleBtn.textContent = 'Kapuk Zárása';
                toggleBtn.disabled = anyMoving;
            } else if (bothClosed) {
                toggleBtn.textContent = 'Kapuk Nyitása';
                toggleBtn.disabled = anyMoving;
            } else {
                toggleBtn.disabled = true;
            }

            console.log('Button states:', {
                gate: gateData.gate,
                buttonText: button.textContent,
                buttonDisabled: button.disabled,
                toggleText: toggleBtn.textContent,
                toggleDisabled: toggleBtn.disabled,
                bothOpen,
                bothClosed,
                anyMoving
            });
        }

        function toggleGate1() {
            const btn = document.getElementById('gate1Btn');
            const command = btn.textContent.includes('Zárása') ? 'closegate1' : 'opengate1';
            socket.send(command);
        }

        function toggleGate2() {
            const btn = document.getElementById('gate2Btn');
            const command = btn.textContent.includes('Zárása') ? 'closegate2' : 'opengate2';
            socket.send(command);
        }

        function toggleGates() {
            const btn = document.getElementById('openGatesBtn');
            const command = btn.textContent.includes('Zárása') ? 'closegates' : 'opengates';
            socket.send(command);
            
            // Letiltjuk a gombokat amíg a szerver nem válaszol
            document.getElementById('gate1Btn').disabled = true;
            document.getElementById('gate2Btn').disabled = true;
            btn.disabled = true;
        }
        
        socket.onopen = function() {
            console.log("WebSocket kapcsolat létrejött");
            document.getElementById('wifiStatus').textContent = "WiFi: Csatlakozva a szerverhez";
        };
        
        socket.onmessage = function(event) {
            console.log("Fogadott üzenet:", event.data);
            try {
                const data = JSON.parse(event.data);
                if (data.type === 'wifi') {
                    document.getElementById('wifiStatus').textContent = 
                        `WiFi: ${data.connected ? 'Csatlakozva' : 'Nem csatlakozva'}`;
                    document.getElementById('signalStrength').textContent = 
                        `Jelerősség: ${data.rssi} dBm`;
                }
                else if (data.type === 'gate') {
                    updateButtonStates(data);
                }
            } catch (e) {
                console.error("Hibás JSON üzenet:", e);
            }
        };
        
        socket.onerror = function(error) {
            console.error("WebSocket hiba:", error);
            document.getElementById('wifiStatus').textContent = "WiFi: Kapcsolódási hiba";
        };
        
        socket.onclose = function() {
            console.log("WebSocket kapcsolat lezárult");
            document.getElementById('wifiStatus').textContent = "WiFi: Kapcsolat megszakadt";
        };
    </script>
</body>
</html>
)rawliteral";
    
    void setupRoutes() {
        // Alap útvonal kezelése
        server.on("/", HTTP_GET, [this]() {
            server.send(200, "text/html", INDEX_HTML);
        });
        
        // 404-es hiba kezelése
        server.onNotFound([this]() {
            Serial.printf("[WebManager] 404: Nem található: %s\n", server.uri().c_str());
            server.send(404, "text/plain", "404: Oldal nem található");
        });
    }

public:
    WebManager(WifiManager& wm) : wifiManager(wm) {}
    
    void begin() {
        // Útvonalak beállítása
        setupRoutes();
        
        // Szerver indítása
        server.begin();
        Serial.println("[WebManager] HTTP szerver elindult a 8080-as porton");
    }
    
    void handle() {
        server.handleClient();
    }
};

#endif