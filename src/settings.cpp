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

}