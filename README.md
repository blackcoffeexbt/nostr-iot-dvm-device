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

Key configuration options in `src/config.h`:

```cpp
// Nostr Configuration
#define NOSTR_RELAY_URI "wss://relay.example.com"
#define NOSTR_PRIVATE_KEY "your_device_private_key_hex"

// LNbits Configuration  
#define LNBITS_HOST_URL "lnbits.example.com"
#define LNBITS_INVOICE_KEY "your_invoice_key"
#define LNBITS_PAYMENTS_ENDPOINT "/api/v1/payments"

// Device Configuration
#define DEVICE_NAME "Nostriot Device"
```

### Adding New IoT Services

1. **Add capability** to `NostriotProvider::capabilities[]`
2. **Implement service logic** in `NostriotProvider::run()`
3. **Set pricing** in `NostriotProvider::getPricePerRequest()`
4. **Test with DVM requests** via Nostr clients

### Code Quality

```bash
# Run code quality checks
pio check

# Find unused functions
cppcheck --enable=unusedFunction src/

# Wider analysis
cppcheck --enable=unusedFunction,style --inconclusive src/
```

## Protocol Implementation

### DVM (Data Vending Machine)
- **Request Format**: NIP-89 compatible job requests (kind 5107)
- **Response Format**: Job results (kind 6107) with proper tagging
- **Payment Integration**: Payment requests (kind 9735) with Lightning invoices
- **Error Handling**: Comprehensive error responses and status codes

### Lightning Integration
- **Invoice Generation**: Automatic BOLT11 invoice creation
- **Payment Monitoring**: Real-time WebSocket payment confirmations
- **Queue Management**: Concurrent payment handling with timeouts
- **Zap Support**: Ready for future Nostr zap payment integration

## License

MIT License