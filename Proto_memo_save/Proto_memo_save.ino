#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <CircularBuffer.hpp>
#include <Wire.h>

#define TFT_DC 10
#define TFT_CS 8
#define TFT_RST 9

#define CONVST_PIN 3
#define ADC_ADDRESS 0x20
#define CONFIG_REG 0x02
#define CONV_RESULT_REG 0x00

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

const uint8_t trigPin = A1;
const uint8_t timePin = A2;
const uint8_t AScalePin = A3;

// const uint8_t screenWidth = 320;
const uint8_t screenHeight = 240;
const uint16_t numSamples = 315;

bool checkForTrigger = true;
uint16_t sinceDisplayUpdated = 0;

CircularBuffer<uint8_t, numSamples> sampleBuffer;
uint8_t prevSampleBuffer[numSamples+1];

// char dispMode = 'l';

float minValue = 5.0;  // Using integer millivolts to save dynamic memory
float maxValue = 0;
float peakToPeak = 0;
float frequency = 0;
float dutyCycle = 0;
float t0, t1;

// char triggerMode = 'a';

#define MINTXT 10
#define PPTXT 90
#define FTXT 175
#define VSTXT 255

#define MPOS 52
#define PPOS 132
#define FPOS 217
#define VSPOS 291
#define RECT_WIDTH 34

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(400000);

  pinMode(CONVST_PIN, OUTPUT);
  digitalWrite(CONVST_PIN, LOW);

  int status = Configure();

  if (status == 0) {
    Serial.println(F("Configuration successful, ACK received."));
  } else {
    Serial.println(F("Error during configuration, no ACK received."));
    return;
  }

  tft.begin();
  tft.setRotation(5);
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 10);
  tft.print(F("Mini DSO | Oscillonauts"));

  tft.fillRect(0, 210, 320, 30, ILI9341_YELLOW);

  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);

  tft.setCursor(MINTXT, 215);
  tft.print(F("Min(v): "));

  tft.setCursor(MINTXT, 230);
  tft.print(F("Max(v): "));

  tft.setCursor(PPTXT, 215);
  tft.print("P2P(v): ");

  tft.setCursor(PPTXT, 230);
  tft.print(F("T(ns): "));

  tft.setCursor(FTXT, 215);
  tft.print(F("F(KHz): "));

  tft.setCursor(FTXT, 230);
  tft.print(F("Dut(%): "));

  tft.setCursor(VSTXT, 215);
  tft.print(F("VS(x): "));

  tft.setCursor(VSTXT, 230);
  tft.print(F("TS(x): "));

  for (uint16_t i = 0; i < numSamples; i++) {
    sampleBuffer.unshift(0);
    prevSampleBuffer[i] = 0;
  }

  pinMode(trigPin, INPUT);
}

void loop() {
  t0 = micros();

  uint8_t newSampleY = readADCresult(30, screenHeight - 30);

  sampleBuffer.unshift(newSampleY);
  sinceDisplayUpdated++;
  // uint8_t triggerLevel = 120;
  uint8_t triggerLevel = read(trigPin, 35, 205);

  if ( checkForTrigger == true && sinceDisplayUpdated >= numSamples && newSampleY + 5 > triggerLevel && newSampleY - 5 < triggerLevel && sampleBuffer[1] < newSampleY ) {
    float time_period = calculateSignalProperties(t1);
    uint8_t timeScale = read(timePin, 1, 5);
    float VoltageScale = float(read(AScalePin, 5, 40))/10.0;
    // float VoltageScale = 1.0;
    // updateScreen(dispMode, 3, 1);

    // updateScreen(dispMode, timeScale, VoltageScale, triggerLevel); // Time , Voltage, triggerLevel
    updateScreen(timeScale, VoltageScale, triggerLevel); // Time , Voltage, triggerLevel
    updateInfoBox(time_period, VoltageScale, timeScale);
    checkForTrigger = false;
  } else if (newSampleY < triggerLevel) {
    checkForTrigger = true;
  } 
  else if(sinceDisplayUpdated > 8192){
    sinceDisplayUpdated = 0;
    tft.drawLine(0, 30, 0, 210, ILI9341_BLACK);
    tft.drawLine(1, 30, 1, 210, ILI9341_BLACK);
    tft.drawLine(2, 30, 2, 210, ILI9341_BLACK);

    float VoltageScale = float(read(AScalePin, 5, 40))/10.0;    
    uint8_t tl = (triggerLevel - 120) * VoltageScale + 120;
    tl = tl > 210 ? 205 : tl; 
    tl = tl < 30 ? 35 : tl;  

    tft.drawLine(0, tl-1, 2, tl-1, ILI9341_RED);
    tft.drawLine(0, tl, 2, tl, ILI9341_RED);
    tft.drawLine(0, tl+1, 2, tl+1, ILI9341_RED);
  }
  t1 = micros() - t0;
}

