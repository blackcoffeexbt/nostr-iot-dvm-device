/**
 *
 * This is the main provider file for Nostriot IoT device operations.
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

    static const String capabilities[] = {"getTemperature", "setLED"};
    static const int capabilities_count = sizeof(capabilities) / sizeof(capabilities[0]);

    // price per request in sats
    static const int price_per_request = 1;

    

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
        count = capabilities_count;
        return (String *)capabilities;
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

    int getPricePerRequest()
    {
        return price_per_request;
    }

    void cleanup()
    {
        // Do any necessary cleanup here
    }

    String run(String &method)
    {
        // Main loop for device operations
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