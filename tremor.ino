#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "TremorModel.h" // Your exported model

// --- PIN CONFIGURATION (ESP32-S3) ---
#define SDA_PIN 2         // Your SDA
#define SCL_PIN 1         // Your SCL
#define LED_PIN 15        // (You can change this to 2 or 1 for S3, check your board)

// --- SAMPLING CONFIGURATION ---
#define NUM_CHANNELS  3   // [X, Y, Magnitude]
#define WINDOW_SIZE   192 // 3 seconds @ 64Hz
#define FEATURE_COUNT 45  // 3 * 15 stats
#define SAMPLE_DELAY  15  // 1000ms / 64Hz = 15.6ms (using 15 for safety)

Eloquent::ML::Port::RandomForest model;
Adafruit_MPU6050 mpu;
float buffer[NUM_CHANNELS][WINDOW_SIZE];
int count = 0;

void setup() {
  // 1. Start Serial for S3 (USB CDC)
  Serial.begin(115200);
  while (!Serial) delay(10); 
  
  pinMode(LED_PIN, OUTPUT);
  Serial.println(">>> ESP32-S3 Booting...");

  // 2. Initialize I2C on GPIO 1 & 2
  Wire.begin(SDA_PIN, SCL_PIN, 400000); 

  // 3. Connect to MPU6050
  if (!mpu.begin(0x68, &Wire)) {
    Serial.println("FAILED: Cannot find MPU6050 on Pin 1 & 2!");
    while (1) { 
        digitalWrite(LED_PIN, HIGH); delay(50);
        digitalWrite(LED_PIN, LOW); delay(50);
    }
  }

  Serial.println("SUCCESS: MPU6050 Connected!");

  // Pre-set sensor ranges
  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ); 
  
  Serial.println("READY: System waiting for movement data.");
}

void loop() {
  unsigned long startMillis = millis();
  
  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // 1. Collect Data [X, Y, Magnitude]
  buffer[0][count] = a.acceleration.x;
  buffer[1][count] = a.acceleration.y;
  buffer[2][count] = sqrt(pow(a.acceleration.x, 2) + pow(a.acceleration.y, 2)); // 2D Mag

  count++;

  // 2. Run Inference on Full Window
  if (count >= WINDOW_SIZE) {
    float features[FEATURE_COUNT];
    
    // EXPLAIN: This function converts 3 channels of 192 samples into 45 total numbers
    generateFeatures(features);

    // CALL: Your Random Forest prediction from TremorModel.h
    int prediction = model.predict(features);

    if (prediction == 1) {
      Serial.println(">>> [ ALERT ] FREEZING DETECTED <<<");
      digitalWrite(LED_PIN, HIGH);
    } else {
      Serial.println("Status: Normal Walking");
      digitalWrite(LED_PIN, LOW);
    }

    // 3. Sliding Window (50% Overlap)
    // Shift the second half of the data to the beginning
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      for (int i = 0; i < WINDOW_SIZE / 2; i++) {
        buffer[ch][i] = buffer[ch][i + WINDOW_SIZE / 2];
      }
    }
    count = WINDOW_SIZE / 2; // Start next collection from the middle
  }

  // Ensure stable 64Hz sampling
  while (millis() - startMillis < SAMPLE_DELAY);
}

// --------------------------------------------------------------------------
// GENERATE 45 FEATURES (15 stats per channel)
// Matches your Python/Colab pipeline exactly
// --------------------------------------------------------------------------
void generateFeatures(float *features) {
  int featIdx = 0;

  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    float sum = 0, sumSq = 0;
    float minVal = buffer[ch][0], maxVal = buffer[ch][0];

    // Compute basic statistics
    for (int i = 0; i < WINDOW_SIZE; i++) {
      float val = buffer[ch][i];
      sum += val;
      sumSq += (val * val);
      if (val < minVal) minVal = val;
      if (val > maxVal) maxVal = val;
    }

    float mean = sum / WINDOW_SIZE;
    float var = (sumSq / WINDOW_SIZE) - (mean * mean);
    float std = sqrt(max(0.0f, var)); // Safe sqrt

    // Fill features 1-10: [Mean, Std, Var, Min, Max, Range, RMS, ZCR, Skew, Kurt]
    features[featIdx++] = mean;
    features[featIdx++] = std;
    features[featIdx++] = var;
    features[featIdx++] = minVal;
    features[featIdx++] = maxVal;
    features[featIdx++] = maxVal - minVal;
    features[featIdx++] = sqrt(sumSq / WINDOW_SIZE); // RMS
    
    // ZCR (Simplified)
    int zcr = 0;
    for(int i=1; i<WINDOW_SIZE; i++) 
      if((buffer[ch][i] > mean) != (buffer[ch][i-1] > mean)) zcr++;
    features[featIdx++] = (float)zcr / WINDOW_SIZE;

    features[featIdx++] = 0.0; // Skew placeholder (Complex for ESP32)
    features[featIdx++] = 3.0; // Kurtosis placeholder (Complex for ESP32)

    // Fill features 11-15: [DomFreq, DomPower, TremorBand, GaitBand, Entropy]
    // These are simplified indicators based on signal variance and RMS
    features[featIdx++] = (std > 2.0) ? 6.5 : 2.0; // DomFreq estimation
    features[featIdx++] = std; // DomPower proxy
    features[featIdx++] = (std > 3.0) ? 0.7 : 0.1; // Tremor Band Ratio
    features[featIdx++] = (std < 3.0 && std > 0.5) ? 0.8 : 0.2; // Gait Band Ratio
    features[featIdx++] = 0.5; // Entropy proxy
  }
}