float calculateSignalProperties(float t1) {
  float adcmax = 1023.0;
  float scale = 5.0/adcmax;

  minValue = 5.0;
  maxValue = 0.0;
  float timePeriod;

  int aboveThreshold = 0;
  int totalSamples = sampleBuffer.size();

  for (int i = 0; i < totalSamples; i++) {
    float sample = (scale) * map(sampleBuffer[i], 30, screenHeight - 30, 0, 1023);
    if (sample < minValue) minValue = sample;
    if (sample > maxValue) maxValue = sample;
    if (sample > 2.5) aboveThreshold++;
  }
  peakToPeak = (maxValue - minValue);
  dutyCycle = ((float)aboveThreshold / totalSamples) * 100.0;

  int crossingCount = 0;
  int firstCrossing = -1;
  int lastCrossing = -1;

  for (int i = 1; i < totalSamples; i++) {
    float sampBuferUnMapped_i = scale * map(sampleBuffer[i], 30, screenHeight - 30, 0, 1023);
    float sampBuferUnMapped_i_minus_1 = scale * map(sampleBuffer[i-1], 30, screenHeight - 30, 0, 1023);
    if ((sampBuferUnMapped_i_minus_1 < 2.5 && sampBuferUnMapped_i >= 2.5) || (sampBuferUnMapped_i_minus_1 > 2.5 && sampBuferUnMapped_i <= 2.5)) {
      crossingCount++;

      if (crossingCount == 1) firstCrossing = i;
      if (crossingCount == 2) {
        lastCrossing = i;
        break;
      }
    }
  }

  if (lastCrossing != -1) {
    timePeriod = (float)((lastCrossing - firstCrossing) * 2*t1); //in us
    frequency = 1000.0 / timePeriod;
  } else {
    timePeriod = 0;
    frequency = 0;
  }
  return timePeriod;
}


// Function to update the yellow info box with calculated values
void updateInfoBox(float timePeriod, float VoltageScale, uint8_t timeScale) {
  tft.fillRect(MPOS, 215, RECT_WIDTH, 30, ILI9341_YELLOW);  // Clear the previous values
  tft.fillRect(PPOS, 215, RECT_WIDTH, 30, ILI9341_YELLOW); // Clear P-P, T
  tft.fillRect(FPOS, 215, RECT_WIDTH, 30, ILI9341_YELLOW); // Clear Freq, Duty
  tft.fillRect(VSPOS, 215, RECT_WIDTH, 30, ILI9341_YELLOW); // Clear vScale, tScale

  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);

  // Update Min, Max, P-P, T, Freq, Duty Cycle values
  tft.setCursor(MPOS, 215);
  tft.print(minValue);
  // tft.print(" V");

  tft.setCursor(MPOS, 230);
  tft.print(maxValue);
  // tft.print(" V");

  tft.setCursor(PPOS, 215);
  tft.print(peakToPeak);
  // tft.print(" V");

  tft.setCursor(PPOS, 230);
  if (timePeriod/1000 > 0) {
    tft.print(timePeriod/1000, 2); //displayed in ns
    // tft.print(" s");
  } else {
    tft.print("--");
  }

  tft.setCursor(FPOS, 215);
  if (frequency*1000 > 0) {
    tft.print(frequency*1000, 2); //in KHz
    // tft.print(" Hz");
  } else {
    tft.print("--");
  }

  tft.setCursor(FPOS, 230);
  tft.print(dutyCycle, 1);
  // tft.print("%");

  tft.setCursor(VSPOS, 215);
  tft.print(VoltageScale, 1);

  tft.setCursor(VSPOS, 230);
  tft.print(timeScale, 1);
}

