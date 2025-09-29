#pragma once

#include <Arduino.h>
#include <Preferences.h>

namespace Settings {
    // Initialization and cleanup
    void init();
    void cleanup();
        
    // Persistence
    void loadFromPreferences();
    void saveToPreferences();
    

}