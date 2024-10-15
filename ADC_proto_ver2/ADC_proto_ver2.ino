#include <Adafruit_GFX.h>
#include <Adafruit_ILI9341.h>
#include <CircularBuffer.hpp>
#include <Wire.h>


//for display
#define TFT_DC 10
#define TFT_CS 8
#define TFT_RST 9

//for ADC
#define CONVST_PIN 3         // Pin connected to the CONVST pin of AD7994
#define ADC_ADDRESS 0x20     // I2C address of the AD7994-1
#define CONFIG_REG 0x02
#define CONV_RESULT_REG 0x00

// Use hardware SPI (on Uno, #13, #12, #11) and the above for CS/DC
Adafruit_ILI9341 tft = Adafruit_ILI9341(TFT_CS, TFT_DC, TFT_RST);

// const int inputPin = A0;
const int trigPin = A1;
const int timePin = A2;
const int AScalePin = A3;

const int screenWidth = 200;
const int screenHeight = 240;

// const int numSamples = 310;
const int numSamples = 310;
// const int samplingInterval = 100;  // 100 microseconds = 10kHz sampling rate

bool checkForTrigger = true;
int sinceDisplayUpdated = 0;

CircularBuffer<int, numSamples> sampleBuffer;
int prevSampleBuffer[numSamples+1];

char dispMode = 'l';

// Variables for signal properties  ///defined in function to remove global variables memory
float minValue = 5.0;
float maxValue = 0.0;
float peakToPeak = 0;
float frequency = 0.0;
// float timePeriod = 0.0;
float dutyCycle = 0.0;
// float t0, t1;

//trigger mode 'm' or 'a'
char triggerMode = 'a';

void setup() {
  Serial.begin(9600);
  //ADC
  Wire.begin();                // Initialize I2C communication
  // Wire.setClock(3400000);  //3.4 MHz
  Wire.setClock(400000);  // Set I2C clock to 400 kHz (maximum on Arduino Uno)


  pinMode(CONVST_PIN, OUTPUT); // Set the CONVST pin as output
  digitalWrite(CONVST_PIN, LOW); // Initialize CONVST to LOW

  // pinMode(3, OUTPUT); // Set the CONVST pin as output
  // digitalWrite(3, LOW); // Initialize CONVST to LOW

  int status = Configure();

  if (status == 0) {
    Serial.println(F("Configuration successful, ACK received."));
  } else {
    Serial.println(F("Error during configuration, no ACK received."));
    return;
  }

  //display
  tft.begin();
  tft.setRotation(5); // Adjust rotation to fit your setup
  tft.fillScreen(ILI9341_BLACK);

  tft.setTextColor(ILI9341_WHITE);
  tft.setTextSize(2);
  tft.setCursor(0, 10);
  tft.print(F("Mini DSO | Oscillonauts"));

  // Create yellow info box at the bottom of the screen
  tft.fillRect(0, 210, 320, 30, ILI9341_YELLOW); // Info box height reduced

  // Write the static labels for the info box (Min, Max, P-P, Time, Freq, Duty Cycle)
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

  for (int i = 0; i < numSamples; i++) {
    sampleBuffer.unshift(0);
    prevSampleBuffer[i] = 0;
  }

  pinMode(trigPin, INPUT);
}

void loop() {
  // float t0, t1;
  // t0 = micros();
  int timeScale = read(timePin, 1, 10);
  // int newSampleY = read(inputPin, 30, screenHeight - 30);
  int newSampleY = readADCresult(30, screenHeight - 30);
  // Serial.print(F("value going into dso:"));
  // Serial.println(newSampleY);

  sampleBuffer.unshift(newSampleY);
  sinceDisplayUpdated++;
  // int triggerLevel = = read(trigPin, 30, screenHeight - 30);
  int triggerLevel = 140;
  // if(triggerMode = 'a'){
  //     triggerLevel = calculateTriggerLevel();
  // }
  // else{
  //     triggerLevel = read(trigPin, 30, screenHeight - 30);
  // }
  

  // float AScale = (float)read(AScalePin, 5, 20)/(float)10;


  // Trigger-based screen update
  if (newSampleY + 5 > triggerLevel && newSampleY - 5 < triggerLevel && sampleBuffer[1] < newSampleY && checkForTrigger == true && sinceDisplayUpdated >= numSamples) {
    // float timePeriod = calculateSignalProperties(t1);
    updateScreen(dispMode, 3, 1);
    // updateInfoBox(timePeriod);  // Update the yellow box with signal properties
    checkForTrigger = false;
  } else if (newSampleY < triggerLevel) {
    checkForTrigger = true;
  }

  // t1 = micros() - t0;
  // // Serial.println(t0);
  // Serial.print(" ||");
  // Serial.println(t1);

  // delayMicroseconds(1);
}