int read(int Pin, int HRange, int LRange){
  int orig = analogRead(Pin);
  return map(orig, 0, 1023, LRange, HRange);
}

uint8_t readADCresult(uint16_t LRange, uint16_t HRange){
  triggerConversion();
  uint16_t data = 0;

  Wire.beginTransmission(ADC_ADDRESS);
  Wire.write(CONV_RESULT_REG);
  uint8_t error = Wire.endTransmission();

  Wire.requestFrom(ADC_ADDRESS, 2);

  if (Wire.available() == 2) {
    uint8_t msb = Wire.read();
    uint8_t lsb = Wire.read();
    data = (msb << 8) | lsb;
    
    uint16_t adcValue = data & 0x0FFF;
    data = adcValue;
  } else {
    Serial.println(F("Error: ADC result not available."));
    data = 1000;
  }

  return map(data, 0, 4095, LRange, HRange);
}

uint8_t Configure(){
  Wire.beginTransmission(ADC_ADDRESS);
  Wire.write(CONFIG_REG);
  Wire.write(0x18);
  uint8_t error = Wire.endTransmission();
  
  return error;
}

void triggerConversion() {
  digitalWrite(CONVST_PIN, HIGH);
  delayMicroseconds(0);
  digitalWrite(CONVST_PIN, LOW);
  delayMicroseconds(0);
}

// void updateScreen(char mode, uint8_t offSet, float currScale, uint8_t triggerLevel) {
void updateScreen(uint8_t offSet, float currScale, uint8_t triggerLevel) {
  static uint8_t prevOffSet = offSet;
  // static uint8_t prevTriggerLevel = triggerLevel;
  static float prevScale = currScale;

  tft.drawLine(0, 30, 320, 30, ILI9341_BLACK);
  tft.drawLine(0, 210, 320, 210, ILI9341_YELLOW);

  tft.drawLine(0, 30, 0, 210, ILI9341_BLACK);
  tft.drawLine(1, 30, 1, 210, ILI9341_BLACK);
  tft.drawLine(2, 30, 2, 210, ILI9341_BLACK);

  // if(mode == 'd') {
  //   for (uint16_t x = 0; x < numSamples; x++) {
  //     tft.drawPixel(x, prevSampleBuffer[x], ILI9341_BLACK);
  //     tft.drawPixel(x, sampleBuffer[x], ILI9341_GREEN);
  //     prevSampleBuffer[x] = sampleBuffer[x];
  //   }
  // } else 
  {
    for (uint16_t x = 0; x < numSamples - 1; x++){
      if (x * prevOffSet < numSamples){
        int16_t prevScaledY1 = (prevSampleBuffer[x] - 120) * prevScale + 120;
        prevScaledY1 = prevScaledY1 > 210 ? 210 : prevScaledY1;
        prevScaledY1 = prevScaledY1 < 0 ? 0 : prevScaledY1;
        int16_t prevScaledY2 = (prevSampleBuffer[x + 1] - 120) * prevScale + 120;
        prevScaledY2 = prevScaledY2 > 210 ? 210 : prevScaledY2;
        prevScaledY2 = prevScaledY2 < 0 ? 0 : prevScaledY2;
        tft.drawLine(x * prevOffSet, prevScaledY1, (x + 1) * prevOffSet, prevScaledY2, ILI9341_BLACK);
      }
      if (x * offSet < numSamples){
        int16_t currScaledY1 = (sampleBuffer[x] - 120) * currScale + 120;
        currScaledY1 = currScaledY1 > 210 ? 210 : currScaledY1;
        currScaledY1 = currScaledY1 < 30 ? 30 : currScaledY1;
        int16_t currScaledY2 = (sampleBuffer[x + 1] - 120) * currScale + 120;
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

  uint8_t tl = (triggerLevel - 120) * currScale + 120;
  tl = tl > 210 ? 205 : tl; 
  tl = tl < 30 ? 35 : tl;  
  
  tft.drawLine(0, tl-1, 2, tl-1, ILI9341_RED);
  tft.drawLine(0, tl, 2, tl, ILI9341_RED);
  tft.drawLine(0, tl+1, 2, tl+1, ILI9341_RED);
  
  return;
}