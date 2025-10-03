#include "app.h"
#include "esp_sleep.h"
#include "driver/gpio.h"
#include "driver/rtc_io.h"

namespace App
{
    using namespace Display;

    // Application state
    static app_state_t current_state = APP_STATE_INITIALIZING;
    static String last_error = "";
    static app_event_callback_t event_callback = nullptr;
    static unsigned long last_health_check = 0;
    static unsigned long last_status_report = 0;

    // Activity tracking (for diagnostics only)
    static unsigned long last_activity_time = 0;

    void init()
    {
        Serial.println("=== Nostriot Device Initializing ===");
        Serial.println("Version: " + Config::VERSION);
        Serial.println("Build Date: " + Config::BUILD_DATE);

        setState(APP_STATE_INITIALIZING);

        try
        {
            // Initialize display
            if (displayManager.initialize())
            {
                Serial.println("[MAIN] Display initialized successfully");
            }
            else
            {
                Serial.println("[MAIN] Display initialization failed");
            }
            displayManager.showUptime();

            // Initialize modules in dependency order
            Serial.println("Initializing Display module...");

            Serial.println("Initializing Settings module...");
            Settings::init();

            Serial.println("Initializing WiFi Manager module...");
            NiotWiFiManager::init();

            // Set up WiFi status callback
            NiotWiFiManager::setStatusCallback([](bool connected, const char *status)
                                               { App::notifyWiFiStatusChanged(connected); });

            Serial.println("Initializing Nostr Manager module...");
            NostrManager::init();

            // Set up Nostr Manager status callback
            NostrManager::setStatusCallback([](bool connected, const String &status)
                                            { App::notifyDeviceStatusChanged(connected); });

            // Check if WiFi is already connected and trigger signer connection if needed
            if (NiotWiFiManager::isConnected())
            {
                Serial.println("WiFi already connected during initialization - connecting to relay");
                notifyWiFiStatusChanged(true);
            }

            setState(APP_STATE_READY);
            Serial.println("=== Application initialized successfully ===");

            fireEvent("app_initialized", "success");
        }
        catch (const std::exception &e)
        {
            setState(APP_STATE_ERROR);
        }
    }

    void cleanup()
    {
        Serial.println("=== Application cleanup starting ===");

        // Cleanup modules in reverse dependency order
        NostrManager::cleanup();
        NiotWiFiManager::cleanup();
        Settings::cleanup();

        setState(APP_STATE_INITIALIZING);
        Serial.println("=== Application cleanup completed ===");

        fireEvent("app_cleanup", "completed");
    }

    void run()
    {
        static bool first_run = true;
        if (first_run)
        {
            Serial.println("=== App::run() started ===");
            first_run = false;
        }

        displayManager.update();

        unsigned long current_time = millis();

        // Process WiFi AP mode events (with error handling)
        try
        {
            NiotWiFiManager::processLoop();
        }
        catch (...)
        {
            Serial.println("ERROR: NiotWiFiManager::processLoop() threw exception");
        }

        // Process WebSocket events (with error handling)
        try
        {
            NostrManager::processLoop();
        }
        catch (...)
        {
            Serial.println("ERROR: NostrManager::processLoop() threw exception");
        }

        // Periodic health checks
        if (current_time - last_health_check >= Config::HEALTH_CHECK_INTERVAL)
        {
            checkModuleHealth();
            last_health_check = current_time;
        }

        // Periodic status reports
        if (current_time - last_status_report >= Config::STATUS_REPORT_INTERVAL)
        {
            reportModuleStatus();
            last_status_report = current_time;
        }

        delay(1);
    }

    void setState(app_state_t state)
    {
        if (current_state != state)
        {
            current_state = state;
            Serial.println("App state changed to: " + getStateString());
            fireEvent("state_changed", getStateString());
        }
    }

    String getStateString()
    {
        switch (current_state)
        {
        case APP_STATE_INITIALIZING:
            return "Initializing";
        case APP_STATE_READY:
            return "Ready";
        case APP_STATE_ERROR:
            return "Error";
        case APP_STATE_UPDATING:
            return "Updating";
        default:
            return "Unknown";
        }
    }

    void notifyWiFiStatusChanged(bool connected)
    {
        static bool last_wifi_status = false;

        // Prevent duplicate notifications
        if (last_wifi_status == connected)
        {
            return;
        }
        last_wifi_status = connected;

        Serial.println("WiFi status changed: " + String(connected ? "Connected" : "Disconnected"));

        if (connected)
        {
            // WiFi connected
            if (!NostrManager::isConnected())
            {
                Serial.println("WiFi connected, attempting relay connection...");
                NostrManager::connectToRelay();
            }
        }
        else
        {
            // WiFi disconnected - notify Nostr Manager module
            Serial.println("WiFi disconnected, disconnecting from relay...");
            NostrManager::disconnect();
        }

        fireEvent("wifi_status", connected ? "connected" : "disconnected");
    }

    void notifyDeviceStatusChanged(bool connected)
    {
        static bool last_device_status = false;

        // Prevent duplicate notifications
        if (last_device_status == connected)
        {
            return;
        }
        last_device_status = connected;

        Serial.println("Device status changed: " + String(connected ? "Connected" : "Disconnected"));
        fireEvent("device_status", connected ? "connected" : "disconnected");
    }

    bool checkModuleHealth()
    {
        bool health_ok = true;

        // Check WiFi module health
        if (!NiotWiFiManager::isConnected() && NiotWiFiManager::getStatus() != WL_IDLE_STATUS)
        {
            Serial.println("WiFi module health check failed");
            health_ok = false;
        }

        // Check Nostr Manager module health
        if (NostrManager::isInitialized() && !NostrManager::isConnected())
        {
            Serial.println("Nostr Manager module health check warning - not connected");
            // This is a warning, not a failure
        }

        return health_ok;
    }

    void reportModuleStatus()
    {
        Serial.println("=== Module Status Report ===");
        Serial.println("WiFi: " + String(NiotWiFiManager::isConnected() ? "Connected" : "Disconnected"));
        if (NiotWiFiManager::isConnected())
        {
            Serial.println("  SSID: " + NiotWiFiManager::getSSID());
            Serial.println("  IP: " + NiotWiFiManager::getLocalIP());
        }
        Serial.println("Nostr Manager: " + String(NostrManager::isConnected() ? "Connected" : "Disconnected"));
        if (NostrManager::isConnected())
        {
            Serial.println("  Relay: " + NostrManager::getRelayUrl());
        }
        Serial.println("Free Heap: " + String(ESP.getFreeHeap()));
        Serial.println("============================");
    }

    void fireEvent(const String &event, const String &data)
    {
        if (event_callback)
        {
            event_callback(event, data);
        }
    }
}
