#include <Wire.h>
#include <RTClib.h>

RTC_DS3231 rtc;

void setup () {
  Serial.begin(9600);
  if (!rtc.begin()) {
    Serial.println("RTC niet gevonden!");
    while (1);
  }

  // Eenmalig tijd instellen — alleen nodig als je RTC nog niet goed loopt!
  // Stel in op tijd van compilatie:
  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  Serial.println("Tijd ingesteld!");
}

void loop () {
  DateTime now = rtc.now();
  Serial.print("Huidige tijd: ");
  Serial.print(now.hour());
  Serial.print(":");
  Serial.print(now.minute());
  Serial.print(":");
  Serial.println(now.second());
  delay(1000);
}
