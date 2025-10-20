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
    const static String SERVICE_NAME = "ESP32C3 Supermini IoT Device";
    const static String SERVICE_DESCRIPTION = "A test device for Nostriot IoT functionality";
    // Device capabilities and pricing
    struct Capability
    {
        String name;
        int price;
    };

    static const std::vector<Capability> capabilities_with_pricing = {
        {"runVacuum", 10},
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
    int getPrice(const String &method, const String &value)
    {
        if(method == "setTemperature") {
            // variable pricing based on target temperature
            return getSetTemperaturePrice(value.toFloat());
        }
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
     * @brief This is an example of variable method pricing 
     * 
     * @return int 
     */
    int getSetTemperaturePrice(float targetTemp) {
        // we determine the cost based on a temperature difference between current and requested temperature
        float currentTemp = getCurrentTemperature();
        float diff = fabs(targetTemp - currentTemp);
        // calc based on price of 1 sat per degree C difference, rounded up
        int price = (int)ceil(diff * 1.0);
        Serial.println("NostriotProvider::getSetTemperaturePrice() - Current temp: " + String(currentTemp) + "C, Target temp: " + String(targetTemp) + ", Diff: " + String(diff) + "C, Price: " + String(price) + " sats");
        return price;
        
    }

    float getCurrentTemperature() {
        // return a fake temperature for now between 5 and 15 degrees C
        return random(50, 150) / 10.0;
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

    boolean vacuumIsRunning = false;

    String run(String &method, String &value)
    {
        // TODO: get real data
        if (method == "getTemperature")
        {
            return String(getCurrentTemperature());
        }
        else if (method == "runVacuum")
        {
            vacuumIsRunning = !vacuumIsRunning;
            Serial.println("NostriotProvider::runVacuum() - Vacuum is now " + String(vacuumIsRunning ? "running" : "stopped"));
            return String(vacuumIsRunning ? "Vacuum started" : "Vacuum stopped");
        }
        else if (method == "getHumidity")
        {
            // return a fake humidity for now between 30 and 70%
            int humidity = random(30, 70);
            return String(humidity);
        }
        else if (method == "toggleLamp")
        {
            // pretend to toggle a lamp
            return "Lamp toggled";
        }
        else if (method == "setTemperature")
        {
            // pretend to set a temperature
            Serial.println("NostriotProvider::setTemperature() - Setting temperature to " + value + " degrees C");
            return "Temperature set to " + value + " degrees C";
        }
        else
        {
            return "Unknown method";
        }
    }
}