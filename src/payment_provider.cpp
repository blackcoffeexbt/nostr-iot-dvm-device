/**
 * @file payment_provider.cpp
 * @brief Lightning payment provider for IoT device services
 * @version 0.1
 * @date 2025-09-30
 * 
 * Handles Lightning invoice generation, payment monitoring via LNbits,
 * payment queue management, and payment confirmation callbacks.
 */

#include "payment_provider.h"

namespace PaymentProvider {
    
    // Configuration constants
    const int MAX_QUEUE_SIZE = 5;
    const unsigned long PAYMENT_TIMEOUT = 5 * 60 * 1000; // 5 minutes

    // Static variables
    static std::vector<PendingPaymentRequest> payment_queue;
    static payment_callback_t payment_callback = nullptr;
    
    // Payment monitoring WebSocket
    static WebSocketsClient payment_ws;
    static bool payment_ws_connected = false;
    
    void init() {
        Serial.println("PaymentProvider::init() - Initializing payment provider");
        
        // Initialize payment monitoring
        initPaymentMonitoring();
        
        Serial.println("PaymentProvider::init() - Payment provider initialized");
    }

    void cleanup() {
        Serial.println("PaymentProvider::cleanup() - Cleaning up payment provider");
        
        payment_ws.disconnect();
        payment_queue.clear();
        payment_callback = nullptr;
        
        Serial.println("PaymentProvider::cleanup() - Payment provider cleaned up");
    }

    void processLoop() {
        // Process payment WebSocket events
        payment_ws.loop();
        
        // Clean up expired payments every 30 seconds
        static unsigned long last_cleanup = 0;
        unsigned long now = millis();
        if (now - last_cleanup > 30000) {
            cleanupExpiredPayments();
            last_cleanup = now;
        }
    }

    bool isPaymentRequired(const String& method) {
        // This will be implemented by checking pricing configuration
        // For now, assume all methods require payment if price > 0
        // This logic should eventually move to a pricing configuration system
        return true; // Placeholder - implement pricing logic
    }

    String createPaymentRequest(int amount_sats, const String& memo) {
        Serial.println("PaymentProvider::createPaymentRequest() - Creating invoice for " + String(amount_sats) + " sats");
        
        String postData = "{\"unit\": \"sat\", \"out\": false, \"amount\": " + String(amount_sats) + ", \"memo\": \"" + memo + "\"}";
        String url = "https://" + String(LNBITS_HOST_URL) + String(LNBITS_PAYMENTS_ENDPOINT) + "?api-key=" + String(LNBITS_INVOICE_KEY);
        String response = httpPost(url, postData);
        
        return response; // Return full LNbits response
    }

    String extractPaymentHashFromResponse(const String& invoice_response) {
        // Parse LNbits response to extract payment_hash
        DynamicJsonDocument doc(4096);
        DeserializationError error = deserializeJson(doc, invoice_response);
        
        if (error) {
            Serial.println("PaymentProvider::extractPaymentHashFromResponse() - JSON parsing failed: " + String(error.c_str()));
            return "";
        }
        
        return doc["payment_hash"];
    }

    String extractBolt11FromResponse(const String& response) {
        int bolt11Index = response.indexOf("\"bolt11\":\"");
        if (bolt11Index == -1) {
            return "";
        }
        int bolt11Start = bolt11Index + 10;
        int bolt11End = response.indexOf("\"", bolt11Start);
        if (bolt11End == -1) {
            return "";
        }
        return response.substring(bolt11Start, bolt11End);
    }

    void addToPaymentQueue(const String& payment_hash, const String& original_event_str, const String& method, const String& value) {
        // Check queue size limit
        if (payment_queue.size() >= MAX_QUEUE_SIZE) {
            Serial.println("PaymentProvider::addToPaymentQueue() - Queue full, removing oldest entry");
            payment_queue.erase(payment_queue.begin());
        }
        
        // Check for duplicate payment_hash
        for (const auto& req : payment_queue) {
            if (req.payment_hash == payment_hash) {
                Serial.println("PaymentProvider::addToPaymentQueue() - Duplicate payment hash, ignoring");
                return;
            }
        }
        
        PendingPaymentRequest request;
        request.payment_hash = payment_hash;
        request.original_event_str = original_event_str;
        request.method = method;
        request.value = value;
        request.created_at = millis();
        request.expires_at = millis() + PAYMENT_TIMEOUT;
        
        payment_queue.push_back(request);
        Serial.println("PaymentProvider::addToPaymentQueue() - Added to queue: " + payment_hash + " for method: " + method);
    }

