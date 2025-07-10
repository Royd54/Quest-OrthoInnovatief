#define LMIC_DEBUG_LEVEL 2
#define LMIC_PRINTF_TO Serial

#include <Arduino.h>
#include <lmic.h>
#include <hal/hal.h>
#include <SPI.h>

#include <SoftwareSerial.h>

SoftwareSerial mySerial(D4, D3);    // RX, TX (omgekeerd aansluiten aan ESP32-S3)

bool isJoined = false;              // Geeft aan of we verbonden zijn
uint32_t lastSendTime = 0;          // Laatste verzendtijd
const uint32_t TX_INTERVAL = 60000; // 60 seconden interval

// OTAA-gegevens (vervang met je eigen keys)
static const u1_t PROGMEM DEVEUI[8] = { 0x1D, 0x34, 0x1B, 0x00, 0x00, 0xAC, 0x59, 0x00 }; 
static const u1_t PROGMEM APPEUI[8] = { 0x9A, 0x0D, 0x01, 0x00, 0x00, 0xAC, 0x59, 0x00 };
static const u1_t PROGMEM APPKEY[16] = { 0x3C, 0x62, 0x79, 0xCD, 0xA0, 0xE0, 0x90, 0xB5, 0xE9, 0x81, 0xC5, 0x3D, 0x47, 0xD8, 0x98, 0xFD };

void os_getDevEui (u1_t* buf) { memcpy_P(buf, DEVEUI, 8); }
void os_getArtEui (u1_t* buf) { memcpy_P(buf, APPEUI, 8); }
void os_getDevKey (u1_t* buf) { memcpy_P(buf, APPKEY, 16); }

// Definieer de juiste pinnen voor de SX1276
const lmic_pinmap lmic_pins = {
  .nss = 15,    // D8 (GPIO15)
  .rxtx = LMIC_UNUSED_PIN,
  .rst = 16,     // D1 (GPIO5)
  .dio = {5, 4, LMIC_UNUSED_PIN}, // D2 (GPIO4), D3 (GPIO0)
};


#pragma pack(push, 1)
struct SensorData {
  uint8_t frameNumber;    // Deel 1: Frame nummer 
  int16_t  angle1;        // Deel 2: Hoek 1 
  int16_t  angle2;        // Deel 3: Hoek 2 
  int16_t  angle3;        // Deel 4: Hoek 3
  uint8_t status;         // Deel 5: Getal 0-2
  uint8_t hour;           // Tijd: Uur (0-23)
  uint8_t minute;         // Tijd: Minuut (0-59)
  uint8_t day;            // Datum: Dag (1-31)
  uint8_t month;          // Datum: Maand (1-12)
  uint16_t year;          // Datum: Jaar
};
#pragma pack(pop)

SensorData receivedData;
const int dataSize = sizeof(SensorData);
uint8_t buffer[dataSize];
int bufferIndex = 0;

void onEvent (ev_t ev) {
  Serial.print(os_getTime());
  Serial.print(": ");
  switch(ev) {
    case EV_JOINING:
      Serial.println(F("Joining..."));
      isJoined = false; // Reset join status
      break;
    case EV_JOINED:
      Serial.println(F("Joined!"));
      isJoined = true; // Verbinding geslaagd
      break;
    case EV_TXCOMPLETE:
      Serial.println(F("TX Complete"));
      if (LMIC.txrxFlags & TXRX_ACK) {
        Serial.println("ACK ontvangen!");
        // Stuur ACK
        mySerial.println("ACK");
        Serial.println("UART ACK verzonden");
      } else {
        Serial.println("Geen ACK ontvangen (herversturen...)");
      }
      break;
          case EV_SCAN_TIMEOUT:   // Event 20
      Serial.println(F("Scan timeout"));
      break;
    case EV_BEACON_FOUND:   // Event 17
      Serial.println(F("Beacon found"));
      break;
    case EV_JOIN_TXCOMPLETE:
      Serial.println(F("Join request TX complete"));
      break;
    case EV_REJOIN_FAILED:
      Serial.println(F("Rejoin failed"));
      isJoined = false; // Reset join status
      break;
    default:
      Serial.print(F("Unknown event: "));
      Serial.println(ev);
      break;
  }
}

void setup() {
  Serial.begin(115200);
  mySerial.begin(9600);
  while (!Serial);
  
  // SPI initialisatie
  SPI.begin();
  SPI.setFrequency(4E6);   // 4 MHz (max voor SX1276)
  
  os_init();
  LMIC_reset();
  for (int i = 1; i < 9; i++) LMIC_disableChannel(i); // Laat alleen kanaal 0 actief
    LMIC_setAdrMode(0);                // ADR UIT
  LMIC_setDrTxpow(DR_SF7, 14);         // SF7 voor snellere joins
  LMIC_setClockError(MAX_CLOCK_ERROR * 5 / 100); // 5% clock error tolerantie
  LMIC.dn2Dr = DR_SF7;                 // Downlink data rate

  LMIC_setLinkCheckMode(0);
  LMIC_startJoining();
}

void sendSensorData() {
  static uint8_t frameCounter = 0; // Frame nummer

  if (isJoined) {
    LMIC_setTxData2(1, (uint8_t*)&receivedData, sizeof(receivedData), 1);
    Serial.println("Data verstuurd!");
  }
}

void loop() {
  os_runloop_once();
  delay(1); // Voorkom watchdog resets

  if (isJoined && (millis() - lastSendTime > TX_INTERVAL)) {
    sendSensorData();
    lastSendTime = millis();
  }

  readUART();
}

void readUART(){
  while (mySerial.available()) {  
    buffer[bufferIndex++] = mySerial.read();  // Lees byte
    
    if (bufferIndex == dataSize) {  // Hele struct ontvangen?
      memcpy(&receivedData, buffer, dataSize);

      // Data gebruiken
      Serial.print("FrameNummer: "); Serial.println(receivedData.frameNumber);
      Serial.print("Zithoek: "); Serial.println(receivedData.angle1);
      Serial.print("Rughoek: "); Serial.println(receivedData.angle2);
      Serial.print("FrameHoek: "); Serial.println(receivedData.angle3);
      Serial.print("DangerStatus: "); Serial.println(receivedData.status);
      Serial.print("Datum: ");
      Serial.print(receivedData.hour); Serial.print(":");
      Serial.print(receivedData.minute); Serial.print(" ");
      Serial.print(receivedData.day); Serial.print("-");
      Serial.print(receivedData.month); Serial.print("-");
      Serial.println(receivedData.year);

      // Reset buffer index om volgende frame te ontvangen
      bufferIndex = 0;
      // mySerial.println("ACK");
      // Serial.println("UART ACK verzonden");
    }
  }
}