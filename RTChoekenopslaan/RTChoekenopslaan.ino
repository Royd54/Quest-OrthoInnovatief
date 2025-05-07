#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

RTC_DS3231 rtc;

const int chipSelect = 10; // Pas aan als jouw SD-module een andere CS pin gebruikt
const unsigned long interval = 10000; // Interval in ms (bijv. 10 sec)
unsigned long previousMillis = 0;

File dataFile;

void setup() {
  Serial.begin(9600);
  Wire.begin();

  // RTC starten
  if (!rtc.begin()) {
    Serial.println("RTC niet gevonden!");
    while (1);
  }

  // SD-kaart starten
  if (!SD.begin(chipSelect)) {
    Serial.println("SD-kaart niet gevonden!");
    while (1);
  }

  // Check of bestand al bestaat, zo niet dan koptekst toevoegen
  if (!SD.exists("test2.csv")) {
    dataFile = SD.open("test2.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.println("Tijd;Zithoek;Rughoek");
      dataFile.close();
    }
  }
}

void loop() {
  unsigned long currentMillis = millis();

  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis;

    // Tijd ophalen van RTC
    DateTime now = rtc.now();

    // Simuleer zithoek en rughoek
    float zithoek = random(800, 1200) / 10.0; // bijv. tussen 80.0 en 120.0
    float rughoek = random(700, 1100) / 10.0;

    // Tijd in leesbaar formaat
    char tijdBuffer[20];
    sprintf(tijdBuffer, "%04d-%02d-%02d %02d:%02d",
            now.year(), now.month(), now.day(), now.hour(), now.minute());

    // Log naar bestand met ; als scheidingsteken
    dataFile = SD.open("test2.csv", FILE_WRITE);
    if (dataFile) {
      dataFile.print(tijdBuffer);
      dataFile.print(";");
      dataFile.print(zithoek, 1); // één decimaal
      dataFile.print(";");
      dataFile.println(rughoek, 1);
      dataFile.close();
      Serial.println("Gegevens opgeslagen.");
    } else {
      Serial.println("Kan bestand niet openen!");
    }
  }
}