    void cleanupExpiredPayments() {
        unsigned long now = millis();
        size_t initial_size = payment_queue.size();
        
        payment_queue.erase(
            std::remove_if(payment_queue.begin(), payment_queue.end(),
                [now](const PendingPaymentRequest& req) {
                    return now > req.expires_at;
                }),
            payment_queue.end()
        );
        
        if (payment_queue.size() < initial_size) {
            Serial.println("PaymentProvider::cleanupExpiredPayments() - Removed " + 
                         String(initial_size - payment_queue.size()) + " expired payments");
        }
    }

    void initPaymentMonitoring() {
        String ws_endpoint = "/api/v1/ws/" + String(LNBITS_INVOICE_KEY);
        
        Serial.println("PaymentProvider::initPaymentMonitoring() - Connecting to payment WebSocket");
        payment_ws.beginSSL(LNBITS_HOST_URL, 443, ws_endpoint.c_str());
        payment_ws.onEvent(paymentWebsocketEvent);
        payment_ws.setReconnectInterval(5000);
    }

    void paymentWebsocketEvent(WStype_t type, uint8_t* payload, size_t length) {
        switch (type) {
        case WStype_DISCONNECTED:
            Serial.println("PaymentProvider::paymentWebsocketEvent() - Payment WebSocket Disconnected");
            payment_ws_connected = false;
            break;

        case WStype_CONNECTED:
            Serial.println("PaymentProvider::paymentWebsocketEvent() - Payment WebSocket Connected");
            payment_ws_connected = true;
            break;

        case WStype_TEXT:
            Serial.println("PaymentProvider::paymentWebsocketEvent() - Payment notification received");
            handlePaymentNotification(payload, length);
            break;

        case WStype_ERROR:
            Serial.println("PaymentProvider::paymentWebsocketEvent() - Payment WebSocket Error");
            payment_ws_connected = false;
            break;

        default:
            break;
        }
    }

    void handlePaymentNotification(uint8_t* payload, size_t length) {
        String message = String((char*)payload);
        Serial.println("PaymentProvider::handlePaymentNotification() - Received: " + message);
        
        DynamicJsonDocument doc(2048);
        DeserializationError error = deserializeJson(doc, message);
        
        if (error) {
            Serial.println("PaymentProvider::handlePaymentNotification() - JSON parsing failed: " + String(error.c_str()));
            return;
        }
        
        if (doc["payment"]["status"] == "success") {
            String payment_hash = doc["payment"]["payment_hash"];
            Serial.println("PaymentProvider::handlePaymentNotification() - Payment confirmed: " + payment_hash);
            processConfirmedPayment(payment_hash);
        }
    }

    void processConfirmedPayment(const String& payment_hash) {
        // Find in queue
        for (auto it = payment_queue.begin(); it != payment_queue.end(); ++it) {
            if (it->payment_hash == payment_hash) {
                Serial.println("PaymentProvider::processConfirmedPayment() - Processing payment for method: " + it->method);
                
                // Call the callback if set
                if (payment_callback) {
                    payment_callback(it->payment_hash, it->original_event_str, it->method, it->value);
                }
                
                // Remove from queue
                payment_queue.erase(it);
                Serial.println("PaymentProvider::processConfirmedPayment() - Payment processed and removed from queue");
                return;
            }
        }
        Serial.println("PaymentProvider::processConfirmedPayment() - Payment hash not found in queue: " + payment_hash);
    }

    void setPaymentCallback(payment_callback_t callback) {
        payment_callback = callback;
        Serial.println("PaymentProvider::setPaymentCallback() - Payment callback registered");
    }

    String httpPost(const String& url, const String& postData) {
        HTTPClient http;
        http.begin(url);
        
        int httpResponseCode = http.POST(postData);
        String response = "";
        
        if (httpResponseCode > 0) {
            response = http.getString();
            Serial.println("PaymentProvider::httpPost() - HTTP Response code: " + String(httpResponseCode));
            Serial.println("PaymentProvider::httpPost() - Response: " + response);
        } else {
            Serial.println("PaymentProvider::httpPost() - Error in HTTP request: " + String(httpResponseCode));
        }
        
        http.end();
        return response;
    }
}