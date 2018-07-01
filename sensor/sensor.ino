#include <Arduino.h>
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <OneWire.h>
#include <DallasTemperature.h>
//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>         //https://github.com/tzapu/WiFiManager
#include <time.h>
#include <Wire.h>
#include <SSCI_BME280.h>
#include <PubSubClient.h>

extern "C" {
#include <sntp.h>
#include <user_interface.h>
};

#define WIFI_SSID "yourssid"
#define WIFI_PASS "yourpass"
#define MQTT_SERVER "yourmqtt"
#define DEFAULT_SEND_INTERVAL_STR "600"

#define ONE_WIRE_BUS (13)
#define RST_WIFI_SETTING (14)

#define BME280_ADDRESS (0x76)

#define JST (3600*9)
#define MICROSECONDS (1000 * 1000)
#define MILLISECONDS (1000)
#define CORR (1)
#define VALID_TIME (1514732400)

#define FNV_OFFSET_BASIS_32 (2166136261U)
#define FNV_PRIME_32 (16777619U)

OneWire oneWire(ONE_WIRE_BUS);
DallasTemperature sensors(&oneWire);
ESP8266WebServer server(80);
SSCI_BME280 bme280;

bool shouldSaveConfig = false;

#pragma pack(1)
typedef struct _rtc_data_t {
  uint32_t hash;
  char mqttserver[16];
  uint32_t sendinterval;
  time_t lasttime;
} RtcData;
#pragma pack()

RtcData rtcData;

void setup() {
  // put your setup code here, to run once:
  Serial.begin(115200);

  uint8_t osrs_t = 1;             //Temperature oversampling x 1
  uint8_t osrs_p = 1;             //Pressure oversampling x 1
  uint8_t osrs_h = 1;             //Humidity oversampling x 1
  uint8_t bme280mode = 3;         //Normal mode
  uint8_t t_sb = 5;               //Tstandby 1000ms
  uint8_t filter = 0;             //Filter off
  uint8_t spi3w_en = 0;           //3-wire SPI Disable

  Wire.begin();
  bme280.setMode(BME280_ADDRESS, osrs_t, osrs_p, osrs_h, bme280mode, t_sb, filter, spi3w_en);
  bme280.readTrim();

  sensors.begin();

  long start = millis();
  
  float temp = getTemp();
  float vout = calcVout();

  double t, p, h;
  bme280.readData(&t, &p, &h);

  ESP.rtcUserMemoryRead(0, (uint32_t*)&rtcData, sizeof(rtcData));
  uint32_t hash = calc_hash(rtcData);

  bool valid = false;
  if (rtcData.hash == hash) {
    valid = true;
  }
  Serial.print("valid="); Serial.println(valid);

 #ifdef USE_WIFI_MANAGER
  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  WiFiManager wifiManager;

  if (!valid) {
    Serial.println("Reset Wifi Setting.");
    //reset saved settings
    wifiManager.resetSettings();
  }
  
  //set custom ip for portal
  //wifiManager.setAPStaticIPConfig(IPAddress(10,0,1,1), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  WiFiManagerParameter custom_mqtt_server("mqttserver", "MQTT Server", "", 16);
  WiFiManagerParameter custom_send_interval("sendinterval", "Send Interval", DEFAULT_SEND_INTERVAL_STR, 20);
  wifiManager.setSaveConfigCallback(saveConfigCallback);
  wifiManager.addParameter(&custom_mqtt_server);
  wifiManager.addParameter(&custom_send_interval);
  
  
  //fetches ssid and pass from eeprom and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  //wifiManager.autoConnect("AutoConnectAP");
  //or use this for auto generated name ESP + ChipID
  wifiManager.autoConnect();
#else
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASS);
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Connection Failed! Rebooting...");
    uint32_t sleeptime = (rtcData.sendinterval * MICROSECONDS - (millis() - start) * MILLISECONDS) / CORR;
    ESP.deepSleep(sleeptime, WAKE_RF_DEFAULT);
    delay(1000);
  }
#endif

  //if you get here you have connected to the WiFi
  Serial.println("connected.");

  char mqttserver[16];
  char sendinterval[20];
