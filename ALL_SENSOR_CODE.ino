#include <Arduino.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <WiFi.h>
#include <HTTPClient.h>

// -------------------- CONFIG --------------------
#define VREF 3.3
#define SCOUNT 30  

const char* mySsid = "SUBHAM 2387";
const char* myPassword = "Subham2005";

const char* serverName = "https://api.thingspeak.com/update";
String apiKey = "CUG8HYAPOP9CZ101";

// Pin definitions
#define PIN_TDS   34
#define PIN_TURB  35
#define PIN_PH    32
#define PIN_TEMP  23
#define PIN_BUZZ  26    // 🔹 Buzzer connected to GPIO 26

#define TEMP_THRESHOLD 30.0   // 🔹 Temperature limit in °C

// -------------------- SENSOR CLASSES --------------------
class TDSSensor {
private:
  int pin;
  int buffer[SCOUNT];
  int index;

public:
  TDSSensor(int p) : pin(p), index(0) {}
  void begin() { pinMode(pin, INPUT); }

  float read() {
    buffer[index] = analogRead(pin);
    index = (index + 1) % SCOUNT;

    int sum = 0;
    for (int i = 0; i < SCOUNT; i++) sum += buffer[i];
    int avg = sum / SCOUNT;

    float voltage = avg * (VREF / 4095.0);
    return (133.42 * voltage * voltage * voltage
          - 255.86 * voltage * voltage
          + 857.39 * voltage) * 0.5;
  }
};

class TurbiditySensor {
private:
  int pin;
public:
  TurbiditySensor(int p) : pin(p) {}
  void begin() { pinMode(pin, INPUT); }
  float read() {
    int raw = analogRead(pin);
    float voltage = raw * (VREF / 4095.0);
    return -1120.4 * voltage * voltage + 5742.3 * voltage - 4352.9;
  }
};

class PHSensor {
private:
  int pin;
  float calibration_offset;
public:
  PHSensor(int p, float offset = 0.0) : pin(p), calibration_offset(offset) {}
  void begin() { pinMode(pin, INPUT); }
  float read() {
    int raw = analogRead(pin);
    float voltage = (raw / 4095.0) * VREF;
    return 7 + ((2.5 - voltage) / 0.18) + calibration_offset;
  }
};

class TempSensor {
private:
  OneWire oneWire;
  DallasTemperature sensors;
public:
  TempSensor(int pin) : oneWire(pin), sensors(&oneWire) {}
  void begin() { sensors.begin(); }
  float read() {
    sensors.requestTemperatures();
    return sensors.getTempCByIndex(0);
  }
};

// -------------------- OBJECTS --------------------
TDSSensor tds(PIN_TDS);
TurbiditySensor turb(PIN_TURB);
PHSensor ph(PIN_PH, 0.0);
TempSensor temp(PIN_TEMP);

unsigned long lastSendTime = 0;

// -------------------- MAIN SETUP --------------------
void setup() {
  Serial.begin(115200);
  delay(200);

  pinMode(PIN_BUZZ, OUTPUT);   // 🔹 Initialize buzzer pin
  digitalWrite(PIN_BUZZ, LOW); // Start with buzzer OFF

  Serial.println("Connecting to WiFi...");
  WiFi.begin(mySsid, myPassword);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("\nWiFi connected!");
  Serial.println(WiFi.localIP());

  tds.begin();
  turb.begin();
  ph.begin();
  temp.begin();
}

// -------------------- MAIN LOOP --------------------
void loop() {
  float tdsVal  = tds.read();
  float turbVal = turb.read();
  float phVal   = ph.read();
  float tempVal = temp.read();

  Serial.print("TDS: "); Serial.println(tdsVal);
  Serial.print("Turbidity: "); Serial.println(turbVal);
  Serial.print("pH: "); Serial.println(phVal);
  Serial.print("Temperature: "); Serial.println(tempVal);

  // 🔹 Buzzer logic
  if (tempVal > TEMP_THRESHOLD) {
    digitalWrite(PIN_BUZZ, HIGH);
    Serial.println("⚠️ ALERT: High Temperature! Buzzer ON");
  } else {
    digitalWrite(PIN_BUZZ, LOW);
  }

  // 🔹 Send to ThingSpeak every 15 seconds
  if (millis() - lastSendTime > 15000) {
    if (WiFi.status() == WL_CONNECTED) {
      HTTPClient http;
      String url = String(serverName) + "?api_key=" + apiKey +
                   "&field1=" + String(tempVal, 2) +
                   "&field2=" + String(phVal, 2) +
                   "&field3=" + String(turbVal, 2) +
                   "&field4=" + String(tdsVal, 2);
      http.begin(url);
      int httpResponseCode = http.GET();
      if (httpResponseCode > 0) {
        Serial.print("ThingSpeak response: ");
        Serial.println(httpResponseCode);
      } else {
        Serial.print("Error sending data: ");
        Serial.println(http.errorToString(httpResponseCode));
      }
      http.end();
    } else {
      Serial.println("WiFi not connected. Retrying...");
    }
    lastSendTime = millis();
  }

  delay(1000);
}
