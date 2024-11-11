#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <CircularBuffer.hpp>
#include <Wire.h>
#include <Arduino_FreeRTOS.h>
#include <semphr.h>

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

const uint8_t screenHeight = 240;
const uint16_t numSamples = 320;
const uint16_t samplingInterval = 100; // Adjust interval in microseconds as needed

bool checkForTrigger = true;
uint16_t sinceDisplayUpdated = 0;

CircularBuffer<uint16_t, numSamples> sampleBuffer;
uint16_t prevSampleBuffer[numSamples+1];

char dispMode = 'l';
uint16_t minValue = 5000;
uint16_t maxValue = 0;
uint16_t peakToPeak = 0;
uint32_t frequency = 0;
uint8_t dutyCycle = 0;

char triggerMode = 'a';
SemaphoreHandle_t xSemaphore;

void setup() {
  Serial.begin(9600);
  Wire.begin();
  Wire.setClock(400000);

  pinMode(CONVST_PIN, OUTPUT);
  digitalWrite(CONVST_PIN, LOW);

  int status = Configure();
  if (status != 0) {
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
  setupDisplayLabels();

  for (uint16_t i = 0; i < numSamples; i++) {
    sampleBuffer.unshift(0);
    prevSampleBuffer[i] = 0;
  }

  pinMode(trigPin, INPUT);
  xSemaphore = xSemaphoreCreateMutex();

  xTaskCreate(adcSamplingTask, "ADC Sampling", 128, NULL, 1, NULL);
  vTaskStartScheduler(); // Start the FreeRTOS scheduler
}

void loop() {
  uint8_t timeScale = read(timePin, 1, 10);
  sinceDisplayUpdated++;
  uint8_t triggerLevel = 190;

  if (checkForTriggerCondition(triggerLevel)) {
    updateScreen(dispMode, 3, 1);
    checkForTrigger = false;
  } else if (sampleBuffer[0] < triggerLevel) {
    checkForTrigger = true;
  }
}

void adcSamplingTask(void *pvParameters) {
  while (true) {
    if (xSemaphoreTake(xSemaphore, (TickType_t)10) == pdTRUE) {
      uint16_t newSampleY = readADCresult(30, screenHeight - 30);
      sampleBuffer.unshift(newSampleY);
      xSemaphoreGive(xSemaphore);
    }
    vTaskDelay(samplingInterval / portTICK_PERIOD_US);
  }
}

uint8_t read(uint8_t Pin, uint8_t HRange, uint8_t LRange) {
  uint16_t orig = analogRead(Pin);
  return map(orig, 0, 1023, LRange, HRange);
}

bool checkForTriggerCondition(uint8_t triggerLevel) {
  return sampleBuffer[0] + 5 > triggerLevel && sampleBuffer[0] - 5 < triggerLevel 
    && sampleBuffer[1] < sampleBuffer[0] && checkForTrigger 
    && sinceDisplayUpdated >= numSamples;
}

void setupDisplayLabels() {
  tft.setCursor(10, 215); tft.print(F("Min: "));
  tft.setCursor(10, 230); tft.print(F("Max: "));
  tft.setCursor(120, 215); tft.print("P-P: ");
  tft.setCursor(120, 230); tft.print(F("T: "));
  tft.setCursor(230, 215); tft.print(F("Freq: "));
  tft.setCursor(230, 230); tft.print(F("Duty: "));
}

uint16_t readADCresult(uint16_t LRange, uint16_t HRange) {
  triggerConversion();
  uint16_t data = 0;
  Wire.beginTransmission(ADC_ADDRESS);
  Wire.write(CONV_RESULT_REG);
  Wire.endTransmission();
  Wire.requestFrom(ADC_ADDRESS, 2);
  if (Wire.available() == 2) {
    data = ((Wire.read() << 8) | Wire.read()) & 0x0FFF;
  } else {
    data = 1000;
  }
  return map(data, 0, 4095, LRange, HRange);
}

uint8_t Configure() {
  Wire.beginTransmission(ADC_ADDRESS);
  Wire.write(CONFIG_REG);
  Wire.write(0x18);
  return Wire.endTransmission();
}

void triggerConversion() {
  digitalWrite(CONVST_PIN, HIGH);
  delayMicroseconds(1);  // Minimum delay for CONVST
  digitalWrite(CONVST_PIN, LOW);
}

void updateScreen(char mode, uint8_t offSet, uint8_t currScale) {
  static uint8_t prevOffSet = offSet;
  static uint8_t prevScale = currScale;
  tft.drawLine(0, 30, 320, 30, ILI9341_BLACK);
  tft.drawLine(0, 210, 320, 210, ILI9341_YELLOW);
  drawWaveform(mode, offSet, currScale, prevOffSet, prevScale);
  sinceDisplayUpdated = 0;
  prevOffSet = offSet;
  prevScale = currScale;
}

void drawWaveform(char mode, uint8_t offSet, uint8_t currScale, uint8_t &prevOffSet, uint8_t &prevScale) {
  for (uint16_t x = 0; x < numSamples - 1; x++) {
    int16_t prevScaledY1 = constrain((prevSampleBuffer[x] - 120) * prevScale + 120, 0, 210);
    int16_t prevScaledY2 = constrain((prevSampleBuffer[x + 1] - 120) * prevScale + 120, 0, 210);
    int16_t currScaledY1 = constrain((sampleBuffer[x] - 120) * currScale + 120, 30, 210);
    int16_t currScaledY2 = constrain((sampleBuffer[x + 1] - 120) * currScale + 120, 30, 210);
    if (mode == 'd') tft.drawPixel(x, sampleBuffer[x], ILI9341_GREEN);
    else tft.drawLine(x * offSet, prevScaledY1, (x + 1) * offSet, prevScaledY2, ILI9341_BLACK);
    tft.drawLine(x * offSet, currScaledY1, (x + 1) * offSet, currScaledY2, ILI9341_GREEN);
    prevSampleBuffer[x] = sampleBuffer[x];
  }
}
