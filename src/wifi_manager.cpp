#include "wifi_manager.h"
#include <WiFi.h>
#include "settings.h"
#include "app.h"
#include "config.h"

#include "nostr_manager.h"

// Import Nostr library components for key derivation
#include "../lib/nostr/nostr.h"

namespace NiotWiFiManager
{
    // Global WiFi state
    static WiFiUDP ntpUDP;
    static NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);

    // Connection management
    static unsigned long wifi_connect_start_time = 0;
    static const unsigned long WIFI_CONNECT_TIMEOUT = 10000; // 10 seconds
    static bool wifi_connection_attempted = false;
    static char current_ssid[33];
    static char current_password[65];

    // Task and queue management
    static TaskHandle_t wifi_task_handle = NULL;
    static QueueHandle_t wifi_command_queue = NULL;

    // Status callback
    static wifi_status_callback_t status_callback = nullptr;

    // Preferences instance
    static Preferences preferences;

    // WiFi task function - runs on Core 0
    static void wifiTask(void *parameter)
    {
        Serial.println("WiFi task started");
        while (true)
        {
            wifi_command_t command;
            if (xQueueReceive(wifi_command_queue, &command, portMAX_DELAY))
            {
                Serial.print("WiFi task received command: ");
                Serial.println(command.type);

                switch (command.type)
                {
                case WIFI_CONNECT:
                {
                    Serial.println("Connecting to WiFi...");
                    Serial.print("SSID: ");
                    Serial.println(command.ssid);

                    WiFi.setAutoReconnect(true);
                    WiFi.persistent(true);
                    // WiFi.begin(command.ssid, command.password);
                    WiFi.setSleep(false);

                    WiFiManager wm;

                    wm.setConfigPortalTimeout(180); // 3 minutes timeout
                    wm.setConnectRetries(3);

                    char ssid[32];
                    uint64_t chipid = ESP.getEfuseMac();
                    snprintf(ssid, sizeof(ssid), "Nostr IoT Device %08X v%d", (uint32_t)(chipid), NOSTRIOT_DEVICE_VERSION);

                    bool connected = wm.autoConnect(ssid);

                    // Monitor connection status with timeout
                    unsigned long start_time = millis();
                    wl_status_t status = WiFi.status();

                    Serial.println("Monitoring connection status...");
                    while (status != WL_CONNECTED && status != WL_CONNECT_FAILED &&
                           status != WL_NO_SSID_AVAIL && status != WL_CONNECTION_LOST &&
                           (millis() - start_time) < WIFI_CONNECT_TIMEOUT)
                    {
                        vTaskDelay(pdMS_TO_TICKS(500));
                        status = WiFi.status();
                        Serial.print("WiFi status: ");
                        Serial.println(status);
                    }
                    connected = (status == WL_CONNECTED);
                    if (connected)
                    {
                        Serial.println("WiFi connected successfully!");
                        Serial.print("IP Address: ");
                        Serial.println(WiFi.localIP());
                        Serial.print("RSSI: ");
                        Serial.println(WiFi.RSSI());
                    }
                    else
                    {
                        Serial.print("WiFi connection failed. Status: ");
                        Serial.println(status);
                        const char *error_msg = "Connection failed";
                        switch (status)
                        {
                        case WL_NO_SSID_AVAIL:
                            error_msg = "SSID not found";
                            break;
                        case WL_CONNECT_FAILED:
                            error_msg = "Wrong password";
                            break;
                        case WL_CONNECTION_LOST:
                            error_msg = "Connection lost";
                            break;
                        default:
                            if ((millis() - start_time) >= WIFI_CONNECT_TIMEOUT)
                            {
                                error_msg = "Connection timeout";
                            }
                            break;
                        }
                        Serial.println(error_msg);
                    }

                    // Trigger status callback if set
                    if (status_callback)
                    {
                        const char *status_msg = connected ? "Connected" : "Failed";
                        Serial.print("Calling status callback with connected=");
                        Serial.print(connected);
                        Serial.print(", status=");
                        Serial.println(status_msg);
                        status_callback(connected, status_msg);
                    }
                    else
                    {
                        Serial.println("Warning: No status callback set");
                    }
                    break;
                }
                case WIFI_DISCONNECT:
                    Serial.println("Disconnecting from WiFi...");
                    WiFi.disconnect(true);
                    if (status_callback)
                    {
                        status_callback(false, "Disconnected");
                    }
                    break;
                case WIFI_STOP_SCAN:
                    Serial.println("Stopping WiFi scan...");
                    break;
                }
            }

            // Periodically check connection status and report changes
            static bool last_connected_state = false;
            static unsigned long last_status_check = 0;
            unsigned long current_time = millis();

            if (current_time - last_status_check >= 5000)
            { // Check every 5 seconds
                bool currently_connected = (WiFi.status() == WL_CONNECTED);
                if (currently_connected != last_connected_state)
                {
                    Serial.print("WiFi status changed: ");
                    Serial.println(currently_connected ? "Connected" : "Disconnected");
                    if (status_callback)
                    {
                        status_callback(currently_connected, currently_connected ? "Connected" : "Disconnected");
                    }
                    last_connected_state = currently_connected;
                }
                last_status_check = current_time;
            }

            vTaskDelay(pdMS_TO_TICKS(100));
        }
    }

    void init()
    {
        WiFi.mode(WIFI_STA);
        timeClient.begin();
        timeClient.setTimeOffset(0);

        // Create queues
        wifi_command_queue = xQueueCreate(10, sizeof(wifi_command_t));

        // Set up WiFi event handler for immediate status updates
        WiFi.onEvent([](WiFiEvent_t event, WiFiEventInfo_t info)
                     {
            switch (event) {
                case ARDUINO_EVENT_WIFI_STA_CONNECTED:
                    Serial.println("WiFi Event: Connected to AP");
                    break;
                case ARDUINO_EVENT_WIFI_STA_GOT_IP:
                    Serial.print("WiFi Event: Got IP - ");
                    Serial.println(WiFi.localIP());
                    if (status_callback) {
                        status_callback(true, "Connected");
                    }
                    break;
                case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
                    Serial.println("WiFi Event: Disconnected from AP");
                    if (status_callback) {
                        status_callback(false, "Disconnected");
                    }
                    break;
                case ARDUINO_EVENT_WIFI_STA_AUTHMODE_CHANGE:
                    Serial.println("WiFi Event: Auth mode changed");
                    break;
                default:
                    Serial.printf("WiFi Event: %d\n", event);
                    break;
            } });

        createTask();

        // preferences.begin("wifi-creds", true);
        // String saved_ssid = preferences.getString("ssid", "");
        // String saved_pass = preferences.getString("password", "");
        // preferences.end();

        // get wifi from config.h for now
        String saved_ssid = WIFI_SSID;
        String saved_pass = WIFI_PASSWORD;

        Serial.println("WiFiManager initialized");
        Serial.print("Saved SSID: ");
        Serial.println(saved_ssid);
        Serial.print("Saved Password: ");
        Serial.println(saved_pass);

        if (saved_ssid.length() > 0)
        {
            Serial.println("Found saved WiFi credentials.");
            Serial.print("Connecting to ");
            Serial.println(saved_ssid);
            startConnection(saved_ssid.c_str(), saved_pass.c_str());
        }
    }

    void cleanup()
    {
        deleteTask();

        if (wifi_command_queue)
        {
            vQueueDelete(wifi_command_queue);
            wifi_command_queue = NULL;
        }
    }

    void processLoop()
    {
    }

    void startConnection(const char *ssid, const char *password)
    {
        strncpy(current_ssid, ssid, sizeof(current_ssid) - 1);
        current_ssid[sizeof(current_ssid) - 1] = '\0';
        strncpy(current_password, password, sizeof(current_password) - 1);
        current_password[sizeof(current_password) - 1] = '\0';

        if (wifi_command_queue != NULL)
        {
            wifi_command_t command;
            command.type = WIFI_CONNECT;
            strncpy(command.ssid, ssid, sizeof(command.ssid) - 1);
            command.ssid[sizeof(command.ssid) - 1] = '\0';
            strncpy(command.password, password, sizeof(command.password) - 1);
            command.password[sizeof(command.password) - 1] = '\0';
            xQueueSend(wifi_command_queue, &command, 0);
        }

        wifi_connect_start_time = millis();
        wifi_connection_attempted = true;
    }

    void disconnect()
    {
        if (wifi_command_queue != NULL)
        {
            wifi_command_t command;
            command.type = WIFI_DISCONNECT;
            xQueueSend(wifi_command_queue, &command, 0);
        }
    }

    bool isConnected()
    {
        Serial.println("WiFiManager::isConnected() - Checking WiFi connection status");
        Serial.println(WiFi.status() == WL_CONNECTED ? "Connected" : "Not Connected");
        return WiFi.status() == WL_CONNECTED;
    }

    String getSSID()
    {
        return WiFi.SSID();
    }

    String getLocalIP()
    {
        return WiFi.localIP().toString();
    }

    wl_status_t getStatus()
    {
        return WiFi.status();
    }

    void createTask()
    {
        xTaskCreatePinnedToCore(
            wifiTask,
            "WiFiTask",
            4096,
            NULL,
            1,
            &wifi_task_handle,
            0);
    }

    void deleteTask()
    {
        if (wifi_task_handle != NULL)
        {
            vTaskDelete(wifi_task_handle);
            wifi_task_handle = NULL;
        }
    }

    void setStatusCallback(wifi_status_callback_t callback)
    {
        status_callback = callback;
    }

}