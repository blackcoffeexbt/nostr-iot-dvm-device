#pragma once

#include <Arduino.h>
#include <WebSocketsClient.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include <vector>
#include <functional>
#include "config.h"

namespace PaymentProvider {
    
    // Payment request structure
    struct PendingPaymentRequest {
        String payment_hash;         // Key for matching payments
        String original_event_str;   // Complete original event for response construction
        String method;               // Method to execute
        String value;                // Method value parameter
        unsigned long created_at;    // Request timestamp
        unsigned long expires_at;    // Payment timeout (15 mins)
    };

    // Payment confirmation callback type
    typedef std::function<void(String payment_hash, String original_event_str, String method, String value)> payment_callback_t;

    // Core payment provider functions
    void init();
    void cleanup();
    void processLoop();

    // Payment request creation
    String createPaymentRequest(int amount_sats, const String& memo);
    String extractPaymentHashFromResponse(const String& invoice_response);
    String extractBolt11FromResponse(const String& response);

    // Payment queue management
    void addToPaymentQueue(const String& payment_hash, const String& original_event_str, const String& method, const String& value);
    void cleanupExpiredPayments();
    
    // Payment monitoring
    void initPaymentMonitoring();
    void paymentWebsocketEvent(WStype_t type, uint8_t* payload, size_t length);
    void handlePaymentNotification(uint8_t* payload, size_t length);
    void processConfirmedPayment(const String& payment_hash);

    // Callback management
    void setPaymentCallback(payment_callback_t callback);

    // HTTP utilities
    String httpPost(const String& url, const String& postData);

    // Configuration
    extern const int MAX_QUEUE_SIZE;
    extern const unsigned long PAYMENT_TIMEOUT;
}