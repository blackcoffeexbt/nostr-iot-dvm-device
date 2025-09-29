#include "nostr_manager.h"
#include "settings.h"
#include "app.h"
#include "display.h"
#include <Preferences.h>
#include "config.h"

// Import Nostr library components from lib/ folder
#include "../lib/nostr/nostr.h"
#include "../lib/nostr/nip44/nip44.h"
#include "../lib/nostr/nip19.h"

namespace NostrManager
{
    // NIP-46 Method constants
    namespace Methods
    {
        const char *CONNECT = "connect";
        const char *SIGN_EVENT = "sign_event";
        const char *PING = "ping";
        const char *GET_PUBLIC_KEY = "get_public_key";
        const char *NIP04_ENCRYPT = "nip04_encrypt";
        const char *NIP04_DECRYPT = "nip04_decrypt";
        const char *NIP44_ENCRYPT = "nip44_encrypt";
        const char *NIP44_DECRYPT = "nip44_decrypt";
    }

    // WebSocket client
    static WebSocketsClient webSocket;
    static unsigned long last_loop_time = 0;

    // Status callback
    static signer_status_callback_t status_callback = nullptr;

    // Configuration
    static String relayUrl = "";
    static String privateKeyHex = "";
    static String publicKeyHex = "";
    static String secretKey = "";
    static String authorizedClients = "";

    // Connection state
    static bool signer_initialized = false;
    static bool connection_in_progress = false;
    static unsigned long last_connection_attempt = 0;
    static unsigned long last_ws_ping = 0;
    static unsigned long last_ws_message_received = 0;
    static int reconnection_attempts = 0;
    static unsigned long last_reconnect_attempt = 0;
    static bool manual_reconnect_needed = false;

    // WebSocket fragment management
    static bool ws_fragment_in_progress = false;
    static String ws_fragmented_message = "";
    static size_t ws_fragment_total_size = 0;
    static size_t ws_fragment_received_size = 0;
    static unsigned long ws_fragment_start_time = 0;

    // NTP time synchronization
    static WiFiUDP ntpUDP;
    static NTPClient timeClient(ntpUDP, "pool.ntp.org", 0, 60000);
    static unsigned long unixTimestamp = 0;

    // Memory allocation for JSON documents
    static const size_t JSON_DOC_SIZE = 100000;
    static DynamicJsonDocument eventDoc(0);
    static DynamicJsonDocument eventParamsDoc(0);

    void updateStatus(bool connected, const char *status)
    {
        Serial.print("NostrManager Status - ");
        Serial.println(status);
        if (status_callback)
        {
            status_callback(connected, status);
        }
    }

    void init()
    {
        Serial.println("NostrManager::init() - Initializing Remote Signer module");

        // Initialize memory for JSON documents
        eventDoc = DynamicJsonDocument(JSON_DOC_SIZE);
        eventParamsDoc = DynamicJsonDocument(JSON_DOC_SIZE);

        // Load configuration
        loadConfigFromPreferences();

        // Initialize time client
        timeClient.begin();

        signer_initialized = true;
        Serial.println("NostrManager::init() - Remote Signer module initialized");
    }

    void cleanup()
    {
        Serial.println("NostrManager::cleanup() - Cleaning up Remote Signer module");

        disconnect();
        signer_initialized = false;

        Serial.println("NostrManager::cleanup() - Remote Signer module cleaned up");
    }

    void loadConfigFromPreferences()
    {
        Preferences prefs;
        prefs.begin("signer", true); // Read-only

        // relayUrl = prefs.getString("relay_url", "wss://relay.nostriot.com");
        // load from config.h for now
        relayUrl = NOSTR_RELAY_URI;
        privateKeyHex = NOSTR_PRIVATE_KEY;
        // derive public key from private key
        if (privateKeyHex.length() == 64)
        {
            try
            {
                int byteSize = 32;
                byte privateKeyBytes[byteSize];
                fromHex(privateKeyHex, privateKeyBytes, byteSize);
                PrivateKey privKey(privateKeyBytes);
                PublicKey pub = privKey.publicKey();
                publicKeyHex = pub.toString();
                // remove leading 2 bytes from public key
                publicKeyHex = publicKeyHex.substring(2);
            }
            catch (...)
            {
                Serial.println("NostrManager: ERROR - Failed to derive public key");
            }
        }

        authorizedClients = prefs.getString("auth_clients", "");

        prefs.end();

        Serial.println("NostrManager::loadConfigFromPreferences() - Configuration loaded");
        Serial.println("Relay URL: " + relayUrl);
        Serial.println("Has private key: " + String(privateKeyHex.length() > 0 ? "Yes" : "No"));
    }

