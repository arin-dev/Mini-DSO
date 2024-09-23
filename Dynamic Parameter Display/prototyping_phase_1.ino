#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <CircularBuffer.hpp>
#include <math.h>

#define TFT_DC 9
#define TFT_CS 5
#define TFT_RST 8
#define TFT_MOSI 6
#define TFT_MISO 12
#define TFT_CLK 7

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

const int inputPin = A0;
const int trigPin = A5;
const int timePin = A4;
const int AScalePin = A3;

const int screenWidth = 320;
const int screenHeight = 240;

const int numSamples = 320;
const int samplingInterval = 100;  // 100 microseconds = 10kHz sampling rate

bool checkForTrigger = true;
int sinceDisplayUpdated = 0;

CircularBuffer<int, numSamples> sampleBuffer;
int prevSampleBuffer[numSamples+1];

char dispMode = 'l';

// Variables for signal properties
float minValue = 5.0;
float maxValue = 0.0;
float peakToPeak = 0;
float frequency = 0.0;
float timePeriod = 0.0;
float dutyCycle = 0.0;
float t0, t1;

void setup() {
  Serial.begin(9600);

  tft.begin();
  tft.setRotation(3); // Adjust rotation to fit your setup
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 10);
  tft.print("Mini DSO | Oscillonauts");

  // Create yellow info box at the bottom of the screen
  tft.fillRect(0, 210, 320, 30, ILI9341_YELLOW); // Info box height reduced

  // Write the static labels for the info box (Min, Max, P-P, Time, Freq, Duty Cycle)
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);

  tft.setCursor(10, 215);
  tft.print("Min: ");

  tft.setCursor(10, 230);
  tft.print("Max: ");

  tft.setCursor(120, 215);
  tft.print("P-P: ");

  tft.setCursor(120, 230);
  tft.print("T: ");

  tft.setCursor(230, 215);
  tft.print("Freq: ");

  tft.setCursor(230, 230);
  tft.print("Duty: ");

  for (int i = 0; i < numSamples; i++) {
    sampleBuffer.unshift(0);
    prevSampleBuffer[i] = 0;
  }

  pinMode(trigPin, INPUT);
}

void loop() {
  t0 = millis();
  int timeScale = read(timePin, 1, 10);
  int newSampleY = read(inputPin, 30, screenHeight - 30);
  sampleBuffer.unshift(newSampleY);
  sinceDisplayUpdated++;

  int triggerLevel = read(trigPin, 30, screenHeight - 30);
  float AScale = (float)read(AScalePin, 5, 20)/(float)10;

  // Trigger-based screen update
  if (newSampleY + 10 > triggerLevel && newSampleY - 10 < triggerLevel && sampleBuffer[1] < newSampleY && checkForTrigger == true && sinceDisplayUpdated >= numSamples) {
    calculateSignalProperties();
    updateScreen(dispMode, timeScale, AScale);
    updateInfoBox();  // Update the yellow box with signal properties
    checkForTrigger = false;
  } else if (newSampleY < triggerLevel) {
    checkForTrigger = true;
  }

  t1 = millis() - t0;
  // Serial.println(t0);
  // Serial.print(" ||");
  // Serial.println(t1);
  
  delayMicroseconds(1);
}

// Function to calculate signal properties
void calculateSignalProperties() {
  float scale = 5.0/1023.0;  //scale sample buffer to voltage
  minValue = 5.0; //max 5 V
  maxValue = 0.0;
  int aboveThreshold = 0;
  int totalSamples = sampleBuffer.size();
  // Serial.print("totalSamp ---> ");
  // Serial.println(totalSamples);

  // Calculate min, max, and duty cycle
  for (int i = 0; i < totalSamples; i++) {
    float sample = (5.0/1023.0) * map(sampleBuffer[i], 30, screenHeight - 30, 0, 1023);
    // Serial.println(sample);
    if (sample < minValue) minValue = sample;
    if (sample > maxValue) maxValue = sample;
    if (sample > 2.5) aboveThreshold++;  // Assume 512 is the midpoint for duty cycle
  }

  // Serial.print("Max ---> ");
  // Serial.print(maxValue);
  // Serial.print(" || ");
  // Serial.println(minValue);
  peakToPeak = (maxValue - minValue);
  dutyCycle = ((float)aboveThreshold / totalSamples) * 100.0;

  // Calculate time period and frequency (based on crossings)
  int crossingCount = 0;
  int firstCrossing = -1;
  int lastCrossing = -1;

  for (int i = 1; i < totalSamples; i++) {
    float sampBuferUnMapped_i = scale * map(sampleBuffer[i], 30, screenHeight - 30, 0, 1023);
    float sampBuferUnMapped_i_minus_1 = scale * map(sampleBuffer[i-1], 30, screenHeight - 30, 0, 1023);
    if ((sampBuferUnMapped_i_minus_1 < 2.5 && sampBuferUnMapped_i >= 2.5) || (sampBuferUnMapped_i_minus_1 > 2.5 && sampBuferUnMapped_i <= 2.5)) {
      crossingCount++;

      if (crossingCount == 1) firstCrossing = i;
      if (crossingCount == 2) {  // Detect the second crossing to estimate the period
        lastCrossing = i;
        break;
      }
    }
  }

  Serial.println(lastCrossing);

  if (lastCrossing != -1) {
    timePeriod = (float)((lastCrossing - firstCrossing) * 2*t1);  //milliseconds
    // Serial.println(timePeriod);
    frequency = 1000.0 / timePeriod;  // Frequency in Hz
  } else {
    timePeriod = 0;
    frequency = 0;
  }

}

