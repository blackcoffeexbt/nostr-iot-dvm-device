/**
 * @file main.cpp
 * @brief Main application entry point Nostriot device
 *
 * @author BlackCoffee bc@lnbits.com
 * @version 1.0.0
 * @date 09-2025
 */

#include <Arduino.h>
#include <WiFi.h>
#include <Preferences.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "app.h"

// Import Nostr library for memory initialization
#include "../lib/nostr/nostr.h"

static const String SOFTWARE_VERSION = "v0.0.1";

// Memory space definitions for Nostr operations to prevent heap fragmentation
// #define EVENT_NOTE_SIZE 2000000
// #define ENCRYPTED_MESSAGE_BIN_SIZE 100000
#define EVENT_NOTE_SIZE 2048 // declare very small to allow use on devices without PSRAM
#define ENCRYPTED_MESSAGE_BIN_SIZE 2048 // declare very small to allow use on devices without PSRAM

// Remaining global variables that main.cpp still needs
static unsigned long wifi_connect_start_time = 0;
static const unsigned long WIFI_CONNECT_TIMEOUT = 10000; // 10 seconds
static bool wifi_connection_attempted = false;

// Legacy AP mode variables removed - now handled by WiFiManager module

// Queue definitions for task communication
typedef enum
{
    WIFI_SCAN,
    WIFI_CONNECT,
    WIFI_DISCONNECT,
    WIFI_STOP_SCAN
} wifi_command_type_t;

typedef struct
{
    wifi_command_type_t type;
    char ssid[33];
    char password[65];
} wifi_command_t;

// Task management
static TaskHandle_t wifi_task_handle = NULL;
static TaskHandle_t invoice_task_handle = NULL;
static QueueHandle_t wifi_command_queue = NULL;
static QueueHandle_t invoice_command_queue = NULL;
static QueueHandle_t invoice_status_queue = NULL;

// Global UI state variables (to be migrated to UI module eventually)
static char current_ssid[33];
static char current_password[65];
static int invoice_counter = 0;

// Global state for preferences
static Preferences preferences;

void setup(void)
{
    Serial.begin(115200);
    Serial.println("=== Remote Nostr Signer Starting ===");
    Serial.println("Software Version: " + SOFTWARE_VERSION);

    // Initialize PSRAM memory space for Nostr operations to prevent heap fragmentation
    Serial.println("Initializing Nostr memory space...");
    nostr::initMemorySpace(EVENT_NOTE_SIZE, ENCRYPTED_MESSAGE_BIN_SIZE);
    Serial.println("Nostr memory space initialized");

    // Initialize all application modules through the App coordinator
    App::init();

    // Create queues for task communication (this will eventually move to respective modules)
    wifi_command_queue = xQueueCreate(10, sizeof(wifi_command_t));

    Serial.println("=== Setup Complete ===");
}

void loop(void)
{
    App::run();

    delay(5);
}
