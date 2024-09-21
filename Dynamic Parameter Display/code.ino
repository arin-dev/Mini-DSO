#include <Adafruit_GFX.h>    
#include <Adafruit_ILI9341.h> 

#define TFT_DC 9
#define TFT_CS 5 
#define TFT_RST 8
#define TFT_MOSI 6 
#define TFT_MISO 12
#define TFT_CLK 7 

Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_MOSI, TFT_CLK, TFT_RST, TFT_MISO);

const int analogPin = A0;
const int screenWidth = 320;
const int screenHeight = 240;
const int numSamples = 320;
const int samplingInterval = 100;  

int sampleBuffer[numSamples];
int prevSampleBuffer[numSamples];
int currentX = 0;

// Frequency calculation variables
int lastSample = 0;
unsigned long timeStart = 0;
unsigned long totalCrossings = 0;
float signalFrequency = 0;
bool lastAboveMid = false;

// Min/Max and P-P variables
int minSample = 1023;
int maxSample = 0;
float peakToPeak = 0;
float timePeriod = 0;

// Duty cycle calculation variables
unsigned long timeHigh = 0;
unsigned long timeLow = 0;
float dutyCycle = 0;

// Store previous values to optimize updates
float prevMinVoltage = 0, prevMaxVoltage = 0, prevPeakToPeak = 0, prevTimePeriod = 0, prevFrequency = 0, prevDutyCycle = 0;

void setup() {
  Serial.begin(9600);
  tft.begin();
  tft.setRotation(3); 
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(10, 10);
  tft.print("Mini DSO");

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

  timeStart = millis();  // Start timing for frequency calculation
}

void loop() {
  int newSample = analogRead(analogPin);

  scrollWaveform(newSample);

  // Calculate min, max, peak-to-peak, time period, and duty cycle
  calculateSignalProperties(newSample);

  // Calculate frequency and duty cycle of the signal
  calculateFrequencyAndDutyCycle(newSample);

  // Update frequency and other signal properties on the screen every second
  if (millis() - timeStart >= 1000) {
    updateInfoBox();
    timeStart = millis();  // Reset the timeStart for the next second
    // Reset min/max and duty cycle tracking for the next round
    resetMinMaxAndDutyCycle();
  }

  delayMicroseconds(samplingInterval);
}

void scrollWaveform(int newSample) {
  int newY = map(newSample, 0, 1023, screenHeight - 50, 50);
  
  int prevY = prevSampleBuffer[currentX];
  tft.drawPixel(currentX, prevY, ILI9341_BLACK);

  tft.drawPixel(currentX, newY, ILI9341_GREEN);

  prevSampleBuffer[currentX] = newY;

  currentX++;

  if (currentX >= screenWidth) {
    currentX = 0;
    tft.drawLine(0, screenHeight / 2, screenWidth, screenHeight / 2, ILI9341_WHITE); 
    tft.drawLine(0, 0, 0, screenHeight, ILI9341_WHITE);
  }
}

void calculateSignalProperties(int newSample) {
  // Track min and max samples
  if (newSample < minSample) minSample = newSample;
  if (newSample > maxSample) maxSample = newSample;

  // Peak-to-peak value
  peakToPeak = maxSample - minSample;

  // Time period is the inverse of frequency
  if (signalFrequency > 0) {
    timePeriod = 1000.0 / signalFrequency;  // Convert to milliseconds
  }
}

void calculateFrequencyAndDutyCycle(int newSample) {
  int midPoint = 512;  // Midpoint of the analog signal (0-1023)
  bool aboveMid = (newSample > midPoint);

  // Duty cycle calculation
  if (aboveMid) {
    timeHigh++;
  } else {
    timeLow++;
  }

  // Detect when signal crosses the midpoint (either rising or falling)
  if (aboveMid != lastAboveMid) {
    totalCrossings++;
    lastAboveMid = aboveMid;
  }

  // To get frequency in Hz: count crossings / (2 * time in seconds)
  unsigned long elapsedTime = millis() - timeStart;  // Elapsed time in milliseconds
  signalFrequency = (float)totalCrossings / (2.0 * (elapsedTime / 1000.0));  // Frequency in Hz

  // Calculate duty cycle percentage
  if (timeHigh + timeLow > 0) {
    dutyCycle = (float)timeHigh / (timeHigh + timeLow) * 100.0;
  }
}

void updateInfoBox() {
  // Update Min, Max, Peak-to-Peak, Time Period, Frequency, and Duty Cycle dynamically without clearing the entire box
  float minVoltage = map(minSample, 0, 1023, 0, 5000) / 1000.0;  // Convert to voltage (assuming 5V ref)
  float maxVoltage = map(maxSample, 0, 1023, 0, 5000) / 1000.0;  // Convert to voltage
  float pToPVoltage = peakToPeak * (5.0 / 1023.0);  // Convert to voltage

  // Overwrite previous values with yellow background before writing new values
  tft.setTextColor(ILI9341_YELLOW);
  tft.setTextSize(1);
  
  overwritePreviousValue(55, 215, prevMinVoltage, "V");  // Min Voltage
  overwritePreviousValue(55, 230, prevMaxVoltage, "V");  // Max Voltage
  overwritePreviousValue(160, 215, prevPeakToPeak, "V");  // Peak-to-Peak Voltage
  overwritePreviousValue(160, 230, prevTimePeriod, "ms");  // Time Period
  overwritePreviousValue(275, 215, prevFrequency, "Hz");  // Frequency
  overwritePreviousValue(275, 230, prevDutyCycle, "%");  // Duty Cycle

  // Write new values in black
  tft.setTextColor(ILI9341_BLACK);

  tft.setCursor(55, 215);
  tft.print(minVoltage, 1);
  tft.print("V");

  tft.setCursor(55, 230);
  tft.print(maxVoltage, 1);
  tft.print("V");

  tft.setCursor(160, 215);
  tft.print(pToPVoltage, 1);
  tft.print("V");

  tft.setCursor(160, 230);
  tft.print(timePeriod, 1);
  tft.print("ms");

  tft.setCursor(275, 215);
  tft.print(signalFrequency, 1);
  tft.print("Hz");

  tft.setCursor(275, 230);
  tft.print(dutyCycle, 1);
  tft.print("%");

  // Store new values for next update
  prevMinVoltage = minVoltage;
  prevMaxVoltage = maxVoltage;
  prevPeakToPeak = pToPVoltage;
  prevTimePeriod = timePeriod;
  prevFrequency = signalFrequency;
  prevDutyCycle = dutyCycle;

  // Reset total crossings and duty cycle times for the next calculation
  totalCrossings = 0;
}

void overwritePreviousValue(int x, int y, float prevValue, const char* unit) {
  tft.setCursor(x, y);
  tft.print(prevValue, 1);  // Print the previous value
  tft.print(unit);          // Print the unit
}
 
void resetMinMaxAndDutyCycle() {
  minSample = 1023;
  maxSample = 0;
  timeHigh = 0;
  timeLow = 0;
}