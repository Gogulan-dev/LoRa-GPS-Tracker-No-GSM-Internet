#include <SoftwareSerial.h>
#include <TinyGPS++.h>
#include <SPI.h>
#include <LoRa.h>

#define RX_PIN 4
#define TX_PIN 3

#define SS 10
#define RST 9
#define DIO0 2

SoftwareSerial gpsSerial(RX_PIN, TX_PIN);
TinyGPSPlus gps;

void setup() {
  Serial.begin(9600);
  gpsSerial.begin(9600);
  
  SPI.begin();
  LoRa.setPins(SS, RST, DIO0);
  
  if (!LoRa.begin(433E6)) {
    Serial.println("LoRa init failed. Check connections.");
    while (1);
  }
  
  Serial.println("LoRa & GPS Transmitter Started...");
}

void loop() {
  while (gpsSerial.available()) {
    char data = gpsSerial.read();
    if (gps.encode(data)) {
      if (gps.location.isValid()) {
        float latitude = gps.location.lat();
        float longitude = gps.location.lng();

        LoRa.beginPacket();
        LoRa.print(latitude, 6);
        LoRa.print(",");
        LoRa.print(longitude, 6);
        LoRa.endPacket();

        Serial.print("Sent: ");
        Serial.print(latitude, 6);
        Serial.print(", ");
        Serial.println(longitude, 6);
      } else {
        Serial.println("Waiting for GPS signal...");
      }
    }
  }
}
