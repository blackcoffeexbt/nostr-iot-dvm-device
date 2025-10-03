# Nostriot Device

A Nostr IoT device implementing Data Vending Machine (DVM) services with Lightning Network payment integration.

## Overview

This is a hardware-based Nostr IoT device built for the ESP32-S3 Guition JC3248W535 touch screen display. The device implements the Nostr DVM (Data Vending Machine) protocol to provide IoT services like temperature readings and device control through the Nostr network, with optional Lightning Network payments for premium services.

## Features

- **Nostr DVM Protocol**: Complete implementation of Data Vending Machine services
- **IoT Services**: Temperature monitoring, LED control, and extensible sensor integration
- **Lightning Payments**: LNbits integration for paid services with automatic payment processing
- **Touch Screen Interface**: 320x480 color touchscreen for device management
- **WiFi Connectivity**: Connect to Nostr relays and payment providers via WiFi
- **Payment Queue**: Robust payment processing with timeout management
- **Real-time Monitoring**: WebSocket-based payment confirmation and status updates
- **Modular Architecture**: Clean separation of concerns for easy maintenance and extension

## Hardware Requirements

- ESP32-S3 dev board
- Compatible sensors (temperature, etc.)
- Power supply (USB or battery)

## Getting Started

### Prerequisites

- [PlatformIO](https://platformio.org/) installed
- ESP32-S3 development board
- LNbits instance for Lightning payments

### Customising the IoT device for your hardware

The `src/nostriot_provider.cpp` module lets you customise your Nostr IoT device to match your own hardware and pricing model. In `src/nostriot_provider.cpp` you define what the device can do (capabilities), and how much each action costs (in sats). The example includes an number of example methods with different pricing. This demonstrates how to set up free and variable paid services.

To adapt it, change the hardware configuration for your pins and sensors in the `init()` function, then add or edit entries in capabilities_with_pricing. Each capability’s behaviour is defined in the `run()` function, where you decide what happens when it’s called.

### Building and Flashing

```bash
# Clone the repository
git clone <repository-url>
cd nostriot-device

# Build the project
pio run

# Upload to device
pio run --target upload

# Monitor serial output with exception decoder
pio device monitor

# Build and upload with monitoring
pio run --target upload --target monitor
```

### Documentation

```bash
# Generate documentation
pio run -t docs

# Run code quality checks
pio check
```

### Initial Setup

1. **Configure** Wifi, Nostr, and LNbits settings in `src/config.h`
2. **Flash** the firmware to the ESP32-S3 device using PlatformIO either using VSCode or command line
1. **Power on the device**
1. **Connect** to the device's access point (if no saved WiFi) and configure WiFi using the web interface

### Usage

**IoT Service Requests:**
1. Send DVM job requests (kind 5107) to the device's public key
2. Free services execute immediately
3. Paid services return payment requests with Lightning invoices
4. Pay the invoice to receive service response

**Available Services:**
- The available services and their pricing can be configured in `src/nostriot_provider.cpp` this is where external sensors / actuators can be integrated.

**Payment Integration:**
- Automatic Lightning invoice generation via LNbits
- Real-time payment monitoring and confirmation
- Payment queue with timeout management
- Support for both BOLT11 invoices and Nostr zaps

## Architecture

The application uses a clean modular architecture:

### Core Modules
- **App**: Central coordinator managing all modules and inter-module communication
- **NostrManager**: Nostr protocol implementation, DVM message handling, and relay communication
- **PaymentProvider**: Lightning payment integration, invoice generation, and payment monitoring
- **NostriotProvider**: IoT device capabilities, sensor reading, and device control
- **WiFiManager**: Network connectivity and configuration management
- **Display**: Touch screen interface and status display
- **Settings**: Persistent configuration storage and management

### Key Features
- **Payment Queue**: Handles concurrent payment requests with proper timeout management
- **WebSocket Monitoring**: Real-time payment confirmations from LNbits
- **Modular Payment System**: Easy to swap payment providers (LNbits → other Lightning services)
- **DVM Protocol**: Complete implementation of Nostr Data Vending Machine specification
- **Event-Driven Architecture**: Clean separation between protocol handling and business logic

## Configuration

Configuration options in `src/config.h`:

```cpp
// Nostr relay. relay.nostriot.com is a public relay run by the Nostriot project
#define NOSTR_RELAY_URI "wss://relay.nostriot.com"

// Nostr private key in hex format (32 bytes, 64 hex characters).
// You can use https://nostrtool.com/ to generate a new key pair
#define NOSTR_PRIVATE_KEY "[YOUR NOSTR PRIVATE KEY IN HEX FORMAT]"

// This is the d tag value that is added to the replacable DVM advert
// You probably don't need to change this unless you run multiple devices with the same private key
#define DVM_ADVERTISEMENT_EVENT_D_TAG_VALUE "dvm-advert-1"

// LNbits configuration for Lightning payments
#define LNBITS_HOST_URL "[YOUR LNBITS HOST URL WITHOUT https:// e.g. demo.lnbits.com]"
#define LNBITS_INVOICE_KEY "[YOUR LNBITS INVOICE KEY]"
#define LNBITS_PAYMENTS_ENDPOINT "/api/v1/payments" // usually don't change this
```

### Code Quality

```bash
# Run code quality checks
pio check

# Find unused functions
cppcheck --enable=unusedFunction src/

# Wider analysis
cppcheck --enable=unusedFunction,style --inconclusive src/
```

## License

MIT License