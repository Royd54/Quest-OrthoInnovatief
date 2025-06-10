#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>

RTC_DS3231 rtc;
const int SD_CS = 39;

uint16_t prevcolor = 1;
uint16_t color;

// Definieer het I2C-adres voor de MPU6050 (0x69 omdat AD0 is verbonden met VCC)
#define MPU6050_ADDR 0x69
// Definieer de registeradressen van de MPU6050
#define PWR_MGMT_1 0x6B
#define ACCEL_XOUT_H 0x3B
#define ACCEL_XOUT_L 0x3C
#define ACCEL_YOUT_H 0x3D
#define ACCEL_YOUT_L 0x3E
#define ACCEL_ZOUT_H 0x3F
#define ACCEL_ZOUT_L 0x40
// Definieer de pinnen voor het scherm
#define TFT_CS    10
#define TFT_DC    8
#define TFT_RST   9  

const int greenLED = 14;   // Groene LED
const int yellowLED = 21;  // Gele LED
const int redLED = 47;     // Rode LED
const int buzzer = 48;     // Buzzer

char dangerState = 0;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

SPIClass SPI1(HSPI);

// Eerste sensor op Wire (standaard adres 0x68)
MPU6050 mpu(Wire);        
// Tweede sensor op Wire1 (adres 0x69)
MPU6050 mpu1(Wire1);      
// Derde sensor op Wire1 (adres 0x68)
MPU6050 mpu2(Wire1);      

unsigned long stableStartTime = 0;
bool isStable = false;
int timerValue = 0;

const int threshold = 5;
const unsigned long alertTime = 90000;

unsigned long lastCheckTime = 0;
const unsigned long checkInterval = 5000; // 5 seconden in milliseconden

unsigned long lastSDcardCheckTime = 0;
const unsigned long SDcardcheckInterval = 60000;  // 60 seconden = 60.000 milliseconden

int zithoekMin = -1, zithoekMax = -1;
int rughoekMin = -1, rughoekMax = -1;

bool awaitingAck = false;
unsigned long ackWaitStart = 0;
// const unsigned long ackTimeout = 600000; // 10 minuten timeout
const unsigned long ackTimeout = 2000; // 1 seconde timeout
String lastSentLine = "";

#define TCAADDR 0x70  // Adres van de TCA9548A multiplexer

void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire1.beginTransmission(TCAADDR);
  Wire1.write(1 << i);
  Wire1.endTransmission();
}

void SDsetup(){
  if (!SD.begin(SD_CS, SPI1)) {
    Serial.println("SD-kaart fout");
    tft.println("SD FOUT");
    return;
  } else {
    delay(100);
    Serial.println("SD-kaart OK");
    File file = SD.open("/DATA.csv");

  if (!file) {
    Serial.println("Kan CSV niet openen.");
    Serial.println("");
    Serial.println("");
    Serial.println("");
    Serial.println("");
    SDsetup();
    return;
  }
    Serial.println("CSV-bestand geopend!");
  }
}


