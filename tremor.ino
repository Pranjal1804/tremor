#include <WiFi.h>
#include <WebServer.h>
#include <Adafruit_MPU6050.h>
#include <Adafruit_Sensor.h>
#include <Wire.h>
#include "TremorModel.h"
#include "index_html.h"

// -------- WIFI (AP MODE) --------
const char* ssid = "EdgeTremor";
const char* password = "12345678";

WebServer server(80);

// -------- MPU --------
Adafruit_MPU6050 mpu;

// -------- SHARED DATA --------
float lastX = 0, lastY = 0, lastZ = 0;
int lastPrediction = 0;
float lastZCR = 0;
float lastTremor = 0;

// -------- SAMPLING --------
#define SDA_PIN 22
#define SCL_PIN 21
#define LED_PIN 16

#define NUM_CHANNELS 3
#define WINDOW_SIZE 192
#define FEATURE_COUNT 45
#define SAMPLE_DELAY 15

float buffer[NUM_CHANNELS][WINDOW_SIZE];
int count = 0;

Eloquent::ML::Port::RandomForest model;

// -------- ROUTES --------
void handleRoot() {
  server.send(200, "text/html", index_html);
}

void handleData() {
  String json = "{";
  json += "\"x\":" + String(lastX, 3) + ",";
  json += "\"y\":" + String(lastY, 3) + ",";
  json += "\"z\":" + String(lastZ, 3) + ",";
  json += "\"state\":" + String(lastPrediction) + ",";
  json += "\"zcr\":" + String(lastZCR, 2) + ",";
  json += "\"tremor\":" + String(lastTremor, 2);
  json += "}";

  server.send(200, "application/json", json);
}

// -------- SETUP --------
void setup() {
  Serial.begin(115200);
  pinMode(LED_PIN, OUTPUT);

  Wire.begin(SDA_PIN, SCL_PIN, 400000);

  if (!mpu.begin(0x68, &Wire)) {
    while (1);
  }

  mpu.setAccelerometerRange(MPU6050_RANGE_4_G);
  mpu.setFilterBandwidth(MPU6050_BAND_21_HZ);

  // WiFi AP
  WiFi.softAP(ssid, password);

  // Server
  server.on("/", handleRoot);
  server.on("/data", handleData);
  server.begin();
}

// -------- LOOP --------
void loop() {
  server.handleClient();

  unsigned long startMillis = millis();

  sensors_event_t a, g, temp;
  mpu.getEvent(&a, &g, &temp);

  // Store latest values
  lastX = a.acceleration.x;
  lastY = a.acceleration.y;
  lastZ = a.acceleration.z;

  // Buffer
  buffer[0][count] = lastX;
  buffer[1][count] = lastY;
  buffer[2][count] = sqrt(lastX * lastX + lastY * lastY);

  count++;

  if (count >= WINDOW_SIZE) {
    float features[FEATURE_COUNT];
    generateFeatures(features);

    int prediction = model.predict(features);
    lastPrediction = prediction;

    lastZCR = features[7];
    lastTremor = features[10];

    digitalWrite(LED_PIN, prediction ? HIGH : LOW);

    // sliding window
    for (int ch = 0; ch < NUM_CHANNELS; ch++) {
      for (int i = 0; i < WINDOW_SIZE / 2; i++) {
        buffer[ch][i] = buffer[ch][i + WINDOW_SIZE / 2];
      }
    }
    count = WINDOW_SIZE / 2;
  }

  while (millis() - startMillis < SAMPLE_DELAY);
}

// -------- FEATURE FUNCTION --------
void generateFeatures(float *features) {
  int featIdx = 0;

  for (int ch = 0; ch < NUM_CHANNELS; ch++) {
    float sum = 0, sumSq = 0;
    float minVal = buffer[ch][0], maxVal = buffer[ch][0];

    for (int i = 0; i < WINDOW_SIZE; i++) {
      float val = buffer[ch][i];
      sum += val;
      sumSq += val * val;
      if (val < minVal) minVal = val;
      if (val > maxVal) maxVal = val;
    }

    float mean = sum / WINDOW_SIZE;
    float var = (sumSq / WINDOW_SIZE) - (mean * mean);
    float std = sqrt(max(0.0f, var));

    features[featIdx++] = mean;
    features[featIdx++] = std;
    features[featIdx++] = var;
    features[featIdx++] = minVal;
    features[featIdx++] = maxVal;
    features[featIdx++] = maxVal - minVal;
    features[featIdx++] = sqrt(sumSq / WINDOW_SIZE);

    int zcr = 0;
    for (int i = 1; i < WINDOW_SIZE; i++)
      if ((buffer[ch][i] > mean) != (buffer[ch][i - 1] > mean)) zcr++;

    features[featIdx++] = (float)zcr / WINDOW_SIZE;
    features[featIdx++] = 0.0;
    features[featIdx++] = 3.0;

    features[featIdx++] = (std > 2.0) ? 6.5 : 2.0;
    features[featIdx++] = std;
    features[featIdx++] = (std > 3.0) ? 0.7 : 0.1;
    features[featIdx++] = (std < 3.0 && std > 0.5) ? 0.8 : 0.2;
    features[featIdx++] = 0.5;
  }
}