#ifdef USE_WIFI_MANAGER
  strcpy(sendinterval, custom_send_interval.getValue());
  strcpy(mqttserver, custom_mqtt_server.getValue());
  if (shouldSaveConfig) {
    strcpy(rtcData.mqttserver, mqttserver);
    rtcData.sendinterval = strtoul(sendinterval, NULL, 10);
  }
#else
  strcpy(rtcData.mqttserver, MQTT_SERVER);
  rtcData.sendinterval = strtoul(DEFAULT_SEND_INTERVAL_STR, NULL, 10);
#endif
  Serial.println(rtcData.mqttserver);
  Serial.println(rtcData.sendinterval);
  
  IPAddress ipaddr = WiFi.localIP();
  uint8_t mac[6];
  WiFi.macAddress(mac);

  Serial.printf("%f,%f\n", temp, vout);
  Serial.printf("%f,%f,%f\n", t, p, h);

  Serial.printf("%d.%d.%d.%d\n", ipaddr[0], ipaddr[1], ipaddr[2], ipaddr[3]);
  char devid[20];
  sprintf(devid, "%02x%02x%02x%02x%02x%02x", mac[0], mac[1], mac[2], mac[3], mac[4], mac[5]);
  Serial.println(devid);

  time_t tm = getTimestamp() - ((millis() - start) / MILLISECONDS);
  rtcData.lasttime = tm;
  Serial.println(tm);

  char json[256];
  sprintf(json, "{\"time\":%d,\"ambient\":{\"temparature\":%.2f,\"humidity\":%.2f,\"pressure\":%.2f},\"aquarium\":{\"temparature\":%.2f},\"battery\":%.2f}",
    tm, t, h, p, temp, vout);

  Serial.println(json);

  sendData(devid, json);
  
  rtcData.hash = calc_hash(rtcData);
  ESP.rtcUserMemoryWrite(0, (uint32_t*)&rtcData, sizeof(rtcData));

  WiFi.disconnect();
  Serial.println("disconnect.");

  uint32_t sleeptime = (rtcData.sendinterval * MICROSECONDS - (millis() - start) * MILLISECONDS) / CORR;
  ESP.deepSleep(sleeptime, WAKE_RF_DEFAULT);
  delay(1000);
}

void loop() {

}

float getTemp() {
  sensors.requestTemperatures();
  float temp = sensors.getTempCByIndex(0);

  return temp;  
}

float calcVout() {
  int adc = system_adc_read();
  float vout = (float)adc / 1024.0f / 20.0f * 120.0f;

  return vout;
}

time_t getTimestamp() {
  configTime(0, 0, "ntp.nict.jp", NULL, NULL);

  time_t t = 0;
  int cnt = 0;
  while (t < JST) {
    t = time(NULL);
    delay(500);
    if (++cnt > 10) {
      break;
    }
  }
  return t;
}

uint32_t hash_fnv_1_32(uint8_t *bytes, size_t length)
{
    uint32_t hash;
    size_t i;

    hash = FNV_OFFSET_BASIS_32;
    for( i = 0 ; i < length ; ++i) {
        hash = (FNV_PRIME_32 * hash) ^ (bytes[i]);
    }

    return hash;
}

uint32_t calc_hash(RtcData &data) {
  return hash_fnv_1_32(((uint8_t*)&data) + sizeof(data.hash), sizeof(RtcData) - sizeof(data.hash));
}

void saveConfigCallback() {
  shouldSaveConfig = true;  
}

void sendData(char* devid, char* data) {
  WiFiClient espClient;
  PubSubClient client(espClient);

  client.setServer(rtcData.mqttserver, 1883);
  client.setCallback(pubSubCallback);

  int retry = 5;
  while (!client.connected()) {
    if (client.connect("ESP-Sensor")) {
      Serial.println("MQTT connected.");
      break;
    }
    Serial.print(".");
    delay(1000);
    retry--;
    if (retry == 0) {
      Serial.println("MQTT connect failed!");
      return;
    }
  }
  
  Serial.print("publish");
  char topic[50];
  sprintf(topic, "sensor/%s", devid);
  if (client.publish(topic, data)) {
    Serial.println(" done.");
  } else {
    Serial.println(" failed.");
  }
  client.loop();
  client.disconnect();
  Serial.println("MQTT disconnect.");
}

void pubSubCallback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
      Serial.print((char)payload[i]);
  }
  Serial.println();
}