    void saveConfigToPreferences()
    {
        Preferences prefs;
        prefs.begin("signer", false); // Read-write

        // prefs.putString("relay_url", relayUrl);
        // prefs.putString("private_key", privateKeyHex);
        // prefs.putString("public_key", publicKeyHex);
        // prefs.putString("auth_clients", authorizedClients);

        prefs.end();

        Serial.println("NostrManager::saveConfigToPreferences() - Configuration saved");
    }

    void connectToRelay()
    {
        if (!signer_initialized || relayUrl.length() == 0)
        {
            Serial.println("NostrManager::connectToRelay() - Cannot connect: not initialized or no relay URL");
            return;
        }

        if (connection_in_progress)
        {
            Serial.println("NostrManager::connectToRelay() - Connection already in progress");
            return;
        }

        Serial.println("NostrManager::connectToRelay() - Connecting to relay: " + relayUrl);
        Serial.println("Connection attempt #" + String(reconnection_attempts + 1) + " of " + String(Config::MAX_RECONNECT_ATTEMPTS));

        connection_in_progress = true;
        last_connection_attempt = millis();

        // Update status display immediately
        displayConnectionStatus(false);

        // Extract hostname from relay URL (remove wss:// prefix)
        String hostname = relayUrl;
        hostname.replace("wss://", "");
        hostname.replace("ws://", "");

        webSocket.beginSSL(hostname.c_str(), 443, "/");
        webSocket.onEvent(websocketEvent);
        webSocket.setReconnectInterval(Config::MIN_RECONNECT_INTERVAL);

        updateStatus(false, "Connecting to relay...");
    }

    void disconnect()
    {
        Serial.println("NostrManager::disconnect() - Disconnecting from relay");
        Serial.println("Connection was active for: " + String((millis() - last_connection_attempt) / 1000) + "s");

        webSocket.disconnect();
        connection_in_progress = false;

        // Update status display immediately
        displayConnectionStatus(false);

        updateStatus(false, "Disconnected");
    }

    void websocketEvent(WStype_t type, uint8_t *payload, size_t length)
    {
        switch (type)
        {
        case WStype_DISCONNECTED:
            Serial.println("NostrManager::websocketEvent() - WebSocket Disconnected");
            connection_in_progress = false;

            // Update status display immediately
            displayConnectionStatus(false);

            if (reconnection_attempts < Config::MAX_RECONNECT_ATTEMPTS)
            {
                reconnection_attempts++;
                Serial.println("NostrManager::websocketEvent() - Scheduling reconnection attempt " + String(reconnection_attempts));
                manual_reconnect_needed = true;

                updateStatus(false, "Reconnecting...");
            }
            else
            {
                Serial.println("NostrManager::websocketEvent() - Max reconnection attempts reached");
                updateStatus(false, "Connection failed");
            }
            break;

        case WStype_CONNECTED:
            Serial.println("NostrManager::websocketEvent() - WebSocket Connected to: " + String((char *)payload));
            connection_in_progress = false;
            reconnection_attempts = 0;
            manual_reconnect_needed = false;
            last_ws_message_received = millis();

            // Update status display immediately
            displayConnectionStatus(true);

            // Subscribe to NIP-46 events for our public key
            if (publicKeyHex.length() > 0)
            {
                String nostrIotDvmJobRequestIds = "[5107]";
                // create a random subscription id of chars 32 characters long
                String subscriptionId = "";
                for (int i = 0; i < 32; i++)
                {
                    char c = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"[random(62)];
                    subscriptionId += c;
                }

                String subscription = "[\"REQ\", \"" + subscriptionId + "\", {\"kinds\":" + nostrIotDvmJobRequestIds + ", \"#p\":[\"" + publicKeyHex + "\"], \"limit\":0}]";
                webSocket.sendTXT(subscription);
                Serial.println("NostrManager::websocketEvent() - Sent subscription: " + subscription);
            }

            updateStatus(true, "Connected");
            break;

        case WStype_TEXT:
            Serial.println("NostrManager::websocketEvent() - Received text message");
            last_ws_message_received = millis();
            handleWebsocketMessage(nullptr, payload, length);
            break;

        case WStype_BIN:
            Serial.println("NostrManager::websocketEvent() - Received binary message");
            last_ws_message_received = millis();
            handleWebsocketMessage(nullptr, payload, length);
            break;

        case WStype_PING:
            Serial.println("NostrManager::websocketEvent() - Received ping");
            last_ws_message_received = millis();
            break;

        case WStype_PONG:
            Serial.println("NostrManager::websocketEvent() - Received pong");
            last_ws_message_received = millis();
            break;

        case WStype_ERROR:
            Serial.println("NostrManager::websocketEvent() - WebSocket Error");
            connection_in_progress = false;
            manual_reconnect_needed = true;

            updateStatus(false, "Connection error");
            break;

        default:
            break;
        }
    }

