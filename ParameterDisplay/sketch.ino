#include <Adafruit_GFX.h>          // Core graphics library
#include <Adafruit_ILI9341.h>  

// For the Adafruit shield, these are the default.
#define TFT_DC 9
#define TFT_CS 5 //4
#define TFT_RST 8
#define TFT_MOSI 6//11
#define TFT_MISO 12
#define TFT_CLK 7 //13

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

void setup() {
  // Start the serial for debugging
  Serial.begin(9600);

  // Initialize the TFT
  tft.begin();
  
  // Test if display initialization worked
  Serial.println("TFT Initialized!");

  // Set rotation to landscape (try other values if nothing shows up)
  tft.setRotation(3);  // Use values 0, 1, 2, 3 for testing

  // Fill the screen with white to check if it's working
  tft.fillScreen(ILI9341_WHITE);

  // Create a yellow background at the bottom for displaying values
  int infoBoxHeight = 30;  // Reduced height
  tft.fillRect(0, 210, 320, infoBoxHeight, ILI9341_YELLOW);

  // Set text color to black and reduce font size
  tft.setTextColor(ILI9341_BLACK);
  tft.setTextSize(1);  // Reduced font size
  
  // Display dummy values with appropriate positions
  tft.setCursor(10, 215);   // Adjust the y-coordinate for new height
  tft.print("Min: 0.5V");

  tft.setCursor(10, 230);
  tft.print("Max: 3.3V");

  tft.setCursor(120, 215);
  tft.print("P-P: 2.8V");

  tft.setCursor(120, 230);
  tft.print("T: 5ms");

  tft.setCursor(230, 215);
  tft.print("Freq: 200Hz");

  Serial.println("Drawing complete!");
}

void loop() {}