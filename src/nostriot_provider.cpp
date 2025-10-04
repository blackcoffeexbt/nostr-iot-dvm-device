/**
 *
 * This is the main provider file for Nostriot device operations.
 * It is where sensors, switches etc are managed.
 *
 * @file nostriot_provider.cpp
 * @version 0.1
 * @date 2025-09-29
 *
 * @copyright Copyright (c) 2025
 *
 */

#include "nostriot_provider.h"

namespace NostriotProvider
{
    const static String SERVICE_NAME = "BC's Coffee Machine";
    const static String SERVICE_DESCRIPTION = "I make coffee but only for BC";
    
    // Hardware configuration
    #define RELAY_PIN 25
    static bool coffeeMachineState = false;
    
    // Device capabilities and pricing
    struct Capability
    {
        String name;
        int price;
    };

    static const std::vector<Capability> capabilities_with_pricing = {
        {"makeCoffee", 5}
    };

    void init()
    {
        pinMode(RELAY_PIN, OUTPUT);
        digitalWrite(RELAY_PIN, LOW); // assuming HIGH is the default "off" state for the relay
        Serial.println("Relay OFF");
        coffeeMachineState = false;
        Serial.println("NostriotProvider::init() - Relay pin " + String(RELAY_PIN) + " initialized");
    }

    /**
     * @brief Get the Capabilities object
     *
     * @param count
     * @return String*
     */
    String *
    getCapabilities(int &count)
    {
        count = capabilities_with_pricing.size();
        String *caps = new String[count];
        for (int i = 0; i < count; i++)
        {
            caps[i] = capabilities_with_pricing[i].name;
        }
        return caps;
    }

    /**
     * @brief Does the provider have the given capability?
     *
     */
    bool hasCapability(const String &capability)
    {
        int count = 0;
        String *capabilities = getCapabilities(count);
        for (int i = 0; i < count; i++)
        {
            if (capabilities[i] == capability)
            {
                return true;
            }
        }
        return false;
    }

    /**
     * @brief Get the Price Per Request object
     *
     * @param method
     * @return int Price in sats, or 0 if free or unknown
     */
    int getPrice(const String &method, const String &value)
    {
        for (const auto &cap : capabilities_with_pricing)
        {
            if (cap.name == method)
            {
                int price = cap.price;
                Display::displayManager.showMessage("Price is " + String(price) + " sats");
                return price;
            }
        }
        return 0; 
    }

    /**
     * @brief Get capabilities advertisement for broadcasting to relay
     * @return String JSON representation of unsigned event for capability advertisement
     */
    String getCapabilitiesAdvertisement()
    {
        String content = "{\\\"name\\\": \\\"" + SERVICE_NAME + "\\\", \\\"about\\\": \\\"" + SERVICE_DESCRIPTION + "\\\"}";
        
        String tags = "[";
        tags += "[\"k\",\"5107\"],"; // IoT kinds
        tags += "[\"t\"";
        
        // Add supported methods to tags
        for (size_t i = 0; i < capabilities_with_pricing.size(); i++)
        {
            tags += ",\"" + capabilities_with_pricing[i].name + "\"";
        }
        tags += "]";
        // d tag
        tags += ",[\"d\",\"" + String(DVM_ADVERTISEMENT_EVENT_D_TAG_VALUE) + "\"]";
        tags += "]";
        
        String eventJson = "{"
            "\"kind\": 31990,"
            "\"content\": \"" + content + "\","
            "\"tags\": " + tags +
        "}";
        
        Serial.println("NostriotProvider::getCapabilitiesAdvertisement() - Generated: " + eventJson);
        return eventJson;
    }

    void cleanup()
    {
        // Do any necessary cleanup here
    }

    String run(String &method, String &value)
    {
        if (method == "makeCoffee")
        {
            // Toggle the LED state
            coffeeMachineState = !coffeeMachineState;
            
            digitalWrite(RELAY_PIN, HIGH); // Trigger relay
            Serial.println("Relay ON");

            delay(500);
            digitalWrite(RELAY_PIN, LOW); // Trigger relay
            Serial.println("Relay OFF");
            
            String state = coffeeMachineState ? "ON" : "OFF";
            Serial.println("NostriotProvider::makeCoffee() - Coffee machine turned " + state);
            Display::displayManager.showMessage("Coffee machine button triggered");
            return "Coffee machine turned " + state;
        }
        else
        {
            return "Unknown method";
        }
    }
}