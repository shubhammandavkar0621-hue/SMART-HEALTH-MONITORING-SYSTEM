#define BLYNK_TEMPLATE_ID "TMPL3FZ3q_hpn"
#define BLYNK_TEMPLATE_NAME "Smart Health Monitoring System"
#define BLYNK_AUTH_TOKEN "6ezDArMeyw-afWHjFQ9HwCEmdZY4LGcX"

#include <WiFi.h>
#include <BlynkSimpleEsp32.h>
#include <Wire.h>
#include "MAX30100_PulseOximeter.h"
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

// OLED Setup
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, -1);

// LED Alert Pin
#define ALERT_LED 2 // Built-in LED on most ESP32s, or connect external LED here

// WiFi credentials
char ssid[] = "BARKU06";
char pass[] = "12345678";

// MAX30100 Pulse Oximeter
PulseOximeter pox;
uint32_t tsLastReport = 0;
#define REPORTING_PERIOD_MS 2000 

// DS18B20 (Body Temp)
#define ONE_WIRE_BUS 4
OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature bodyTempSensor(&oneWire);
float currentBodyTemp = 0;

// DHT11 (Room Temp)
#define DHTPIN 16
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);
float currentRoomTemp = 0;

BlynkTimer timer;
bool ledState = LOW; // For blinking logic

void onBeatDetected() {
    Serial.println("Beat!");
}

void readOtherSensors() {
    bodyTempSensor.requestTemperatures();
    float tempC = bodyTempSensor.getTempCByIndex(0);
    if (tempC != DEVICE_DISCONNECTED_C) {
        currentBodyTemp = (tempC * 1.8) + 32.0;
        Blynk.virtualWrite(V2, currentBodyTemp);
    }

    float roomTemp = dht.readTemperature();
    if (!isnan(roomTemp)) {
        currentRoomTemp = roomTemp;
        Blynk.virtualWrite(V3, currentRoomTemp);
    }
}

void setup() {
    Serial.begin(115200);
    
    // Initialize Alert LED
    pinMode(ALERT_LED, OUTPUT);
    digitalWrite(ALERT_LED, LOW);

    if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) { 
        Serial.println(F("SSD1306 allocation failed"));
    }
    display.clearDisplay();
    display.setTextColor(WHITE);
    display.setCursor(10, 20);
    display.print("Initializing...");
    display.display();

    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pass);
    dht.begin();
    bodyTempSensor.begin();

    Serial.print("Initializing MAX30100..");
    if (!pox.begin()) {
        Serial.println("FAILED");
        display.clearDisplay();
        display.setCursor(0,0);
        display.print("MAX30100 Error");
        display.display();
    } else {
        Serial.println("SUCCESS");
        pox.setIRLedCurrent(MAX30100_LED_CURR_7_6MA); 
        pox.setOnBeatDetectedCallback(onBeatDetected);
    }

    timer.setInterval(3000L, readOtherSensors);
}

void loop() {
    pox.update();
    Blynk.run();
    timer.run();

    if (millis() - tsLastReport > REPORTING_PERIOD_MS) {
        float hr = pox.getHeartRate();
        float spo2 = pox.getSpO2();

        // --- LED BLINK LOGIC ---
        // Checks if SpO2 is below 94% or Heart Rate is below 60 BPM
        // (Note: SpO2 needs to be > 0 to avoid triggering on a disconnected sensor)
        if ((spo2 > 0 && spo2 < 94) || (hr > 0 && hr < 60)) {
            ledState = !ledState; // Toggle state to create blinking effect
            digitalWrite(ALERT_LED, ledState);
        } else {
            digitalWrite(ALERT_LED, LOW); // Turn off if everything is normal
        }

        Blynk.virtualWrite(V0, hr);
        Blynk.virtualWrite(V1, spo2);

        display.clearDisplay();
        display.setTextSize(1);
        display.setCursor(0, 0);
        display.print("BPM: ");
        display.setTextSize(2);
        display.print(hr, 0);

        display.setTextSize(1);
        display.setCursor(70, 0);
        display.print("SpO2: ");
        display.setTextSize(2);
        display.print(spo2, 0);
        display.print("%");

        display.drawLine(0, 30, 128, 30, WHITE);

        display.setTextSize(1);
        display.setCursor(0, 38);
        display.print("Body: ");
        display.print(currentBodyTemp, 1);
        display.print(" F");

        display.setCursor(0, 52);
        display.print("Room: ");
        display.print(currentRoomTemp, 1);
        display.print(" C");

        display.display();
        tsLastReport = millis();
    }
}