#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <MPU6050_tockn.h>

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

const int buttonPin = 13;  // Pin waar de knop op is aangesloten
const int greenLED = 14;   // Groene LED
const int yellowLED = 21;  // Gele LED
const int redLED = 47;     // Rode LED
const int buzzer = 48;     // Buzzer

bool buttonState = false;
unsigned long buttonPressTime = 0;
bool yellowBuzzerActivated = false;

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

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
const unsigned long alertTime = 70000;

#define TCAADDR 0x70  // Adres van de TCA9548A multiplexer

void tcaSelect(uint8_t i) {
  if (i > 7) return;
  Wire1.beginTransmission(TCAADDR);
  Wire1.write(1 << i);
  Wire1.endTransmission();
}

void setup() {
    Serial.begin(115200);

    // Eerste I2C bus (Wire) voor de eerste sensor
    Wire.begin(6, 7); // SDA = GPIO6, SCL = GPIO7

    // Tweede I2C bus (Wire1) voor de tweede sensor
    Wire1.begin(18, 17); // SDA = GPIO18, SCL = GPIO17

    pinMode(buttonPin, OUTPUT); // Interne pull-up weerstand gebruiken
    pinMode(greenLED, OUTPUT);
    pinMode(yellowLED, OUTPUT);
    pinMode(redLED, OUTPUT);
    pinMode(buzzer, OUTPUT);

    // Initialiseer de MPU6050 sensoren zonder het adres expliciet op te geven
    mpu.begin();  // Eerste sensor
    mpu.calcGyroOffsets();
    tcaSelect(0);
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
    tft.println("Prototype v0.7");
}

void loop() {
  // drawWheelchair();
  // Sensor 1: frame
  mpu.update();
  float roll1 = mpu.getAngleY();

  // Sensor 2: zitvlak (sensor zit op de kop)
  tcaSelect(0);
  mpu1.update();
  float roll2 = -mpu1.getAngleY();  // Omdraaien

  // Sensor 3: rug (sensor zit op de kop)
  tcaSelect(1);
  mpu2.update();
  float roll3 = -mpu2.getAngleY();  // Omdraaien

  // Berekeningen
  float zithoek = roll2 - roll1 + 90;
  float rughoek = roll3 + zithoek;

  // Wis het schermgedeelte voor de nieuwe data
  tft.fillRect(140, 35, 80, 30, ILI9341_BLACK);
  tft.fillRect(140, 75, 80, 30, ILI9341_BLACK);
  tft.fillRect(140, 115, 80, 30, ILI9341_BLACK);

  // Toon pitch van de eerste sensor op het scherm
  tft.setCursor(20, 40);
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(3);
  tft.print("Frame: ");
  tft.println((int)roll1);

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

  checkStableAngle(zithoek);
  drawWheelchair();
  delay(200); // Wacht 200ms voor de volgende update
}

void checkStableAngle(float pitch1) {
    static float lastPitch = pitch1;
    if (abs(pitch1 - lastPitch) <= threshold) {
        if (!isStable) {
            stableStartTime = millis();
            isStable = true;
        }
        timerValue = (millis() - stableStartTime) / 1000;
        drawRectangle(timerValue);
        drawTimer(timerValue);
        if (millis() - stableStartTime >= alertTime) {
            // tone(buzzer, 1000, 500);
        }
    } else {
        isStable = false;
        timerValue = 0;
        drawRectangle(timerValue);
        drawTimer(timerValue);
        noTone(buzzer);
    }
    lastPitch = pitch1;
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

    if (seconds < 60) {
        color = ILI9341_GREEN;
        digitalWrite(greenLED, HIGH);
        digitalWrite(yellowLED, LOW);
        digitalWrite(redLED, LOW);
    } else if (seconds < 70) {
        color = ILI9341_ORANGE;
        digitalWrite(greenLED, LOW);
        digitalWrite(yellowLED, HIGH);
        digitalWrite(redLED, LOW);
    } else {
        color = ILI9341_RED;
        digitalWrite(greenLED, LOW);
        digitalWrite(yellowLED, LOW);
        digitalWrite(redLED, HIGH);
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