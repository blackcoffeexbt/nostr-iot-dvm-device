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
    const static String SERVICE_NAME = "Nostriot IoT Service";
    const static String SERVICE_DESCRIPTION = "Provides IoT device functionalities via Nostr protocol";
    // Device capabilities and pricing
    struct Capability
    {
        String name;
        int price;
    };

    static const std::vector<Capability> capabilities_with_pricing = {
        {"getTemperature", 1},
        {"setLED", 2},
    };

    void init()
    {
        // set up the sensor or whatever is needed here
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
    int getPrice(const String &method)
    {
        for (const auto &cap : capabilities_with_pricing)
        {
            if (cap.name == method)
            {
                return cap.price;
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

    String run(String &method)
    {
        // TODO: get real data
        if (method == "getTemperature")
        {
            // return a fake temperature for now between 20 and 25 degrees C
            float temperature = 20.0 + static_cast<float>(random(0, 500)) / 100.0;
            return String(temperature);
        }
        else if (method == "setLED")
        {
            // pretend to set an LED
            return "LED set";
        }
        else
        {
            return "Unknown method";
        }
    }
}