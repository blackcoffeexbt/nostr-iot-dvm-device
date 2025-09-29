#pragma once
#include <WString.h>

namespace NostriotProvider {
    void init();
    void cleanup();
    String* getCapabilities(int &count);
    bool hasCapability(const String &capability);
    String run(String &method);
}