    void handleWebsocketMessage(void *arg, uint8_t *data, size_t len)
    {
        String message = String((char *)data);

        if (message.indexOf("EVENT") != -1)
        {
            Serial.println("NostrManager::handleWebsocketMessage() - Received signing request");
            handleEvent(data);
        }
    }

    void handleEvent(uint8_t *data)
    {
        String dataStr = String((char *)data);
        Serial.println("NostrManager::handleEvent() - Processing event: " + dataStr);

        // Extract sender public key from the event using nostr library
        String requestingPubKey = nostr::getSenderPubKeyHex(dataStr);
        String eventContentStr = "";
        Serial.println("NostrManager::handleEvent() - Requesting pubkey: " + requestingPubKey);

        // if the dataStr has the ["encrypted"] tag then it is encrypted and needs to be decrypted
        // TODO: implement decryption of i and param tags instead of content property
        if (dataStr.indexOf("[\"encrypted\"]") >= 0)
        {
            Serial.println("NostrManager::handleEvent() - Encrypted DVM requests are not supported yet");
            // Serial.println("NostrManager::handleEvent() - Event is encrypted, decrypting");
            // // Determine encryption type (NIP-04 vs NIP-44) and decrypt
            // String decryptedMessage = "";
            // if (dataStr.indexOf("?iv=") != -1)
            // {
            //     Serial.println("NostrManager::handleEvent() - Using NIP-04 decryption");
            //     decryptedMessage = nostr::nip04Decrypt(privateKeyHex.c_str(), dataStr);
            // }
            // else
            // {
            //     Serial.println("NostrManager::handleEvent() - Using NIP-44 decryption");
            //     decryptedMessage = nostr::nip44Decrypt(privateKeyHex.c_str(), dataStr);
            // }

            // if (decryptedMessage.length() == 0)
            // {
            //     Serial.println("NostrManager::handleEvent() - Failed to decrypt message");
            //     return;
            // }
            // Serial.println("NostrManager::handleEvent() - Decrypted message: " + decryptedMessage);
            // Parse the decrypted JSON
            // DeserializationError error = deserializeJson(eventDoc, decryptedMessage);
            // if (error)
            // {
            //     Serial.println("NostrManager::handleEvent() - JSON parsing failed: " + String(error.c_str()));
            //     return;
            // }
        }
        else
        {
            Serial.println("NostrManager::handleEvent() - Event is not encrypted" + dataStr);
            // declare a new DynamicJsonDocument with size 2048
            DynamicJsonDocument doc(2048);
            DeserializationError error = deserializeJson(doc, dataStr);
            if (error)
            {
                Serial.println("NostrManager::handleEvent() - JSON parsing failed: " + String(error.c_str()));
                return;
            }
                        
        }
        JsonArray tags = nostr::getTags(dataStr);

        if (tags.size() > 0)
        {
            // print all tags
            Serial.println("NostrManager::handleEvent() - Event tags:");
            for (JsonVariant tag : tags)
            {
                // String tagStr = tag.as<String>();
                // Serial.println(" - " + tagStr);
                // print tag key and values
                if (tag.size() > 0)
                {
                    String tagKey = tag[0];
            }
        }
    }

    void handleConnect(DynamicJsonDocument &doc, const String &requestingPubKey)
    {
        // String requestId = doc["id"];
        // String secret = doc["params"][1];

        // Serial.println("NostrManager::handleConnect() - Connect request from: " + requestingPubKey);

        // if (!checkClientIsAuthorized(requestingPubKey.c_str(), secret.c_str())) {
        //     Serial.println("NostrManager::handleConnect() - Client not authorized");
        //     return;
        // }

        // // Send acknowledgment
        // String responseMsg = secret.length() > 0 ?
        //     "{\"id\":\"" + requestId + "\",\"result\":\"" + secret + "\"}" :
        //     "{\"id\":\"" + requestId + "\",\"result\":\"ack\"}";

        // Serial.println("NostrManager::handleConnect() - Sending connect response: " + responseMsg);

        // // Encrypt and send response using NIP-44
        // String encryptedResponse = nostr::getEncryptedDm(
        //     privateKeyHex.c_str(),
        //     publicKeyHex.c_str(),
        //     requestingPubKey.c_str(),
        //     24133,
        //     unixTimestamp,
        //     responseMsg,
        //     "nip44"
        // );

        // webSocket.sendTXT(encryptedResponse);
        Serial.println("NostrManager::handleConnect() - Response sent");
    }

    void handleSignEvent(DynamicJsonDocument &doc, const char *requestingPubKey)
    {
        String requestId = doc["id"];

        Serial.println("NostrManager::handleSignEvent() - Sign event request from: " + String(requestingPubKey));

        // Parse event parameters - first parameter contains the event to sign
        String eventParams = doc["params"][0].as<String>();

        // Parse the event data from the first parameter
        DeserializationError parseError = deserializeJson(eventParamsDoc, eventParams);
        if (parseError)
        {
            Serial.println("NostrManager::handleSignEvent() - Failed to parse event params: " + String(parseError.c_str()));
            return;
        }

        // Extract event details
        uint16_t kind = eventParamsDoc["kind"];
        String content = eventParamsDoc["content"].as<String>();
        String tags = eventParamsDoc["tags"].as<String>();
        unsigned long timestamp = eventParamsDoc["created_at"];

        Serial.println("NostrManager::handleSignEvent() - Event kind: " + String(kind));
        Serial.println("NostrManager::handleSignEvent() - Content: " + content.substring(0, 50) + "...");

        // For now, auto-approve (TODO: Add UI confirmation)
        // Sign the event using nostr library
        String signedEvent = nostr::getNote(
            privateKeyHex.c_str(),
            publicKeyHex.c_str(),
            timestamp,
            content,
            kind,
            tags);

        // Escape quotes in the signed event for JSON response
        signedEvent.replace("\\", "\\\\");
        signedEvent.replace("\"", "\\\"");

        // Create response
        String responseMsg = "{\"id\":\"" + requestId + "\",\"result\":\"" + signedEvent + "\"}";

        // Encrypt and send response
        String encryptedResponse = nostr::getEncryptedDm(
            privateKeyHex.c_str(),
            publicKeyHex.c_str(),
            requestingPubKey,
            24133,
            unixTimestamp,
            responseMsg,
            "nip44");

        webSocket.sendTXT(encryptedResponse);
        Serial.println("NostrManager::handleSignEvent() - Event signed and response sent");
    }

    void handlePing(DynamicJsonDocument &doc, const char *requestingPubKey)
    {
        String requestId = doc["id"];

        String responseMsg = "{\"id\":\"" + requestId + "\",\"result\":\"pong\"}";

        String encryptedResponse = nostr::getEncryptedDm(
            privateKeyHex.c_str(),
            publicKeyHex.c_str(),
            requestingPubKey,
            24133,
            unixTimestamp,
            responseMsg,
            "nip44");

        webSocket.sendTXT(encryptedResponse);
        Serial.println("NostrManager::handlePing() - Pong sent to: " + String(requestingPubKey));
    }

    void handleGetPublicKey(DynamicJsonDocument &doc, const char *requestingPubKey)
    {
        String requestId = doc["id"];

        String responseMsg = "{\"id\":\"" + requestId + "\",\"result\":\"" + publicKeyHex + "\"}";

        String encryptedResponse = nostr::getEncryptedDm(
            privateKeyHex.c_str(),
            publicKeyHex.c_str(),
            requestingPubKey,
            24133,
            unixTimestamp,
            responseMsg,
            "nip44");

        webSocket.sendTXT(encryptedResponse);
        Serial.println("NostrManager::handleGetPublicKey() - Public key sent to: " + String(requestingPubKey));
    }

    void handleNip04Encrypt(DynamicJsonDocument &doc, const char *requestingPubKey)
    {

        String requestId = doc["id"];
        String thirdPartyPubKey = doc["params"][0];
        String plaintext = doc["params"][1];

        String encryptedMessage = nostr::getCipherText(privateKeyHex.c_str(), thirdPartyPubKey.c_str(), plaintext);
        String responseMsg = "{\"id\":\"" + requestId + "\",\"result\":\"" + encryptedMessage + "\"}";

        String encryptedResponse = nostr::getEncryptedDm(
            privateKeyHex.c_str(),
            publicKeyHex.c_str(),
            requestingPubKey,
            24133,
            unixTimestamp,
            responseMsg,
            "nip04");

        webSocket.sendTXT(encryptedResponse);
        Serial.println("NostrManager::handleNip04Encrypt() - NIP-04 encryption completed");
    }

    void handleNip04Decrypt(DynamicJsonDocument &doc, const char *requestingPubKey)
    {

        String requestId = doc["id"];
        String thirdPartyPubKey = doc["params"][0];
        String cipherText = doc["params"][1];

        String decryptedMessage = nostr::decryptNip04Ciphertext(cipherText, privateKeyHex, thirdPartyPubKey);
        String responseMsg = "{\"id\":\"" + requestId + "\",\"result\":\"" + decryptedMessage + "\"}";

        String encryptedResponse = nostr::getEncryptedDm(
            privateKeyHex.c_str(),
            publicKeyHex.c_str(),
            requestingPubKey,
            24133,
            unixTimestamp,
            responseMsg,
            "nip04");

        webSocket.sendTXT(encryptedResponse);
        Serial.println("NostrManager::handleNip04Decrypt() - NIP-04 decryption completed");
    }

    void handleNip44Encrypt(DynamicJsonDocument &doc, const char *requestingPubKey)
    {

        String requestId = doc["id"];
        String thirdPartyPubKey = doc["params"][0];
        String plaintext = doc["params"][1];

        // Use NIP-44 encryption functions
        String encryptedMessage = executeEncryptMessageNip44(plaintext, privateKeyHex, thirdPartyPubKey);
        String responseMsg = "{\"id\":\"" + requestId + "\",\"result\":\"" + encryptedMessage + "\"}";

        String encryptedResponse = nostr::getEncryptedDm(
            privateKeyHex.c_str(),
            publicKeyHex.c_str(),
            requestingPubKey,
            24133,
            unixTimestamp,
            responseMsg,
            "nip44");

        webSocket.sendTXT(encryptedResponse);
        Serial.println("NostrManager::handleNip44Encrypt() - NIP-44 encryption completed");
    }

    void handleNip44Decrypt(DynamicJsonDocument &doc, const char *requestingPubKey)
    {

        String requestId = doc["id"];
        String thirdPartyPubKey = doc["params"][0];
        String cipherText = doc["params"][1];

        // Use NIP-44 decryption functions
        String decryptedMessage = executeDecryptMessageNip44(cipherText, privateKeyHex, thirdPartyPubKey);
        String responseMsg = "{\"id\":\"" + requestId + "\",\"result\":\"" + decryptedMessage + "\"}";

        String encryptedResponse = nostr::getEncryptedDm(
            privateKeyHex.c_str(),
            publicKeyHex.c_str(),
            requestingPubKey,
            24133,
            unixTimestamp,
            responseMsg,
            "nip44");

        webSocket.sendTXT(encryptedResponse);
        Serial.println("NostrManager::handleNip44Decrypt() - NIP-44 decryption completed");
    }

    void processLoop()
    {
        // Update time
        timeClient.update();
        unixTimestamp = timeClient.getEpochTime();

        // Process WebSocket events
        webSocket.loop();

        // Send periodic ping
        unsigned long now = millis();
        if (now - last_ws_ping > Config::WS_PING_INTERVAL)
        {
            sendPing();
            last_ws_ping = now;
        }

        // Periodic status updates every 5 seconds
        static unsigned long last_status_update = 0;
        if (now - last_status_update > 5000)
        {
            displayConnectionStatus(isConnected());
            last_status_update = now;
        }

        // Debug: Log connection health every 30 seconds
        static unsigned long last_debug_log = 0;
        if (now - last_debug_log > 30000)
        {
            if (isConnected())
            {
                Serial.println("NostrManager::processLoop() - Connection healthy. Last message: " + String((now - last_ws_message_received) / 1000) + "s ago");
            }
            else
            {
                Serial.println("NostrManager::processLoop() - Not connected. Manual reconnect needed: " + String(manual_reconnect_needed ? "Yes" : "No"));
            }
            last_debug_log = now;
        }

        // Check connection health
        if (isConnected() && (now - last_ws_message_received > Config::CONNECTION_TIMEOUT))
        {
            Serial.println("NostrManager::processLoop() - Connection timeout detected");
            Serial.println("Last message received: " + String((now - last_ws_message_received) / 1000) + "s ago");
            disconnect();
            manual_reconnect_needed = true;
        }

        // Handle manual reconnection with exponential backoff
        if (manual_reconnect_needed && !connection_in_progress && !isConnected())
        {
            unsigned long backoff_delay = Config::MIN_RECONNECT_INTERVAL * (1 << min(reconnection_attempts, 5)); // Cap at 32x base interval

            if (now - last_reconnect_attempt >= backoff_delay)
            {
                if (reconnection_attempts < Config::MAX_RECONNECT_ATTEMPTS)
                {
                    Serial.println("NostrManager::processLoop() - Attempting manual reconnection #" + String(reconnection_attempts + 1));
                    Serial.println("Backoff delay was: " + String(backoff_delay) + "ms");

                    connectToRelay();
                    last_reconnect_attempt = now;
                    reconnection_attempts++;
                }
                else
                {
                    Serial.println("NostrManager::processLoop() - Max reconnection attempts reached, giving up");
                    // reboot the device
                    ESP.restart();
                    manual_reconnect_needed = false;
                    reconnection_attempts = 0;

                    // Update status display immediately
                    displayConnectionStatus(false);

                    updateStatus(false, "Connection failed permanently");
                }
            }
        }
    }

    void sendPing()
    {
        if (isConnected())
        {
            Serial.println("NostrManager::sendPing() - Sending ping to relay");
            webSocket.sendPing();
        }
        else
        {
            Serial.println("NostrManager::sendPing() - Cannot send ping: not connected");
        }
    }

    bool isInitialized()
    {
        return signer_initialized;
    }

    bool isConnected()
    {
        return webSocket.isConnected();
    }

    void displayConnectionStatus(bool connected)
    {
        Serial.println("NostrManager::displayConnectionStatus() - Connection status: " + String(connected ? "Connected" : "Disconnected"));
    }

    // Getters
    String getRelayUrl() { return relayUrl; }
    void setRelayUrl(const String &url) { relayUrl = url; }
    String getPrivateKey() { return privateKeyHex; }
    void setPrivateKey(const String &privKeyHex)
    {
        privateKeyHex = privKeyHex;
        // Derive public key automatically
        if (privKeyHex.length() == 64)
        {
            try
            {
                int byteSize = 32;
                byte privateKeyBytes[byteSize];
                fromHex(privKeyHex, privateKeyBytes, byteSize);
                PrivateKey privKey(privateKeyBytes);
                PublicKey pub = privKey.publicKey();
                publicKeyHex = pub.toString();
                // remove leading 2 bytes from public key
                publicKeyHex = publicKeyHex.substring(2);
                Serial.println("NostrManager: Derived public key: " + publicKeyHex);
            }
            catch (...)
            {
                Serial.println("NostrManager: ERROR - Failed to derive public key");
            }
        }
    }
    String getPublicKey() { return publicKeyHex; }

    void setStatusCallback(signer_status_callback_t callback)
    {
        status_callback = callback;
    }
}