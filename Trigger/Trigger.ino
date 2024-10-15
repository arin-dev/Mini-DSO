#include <Adafruit_GFX.h>    // Core graphics library
#include <Adafruit_ILI9341.h> // TFT display library
#include <CircularBuffer.hpp>

// For the Adafruit shield, these are the default.
// #define TFT_DC 9
// #define TFT_CS 5 //4
// #define TFT_RST 8
// #define TFT_MOSI 6 //11
// #define TFT_MISO 12
// #define TFT_CLK 7 //13

// // Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
// Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

// For the Adafruit shield, these are the default.
#define TFT_DC 10
#define TFT_RST 9
#define TFT_CS 8

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

const int analogPin = A0;
const int potPin = A5;

const int screenWidth = 320;
const int screenHeight = 240;

// Number of samples
const int numSamples = 320;
// const int numSamples = 20;
// Sampling rate (time between samples in microseconds)
const int samplingInterval = 100;  // 100 microseconds = 10kHz sampling rate


bool checkForTrigger = true;
// Buffer to store samples
CircularBuffer<int, numSamples> sampleBuffer;
// Previous sample buffer for erasing the old waveform pixel
int prevSampleBuffer[numSamples];

// X-axis position for scrolling the waveform
// int currentX = 0;

void setup() {
  Serial.begin(9600);

  // Initialize TFT display
  tft.begin();
  tft.setRotation(3); // Adjust rotation to fit your setup
  tft.fillScreen(ILI9341_BLACK);

  // Display initial message
  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Mini DSO");

  // Initialize sample buffer
  for (int i = 0; i < numSamples; i++) {
    // sampleBuffer[i] = 0;
    sampleBuffer.unshift(0);
    prevSampleBuffer[i] = 0;
  }

  ///////// TRIGGER CODE STARTS ///////
  pinMode(potPin, INPUT); // Set potentiometer pin as input
  ///////// TRIGGER CODE ENDS ///////
  Serial.println("Chus mera loda jese aloo ka pakoda");
}

void loop() {

  // Read a new sample from the analog input
  int newSample = analogRead(analogPin);
  int newSampleY = map(newSample, 0, 1023, screenHeight - 50, 50);
  sampleBuffer.unshift(newSampleY);
  // Read the potentiometer value to adjust the trigger level

  // for(int i=0; i<numSamples; i++){
  //   Serial.print(sampleBuffer[i]);
  //   Serial.print(" ");
  // }
  // Serial.println(" ");
  // int potValue = analogRead(potPin);
  int potValue = 150;
  // int triggerLevel = map(potValue, 0, 1023, 0, screenHeight); // Map pot value to screen height
  int triggerLevel = map(potValue, 0, 1023, screenHeight - 50, 50); // Map pot value to screen height

  Serial.print(newSampleY);
  // Serial.print(" :: ");
  // Serial.print(potValue);
  Serial.print(" -->  ");
  Serial.print(triggerLevel);


  // Check if the new sample exceeds the trigger level
  int length = sampleBuffer.size(); 
  Serial.print("  < :: >  ");
  Serial.print(length);
  Serial.print("  < :: >  ");
  Serial.println(checkForTrigger);
  if (newSampleY > triggerLevel && checkForTrigger == true && length == numSamples ) {
    updateScreen(length);
    checkForTrigger = false;
    // scrollWaveform(newSample);
  }

  if(newSampleY < triggerLevel){
    checkForTrigger = true;
  }

  // Small delay between frames
  delayMicroseconds(100);
}

// Function to scroll the waveform and draw the new sample
void updateScreen(int length) {
  Serial.println("Updating Display");
  delayMicroseconds(1000);
  for(int currentX=0; currentX<length; currentX++){
    int prevY = prevSampleBuffer[currentX];
    int newY = sampleBuffer[currentX];
    prevSampleBuffer[currentX] = newY;
    tft.drawPixel(currentX, prevY, ILI9341_BLACK);
    tft.drawPixel(currentX, newY, ILI9341_GREEN);
  }
  // tft.drawLine(0, screenHeight / 2, screenWidth, screenHeight / 2, ILI9341_WHITE); // X-axis
  // tft.drawLine(0, 0, 0, screenHeight, ILI9341_WHITE);  // Y-axis
  return ;
}