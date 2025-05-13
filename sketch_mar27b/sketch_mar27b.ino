#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <MPU6050_tockn.h>
#include <SPI.h>
#include <RTClib.h>
#include <SD.h>

RTC_DS3231 rtc;
const int SD_CS = 39;

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

  tcaSelect(2);

  if (!rtc.begin()) {
    Serial.println("RTC niet gevonden!");
  }
  else if(rtc.begin()){Serial.println("RTC gevonden");}

  rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  pinMode(greenLED, OUTPUT);
  pinMode(yellowLED, OUTPUT);
  pinMode(redLED, OUTPUT);
  pinMode(buzzer, OUTPUT);

  // Initialiseer de MPU6050 sensoren zonder het adres expliciet op te geven
  tcaSelect(0);
  mpu.begin();  // Eerste sensor
  mpu.calcGyroOffsets();
  mpu1.begin(); // Tweede sensor
  mpu1.calcGyroOffsets();
  tcaSelect(1);
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
  tcaSelect(0);
  mpu1.update();
  float roll2 = mpu1.getAngleY() + 3; //+3 offset

  // Sensor 3: rug (sensor zit op de kop)
  tcaSelect(1);
  mpu2.update();
  float roll3 = -mpu2.getAngleY();  // Omdraaien

  // Berekeningen
  float zithoek = roll2 - roll1 + 90;
  float rughoek = roll3 - zithoek;

  // Wis het schermgedeelte voor de nieuwe data
  // tft.fillRect(140, 35, 80, 30, ILI9341_BLACK);
  tft.fillRect(140, 75, 80, 30, ILI9341_BLACK);
  tft.fillRect(140, 115, 80, 30, ILI9341_BLACK);

  // Toon pitch van de eerste sensor op het scherm
  tft.setCursor(20, 40);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  // tft.print("Frame: ");
  // tft.println((int)roll1);

  // Toon pitch van de tweede sensor op het scherm
  tft.setCursor(20, 80);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.print("Zit:   ");
  tft.println((int)zithoek);

  // Toon pitch van de derde sensor op het scherm
  tft.setCursor(20, 120);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.print("Rug:   ");
  tft.println((int)rughoek);

  checkStableAngle(zithoek, rughoek);

  drawWheelchair();

    if (millis() - lastSDcardCheckTime >= SDcardcheckInterval) {
    lastSDcardCheckTime = millis();
    SDKaartLezen();
    logDataToSDCard(zithoek, rughoek, dangerState);
  }

  delay(200); // Wacht 200ms voor de volgende update
}

void logDataToSDCard(float zit, float rug, int dangerStatus) {
  DateTime now = rtc.now();
  // Bestand openen in "append"-modus
  File file = SD.open("/dataopslag.csv", FILE_APPEND);
  if (file.size() == 0) {
  file.println("Tijd;Zithoek;Rughoek;LEDkleur");
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
    file.print(rug, 2);
    file.print(";");
    file.print(dangerStatus);
    file.println();

    file.close();
    Serial.println("Data gelogd naar SD-kaart.");
  } else {
    Serial.println("Kan DATA.csv niet openen om te schrijven.");
  }
}

void SDKaartLezen (){
  tcaSelect(2);
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
  int zithoekMin = -1, zithoekMax = -1;
  int rughoekMin = -1, rughoekMax = -1;

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

    char combinedString[6];  // Buffer voor "MM:SS"
    tft.setCursor(20, 10);
    tft.setTextColor(ILI9341_WHITE);
    // tft.println(now.hour() + now.minute());
    sprintf(combinedString, "%d:%d", now.hour(), now.minute());
    tft.println(combinedString);

  if (vorigeTijd != -1) {
    // Serial.print("Tijd: ");
    // Serial.println(vorigeTijd);

    tft.setTextSize(2);
    // Serial.print("Zithoek min/max: ");
    // Serial.print(zithoekMin); Serial.print("/"); Serial.println(zithoekMax);
    tft.setCursor(20, 40);
    tft.setTextColor(ILI9341_WHITE);

    sprintf(combinedString, "%d/%d", zithoekMin, zithoekMax);
    tft.println(combinedString);

    // Serial.print("Rughoek min/max: ");
    // Serial.print(rughoekMin); Serial.print("/"); Serial.println(rughoekMax);
    tft.setCursor(100, 40);
    tft.setTextColor(ILI9341_WHITE);
    sprintf(combinedString, "%d/%d", rughoekMin, rughoekMax);
    tft.println(combinedString);
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

        if (abs(pitch1 - lastPitch1) <= threshold && abs(pitch2 - lastPitch2) <= threshold) {
            if (!isStable) {
                stableStartTime = millis();
                isStable = true;
            }
        } else {
            isStable = false;
            stableStartTime = millis(); // reset starttijd
        }

        lastPitch1 = pitch1;
        lastPitch2 = pitch2;
    }

    // Timer telt gewoon door als het stabiel is
    if (isStable) {
        timerValue = (millis() - stableStartTime) / 1000;
        drawRectangle(timerValue);
        drawTimer(timerValue);
        if (millis() - stableStartTime >= alertTime) {
           // tone(buzzer, 1000, 500);
        }
    } else {
        timerValue = 0;
        drawRectangle(timerValue);
        drawTimer(timerValue);
        noTone(buzzer);
    }
}

void drawTimer(int seconds) {
    tft.setCursor(20, 200);
    tft.setTextColor(ILI9341_WHITE);
    tft.setTextSize(3);
    int minutes = seconds / 60;
    int remainingSeconds = seconds % 60;

    char timeString[6];  // Buffer voor "MM:SS"
    sprintf(timeString, "%02d:%02d", minutes, remainingSeconds);

    int screenWidth = 240;  // Breedte van het scherm
    int textWidth = strlen(timeString) * 18; // 6 pixels per char * text size (2)
    int xCentered = (screenWidth - textWidth) / 2;

    tft.setCursor(xCentered, 260);
    tft.setTextColor(ILI9341_WHITE);
    tft.println(timeString);
}

    uint16_t prevcolor;
void drawRectangle(int seconds) {
    uint16_t color;

    if (seconds < ((alertTime/1000)/2)) {
        color = ILI9341_GREEN;
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
        tft.fillRect(10, 170, 220, 140, color);
    }else{
        tft.fillRect(40, 255, 140, 30, color);
    }
    prevcolor = color;
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