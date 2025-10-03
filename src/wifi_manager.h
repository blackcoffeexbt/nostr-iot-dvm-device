#pragma once

#include <Arduino.h>
#include <WiFiManager.h>
#include <WebServer.h>
#include <DNSServer.h>
#include <HTTPClient.h>
#include <Preferences.h>
#include <NTPClient.h>
#include <vector>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

namespace NiotWiFiManager
{
    // WiFi command types
    typedef enum
    {
        WIFI_SCAN,
        WIFI_CONNECT,
        WIFI_DISCONNECT,
        WIFI_STOP_SCAN
    } wifi_command_type_t;

    // WiFi command structure
    typedef struct
    {
        wifi_command_type_t type;
        char ssid[33];
        char password[65];
    } wifi_command_t;

    // Initialization
    void init();
    void cleanup();
    void processLoop();

    // Connection management
    void startConnection(const char *ssid, const char *password);
    void disconnect();
    bool isConnected();
    String getSSID();
    String getLocalIP();
    wl_status_t getStatus();

    // Task management
    void createTask();
    void deleteTask();

    // Status callbacks for integration
    typedef void (*wifi_status_callback_t)(bool connected, const char *status);
    void setStatusCallback(wifi_status_callback_t callback);

    // Timer management
    void createStatusTimer();
    void deleteStatusTimer();

}