void setup() {
  Serial.begin(115200);
  // delay(500);
  // Serial2.begin(9600, SERIAL_8N1, 43, 44);  // RX, TX
  Serial2.begin(9600, SERIAL_8N1, 41, 40);
  Serial2.println("TEST VANUIT ESP32 S3");
  // delay(1000);

  SPI1.begin(5, 15, 16, -1);  // SCK, MISO, MOSI, SS was 45
  if (!SD.begin(SD_CS, SPI1)) {
    Serial.println("SD-kaart niet gevonden!");
  }
  else Serial.println("SD-kaart gevonden");

  // Eerste I2C bus (Wire) voor de eerste sensor
  Wire.begin(6, 7); // SDA = GPIO6, SCL = GPIO7

  // Tweede I2C bus (Wire1) voor de tweede sensor
  Wire1.begin(18, 17); // SDA = GPIO18, SCL = GPIO17
  delay(100);
  tcaSelect(6);
  delay(100);
  
  if (!rtc.begin(&Wire1)) {
    Serial.println("RTC niet gevonden!");
  }
  else{
    Serial.println("RTC gevonden");
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }

  pinMode(greenLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(buzzer, OUTPUT);

  // Initialiseer de MPU6050 sensoren zonder het adres expliciet op te geven
  tcaSelect(1);
  mpu.begin();  // Eerste sensor
  mpu.calcGyroOffsets();
  mpu1.begin(); // Tweede sensor
  mpu1.calcGyroOffsets();
  tcaSelect(0);
  mpu2.begin(); // Derde sensor
  mpu2.calcGyroOffsets();

  // Initialiseer het scherm
  tft.begin();
  tft.setRotation(2);
  tft.fillScreen(ILI9341_BLACK);

  // Titel tonen op het scherm
  tft.setCursor(10, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  // tft.println("Prototype v0.7");
  delay(1000);
}

void loop() {
  // Sensor 1: frame
  mpu.update();
  float roll1 = -mpu.getAngleY() + 9; //+9 offset

  // Sensor 2: zitvlak (sensor zit op de kop)
  tcaSelect(1);
  mpu1.update();
  float roll2 = mpu1.getAngleY() + 3; //+3 offset

  // Sensor 3: rug (sensor zit op de kop)
  tcaSelect(0);
  mpu2.update();
  float roll3 = -mpu2.getAngleY();  // Omdraaien

  // Berekeningen
  float zithoek = roll2 - roll1 + 90;
  float rughoek = roll3 - zithoek;

  // Wis het schermgedeelte voor de nieuwe data
  // tft.fillRect(140, 35, 80, 30, ILI9341_BLACK);
  tft.fillRect(90, 155, 133, 30, color);
  tft.fillRect(90, 195, 133, 30, color);

  // Toon pitch van de eerste sensor op het scherm
  tft.setCursor(20, 10);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("Verwachte hoeken");

  // tft.print("Frame: ");
  // tft.println((int)roll1);

  tft.setCursor(30, 130);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.print("Actuele hoeken");

  // Toon pitch van de tweede sensor op het scherm
  tft.setCursor(20, 200);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.print("Zit: ");
  tft.println((int)zithoek);

  // Toon pitch van de derde sensor op het scherm
  tft.setCursor(20, 160);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.print("Rug: ");
  tft.println((int)rughoek);
  
  checkStableAngle(zithoek, rughoek);

  // drawWheelchair();

  if (millis() - lastSDcardCheckTime >= SDcardcheckInterval) {
    lastSDcardCheckTime = millis();
    // Wis het schermgedeelte voor de nieuwe data
    // tft.fillRect(140, 35, 80, 30, ILI9341_BLACK);
    tft.fillRect(90, 35, 133, 30, color);
    tft.fillRect(90, 75, 133, 30, color);
    tft.fillRect(70, 255, 133, 30, color);
    SDKaartLezen();
    logDataToSDCard(zithoek, rughoek, roll1, dangerState);
    // readUartForACK();
    drawTime();
  }

  delay(200); // Wacht 200ms voor de volgende update
}

void readUartForACK(){
  // Check of er data is ontvangen op Serial2 (de UART2)
  if (Serial2.available()) {
    String incoming = Serial2.readStringUntil('\n');
    incoming.trim();
    Serial.print("UART Ontvangen: ");
    Serial.println(incoming);

    if (incoming == "ACK" && awaitingAck) {
      Serial.println("UART ACK ontvangen!");
      awaitingAck = false;
      verwijderTweedeRegel();
      Serial.println("REGEL VERWIJDERD");
    }
  }
}

#pragma pack(push, 1)
struct SensorData {
  uint8_t frameNumber;    // Deel 1: Frame nummer (0-255)
  int16_t  angle1;        // Deel 2: Hoek 1 (0-65535)
  int16_t  angle2;        // Deel 3: Hoek 2 
  int16_t  angle3;        // Deel 4: Hoek 3
  uint8_t status;         // Deel 5: Getal 0-2
  uint8_t hour;           // Tijd: Uur (0-23)
  uint8_t minute;         // Tijd: Minuut (0-59)
  uint8_t day;            // Datum: Dag (1-31)
  uint8_t month;          // Datum: Maand (1-12)
  uint16_t year;          // Datum: Jaar (2024=0x07E8)
};
#pragma pack(pop)

SensorData dataToSend = {
  0,         // frameNumber
  1,       // angle1
  2,       // angle2
  3,       // angle3
  1,          // status
  14,         // hour
  30,         // minute
  19,         // day
  5,          // month
  2025        // year
};

void sendDataUART(){
  Serial.printf("Datum: %02d-%02d-%04d %02d:%02d\n", dataToSend.day, dataToSend.month, dataToSend.year, dataToSend.hour, dataToSend.minute);
  Serial.print("Zithoek: ");
  Serial.println(dataToSend.angle1);
  Serial.print("Rughoek: ");
  Serial.println(dataToSend.angle2);
  Serial.print("FrameHoek: ");
  Serial.println(dataToSend.angle3);
  Serial.print("Status: ");
  Serial.println(dataToSend.status);
  if (!awaitingAck) {
    Serial2.write((uint8_t*)&dataToSend, sizeof(dataToSend));
    // lastSentLine = "Test regel om te sturen";  // Vervang door je eigen leesfunctie van SD-kaart
    // Serial2.println(lastSentLine);
    Serial.print("UART VERZONDEN ");
    // Serial.println(lastSentLine);
    // Serial.println("%s",(uint8_t*)&dataToSend, sizeof(dataToSend));
    awaitingAck = true;
    ackWaitStart = millis();
  } else {
    // Check timeout op ACK
    if (millis() - ackWaitStart > ackTimeout) {
      Serial.println("ACK niet ontvangen, probeer opnieuw");
      // Serial2.println(lastSentLine);
      Serial2.write((uint8_t*)&dataToSend, sizeof(dataToSend));
      ackWaitStart = millis();
    }
  }
}

void logDataToSDCard(float zit, float rug, float frameHoek,int dangerStatus) {
  DateTime now = rtc.now();
  // Bestand openen in "append"-modus
  File file = SD.open("/dataopslag.csv", FILE_APPEND);
  if (file.size() == 0) {
  file.println("Tijd;Zithoek;ZithoekMin;ZithoekMax;Rughoek;RughoekMin;RughoekMax;FrameHoek;LEDkleur");
  }
  if (file) {
    // Formatteer tijd als "YYYY-MM-DD HH:MM:SS"
    char tijdBuffer[20];
    sprintf(tijdBuffer, "%04d-%02d-%02d %02d:%02d:%02d",
            now.year(), now.month(), now.day(),
            now.hour(), now.minute(), now.second());

    // Schrijf de regel in CSV-formaat
    file.print(tijdBuffer);
    file.print(";");
    file.print(zit, 2);
    file.print(";");
    file.print(zithoekMin);
    file.print(";");
    file.print(zithoekMax);
    file.print(";");
    file.print(rug, 2);
    file.print(";");
    file.print(rughoekMin);
    file.print(";");
    file.print(rughoekMax);
    file.print(";");
    file.print(frameHoek, 2);
    file.print(";");
    file.print(dangerStatus);
    file.print(";");
    file.println();

    file.close();
    Serial.println("Data gelogd naar SD-kaart.");
    
    // leesEersteDataregelVanSD();
    // sendDataUART();
  } else {
    Serial.println("Kan DATA.csv niet openen om te schrijven.");
  }
}

void leesEersteDataregelVanSD() {
  File file = SD.open("/dataopslag.csv");

  if (!file) {
    Serial.println("Kan dataopslag.csv niet openen.");
    return;
  }

  // Sla de headerregel over
  file.readStringUntil('\n');

  if (!file.available()) {
    Serial.println("Geen datarijen gevonden.");
    file.close();
    return;
  }

  // Lees tweede regel (eerste echte data)
  String regel = file.readStringUntil('\n');
  file.close();

  regel.trim();
  regel.replace('\t', ';');
  regel.replace(',', '.');

  // Split in velden
  const int AANTAL_VELDEN = 9;
  String velden[AANTAL_VELDEN];
  int start = 0;

  for (int i = 0; i < AANTAL_VELDEN; i++) {
    int einde = regel.indexOf(';', start);
    if (einde == -1 && i < AANTAL_VELDEN - 1) {
      Serial.println("CSV-regel heeft te weinig velden.");
      return;
    }
    velden[i] = regel.substring(start, (einde == -1) ? regel.length() : einde);
    velden[i].trim();
    start = einde + 1;
  }

  // Tijd parsen (YYYY-MM-DD HH:MM:SS)
  String tijd = velden[0];
  if (tijd.length() >= 19) {
    dataToSend.year   = tijd.substring(0, 4).toInt();
    dataToSend.month  = tijd.substring(5, 7).toInt();
    dataToSend.day    = tijd.substring(8, 10).toInt();
    dataToSend.hour   = tijd.substring(11, 13).toInt();
    dataToSend.minute = tijd.substring(14, 16).toInt();
  } else {
    Serial.println("Tijdformaat ongeldig.");
  }

  dataToSend.frameNumber = 1;
  dataToSend.angle1 = (int16_t)velden[1].toFloat(); // zithoek
  dataToSend.angle2 = (int16_t)velden[4].toFloat(); // rughoek
  dataToSend.angle3 = (int16_t)velden[7].toFloat(); // framehoek
  dataToSend.status = velden[8].toInt();   // dangerStatus / LEDkleur
}

void verwijderTweedeRegel() {
  File origineel = SD.open("/dataopslag.txt");
  if (!origineel) {
    Serial.println("Kan origineel bestand niet openen!");
    return;
  }
  
  File tijdelijk = SD.open("/temp.txt", FILE_WRITE);
  if (!tijdelijk) {
    Serial.println("Kan tijdelijk bestand niet openen!");
    origineel.close();
    return;
  }

  int regelNummer = 0;

  while (origineel.available()) {
    String regel = origineel.readStringUntil('\n');
    regel.trim();  // Spaties en carriage returns weg

    if (regel.length() == 0) continue; // Skip lege regels

    if (regelNummer != 1) {  // Skip de tweede regel (regel 1)
      tijdelijk.println(regel);
    }
    regelNummer++;
  }

  origineel.close();
  tijdelijk.close();

  SD.remove("/dataopslag.txt");
  SD.rename("/temp.txt", "/dataopslag.txt");
}

void SDKaartLezen(){
  tcaSelect(6);
  DateTime now = rtc.now();
  float ingesteldeTijd = now.hour() + now.minute() / 60.0;

  SD.begin(SD_CS);
  File file = SD.open("/DATA.csv");

  if (!file) {
    Serial.println("Kan CSV niet openen.");
    Serial.println("");
    Serial.println("");
    Serial.println("");
    Serial.println("");
    SDsetup();
    return;
  }

  Serial.println("CSV-bestand geopend!");

  // Sla header over
  file.readStringUntil('\n');

  float vorigeTijd = -1;
  zithoekMin = -1;
  zithoekMax = -1;
  rughoekMin = -1;
  rughoekMax = -1;

  while (file.available()) {
    String regel = file.readStringUntil('\n');
    regel.trim();

    if (regel.length() == 0) continue;

    // Verwerk CSV-regel
    regel.replace('\t', ';');
    regel.replace(',', '.');
    regel.replace(" ", "");

    int i1 = regel.indexOf(';');
    int i2 = regel.indexOf(';', i1 + 1);
    if (i1 == -1 || i2 == -1) continue;

    float tijd = regel.substring(0, i1).toFloat();
    String zithoekStr = regel.substring(i1 + 1, i2);
    String rughoekStr = regel.substring(i2 + 1);

    if (tijd > ingesteldeTijd) break;

    // Parse zithoek
    int slash1 = zithoekStr.indexOf('/');
    if (slash1 > 0) {
      zithoekMin = zithoekStr.substring(0, slash1).toInt();
      zithoekMax = zithoekStr.substring(slash1 + 1).toInt();
    }

    // Parse rughoek
    int slash2 = rughoekStr.indexOf('/');
    if (slash2 > 0) {
      rughoekMin = rughoekStr.substring(0, slash2).toInt();
      rughoekMax = rughoekStr.substring(slash2 + 1).toInt();
    }

    vorigeTijd = tijd;
  }

  file.close();

  char combinedString[6];

  if (vorigeTijd != -1) {
    tft.setTextSize(3);
    tft.setCursor(20, 40);
    tft.setTextColor(ILI9341_WHITE);
    tft.print("Zit:");
    sprintf(combinedString, "%d/%d", zithoekMin, zithoekMax);
    tft.println(combinedString);

    tft.setCursor(20, 80);
    tft.setTextColor(ILI9341_WHITE);
    tft.print("Rug:");
    sprintf(combinedString, "%d/%d", rughoekMin, rughoekMax);
    tft.println(combinedString);
    tft.setTextSize(2);
  } else {
    Serial.println("Geen instellingen gevonden voor deze tijd.");
  }
}

void checkStableAngle(float pitch1, float pitch2) {
    static float lastPitch1 = pitch1;
    static float lastPitch2 = pitch2;
    static unsigned long lastCompareTime = 0;
    static const unsigned long compareInterval = 5000; // elke 5 seconden
    static unsigned long stableStartTime = 0;
    static bool isStable = false;

    // Check alleen elke 5 seconden of de hoeken veranderd zijn
    if (millis() - lastCompareTime >= compareInterval) {
        lastCompareTime = millis();

        if (((pitch1 < zithoekMax) && (pitch1 > zithoekMin)) && ((pitch2 < rughoekMax) && (pitch2 > rughoekMin))) {
            // if (!isStable) {
            //     stableStartTime = millis();
            //     isStable = true;
            // }
            stableStartTime = millis(); // reset starttijd
            timerValue = 0;
            drawRectangle(timerValue);
            // drawTimer(timerValue);
            noTone(buzzer);

        } else {
            // isStable = false;
            // stableStartTime = millis(); // reset starttijd

            timerValue = (millis() - stableStartTime) / 1000;
            drawRectangle(timerValue);
            // drawTimer(timerValue);
            if (millis() - stableStartTime >= alertTime) {
               tone(buzzer, 1000, 500);
            }

        }

        lastPitch1 = pitch1;
        lastPitch2 = pitch2;
    }

    // Timer telt gewoon door als het stabiel is
    // if (isStable) {
    //     timerValue = (millis() - stableStartTime) / 1000;
    //     drawRectangle(timerValue);
    //     // drawTimer(timerValue);
    //     if (millis() - stableStartTime >= alertTime) {
    //        tone(buzzer, 1000, 500);
    //     }
    // } else {
    //     timerValue = 0;
    //     drawRectangle(timerValue);
    //     // drawTimer(timerValue);
    //     noTone(buzzer);
    // }
}

void drawTime(){
    tcaSelect(6);
    DateTime now = rtc.now();
    tft.setCursor(20, 200);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(3);

    char timeString[6];  // Buffer voor "MM:SS"
    sprintf(timeString, "%d:%d", now.hour(), now.minute());

    int screenWidth = 240;  // Breedte van het scherm
    int textWidth = strlen(timeString) * 18; // 6 pixels per char * text size (2)
    int xCentered = (screenWidth - textWidth) / 2;

    tft.setCursor(xCentered, 260);
    tft.setTextColor(ILI9341_WHITE);
    tft.println(timeString);
    // drawClock(now.hour(), now.minute());
}

uint16_t color565(uint8_t r, uint8_t g, uint8_t b) {
  return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3);
}

void drawRectangle(int seconds) {
    if (seconds < ((alertTime/1000)/2)) {
        color = color565(39, 140, 21);
        digitalWrite(greenLED, HIGH);
        digitalWrite(yellowLED, LOW);
        digitalWrite(redLED, LOW);
        dangerState = 0;
    } else if (seconds < (alertTime/1000)) {
        color = ILI9341_ORANGE;
        digitalWrite(greenLED, LOW);
        digitalWrite(yellowLED, HIGH);
        digitalWrite(redLED, LOW);
        dangerState = 1;
    } else {
        color = ILI9341_RED;
        digitalWrite(greenLED, LOW);
        digitalWrite(yellowLED, LOW);
        digitalWrite(redLED, HIGH);
        dangerState = 2;
    }
    if(color != prevcolor){
      // tft.fillRect(10, 170, 220, 140, color);
      tft.fillScreen(color);
      prevcolor = color;
      int thickness = 3;  // Dikte van de rand

      for (int i = 0; i < thickness; i++) {
        tft.drawRoundRect(10 + i, 2 + i, 220 - 2 * i, 115 - 2 * i, 10, ILI9341_WHITE);
      }
      tft.fillRoundRect(14, 6, 212, 20, 10, ILI9341_BLACK);

      for (int i = 0; i < thickness; i++) {
        tft.drawRoundRect(10 + i, 123 + i, 220 - 2 * i, 120 - 2 * i, 10, ILI9341_WHITE);
      }
      tft.fillRoundRect(14, 126, 212, 20, 10, ILI9341_BLACK);

      SDKaartLezen();
      tft.fillRect(70, 255, 133, 30, color);
      drawTime();
    }else{
      // tft.fillScreen(prevcolor);
        // tft.fillRect(40, 255, 140, 30, color);
    }
}

void drawWheelchair() {
    // tft.fillScreen(ILI9341_BLACK);

    // Schaalfactor en offset voor centrering
    float scale = 0.5; // Vergroot de tekening
    int xOffset = 130; // X-positie van het midden
    int yOffset = 220; // Y-positie van het midden

    // Grote achterwiel
    tft.drawCircle(xOffset - 45 * scale, yOffset + 10 * scale, 40 * scale, ILI9341_WHITE);
    tft.drawCircle(xOffset - 45 * scale, yOffset + 10 * scale, 38 * scale, ILI9341_WHITE);
    tft.fillCircle(xOffset - 45 * scale, yOffset + 10 * scale, 5 * scale, ILI9341_WHITE);

    // Kleine voorwiel
    tft.drawCircle(xOffset + 20 * scale, yOffset + 30 * scale, 15 * scale, ILI9341_WHITE);
    tft.fillCircle(xOffset + 20 * scale, yOffset + 30 * scale, 3 * scale, ILI9341_WHITE);

    // Zitting
    tft.fillRect(xOffset - 50 * scale, yOffset - 20 * scale, 60 * scale, 10 * scale, ILI9341_WHITE);
    tft.fillRect(xOffset - 50 * scale, yOffset - 70 * scale, 10 * scale, 60 * scale, ILI9341_WHITE);

    // Frame
    tft.drawLine(xOffset - 45 * scale, yOffset - 20 * scale, xOffset - 45 * scale, yOffset + 10 * scale, ILI9341_WHITE);
    tft.drawLine(xOffset + 10 * scale, yOffset - 20 * scale, xOffset + 20 * scale, yOffset + 30 * scale, ILI9341_WHITE);

    // Handvat
    tft.drawLine(xOffset - 50 * scale, yOffset - 70 * scale, xOffset - 70 * scale, yOffset - 80 * scale, ILI9341_WHITE);
}

void drawClock(float hour, float minute) {
  // tft.fillScreen(ILI9341_BLACK); // Optioneel: wissen scherm

  // Klok parameters
  float scale = 0.5;
  int xCenter = 30;
  int yCenter = 265;
  int radius = 60 * scale;

  // Teken klokrand
  tft.drawCircle(xCenter, yCenter, radius, ILI9341_WHITE);
  tft.drawCircle(xCenter, yCenter, radius - 2, ILI9341_WHITE);

  // Uuraanduidingen
  for (int i = 0; i < 12; i++) {
    float angle = i * 30 * DEG_TO_RAD;
    int x1 = xCenter + cos(angle) * (radius - 5);
    int y1 = yCenter + sin(angle) * (radius - 5);
    int x2 = xCenter + cos(angle) * (radius - 10);
    int y2 = yCenter + sin(angle) * (radius - 10);
    tft.drawLine(x1, y1, x2, y2, ILI9341_WHITE);
  }

  // Wijzers tekenen
  // Uurwijzer
  float hourAngle = ((hour + minute / 60.0) / 12.0) * 2 * PI - PI/2;
  int hourX = xCenter + cos(hourAngle) * (radius * 0.5);
  int hourY = yCenter + sin(hourAngle) * (radius * 0.5);
  tft.drawLine(xCenter, yCenter, hourX, hourY, ILI9341_WHITE);

  // Minuutwijzer
  float minuteAngle = (minute / 60.0) * 2 * PI - PI/2;
  int minX = xCenter + cos(minuteAngle) * (radius * 0.75);
  int minY = yCenter + sin(minuteAngle) * (radius * 0.75);
  tft.drawLine(xCenter, yCenter, minX, minY, ILI9341_WHITE);

  // Middelpunt
  tft.fillCircle(xCenter, yCenter, 3, ILI9341_WHITE);
}