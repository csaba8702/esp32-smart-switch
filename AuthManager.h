#ifndef AUTH_MANAGER_H
#define AUTH_MANAGER_H

#include <Arduino.h>
#include "EepromManager.h"

// ----------------------------------------------------------------
// AuthManager – autentikációs logika
//
// Felelősségei:
//   - Jelszó ellenőrzés
//   - Session token generálás, tárolás, ellenőrzés
//   - Jelszócsere
//   - Kijelentkezés
//
// Nem felelős:
//   - HTTP kezelés (WebManager feladata)
//   - EEPROM olvasás/írás implementáció (EepromManager feladata)
// ----------------------------------------------------------------

class AuthManager {
private:
    EepromManager& eeprom;
    String sessionToken = "";

    // Token generálás: 32 hex karakter, hardware RNG alapján
    String generateToken() {
        String t = "";
        const char chars[] = "0123456789abcdef";
        for (int i = 0; i < 32; i++) t += chars[esp_random() % 16];
        return t;
    }

public:
    AuthManager(EepromManager& em) : eeprom(em) {}

    // Inicializálás:
    //  - Alapértelmezett jelszó beállítása ha az EEPROM még üres
    //  - Token betöltése hogy újraindítás után is bejelentkezve maradjon
    void begin() {
        if (eeprom.loadWebPassword().isEmpty()) {
            eeprom.saveWebPassword("admin");
            Serial.println("[Auth] Alapertelmezett jelszo beallitva: admin");
        }
        sessionToken = eeprom.loadToken();
        Serial.println("[Auth] AuthManager inicializalva.");
    }

    // Jelszó ellenőrzés + session token generálás
    // Visszatér: true ha sikeres, false ha rossz jelszó
    bool login(const String& password) {
        if (password != eeprom.loadWebPassword()) {
            Serial.println("[Auth] Sikertelen bejelentkezes – hibas jelszo.");
            return false;
        }
        sessionToken = generateToken();
        eeprom.saveToken(sessionToken);
        Serial.println("[Auth] Sikeres bejelentkezes.");
        return true;
    }

    // Session token törlése – kijelentkezés
    void logout() {
        sessionToken = "";
        eeprom.saveToken("");
        Serial.println("[Auth] Kijelentkezve.");
    }

    // Token alapú hitelesítés ellenőrzése
    bool isAuthenticated(const String& token) const {
        if (sessionToken.isEmpty()) return false;
        return token == sessionToken;
    }

    // Az aktuális session token lekérése (cookie-ba kerül)
    const String& getToken() const {
        return sessionToken;
    }

    // Jelszócsere eredmény típusa
    enum class ChangeResult {
        OK,           // Sikeres
        WRONG_OLD,    // Rossz régi jelszó
        TOO_SHORT     // Új jelszó túl rövid (min. 4 karakter)
    };

    // Jelszó megváltoztatása
    ChangeResult changePassword(const String& oldPass, const String& newPass) {
        if (oldPass != eeprom.loadWebPassword()) {
            Serial.println("[Auth] Sikertelen jelszovaltas – hibas regi jelszo.");
            return ChangeResult::WRONG_OLD;
        }
        if (newPass.length() < 4) {
            return ChangeResult::TOO_SHORT;
        }
        eeprom.saveWebPassword(newPass);
        Serial.println("[Auth] Jelszo sikeresen megvaltoztatva.");
        return ChangeResult::OK;
    }
};

#endif // AUTH_MANAGER_H
