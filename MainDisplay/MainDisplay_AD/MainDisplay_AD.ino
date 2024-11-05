#include <Adafruit_GFX.h>          // Core graphics library
#include <Adafruit_ILI9341.h>      // Display library
#include <CircularBuffer.hpp>        // Circular buffer library

// #define TFT_DC 9
// #define TFT_CS 5 //4
// #define TFT_RST 8
// #define TFT_MOSI 6//11
// #define TFT_MISO 12
// #define TFT_CLK 7 //13

// Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

// For the Adafruit shield, these are the default.
#define TFT_DC 10
#define TFT_RST 9
#define TFT_CS 8

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

const int bufferSize = 321;                 // Circular buffer size
int currentY[bufferSize];     // Circular buffer to hold analog data
int prevY[bufferSize];     // Circular buffer to hold analog data

int displayWidth = 320;                     // Display width in pixels
int displayHeight = 240;                    // Display height in pixels
// int lastYValues[320];                       // Array to store last Y positions for clearing
const int analogPin = A0;

void setup() {
  Serial.begin(9600);                       // Initialize serial communication
  tft.begin();                              // Initialize display
  tft.setRotation(3);                       // Set rotation for landscape view
  tft.fillScreen(ILI9341_BLACK);            // Clear the screen

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Mini DSO");

  for(int i=0; i<bufferSize; i++){
    currentY[i]=5;
    prevY[i]=5;
  }
}


void loop() {
  // Read analog value from A0 and map to display height
  int analogValue = analogRead(A0);
  int mappedValue = map(analogValue, 0, 1023, 50, displayHeight - 50);
  Serial.print("I/P : ");
  Serial.println(mappedValue);
  // Add the value to the circular buffer
  // if (buffer.pop() != mappedValue){
  // Draw the data
  drawData(mappedValue);
  // delay(5);  // Adjust delay for data update rate
  // }
}

void drawData(int newVal) {
  // static int ind = 0;
  memcpy(prevY, currentY, bufferSize * sizeof(int) );

  for(int i=0; i<bufferSize-1; i++)
    currentY[i]=currentY[i+1];
  
  currentY[bufferSize-1] = newVal;

  for (int i = 1; i < bufferSize; i++) {
    // Clear the previous pixel
    tft.drawLine(i, prevY[i], i-1, prevY[i-1], ILI9341_BLACK);

    // Draw the new pixel
    tft.drawLine(i, currentY[i], i-1, currentY[i-1], ILI9341_GREEN);
    // tft.drawPixel(i, currentValue, ILI9341_GREEN);
  }

  // ind = ( ind + 1 ) % bufferSize;
}