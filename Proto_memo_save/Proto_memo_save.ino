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

const uint8_t screenWidth = 200;
const uint8_t screenHeight = 240;

const uint16_t numSamples = 320;

bool checkForTrigger = true;
uint16_t sinceDisplayUpdated = 0;

CircularBuffer<uint16_t, numSamples> sampleBuffer;
uint16_t prevSampleBuffer[numSamples+1];

char dispMode = 'l';

uint16_t minValue = 5000;  // Using integer millivolts to save dynamic memory
uint16_t maxValue = 0;
uint16_t peakToPeak = 0;
uint32_t frequency = 0;
uint8_t dutyCycle = 0;

char triggerMode = 'a';

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

  tft.setCursor(10, 215);
  tft.print(F("Min: "));

  tft.setCursor(10, 230);
  tft.print(F("Max: "));

  tft.setCursor(120, 215);
  tft.print("P-P: ");

  tft.setCursor(120, 230);
  tft.print(F("T: "));

  tft.setCursor(230, 215);
  tft.print(F("Freq: "));

  tft.setCursor(230, 230);
  tft.print(F("Duty: "));

  for (uint16_t i = 0; i < numSamples; i++) {
    sampleBuffer.unshift(0);
    prevSampleBuffer[i] = 0;
  }

  pinMode(trigPin, INPUT);
}

void loop() {
  uint8_t timeScale = read(timePin, 1, 10);
  uint16_t newSampleY = readADCresult(30, screenHeight - 30);

  sampleBuffer.unshift(newSampleY);
  sinceDisplayUpdated++;
  uint8_t triggerLevel = 190;

  if (newSampleY + 5 > triggerLevel && newSampleY - 5 < triggerLevel && sampleBuffer[1] < newSampleY && checkForTrigger == true && sinceDisplayUpdated >= numSamples) {
    updateScreen(dispMode, 3, 1);
    checkForTrigger = false;
  } else if (newSampleY < triggerLevel) {
    checkForTrigger = true;
  }
}

uint16_t calculateSignalProperties(uint32_t t1) {
  const uint16_t adcmax = 4095;
  const uint16_t scale = 5000 / adcmax;
  minValue = 5000;
  maxValue = 0;
  uint16_t timePeriod = 0;

  uint16_t aboveThreshold = 0;
  uint16_t totalSamples = sampleBuffer.size();

  for (uint16_t i = 0; i < totalSamples; i++) {
    uint16_t sample = (scale) * map(sampleBuffer[i], 30, screenHeight - 30, 0, 4095);
    if (sample < minValue) minValue = sample;
    if (sample > maxValue) maxValue = sample;
    if (sample > 2500) aboveThreshold++;
  }

  peakToPeak = (maxValue - minValue);
  dutyCycle = (aboveThreshold * 100) / totalSamples;

  uint16_t crossingCount = 0;
  int16_t firstCrossing = -1;
  int16_t lastCrossing = -1;

  for (uint16_t i = 1; i < totalSamples; i++) {
    uint16_t sampBuferUnMapped_i = scale * map(sampleBuffer[i], 30, screenHeight - 30, 0, 4095);
    uint16_t sampBuferUnMapped_i_minus_1 = scale * map(sampleBuffer[i-1], 30, screenHeight - 30, 0, 4095);
    if ((sampBuferUnMapped_i_minus_1 < 2500 && sampBuferUnMapped_i >= 2500) || (sampBuferUnMapped_i_minus_1 > 2500 && sampBuferUnMapped_i <= 2500)) {
      crossingCount++;

      if (crossingCount == 1) firstCrossing = i;
      if (crossingCount == 2) {
        lastCrossing = i;
        break;
      }
    }
  }

  if (lastCrossing != -1) {
    timePeriod = (lastCrossing - firstCrossing) * 2 * t1;
    frequency = 1000000 / timePeriod;
  } else {
    timePeriod = 0;
    frequency = 0;
  }
  return timePeriod;
}

void updateInfoBox(uint32_t timePeriod) {
  tft.fillRect(45, 215, 65, 30, ILI9341_YELLOW);
  tft.fillRect(155, 215, 65, 30, ILI9341_YELLOW);
  tft.fillRect(260, 215, 60, 30, ILI9341_YELLOW);

  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);

  tft.setCursor(45, 215);
  tft.print(minValue / 1000.0, 2);

  tft.setCursor(45, 230);
  tft.print(maxValue / 1000.0, 2);

  tft.setCursor(155, 215);
  tft.print(peakToPeak / 1000.0, 2);

  tft.setCursor(155, 230);
  if (timePeriod > 0) {
    tft.print(timePeriod / 1000.0, 2);
  } else {
    tft.print("---");
  }

  tft.setCursor(260, 215);
  if (frequency > 0) {
    tft.print(frequency, 2);
  } else {
    tft.print("---");
  }

  tft.setCursor(260, 230);
  tft.print(dutyCycle, 1);
}

uint8_t read(uint8_t Pin, uint8_t HRange, uint8_t LRange){
  uint16_t orig = analogRead(Pin);
  return map(orig, 0, 1023, LRange, HRange);
}

uint16_t readADCresult(uint16_t LRange, uint16_t HRange){
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

void updateScreen(char mode, uint8_t offSet, uint8_t currScale) {
  static uint8_t prevOffSet = offSet;
  static uint8_t prevScale = currScale;

  tft.drawLine(0, 30, 320, 30, ILI9341_BLACK);
  tft.drawLine(0, 210, 320, 210, ILI9341_YELLOW);

  if(mode == 'd') {
    for (uint16_t x = 0; x < numSamples; x++) {
      tft.drawPixel(x, prevSampleBuffer[x], ILI9341_BLACK);
      tft.drawPixel(x, sampleBuffer[x], ILI9341_GREEN);
      prevSampleBuffer[x] = sampleBuffer[x];
    }
  } else {
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
  return;
}
