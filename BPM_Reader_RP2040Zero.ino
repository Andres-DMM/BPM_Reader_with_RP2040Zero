#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Adafruit_NeoPixel.h>
#include "MAX30105.h"

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
const unsigned long READ_DURATION_MS = 20000; // 20 Seconds reading window
const unsigned long RESULT_HOLD_MS   = 10000; // 10 Seconds display hold window
const long FINGER_THRESHOLD          = 20000; // IR threshold for finger detection

// Fast Peak Detection Variables
long irMin = 0;
long irMax = 0;
long lastBeatTime = 0;
bool lookingForPeak = true;

// Helper Declarations
void showIdleScreen();
void showLiveReading(int currentBpm, int secondsLeft);
void showAverageResult(int avgBpm, int holdSecondsLeft);
void updateNeoPixelFade();
void setLedColor(uint8_t r, uint8_t g, uint8_t b);
bool detectBeatFast(long irValue);

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

  // ULTRA-FAST SENSOR SETTINGS
  byte ledBrightness = 0x24; // ~7.2mA power
  byte sampleAverage = 1;    // No sample averaging
  byte ledMode       = 2;    // Red + IR
  int sampleRate     = 200;  // 200 Hz gives stable pulse curves
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
    // STAGE 2: INSTANT CALIBRATION RESET
    // -------------------------------------------------------------
    particleSensor.clearFIFO();
    irMin = irValue;
    irMax = irValue;
    lastBeatTime = millis();
    lookingForPeak = true;

    // -------------------------------------------------------------
    // STAGE 3: 20-SECOND MEASUREMENT
    // -------------------------------------------------------------
    unsigned long startTime = millis();
    unsigned long lastDisplayUpdate = 0;
    
    long bpmSum = 0;
    int validSamples = 0;
    int latestBpm = 0;

    while (millis() - startTime < READ_DURATION_MS) {
      // 1. PROCESS ALL WAITING SAMPLES FROM SENSOR FIFO FAST
      particleSensor.check(); // Check sensor buffer
      
      while (particleSensor.available()) {
        long currentIR = particleSensor.getFIFOIR();
        particleSensor.nextSample(); // Advance to next sample in FIFO

        // Check if finger slipped off
        if (currentIR < FINGER_THRESHOLD) {
          continue;
        }

        // Check for pulse peak
        if (detectBeatFast(currentIR)) {
          long delta = millis() - lastBeatTime;
          lastBeatTime = millis();

          float beatsPerMinute = 60000.0 / delta;

          // Realistic BPM filter
          if (beatsPerMinute >= 45.0 && beatsPerMinute <= 180.0) {
            bpmSum += beatsPerMinute;
            validSamples++;
            latestBpm = (int)beatsPerMinute;
          }
        }
      }

      // 2. UPDATE DISPLAY ONLY EVERY 250ms (PREVENTS SENSOR STARVATION)
      if (millis() - lastDisplayUpdate >= 250) {
        lastDisplayUpdate = millis();
        updateNeoPixelFade();
        
        int secondsLeft = (READ_DURATION_MS - (millis() - startTime)) / 1000;
        showLiveReading(latestBpm, secondsLeft);
      }
    }

    // Turn LED off after scan finishes
    setLedColor(0, 0, 0);

    // -------------------------------------------------------------
    // STAGE 4: SHOW AVERAGE RESULT FOR 10 SECONDS
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
    // STAGE 5: REQUIRE FINGER REMOVAL BEFORE RESETTING
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
// DYNAMIC PEAK DETECTOR
// -------------------------------------------------------------
bool detectBeatFast(long irValue) {
  // Smoothly adjust min/max tracking bounds
  irMin = (irMin * 0.98) + (irValue * 0.02);
  irMax = (irMax * 0.98) + (irValue * 0.02);

  if (irValue < irMin) irMin = irValue;
  if (irValue > irMax) irMax = irValue;

  long threshold = irMin + ((irMax - irMin) * 0.55); // Dynamic peak threshold (55%)

  // Refractory period: Minimum 330ms between beats (~180 BPM max limit)
  if (millis() - lastBeatTime < 330) {
    return false;
  }

  if (lookingForPeak && irValue > threshold) {
    lookingForPeak = false; // Beat peak found!
    return true;
  }

  if (!lookingForPeak && irValue < (irMin + ((irMax - irMin) * 0.35))) {
    lookingForPeak = true; // Valley found, ready for next beat
  }

  return false;
}

// -------------------------------------------------------------
// NEOPIXEL HELPER FUNCTIONS
// -------------------------------------------------------------

void updateNeoPixelFade() {
  static unsigned long lastUpdate = 0;
  static int brightness = 0;
  static int fadeAmount = 15;

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
  
  if (currentBpm > 0) {
    display.println(currentBpm);
  } else {
    display.println(F("--"));
  }
  
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
