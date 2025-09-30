#pragma once
#include <WString.h>
#include <Arduino.h>
#include <WiFi.h>
#include <HTTPClient.h>

#include "config.h"

namespace NostriotProvider {
    void init();
    void cleanup();
    int getPrice(const String &method);
    String* getCapabilities(int &count);
    bool hasCapability(const String &capability);
    String getCapabilitiesAdvertisement();
    String run(String &method, String &value);
}