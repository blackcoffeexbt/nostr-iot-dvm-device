#pragma once

/**
 * App.h - Main Application Coordinator
 * 
 * This header provides a unified interface to all application modules
 * and coordinates initialization, cleanup, and inter-module communication
 */

#include <Arduino.h>
#include "driver/gpio.h"
#include "settings.h"
#include "display_manager.h"
#include "wifi_manager.h"
#include "nostr_manager.h"

namespace App {
    /**
     * Application State Management
     */
    typedef enum {
        APP_STATE_INITIALIZING,
        APP_STATE_READY,
        APP_STATE_ERROR,
        APP_STATE_UPDATING
    } app_state_t;
    
    /**
     * Core Application Functions
     */
    void init();                    // Initialize all modules
    void cleanup();                 // Cleanup all modules
    void run();                     // Main application loop
    
    /**
     * State Management
     */
    void setState(app_state_t state);
    String getStateString();
    
    /**
     * Inter-Module Communication
     */
    void notifyWiFiStatusChanged(bool connected);
    void notifyDeviceStatusChanged(bool connected);
    
    /**
     * Module Health Monitoring
     */
    bool checkModuleHealth();
    void reportModuleStatus();
    
    /**
     * Event System
     */
    typedef void (*app_event_callback_t)(const String& event, const String& data);
    void fireEvent(const String& event, const String& data);
    
    /**
     * Constants
     */
    namespace Config {
        const String VERSION = "v0.0.1";
        const String BUILD_DATE = __DATE__ " " __TIME__;
        const unsigned long HEALTH_CHECK_INTERVAL = 30000; // 30 seconds
        const unsigned long STATUS_REPORT_INTERVAL = 300000; // 5 minutes
        
        // Touch handling configuration
        const unsigned long TOUCH_DEBOUNCE_TIME = 50; // 50ms debounce
    }
}