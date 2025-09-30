#pragma once
#include <WString.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "config.h"

namespace NostriotProvider {
    void init();
    void cleanup();
    String httpPost(const String &url, const String &headers, const String &postData);
    String getInvoice(int amount_sats, String &memo);
    String extractBolt11FromResponse(String &response);
    int getPricePerRequest();
    String* getCapabilities(int &count);
    bool hasCapability(const String &capability);
    String run(String &method);
}