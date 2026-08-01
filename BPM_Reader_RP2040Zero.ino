#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "MAX30105.h"
#include "heartRate.h"

// RP2040 Zero Onboard NeoPixel Configuration
#define NEOPIXEL_PIN  16
#define NUM_PIXELS    1
Adafruit_NeoPixel rgbLed(NUM_PIXELS, NEOPIXEL_PIN, NEO_GRB + NEO_KHZ800);

// OLED configuration on Wire (I2C0)
#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET    -1
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// MAX30102 configuration on Wire1 (I2C1)
MAX30105 particleSensor;

// Constants & Timing
const unsigned long READ_DURATION_MS   = 20000; // 20 Seconds reading window
const unsigned long RESULT_HOLD_MS    = 10000; // 10 Seconds display hold window
const long FINGER_THRESHOLD           = 20000; // IR threshold for finger detection

// Helper Declarations
void showIdleScreen();
void showLiveReading(int currentBpm, int secondsLeft);
void showAverageResult(int avgBpm);
void updateNeoPixelFade();
void setLedColor(uint8_t r, uint8_t g, uint8_t b);

void setup() {
  Serial.begin(115200);

  // Initialize NeoPixel
  rgbLed.begin();
  rgbLed.setBrightness(255);
  setLedColor(0, 0, 0);

  // Initialize Wire (I2C0) for OLED
  Wire.begin(); 
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
    Serial.println(F("OLED allocation failed"));
    for (;;);
  }

  display.clearDisplay();
  display.setTextSize(1);
  display.setTextColor(SSD1306_WHITE);
  display.setCursor(0, 20);
  display.println(F("Initializing..."));
  display.display();

  // Initialize Wire1 (I2C1) for MAX30102
  Wire1.begin(); 
  if (!particleSensor.begin(Wire1, I2C_SPEED_FAST)) {
    display.clearDisplay();
    display.setTextSize(1);
    display.setCursor(0, 20);
    display.println(F("MAX30102 not found!"));
    display.display();
    while (1);
  }

  // MAX30102 Setup
  byte ledBrightness = 0x1F;
  byte sampleAverage = 4;
  byte ledMode       = 2;
  int sampleRate     = 400;
  int pulseWidth     = 411;
  int adcRange       = 4096;

  particleSensor.setup(ledBrightness, sampleAverage, ledMode, sampleRate, pulseWidth, adcRange);
  particleSensor.enableDIETEMPRDY(); 
}

void loop() {
  // -------------------------------------------------------------
  // STAGE 1: IDLE - WAIT FOR FINGER
  // -------------------------------------------------------------
  setLedColor(0, 0, 0);
  showIdleScreen();

  long irValue = particleSensor.getIR();
  
  if (irValue > FINGER_THRESHOLD) {
    // -------------------------------------------------------------
    // STAGE 2: 20-SECOND MEASUREMENT
    // -------------------------------------------------------------
    unsigned long startTime = millis();
    long bpmSum = 0;
    int validSamples = 0;
    long lastBeat = 0;
    unsigned long warnStartTime = 0;
    bool displayingWarning = false;

    while (millis() - startTime < READ_DURATION_MS) {
      updateNeoPixelFade(); // Red fading LED during scanning

      long currentIR = particleSensor.getIR();

      // Finger slipped off
      if (currentIR < FINGER_THRESHOLD) {
        if (!displayingWarning) {
          warnStartTime = millis();
          displayingWarning = true;
          
          display.clearDisplay();
          display.setTextSize(1);
          display.setCursor(8, 28);
          display.println(F("Keep Finger Still!"));
          display.display();
        }

        if (displayingWarning && (millis() - warnStartTime >= 1000)) {
          displayingWarning = false;
        }
        
        delay(10);
        continue;
      }

      displayingWarning = false;

      // Check for pulse beat
      if (checkForBeat(currentIR) == true) {
        long delta = millis() - lastBeat;
        lastBeat = millis();

        float beatsPerMinute = 60.0 / (delta / 1000.0);

        if (beatsPerMinute >= 45.0 && beatsPerMinute <= 170.0) {
          bpmSum += beatsPerMinute;
          validSamples++;

          int secondsLeft = (READ_DURATION_MS - (millis() - startTime)) / 1000;
          showLiveReading((int)beatsPerMinute, secondsLeft);
        }
      }
    }

    // Turn LED off after scan finishes
    setLedColor(0, 0, 0);

    // -------------------------------------------------------------
    // STAGE 3: SHOW AVERAGE RESULT FOR 10 SECONDS
    // -------------------------------------------------------------
    if (validSamples > 0) {
      int finalBpmAvg = bpmSum / validSamples;
      
      unsigned long resultStartTime = millis();
      while (millis() - resultStartTime < RESULT_HOLD_MS) {
        int holdSecondsLeft = (RESULT_HOLD_MS - (millis() - resultStartTime)) / 1000;
        showAverageResult(finalBpmAvg, holdSecondsLeft);
        delay(100);
      }
    } else {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(12, 20);
      display.println(F("No Pulse Detected"));
      display.setCursor(20, 36);
      display.println(F("Try Again..."));
      display.display();
      delay(2000);
    }

    // -------------------------------------------------------------
    // STAGE 4: REQUIRE FINGER REMOVAL BEFORE RESETTING
    // -------------------------------------------------------------
    while (particleSensor.getIR() > FINGER_THRESHOLD) {
      display.clearDisplay();
      display.setTextSize(1);
      display.setCursor(10, 20);
      display.println(F("Remove Finger To"));
      display.setCursor(25, 36);
      display.println(F("Measure Again"));
      display.display();
      delay(200);
    }
  }

  delay(50);
}

// -------------------------------------------------------------
// NEOPIXEL HELPER FUNCTIONS
// -------------------------------------------------------------

void updateNeoPixelFade() {
  static unsigned long lastUpdate = 0;
  static int brightness = 0;
  static int fadeAmount = 5;

  if (millis() - lastUpdate > 15) {
    lastUpdate = millis();
    brightness += fadeAmount;

    if (brightness <= 0 || brightness >= 255) {
      fadeAmount = -fadeAmount;
    }
    
    brightness = constrain(brightness, 0, 255);
    setLedColor(brightness, 0, 0);
  }
}

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
  rgbLed.setPixelColor(0, rgbLed.Color(r, g, b));
  rgbLed.show();
}

// -------------------------------------------------------------
// DISPLAY HELPER FUNCTIONS (ALL TEXT SIZE 1)
// -------------------------------------------------------------

void showIdleScreen() {
  display.clearDisplay();
  display.setTextSize(1);
  display.setCursor(18, 16);
  display.println(F("READY TO MEASURE"));
  display.setCursor(28, 36);
  display.println(F("Place Finger"));
  display.display();
}

void showLiveReading(int currentBpm, int secondsLeft) {
  display.clearDisplay();
  display.setTextSize(1);
  
  display.setCursor(0, 10);
  display.print(F("Measuring... "));
  display.print(secondsLeft);
  display.println(F("s left"));

  display.setCursor(0, 32);
  display.print(F("Current BPM: "));
  display.println(currentBpm);
  
  display.display();
}

void showAverageResult(int avgBpm, int holdSecondsLeft) {
  display.clearDisplay();
  display.setTextSize(1);

  display.setCursor(15, 8);
  display.println(F("20s AVERAGE RESULT"));

  display.setCursor(25, 28);
  display.print(F("Average: "));
  display.print(avgBpm);
  display.println(F(" BPM"));

  display.setCursor(15, 48);
  display.print(F("Screen hold: "));
  display.print(holdSecondsLeft);
  display.println(F("s"));

  display.display();
}