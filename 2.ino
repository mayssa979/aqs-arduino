#include "BLEDevice.h"

// Define UUIDs for the BLE service and characteristic
static BLEUUID serviceUUID("C760");
static BLEUUID charUUID("C761");

// Define state variables for connection status
static boolean doConnect = false;
static boolean connected = false;
static boolean doScan = false;
static BLERemoteCharacteristic* pRemoteCharacteristic;
static BLEAdvertisedDevice* myDevice;
const char* targetMacAddress = "17:71:06:24:0b:27";

// Callback function to handle notifications from the BLE device
static void notifyCallback(
    BLERemoteCharacteristic* pBLERemoteCharacteristic,
    uint8_t* pData,
    size_t length,
    bool isNotify) {
    
    Serial.print("Received data: ");
    for (int i = 0; i < length; i++) {
        // Print each byte in hexadecimal format
        Serial.print(pData[i] < 16 ? "0" : "");
        Serial.print(pData[i], HEX);
        Serial.print(" ");
    }
    Serial.println();

    // Check if the frame begins with 23 06 for temperature and humidity
    if (length >= 2 && pData[0] == 0x23 && pData[1] == 0x06) {
        // Calculate the humidity and temperature values
        size_t humidityBytePos = length - 2;
        size_t tempBytePos = length - 4;

        // Extract the humidity value
        uint8_t humidityValue = pData[humidityBytePos];
        
        // Extract the temperature value
        uint8_t tempValue = ((pData[4] << 8) + pData[5]) * 0.1f;

        // Print humidity and temperature values
        Serial.print("Humidity value: ");
        Serial.print(humidityValue);
        Serial.println();

        Serial.print("Temperature value: ");
        Serial.print(tempValue);
        Serial.println();
    } 
    // Check if the frame begins with 23 08 for CO2, HCHO, and TVOC
    else if (length >= 2 && pData[0] == 0x23 && pData[1] == 0x08) {
        //////////////////////////// CO2 /////////////////////////////////
        size_t co2BytePos1 = length - 6;
        size_t co2BytePos2 = length - 7;
        uint16_t co2Value = (pData[co2BytePos2] << 8) + pData[co2BytePos1];
        Serial.print("CO2 value: ");
        Serial.print(co2Value);
        Serial.println();

        ////////////////////// HCHO /////////////////////////////////////
        size_t hchoBytePos1 = length - 2;
        size_t hchoBytePos2 = length - 3;
        uint16_t hchoValue = (pData[hchoBytePos2] << 8) + pData[hchoBytePos1];
        Serial.print("HCHO value: ");
        Serial.print(hchoValue);
        Serial.println();

        /////////////////////// TVOC ////////////////////////////////////
        size_t tvocBytePos1 = length - 4;
        size_t tvocBytePos2 = length - 5;
        uint16_t tvocValue = (pData[tvocBytePos2] << 8) + pData[tvocBytePos1];
        Serial.print("TVOC value: ");
        Serial.print(tvocValue);
        Serial.println();
    }

    // Optional delay between reading notifications
    delay(30000);
}

// Callback class for handling connection events
class MyClientCallback : public BLEClientCallbacks {
    void onConnect(BLEClient* pclient) {
        Serial.println("Connected to BLE Server.");
    }

    void onDisconnect(BLEClient* pclient) {
        connected = false;
        Serial.println("Disconnected from BLE Server.");
    }
};

// Function to connect to the BLE server
bool connectToServer() {
    Serial.print("Attempting to connect to ");
    Serial.println(myDevice->getAddress().toString().c_str());

    BLEClient* pClient = BLEDevice::createClient();
    pClient->setClientCallbacks(new MyClientCallback());

    // Try to connect to the server
    if (pClient->connect(myDevice)) {
        Serial.println("Connected to server.");
    } else {
        Serial.println("Failed to connect to server.");
        return false;
    }

    // Set MTU for data transmission
    pClient->setMTU(517);

    // Attempt to find the service
    BLERemoteService* pRemoteService = pClient->getService(serviceUUID);
    if (pRemoteService == nullptr) {
        Serial.println("Failed to find service with UUID: ");
        Serial.println(serviceUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println("Service with UUID found.");

    // Attempt to find the characteristic
    pRemoteCharacteristic = pRemoteService->getCharacteristic(charUUID);
    if (pRemoteCharacteristic == nullptr) {
        Serial.println("Failed to find characteristic with UUID: ");
        Serial.println(charUUID.toString().c_str());
        pClient->disconnect();
        return false;
    }
    Serial.println("Characteristic with UUID found.");

    // Register for notifications if possible
    if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify(notifyCallback);
    }

    connected = true;
    return true;
}

// Callback class for handling advertised device events
class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
    void onResult(BLEAdvertisedDevice advertisedDevice) {
        Serial.print("BLE Advertised Device found: ");
        Serial.println(advertisedDevice.toString().c_str());

        String deviceAddress = advertisedDevice.getAddress().toString();
        Serial.print("Device Address: ");
        Serial.println(deviceAddress);

        // Check if the advertised device matches the target MAC address
        if (deviceAddress.equals(targetMacAddress)) {
            Serial.println("Target device found!");
            BLEDevice::getScan()->stop();
            myDevice = new BLEAdvertisedDevice(advertisedDevice);
            doConnect = true;
            doScan = true;
        }
    }
};

// Setup function to initialize BLE and start scanning
void setup() {
    Serial.begin(115200);
    Serial.println("Starting BLE Client...");

    BLEDevice::init("");

    // Configure BLE scan settings
    BLEScan* pBLEScan = BLEDevice::getScan();
    pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pBLEScan->setInterval(1349);
    pBLEScan->setWindow(449);
    pBLEScan->setActiveScan(true);
    pBLEScan->start(5, false);
}

// Main loop to maintain connection and handle events
void loop() {
    // Attempt to connect if required
    if (doConnect == true) {
        if (connectToServer()) {
            Serial.println("Success! Connected to the Air Quality Sensor.");
        } else {
            Serial.println("Failed to connect to the Air Quality Sensor.");
        }
        doConnect = false;
    }

    // If connected, write data to the characteristic
    if (connected) {
        String newValue = "Time since boot: " + String(millis() / 1000);
        pRemoteCharacteristic->writeValue(newValue.c_str(), newValue.length());
    } 
    // If not connected, start scanning again
    else if (doScan) {
        BLEDevice::getScan()->start(0);  // Start scanning indefinitely
    }

    delay(100);  // Wait for 1 second before checking again
}
