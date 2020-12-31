#define LARGE_JSON_BUFFERS 1
#define ARDUINOJSON_USE_LONG_LONG 1

#include <Ticker.h>
#include <Arduino.h>
#include <ArduinoJson.h>

// webthings
#include <WebThingAdapter.h>
#include <Thing.h>

// Network
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>

// OTA
#include <ESP8266HTTPClient.h>
#include <ESP8266httpUpdate.h>

// PMS
#include <PMS.h>

// BME280
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#define BME_SCK 13
#define BME_MISO 12
#define BME_MOSI 11
#define BME_CS 10

#define SEALEVELPRESSURE_HPA (1013.25)

#define DEVICE_NAME "esp-airq-thing"

#define OTA_REMOTE_URL ""

WebThingAdapter *adapter;
Ticker checkPropTimer;
PMS pms(Serial);
PMS::DATA data;
Adafruit_BME280 bme;

const char *airqTypes[] = {"AirQualitySensor", nullptr};
ThingDevice airqSensor("airq", DEVICE_NAME, airqTypes);
ThingProperty pm1Prop("PM1.0", "Level of PM1.0 particles", INTEGER, "DensityProperty");
ThingProperty pm2_5Prop("PM2.5", "Level of PM2.5 particles", INTEGER, "DensityProperty");
ThingProperty pm10Prop("PM10", "Level of PM10 particles", INTEGER, "DensityProperty");
ThingProperty temperatureProp("Temperature", "Temperature", NUMBER, "TemperatureProperty");
ThingProperty humidityProp("Humidity", "Humidity", NUMBER, "LevelProperty");
ThingProperty pressureProp("Pressure", "Pressure", NUMBER, "BarometricPressureProperty");

void setupWiFi(String deviceName) {
  WiFi.mode(WIFI_STA);
  WiFi.hostname(deviceName.c_str());
  WiFi.setAutoReconnect(true);

  bool blink = true;
  if (WiFi.SSID() == "") {
    WiFi.beginSmartConfig();

    while (!WiFi.smartConfigDone()) {
      delay(1000);
      digitalWrite(LED_BUILTIN, blink ? HIGH : LOW);
      blink = !blink;
    }
  }

  WiFi.begin();

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    digitalWrite(LED_BUILTIN, blink ? HIGH : LOW);
    blink = !blink;
  }

  WiFi.stopSmartConfig();

  digitalWrite(LED_BUILTIN, HIGH);
}

void setupBME280() {
  bool blink = true;
  while (!bme.begin(0x76, &Wire)) {
    delay(300);
    digitalWrite(LED_BUILTIN, blink ? HIGH : LOW);
    blink = !blink;
  }

  digitalWrite(LED_BUILTIN, HIGH);
}

void setupWebThing(String deviceName) {
  pm1Prop.readOnly = true;
  pm1Prop.title = "PM 1.0";

  pm2_5Prop.readOnly = true;
  pm2_5Prop.title = "PM 2.5";

  pm10Prop.readOnly = true;
  pm10Prop.title = "PM 10";

  humidityProp.readOnly = true;
  humidityProp.unit = "percent";
  humidityProp.title = "Humidity";
  humidityProp.minimum = 0;
  humidityProp.maximum = 100;

  pressureProp.readOnly = true;
  pressureProp.unit = "hPa";
  pressureProp.title = "Pressure";

  adapter = new WebThingAdapter(deviceName, WiFi.localIP());
  airqSensor.addProperty(&pm1Prop);
  airqSensor.addProperty(&pm2_5Prop);
  airqSensor.addProperty(&pm10Prop);
  airqSensor.addProperty(&temperatureProp);
  airqSensor.addProperty(&humidityProp);
  airqSensor.addProperty(&pressureProp);
  adapter->addDevice(&airqSensor);
  adapter->begin();
}

void readBME280Data() {
  ThingPropertyValue value;

  float temp = bme.readTemperature(); // C
  if (temp != temperatureProp.getValue().number) {
    value.number = temp;
    temperatureProp.setValue(value);
  }

  float hum = bme.readHumidity(); // %
  if (hum != humidityProp.getValue().number) {
    value.number = hum;
    humidityProp.setValue(value);
  }

  float pressure = bme.readPressure() / 100.0F; // hPa
  if (pressure != pressureProp.getValue().number) {
    value.number = pressure;
    pressureProp.setValue(value);
  }
}

void readPMSSensorData() {
  if (pms.readUntil(data)) {
    // (ug/m3)
    if (data.PM_AE_UG_1_0 != pm1Prop.getValue().integer) {
      ThingPropertyValue value;
      value.integer = data.PM_AE_UG_1_0;
      pm1Prop.setValue(value);
    }

    if (data.PM_AE_UG_2_5 != pm2_5Prop.getValue().integer) {
      ThingPropertyValue value;
      value.integer = data.PM_AE_UG_2_5;
      pm2_5Prop.setValue(value);
    }

    if (data.PM_AE_UG_10_0 != pm10Prop.getValue().integer) {
      ThingPropertyValue value;
      value.integer = data.PM_AE_UG_10_0;
      pm10Prop.setValue(value);
    }
  }
}

void checkProps() {
  MDNS.update();

  readPMSSensorData();
  readBME280Data();

  adapter->update();
}

void blinkNTimes(int n) {
  bool blink = true;
  for (int i = 0; i < n; i++) {
    digitalWrite(LED_BUILTIN, blink ? HIGH : LOW);
    blink = !blink;
    delay(200);
  }
}

void startRemoteUpdate() {
  digitalWrite(LED_BUILTIN, LOW);
  ESPhttpUpdate.setLedPin(LED_BUILTIN, LOW);

  blinkNTimes(10);

  WiFiClient client;
  t_httpUpdate_return ret = ESPhttpUpdate.update(client, OTA_REMOTE_URL);

  if (ret == HTTP_UPDATE_FAILED) {
    blinkNTimes(10);
  }
  
  digitalWrite(LED_BUILTIN, HIGH);
}

void setup()
{
  // put your setup code here, to run once:
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.begin(9600);

  setupBME280();

  String deviceName(DEVICE_NAME);
  deviceName.concat("-");
  deviceName.concat(ESP.getChipId());
  deviceName.toLowerCase();

  setupWiFi(deviceName);
  setupWebThing(deviceName);

  checkPropTimer.attach(5, checkProps);
}

void loop()
{
  // put your main code here, to run repeatedly:
  //readPMSSensorData();
  //readBME280Data();
  //adapter->update();
}