// //auto trigger computation //the following using mean
// int calculateTriggerLevel() {
//     float sum = 0;
//     int count = sampleBuffer.size();

//     for (int i = 0; i < count; i++) {
//         sum += sampleBuffer[i];
//     }

//     // Calculate the average and use it as the trigger level
//     return sum / count;
// }

// Function to calculate signal properties
float calculateSignalProperties(float t1) {
  float adcmax = 4095.0;
  float scale = 5.0/adcmax;  //scale sample buffer to voltage

  //float 
  minValue = 5.0; //max 5 V
  //float 
  maxValue = 0.0;
  float timePeriod;

  int aboveThreshold = 0;
  int totalSamples = sampleBuffer.size();
  // Serial.print("totalSamp ---> ");
  // Serial.println(totalSamples);

  // Calculate min, max, and duty cycle
  for (int i = 0; i < totalSamples; i++) {
    float sample = (scale) * map(sampleBuffer[i], 30, screenHeight - 30, 0, 4095);
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
    float sampBuferUnMapped_i = scale * map(sampleBuffer[i], 30, screenHeight - 30, 0, 4095);
    float sampBuferUnMapped_i_minus_1 = scale * map(sampleBuffer[i-1], 30, screenHeight - 30, 0, 4095);
    if ((sampBuferUnMapped_i_minus_1 < 2.5 && sampBuferUnMapped_i >= 2.5) || (sampBuferUnMapped_i_minus_1 > 2.5 && sampBuferUnMapped_i <= 2.5)) {
      crossingCount++;

      if (crossingCount == 1) firstCrossing = i;
      if (crossingCount == 2) {  // Detect the second crossing to estimate the period
        lastCrossing = i;
        break;
      }
    }
  }

  // Serial.println(lastCrossing);

  if (lastCrossing != -1) {
    timePeriod = (float)((lastCrossing - firstCrossing) * 2*t1);  //milliseconds
    // Serial.println(timePeriod);
    frequency = 1000.0*1000.0 / timePeriod;  // Frequency in Hz
  } else {
    timePeriod = 0;
    frequency = 0;
  }
  return timePeriod;
}

// Function to update the yellow info box with calculated values
void updateInfoBox(float timePeriod) {
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

int readADCresult(int LRange, int HRange){
  triggerConversion();
  int data = 0;

  Wire.beginTransmission(ADC_ADDRESS);
  Wire.write(CONV_RESULT_REG); // Point to result register
  int error = Wire.endTransmission();

  // Wire.requestFrom(ADC_ADDRESS, 2); // Request 2 bytes of data
  Wire.requestFrom(ADC_ADDRESS, 2); // Request 2 bytes of data

  if (Wire.available() == 2) {
    uint8_t msb = Wire.read();   // Read MSB
    uint8_t lsb = Wire.read();   // Read LSB
    data = (msb << 8) | lsb;     // Combine MSB and LSB into a 16-bit result
    
    // Extract the 12-bit result
    int adcValue = data & 0x0FFF;  // Mask to get only the lower 12 bits

    // Serial.print("Combined Data (16-bit): ");
    // Serial.println(data, DEC);         // Print the full 16-bit value
    // Serial.print("ADC Value (12-bit): ");
    // Serial.println(adcValue);
    data = adcValue;
    // data = lsb;

  }
  else {
    Serial.println(F("Error: ADC result not available."));
    data = 1000;  // Default error value
  }

  return map(data, 0, 4095, LRange, HRange);
}

int Configure(){
  Wire.beginTransmission(ADC_ADDRESS);
  Wire.write(CONFIG_REG);
  Wire.write(0x18); //0001 1000
  int error = Wire.endTransmission();
  
  return error;
}

// Function to trigger ADC conversion using CONVST pin (Mode-1)
void triggerConversion() {
  digitalWrite(CONVST_PIN, HIGH); // Set CONVST high to power up the ADC
  delayMicroseconds(0);            // Wait at least 1 microsecond
  digitalWrite(CONVST_PIN, LOW);   // Set CONVST low to initiate conversion
  delayMicroseconds(0);            // Wait 2 microseconds for conversion to complete
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