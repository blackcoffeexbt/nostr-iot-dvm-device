#include "settings.h"
#include "app.h"
#include <Arduino.h>

#include "wifi_manager.h"

namespace Settings
{
    // Global preferences instance
    static Preferences preferences;

    void init()
    {
        loadFromPreferences();
    }

    void cleanup()
    {
        // Close any open preferences
        preferences.end();
        Serial.println("Settings module cleaned up");
    }

    // Persistence
    void loadFromPreferences()
    {
        // Load shop settings
        // preferences.begin("config", true);
        // ap_password = preferences.getString("ap_password", "GoodMorning21");
        // preferences.end();
        // preferences.end();
        Serial.println("Loaded preferences");
    }

    void saveToPreferences()
    {
        preferences.begin("config", false);
        // preferences.putString("wifi_password", wifi_password);
        preferences.end();
        Serial.println("Saved settings");
    }

}