// Function to update the yellow info box with calculated values
void updateInfoBox() {
  tft.fillRect(45, 215, 65, 30, ILI9341_YELLOW);  // Clear the previous values
  tft.fillRect(155, 215, 65, 30, ILI9341_YELLOW); // Clear P-P, T
  tft.fillRect(260, 215, 60, 30, ILI9341_YELLOW); // Clear Freq, Duty

  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);

  // Update Min, Max, P-P, T, Freq, Duty Cycle values
  tft.setCursor(45, 215);
  tft.print(minValue);
  // tft.print(" V");

  tft.setCursor(45, 230);
  tft.print(maxValue);
  // tft.print(" V");

  tft.setCursor(155, 215);
  tft.print(peakToPeak);
  // tft.print(" V");

  tft.setCursor(155, 230);
  if (timePeriod > 0) {
    tft.print(timePeriod, 2);
    // tft.print(" s");
  } else {
    tft.print("---");
  }

  tft.setCursor(260, 215);
  if (frequency > 0) {
    tft.print(frequency, 2);
    // tft.print(" Hz");
  } else {
    tft.print("---");
  }

  tft.setCursor(260, 230);
  tft.print(dutyCycle, 1);
  // tft.print("%");
}

int read(int Pin, int HRange, int LRange){
  int orig = analogRead(Pin);
  return map(orig, 0, 1023, LRange, HRange);
}

void updateScreen(char mode, int offSet, float currScale) {
  static int prevOffSet = offSet;
  static float prevScale = currScale;

  tft.drawLine(0, 30, 320, 30, ILI9341_BLACK);
  tft.drawLine(0, 210, 320, 210, ILI9341_YELLOW);

  if(mode == 'd') {
    for (int x = 0; x < numSamples; x++) {
      tft.drawPixel(x, prevSampleBuffer[x], ILI9341_BLACK);
      tft.drawPixel(x, sampleBuffer[x], ILI9341_GREEN);
      prevSampleBuffer[x] = sampleBuffer[x];
    }
  } else {
    for (int x = 0; x < numSamples - 1; x++){
      if (x * prevOffSet < numSamples){
        int prevScaledY1 = (prevSampleBuffer[x] - 120) * prevScale + 120;
        prevScaledY1 = prevScaledY1 > 210 ? 210 : prevScaledY1;
        prevScaledY1 = prevScaledY1 < 0 ? 0 : prevScaledY1;
        int prevScaledY2 = (prevSampleBuffer[x + 1] - 120) * prevScale + 120;
        prevScaledY2 = prevScaledY2 > 210 ? 210 : prevScaledY2;
        prevScaledY2 = prevScaledY2 < 0 ? 0 : prevScaledY2;
        tft.drawLine(x * prevOffSet, prevScaledY1, (x + 1) * prevOffSet, prevScaledY2, ILI9341_BLACK);
      }
      if (x * offSet < numSamples){
        int currScaledY1 = (sampleBuffer[x] - 120) * currScale + 120;
        currScaledY1 = currScaledY1 > 210 ? 210 : currScaledY1;
        currScaledY1 = currScaledY1 < 30 ? 30 : currScaledY1;
        int currScaledY2 = (sampleBuffer[x + 1] - 120) * currScale + 120;
        currScaledY2 = currScaledY2 > 210 ? 210 : currScaledY2;
        currScaledY2 = currScaledY2 < 30 ? 30 : currScaledY2;
        tft.drawLine(x * offSet, currScaledY1, (x + 1) * offSet, currScaledY2, ILI9341_GREEN);
      }
      prevSampleBuffer[x] = sampleBuffer[x];
    }
  }
  
  sinceDisplayUpdated = 0;
  prevOffSet = offSet;
  prevScale = currScale;
  return;
}