#include <Wire.h>
#include <RTClib.h>
#include <SPI.h>
#include <SD.h>

RTC_DS3231 rtc;
const int chipSelect = 10;

void setup() {
  Serial.begin(9600);
  while (!Serial);

  if (!rtc.begin()) {
    Serial.println("RTC niet gevonden!");
    while (1);
  }

  if (!SD.begin(chipSelect)) {
    Serial.println("SD-kaart niet gevonden!");
    while (1);
  }
}

void loop() {
  DateTime now = rtc.now();
  float currentTime = now.hour() + now.minute() / 60.0;

  Serial.print("Huidige tijd: ");
  Serial.println(currentTime, 2);

  File file = SD.open("DATA.csv");
  if (!file) {
    Serial.println("Kan CSV niet openen.");
    delay(1000);
    return;
  }

  // Header overslaan
  file.readStringUntil('\n');

  float vorigeTijd = -1;
  char zithoek[12];
  char rughoek[12];
  bool gevonden = false;

  while (file.available()) {
    String line = file.readStringUntil('\n');
    line.trim();
    if (line.length() == 0) continue;

    int s1 = line.indexOf(';');
    int s2 = line.indexOf(';', s1 + 1);
    if (s1 == -1 || s2 == -1) continue;

    float tijd = line.substring(0, s1).toFloat();
    String zStr = line.substring(s1 + 1, s2);
    String rStr = line.substring(s2 + 1);

    if (currentTime < tijd) {
      if (vorigeTijd != -1) {
        Serial.println("Instellingen gevonden:");
        Serial.print("Tijd tussen: ");
        Serial.print(vorigeTijd); Serial.print(" en "); Serial.println(tijd);
        Serial.print("Zithoek: "); Serial.println(zithoek);
        Serial.print("Rughoek: "); Serial.println(rughoek);
        gevonden = true;
      }
      break;
    }

    vorigeTijd = tijd;
    zStr.toCharArray(zithoek, sizeof(zithoek));
    rStr.toCharArray(rughoek, sizeof(rughoek));
  }

  file.close();

  if (!gevonden) {
    Serial.println("Geen instellingen gevonden voor deze tijd.");
  }

  delay(6000); // Wacht 1 minuut
}
