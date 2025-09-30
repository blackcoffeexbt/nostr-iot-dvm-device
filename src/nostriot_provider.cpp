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
     * @brief Make an HTTP POST request
     * @param url The URL to send the request to
     * @param headers The headers to include in the request
     * @param postData The POST data to send
     * @return String Response body from the server
     */
    String httpPost(const String &url, const String &postData)
    {
        HTTPClient http;
        http.begin(url);
        
        int httpResponseCode = http.POST(postData);
        String response = "";
        
        if (httpResponseCode > 0)
        {
            response = http.getString();
            Serial.println("NostriotProvider::httpPost() - HTTP Response code: " + String(httpResponseCode));
            Serial.println("NostriotProvider::httpPost() - Response: " + response);
        }
        else
        {
            Serial.println("NostriotProvider::httpPost() - Error in HTTP request: " + String(httpResponseCode));
        }
        
        http.end();
        return response;
    }

    /**
     * @brief Get a Lightning invoice
     * @param amount_sats Amount in satoshis
     * @param memo Invoice memo
     * @return String Full LNbits response with payment_hash and bolt11
     */
    String getInvoice(int amount_sats, String &memo) {
        String postData = "{\"unit\": \"sat\", \"out\": false, \"amount\": " + String(amount_sats) + ", \"memo\": \"" + memo + "\"}";
        String url = "https://" + String(LNBITS_HOST_URL) + String(LNBITS_PAYMENTS_ENDPOINT) + "?api-key=" + String(LNBITS_INVOICE_KEY);
        String response = httpPost(url, postData);
        return response; // Return full response instead of just bolt11
    }

    /**
     * @brief Extract BOLT11 invoice from LNbits response
     * @param response Full LNbits response
     * @return String BOLT11 invoice string
     */
    String extractBolt11FromResponse(String &response) {
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