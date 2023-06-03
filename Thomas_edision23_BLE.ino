#include <TFT_eSPI.h>
#include <SPI.h>
#include <Wire.h>
#include <BLEDevice.h>
#include <BLEUtils.h>
#include <BLEServer.h>
#include <BLE2902.h>

TFT_eSPI tft = TFT_eSPI();

bool isServer =true;
int txPower = 2;
BLEAdvertising *pAdvertising;
BLEServer *pServer;
BLECharacteristic *pCharacteristic;

const char* msgDataServer = "Pedestrian Detected";
const char* msgDataClient = "Car Detected";

#define SERVICE_UUID        "4fafc201-1fb5-459e-8fcc-c5c9c331914b"
#define CHARACTERISTIC_UUID "beb5483e-36e1-4688-b7f5-ea07361b26a8"

class MyServerCallbacks: public BLEServerCallbacks {
  void onConnect(BLEServer* pServer) {
    pAdvertising->start();
    Serial.println("Client connected");
    tft.println("Client connected");
  }

  void onDisconnect(BLEServer* pServer) {
    pAdvertising->start();
    Serial.println("Client disconnected");
    tft.println("Client disconnected");
  }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onWrite(BLECharacteristic *pCharacteristic) {
    std::string rxValue = pCharacteristic->getValue();

    if (rxValue.length() > 0) {
      tft.fillScreen(TFT_BLACK);
      tft.setCursor(0, 0);
      tft.setTextColor(TFT_WHITE, TFT_BLACK);
      tft.setTextSize(2);
      tft.println("Received message:");
      tft.println(rxValue.c_str());
      Serial.println("Received message:");
      Serial.println(rxValue.c_str());
    }
  }
};

#define RSSI_THRESHOLD -60

class MyAdvertisedDeviceCallbacks: public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice advertisedDevice) {
    int rssi = advertisedDevice.getRSSI();
    if (rssi > RSSI_THRESHOLD) {
      Serial.printf("Device found with RSSI: %d\n", rssi);
      tft.printf("Device found with RSSI: %d\n", rssi);

      BLEClient* pClient  = BLEDevice::createClient();
      pClient->connect(advertisedDevice.getAddress());

      BLERemoteService* pRemoteService = pClient->getService(SERVICE_UUID);

      if (pRemoteService == nullptr) {
        Serial.print("Failed to find service UUID: ");
        Serial.println(SERVICE_UUID);
        pClient->disconnect();
        return;
      }

      BLERemoteCharacteristic* pRemoteCharacteristic = pRemoteService->getCharacteristic(CHARACTERISTIC_UUID);

      if (pRemoteCharacteristic == nullptr) {
        Serial.print("Failed to find characteristic UUID: ");
        Serial.println(CHARACTERISTIC_UUID);
        pClient->disconnect();
        return;
      }

      if (pRemoteCharacteristic->canNotify()) {
        pRemoteCharacteristic->registerForNotify([](BLERemoteCharacteristic* pCharacteristic, uint8_t* pData, size_t length, bool isNotify) {
          Serial.print("Received notification: ");
          for (int i = 0; i < length; i++) {
            Serial.print(static_cast<char>(pData[i]));
          }
          Serial.println();
        });
      }

      delay(2000); // Keep the connection open
    }
  }
};

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.setTextSize(2);
  tft.setCursor(0, 0);

  BLEDevice::init("");

  // Determine if the device should act as a server or client based on MAC address
  uint64_t macAddress = ESP.getEfuseMac();
  //isServer = (macAddress & 0x01) == 0;

  if (isServer) {
    pServer = BLEDevice::createServer();
    pServer->setCallbacks(new MyServerCallbacks());

    BLEService *pService = pServer->createService(SERVICE_UUID);

    pCharacteristic = pService->createCharacteristic(
                        CHARACTERISTIC_UUID,
                        BLECharacteristic::PROPERTY_READ |
                        BLECharacteristic::PROPERTY_WRITE |
                        BLECharacteristic::PROPERTY_NOTIFY
                      );

    pCharacteristic->setCallbacks(new MyCallbacks());

    pCharacteristic->addDescriptor(new BLE2902());

    pService->start();

    pAdvertising = BLEDevice::getAdvertising();
    pAdvertising->addServiceUUID(SERVICE_UUID);
    pAdvertising->setScanResponse(true);
    pAdvertising->setMinPreferred(0x06);
    pAdvertising->setMaxPreferred(0x12);
    pAdvertising->start();
    Serial.println("Server is advertising...");
    tft.println("Server is advertising...");

  } else {
    BLEScan *pScan = BLEDevice::getScan();
    pScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
    pScan->setInterval(100);
    pScan->setWindow(99);
    pScan->setActiveScan(true);
    pScan->start(0, nullptr, false);
    Serial.println("Client is connecting...");
    tft.println("Client is connecting...");
  }
}

void loop() {
  if (isServer) {
    pCharacteristic->setValue((uint8_t*)msgDataServer, strlen(msgDataServer));
    pCharacteristic->notify();
    Serial.println("Pedestrian detected message sent...");
    tft.println("Pedestrian detected message sent...");
  } else {
    BLEDevice::getScan()->start(0, false);
    if (pCharacteristic != nullptr) {
      pCharacteristic->setValue((uint8_t*)msgDataClient, strlen(msgDataClient));
      pCharacteristic->notify();
      Serial.println("Car detected message sent...");
      tft.println("Car detected message sent...");
    }
  }
  delay(1000);
}
