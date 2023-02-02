#define WM_NODEBUG
#include <WiFiManager.h>  // https://github.com/tzapu/WiFiManager
// include MDNS
#ifdef ESP8266
#include <ESP8266mDNS.h>
#elif defined(ESP32)
#include <ESPmDNS.h>
#endif
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_ADS1X15.h>
#include "DAC_cmd.h"
#include <WebServer.h>
#include "timer.h"
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <String.h>
#include "sha256.h"
#include "Base64.h"
#include <ArduinoJson.h>
#include <EasyButton.h>
#include <NeoPixelBus.h>
#include <NeoPixelAnimator.h>
#include <Preferences.h>
#include "cert.h"
#define CONFIG_PIN 2
#define MAX_I2C_ADDR 10
#define buffersize_i2c 1000
#define RXD2 16
#define TXD2 17
#define PRECISION 3
#define MAX_SENSORS 7
String FirmwareVer = {
  "1.0"
};
#define URL_fw_Version "https://raw.githubusercontent.com/kiwi85/iforce/main/Iforce_AU_Azure_ESP32_standalone/build/esp32.esp32.esp32/version.txt"
#define URL_fw_Bin "https://raw.githubusercontent.com/kiwi85/iforce/main/Iforce_AU_Azure_ESP32_standalone/build/esp32.esp32.esp32/Iforce_AU_Azure_ESP32_standalone.ino.bin"

/*declarations for aquisition unit */
uint8_t cell_num, sensor_num, phase_num, measures = 1;
uint8_t sensor_start = 1;  //sensor selecetion
int delaytime = 5;
Adafruit_ADS1115 ads;
const float multiplier = 0.1875;  //ADS1115
int round_num = 1;
int level_high = 2845;  //adc parameter
int level_medium = 2345;
int level_low = 1860;
float received_val1 = 0, received_val2 = 0, received_val3 = 0;
int bVolt[12] = { 1050, 2100, 3000, 1050, 2100, 3000, 0, 0, 0, 0, 0, 0 };
int cVolt[12] = { 0, 0, 0, 0, 0, 0, 800, 1600, 2400, 800, 1600, 2400 };
int voltCount = 0;
TaskHandle_t Task_sensor_data_gen;
TaskHandle_t Task_sensor_data_send;
QueueHandle_t queue;
StaticJsonDocument<1024> doc_iforce;

/*declarations for Neopixels */
const uint16_t PixelCount = 1;  // make sure to set this to the number of pixels in your strip
const uint8_t PixelPin = 12;    // make sure to set this to the correct pin, ignored for Esp8266



// START: Azure Evet Hub settings
const int httpsPort = 443;
String azure_host, azure_key, azure_keyname, azure_endpoint;
float azure_publish_speed = 1000;
float upd_interval = 60;
bool simulation = false;
String request, http_response, data;
char chip_id[10];
uint8_t i2c_device_adr[MAX_I2C_ADDR];

String fullSas;

WiFiClientSecure client;
Preferences preferences;
EasyButton button_config(CONFIG_PIN, 35, false, true);

Timer data_timer;
Timer status_timer;
Timer timer_update;
Timer timer_idle;
String data_json;
String status;
String status_serial;
int content_length = -1;
int http_status_code = -1;

enum status { IDLE,
              CONNECTING,
              SENDING,
              CONFIG,
              ERROR,
              UPDATE,
              RESET
};
int state = IDLE;


WiFiManager wm;
WebServer server(80);
/*
Wifi manager Settings
*/
// END: WiFi settings
WiFiManagerParameter custom_azure_host("host", "host", "iforce-test.servicebus.windows.net", 80);
WiFiManagerParameter custom_azure_endpoint("endpoint", "end point", "/device1/messages/", 80);
WiFiManagerParameter custom_azure_key("key", "key", "YffB2uluTpVFfj0D3LoRdHz9+I1JIqwDr3wF3rjsV44=", 100);
WiFiManagerParameter custom_azure_keyname("keyname", "key name", "RootManageSharedAccessKey", 50);
WiFiManagerParameter custom_azure_pubspeed("pub_speed", "azure publish speed", "1", 6);
WiFiManagerParameter custom_options_i2c;
WiFiManagerParameter custom_options;
bool wm_nonblocking = false;
uint32_t chipId = 0;
String deviceName = "AU-";  //Aquisition Unit Identifier

NeoPixelBus<NeoGrbFeature, NeoEsp32Rmt0800KbpsMethod> strip(PixelCount, PixelPin);

// NeoPixel animation time management object
NeoPixelAnimator animations(PixelCount, NEO_CENTISECONDS);
RgbColor color_idle = RgbColor(0, 12, 0);
RgbColor color_config = RgbColor(25, 0, 12);
struct anim_set {
  RgbColor originalColor;
  RgbColor targetColor;
  uint16_t time;
  uint8_t blend_effect;
  uint8_t state;
};

anim_set animation = { color_idle, color_config, 100, 0, 0 };


void trigger_idle_effect() {
  animation.originalColor = color_idle;
  animation.targetColor = color_idle;
  animation.time = 5000;
  animation.blend_effect = 0;
  animation.state = IDLE;
}
void trigger_config_effect() {
  animation.originalColor = color_idle;
  animation.targetColor = RgbColor(10, 10, 0);
  animation.time = 30;
  animation.blend_effect = 0;
  animation.state = CONFIG;
}
void trigger_connect_effect() {
  animation.originalColor = color_idle;
  animation.targetColor = RgbColor(0, 10, 10);
  animation.time = 20;
  animation.blend_effect = 0;
  animation.state = CONNECTING;
}
void trigger_error_effect() {
  animation.originalColor = color_idle;
  animation.targetColor = RgbColor(25, 0, 0);
  animation.time = 50;
  animation.blend_effect = 0;
  animation.state = ERROR;
}
void trigger_reset_effect() {
  animation.originalColor = color_idle;
  animation.targetColor = RgbColor(20, 15, 0);
  animation.time = 10;
  animation.blend_effect = 0;
  animation.state = RESET;
}
void trigger_puplish_effect() {
  animation.originalColor = color_idle;
  animation.targetColor = RgbColor(0, 0, 200);
  animation.time = 10;
  animation.blend_effect = 2;
  animation.state = SENDING;
}
void trigger_update_effect() {
  animation.originalColor = RgbColor(0, 0, 100);
  ;
  animation.targetColor = RgbColor(10, 50, 100);
  animation.time = 50;
  animation.blend_effect = 0;
  animation.state = UPDATE;
}


void SetupAnimationSet() {
  // setup some animations

  AnimEaseFunction easing;

  switch (animation.blend_effect) {
    case 0:
      easing = NeoEase::CubicIn;
      break;
    case 1:
      easing = NeoEase::CubicOut;
      break;
    case 2:
      easing = NeoEase::QuadraticInOut;
      break;
  }


  AnimUpdateCallback animUpdate = [=](const AnimationParam& param) {
    // progress will start at 0.0 and end at 1.0
    // we convert to the curve we want
    float progress = easing(param.progress);

    // use the curve value to apply to the animation
    RgbColor updatedColor = RgbColor::LinearBlend(animation.originalColor, animation.targetColor, progress);
    strip.SetPixelColor(0, updatedColor);
  };

  // now use the animation properties we just calculated and start the animation
  // which will continue to run and call the update function until it completes
  animations.StartAnimation(0, animation.time, animUpdate);
}

struct i2c_device {
  String name;
  uint8_t adr;
  bool selected;
};
struct i2c_device i2c_devices[] = {
  { "Ultrasonic UNIT", 0x57 },
  { "SONIC UNIT(RCWL-9600)", 0x57 },
  { "Gesture UNIT(PAJ7620U2)", 0x73 },
  { "DLight UNIT(BH1750FVI)", 0x23 },
  { "RTC UNIT(BM8563)", 0x51 },
  { "THERMAL UNIT(MLX90640)", 0x33 },
  { "IMU UNIT(MPU6886)", 0x68 },
  { "DHT12", 0x5C },
  { "BMP280", 0x76 },
  { "SHT30", 0x44 },
  { "QMP6988", 0x70 },
  { "NCIR UNIT(MLX90614)", 0x5A },
  { "Encoder UNIT", 0x40 },
  { "4-Relay UNIT(STM32F030)", 0x26 },
  { "ACSSR UNIT", 0x50 },
  { "DDS UNIT", 0x31 },
  { "OLED UNIT", 0x3C },
  { "LCD UNIT", 0x3E },
  { "Ameter UNIT(ADS1115)", 0x48 },
  { "Kmeter UNIT", 0x66 },
  { "Vmeter UNIT(ADS1115)", 0x49 },
  { "TVOC UNIT(SGP30)", 0x58 },
  { "Themal UNIT(MLX90640)", 0x33 },
  { "Color UNIT(TCS3472)", 0x29 },
  { "ToF UNIT(VL53L0X)", 0x29 },
  { "Heart UNIT(MAX30100)", 0x57 },
  { "ADC UNIT(ADS1100)", 0x48 },
  { "Trace UNIT", 0x5A },
  { "ACCEL UNIT(ADXL345)", 0x53 },
  { "CardKB UNIT", 0x5F },
  { "EXT.IO UNIT(PCA9554PW)", 0x27 },
  { "DAC UNIT(MCP4725)", 0x60 },
  { "PaHUB UNIT(TCA9548A)", 0x70 },
  { "RFID UNIT(MFRC522)", 0x28 },
  { "ID UNIT(ATECC608B-TNGTLSU-G)", 0x35 },
  { "Digi Clock(TM1637)", 0x30 },
  { "SSD1306", 0x3c }
};

void saveWifiCallback() {
  Serial.println("[CALLBACK] saveWifiCallback fired");
  status = "wifi settings saved";
  delay(2000);
  ESP.restart();
}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager* myWiFiManager) {

  server.close();
  server.stop();
  Serial.println("[CALLBACK] configModeCallback fired");
  status = "configuration";
  state = CONFIG;
}

bool getParam_bool(String name) {
  //read parameter from server, for customhmtl input
  String value;

  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }
  if (value.equals("on")) return true;
  return false;
}

String getParam(String name) {
  //read parameter from server, for customhmtl input
  String value;

  if (wm.server->hasArg(name)) {
    value = wm.server->arg(name);
  }

  return value;
}
String get_str_from_bool(bool var) {
  if (var == 1) return "checked='checked'";
  if (var == 0) return "";
}

void set_i2c_dev_status(uint8_t value) {
  for (int i = 0; i < sizeof(i2c_devices) / sizeof(i2c_devices[0]); i++) {
    if (i2c_devices[i].adr == value) {

      i2c_devices[i].selected = true;

    } else i2c_devices[i].selected = false;
  }
}

String get_device_by_adr(uint8_t value) {
  for (int i = 0; i < sizeof(i2c_devices) / sizeof(i2c_devices[0]); i++) {
    if (i2c_devices[i].adr == value) {
      i2c_devices[i].selected = 1;
      return i2c_devices[i].name;
    }
  }
  return "n/a";
}

void i2c_scan() {
  preferences.begin("Iforce", false);
  StaticJsonDocument<256> doc_i2c;
  memset(i2c_device_adr, 0, sizeof(i2c_device_adr));
  Serial.println("I2C scanner. Scanning ...");
  byte count = 0;
  Wire.begin();
  for (byte i = 8; i < 120; i++) {
    Wire.beginTransmission(i);        // Begin I2C transmission Address (i)
    if (Wire.endTransmission() == 0)  // Receive 0 = success (ACK response)
    {
      i2c_device_adr[count] = i;
      //Serial.printf("device: %i  count: %i\n",i,count);
      count++;
    }
  }

  for (int i = 0; i < sizeof(i2c_device_adr) / sizeof(i2c_device_adr[0]); i++) {
    if (i2c_device_adr[i] != 0) {
      set_i2c_dev_status(i2c_device_adr[i]);

      String translatedValue = get_device_by_adr(i2c_device_adr[i]);
      JsonArray i2c_dev = doc_i2c.createNestedArray(String(i2c_device_adr[i]));
      i2c_dev.add(translatedValue);
      if (i2c_device_adr[i] > 0) i2c_dev.add(true);
    }
    String temp_i2c_string;
    serializeJson(doc_i2c, temp_i2c_string);
    preferences.putString("i2c", temp_i2c_string);
    preferences.end();
  }
}


char* generate_option_html_str() {
  preferences.begin("Iforce", false);
  upd_interval = preferences.getFloat("updInterval", 60);
  i2c_scan();

  static char buffer[1024];
  int buff_len = sizeof(buffer) / sizeof(buffer[0]);
  strncpy(buffer, "<label><input type='checkbox' name='sim' id='sim'", buff_len);
  strncat(buffer, get_str_from_bool(preferences.getBool("sim", false)).c_str(), buff_len - strlen(buffer));
  strncat(buffer, "><span>simulation</span></label>", buff_len - strlen(buffer));
  strncat(buffer, "<p><label for='upd_interval'>check for update</label></p><select id='upd_interval'>", buff_len - strlen(buffer));
  if (upd_interval == 60) strncat(buffer, "<option value='60' selected>1 minute</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='60'>1 minute</option>", buff_len - strlen(buffer));
  if (upd_interval == 3600) strncat(buffer, "<option value='3600' selected>1 hour</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='3600'>1 hour</option>", buff_len - strlen(buffer));
  if (upd_interval == 86400) strncat(buffer, "<option value='86400' selected>24 hours</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='86400'>24 hours</option>", buff_len - strlen(buffer));
  if (upd_interval == 0) strncat(buffer, "<option value='0' selected>never</option></select>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='0'>never</option></select>", buff_len - strlen(buffer));

  strncat(buffer, "<p><label for='sensor_num'>number of sensors</label></p><select id='sensor_num'>", buff_len - strlen(buffer));
  if (sensor_num == 1) strncat(buffer, "<option value='1' selected>1 sensor</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='1'>1 sensor</option>", buff_len - strlen(buffer));
  if (sensor_num == 2) strncat(buffer, "<option value='1' selected>2 sensors</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='2'>2 sensors</option>", buff_len - strlen(buffer));
  if (sensor_num == 3) strncat(buffer, "<option value='1' selected>3 sensors</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='3'>3 sensors</option>", buff_len - strlen(buffer));
  if (sensor_num == 4) strncat(buffer, "<option value='1' selected>4 sensors</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='4'>4 sensors</option>", buff_len - strlen(buffer));
  if (sensor_num == 5) strncat(buffer, "<option value='1' selected>5 sensors</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='5'>5 sensors</option>", buff_len - strlen(buffer));
  if (sensor_num == 6) strncat(buffer, "<option value='1' selected>6 sensors</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='6'>6 sensors</option>", buff_len - strlen(buffer));
  if (sensor_num == 7) strncat(buffer, "<option value='1' selected>7 sensors</option>", buff_len - strlen(buffer));
  else strncat(buffer, "<option value='7'>7 sensors</option>", buff_len - strlen(buffer));

  strncat(buffer, "<p>sensor start: <input max='", buff_len - strlen(buffer));
  strncat(buffer, String(preferences.getInt("sensor_num", 5)).c_str(), buff_len - strlen(buffer));
  strncat(buffer, "'min='1' name='sensor_start' type='number' value='", buff_len - strlen(buffer));
  strncat(buffer, String(preferences.getInt("sensor_start", 1)).c_str(), buff_len - strlen(buffer));
  strncat(buffer, "' /></p>", buff_len - strlen(buffer));
  strncat(buffer, "<p>delay time: <input max='100' min='1' name='delay_time' type='number' value='", buff_len - strlen(buffer));
  strncat(buffer, String(preferences.getInt("delay_time", 5)).c_str(), buff_len - strlen(buffer));
  strncat(buffer, "' /></p>", buff_len - strlen(buffer));

  strncat(buffer, "<p>phase number: <input max='12' min='1' name='phase_num' type='number' value='", buff_len - strlen(buffer));
  strncat(buffer, String(preferences.getInt("phase_num", 7)).c_str(), buff_len - strlen(buffer));
  strncat(buffer, "' /></p>", buff_len - strlen(buffer));

  strncat(buffer, "<p>measures: <input max='20' min='1' name='measures' type='number' value='", buff_len - strlen(buffer));
  strncat(buffer, String(preferences.getInt("measures", 1)).c_str(), buff_len - strlen(buffer));
  strncat(buffer, "' /></p>", buff_len - strlen(buffer));

  strncat(buffer, "<p>cell number: <input max='7' min='0' name='cell_num' type='number' value='", buff_len - strlen(buffer));
  strncat(buffer, String(preferences.getInt("cell_num", 0)).c_str(), buff_len - strlen(buffer));
  strncat(buffer, "' /></p>", buff_len - strlen(buffer));
  preferences.end();
  return buffer;
}


char* generate_i2c_html_str() {
  preferences.begin("Iforce", false);
  i2c_scan();
  StaticJsonDocument<256> doc_i2c;
  static char buffer[1000];
  int buff_len = sizeof(buffer) / sizeof(buffer[0]);
  const char* html_head = R"(
<p>I2C Devices:</p>
<hr/>
<p>
)";
  strncpy(buffer, html_head, buff_len);
  for (int i = 0; i < sizeof(i2c_device_adr) / sizeof(i2c_device_adr[0]); i++) {
    if (i2c_device_adr[i] != 0) {
      set_i2c_dev_status(i2c_device_adr[i]);

      String translatedValue = get_device_by_adr(i2c_device_adr[i]);
      JsonArray i2c_dev = doc_i2c.createNestedArray(String(i2c_device_adr[i]));
      i2c_dev.add(translatedValue);
      if (i2c_device_adr[i] > 0) i2c_dev.add(true);
      strncat(buffer, "<label> <input id='", buff_len - strlen(buffer));
      strncat(buffer, String(i2c_device_adr[i]).c_str(), buff_len - strlen(buffer));
      strncat(buffer, "'name='", buff_len - strlen(buffer));
      strncat(buffer, translatedValue.c_str(), buff_len - strlen(buffer));
      strncat(buffer, "'type='checkbox' ", buff_len - strlen(buffer));
      if (i2c_device_adr[i] > 0) strncat(buffer, "checked", buff_len - strlen(buffer));
      strncat(buffer, "/> ", buff_len - strlen(buffer));
      strncat(buffer, translatedValue.c_str(), buff_len - strlen(buffer));
      strncat(buffer, "</label>", buff_len - strlen(buffer));

      //Serial.println(translatedValue); // Outputs: "Device3"
    }
  }
  strncat(buffer, "</p>", buff_len - strlen(buffer));
  String temp_i2c_string;
  serializeJson(doc_i2c, temp_i2c_string);
  preferences.putString("i2c", temp_i2c_string);
  preferences.end();
  return buffer;
}


void get_parameters() {
  DynamicJsonDocument doc_config(1500);
  JsonObject i2c = doc_config.createNestedObject("i2c");
  DynamicJsonDocument doc_active_i2c_devs(256);


  preferences.begin("Iforce", false);
  azure_host = preferences.getString("azure_host", "iforce-test.servicebus.windows.net");
  azure_endpoint = preferences.getString("azure_endpoint", "/device1/messages/");
  azure_key = preferences.getString("azure_key", "YffB2uluTpVFfj0D3LoRdHz9+I1JIqwDr3wF3rjsV44=");
  azure_keyname = preferences.getString("azure_keyname", "RootManageSharedAccessKey");
  azure_publish_speed = preferences.getFloat("logspeed_data", 5);
  String selected_i2c_devices = preferences.getString("i2c", "");
  FirmwareVer = preferences.getString("sw_version", "1.0");
  simulation = preferences.getBool("sim", false);
  upd_interval = preferences.getFloat("updInterval", 60);
  sensor_num = preferences.getInt("sensor_num", 5);
  phase_num = preferences.getInt("phase_num", 7);
  cell_num = preferences.getInt("cell_num", 0);
  measures = preferences.getInt("measures", 1);
  sensor_start = preferences.getInt("sensor_start", 1);
  delaytime = preferences.getInt("delay_time", 5);
  preferences.end();
  DeserializationError error = deserializeJson(doc_active_i2c_devs, selected_i2c_devices);
  if (!error) i2c = doc_active_i2c_devs.to<JsonObject>();


  JsonObject log_config = doc_config.createNestedObject("log_config");
  log_config["host"] = azure_host;
  log_config["endp"] = azure_endpoint;
  log_config["key"] = azure_key;
  log_config["keyn"] = azure_keyname;
  log_config["spd"] = azure_publish_speed;
  log_config["updInterval"] = upd_interval;

  serializeJson(doc_config, Serial);
  Serial.println();
  doc_config.clear();
  doc_config.garbageCollect();
}

void saveParamCallback() {
  Serial.println("[CALLBACK] saveParamCallback fired");

  preferences.begin("Iforce", false);
  String temp_azure_host = String(custom_azure_host.getValue());
  String temp_azure_endpoint = String(custom_azure_endpoint.getValue());
  String temp_azure_key = String(custom_azure_key.getValue());
  String temp_azure_keyname = String(custom_azure_keyname.getValue());
  float temp_speed = atof(custom_azure_pubspeed.getValue());
  Serial.print("temp_speed: ");
  Serial.println(temp_speed);
  preferences.putFloat("data_speed", temp_speed);
  preferences.putString("azure_host", temp_azure_host);
  preferences.putString("azure_endpoint", temp_azure_endpoint);
  preferences.putString("azure_key", temp_azure_key);
  preferences.putString("azure_keyname", temp_azure_keyname);
  preferences.putBool("sim", getParam_bool("sim"));
  preferences.putFloat("updInterval", getParam("upd_interval").toFloat());
  preferences.putInt("sensor_num", getParam("sensor_num").toInt());
  preferences.putInt("phase_num", getParam("phase_num").toInt());
  preferences.putInt("cell_num", getParam("cell_num").toInt());
  preferences.putInt("measures", getParam("measures").toInt());
  preferences.putInt("sensor_start", getParam("sensor_start").toInt());
  preferences.putInt("delay_time", getParam("delay_time").toInt());

  preferences.end();
  get_parameters();
  delay(1000);
  wm.server->send(200, "text/plain", "saved changes");
  delay(4000);
  wm.stopConfigPortal();

  //ESP.restart();
}


void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += (server.method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for (uint8_t i = 0; i < server.args(); i++) {
    message += " " + server.argName(i) + ": " + server.arg(i) + "\n";
  }

  server.send(404, "text/plain", message);
}

void on_pressed_button_config() {
  strip.SetPixelColor(0, RgbColor(10, 10, 0));
  strip.Show();
  // start portal w delay
  Serial.println("Starting config portal");
  wm.setConfigPortalTimeout(120);
  state = CONFIG;

  if (!wm.startConfigPortal(deviceName.c_str())) {
    Serial.println("failed to connect or hit timeout");
    delay(3000);
    status = "Config Timeout";
    state = ERROR;
    //start_http_server();
    // ESP.restart();
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    status = "connected";
    state = IDLE;
    //start_http_server();
  }
}

void on_pressed_button_config_reset() {
  Serial.println("Erasing Config, restarting");
  //preferences.clear();
  state = RESET;
  wm.resetSettings();
  delay(3000);
  ESP.restart();
}

void status_timer_callback() {
  DynamicJsonDocument doc_status(312);
  doc_status["status"] = status;
  doc_status["serial"] = status_serial;
  doc_status["wifi"] = wm.getWiFiSSID(true);
  doc_status["http"] = http_status_code;
  doc_status["length"] = content_length;
  doc_status["node"] = deviceName;
  doc_status["firmware"] = FirmwareVer;
  doc_status["updInterval"] = upd_interval;
  doc_status["sensor_num"] = sensor_num;
  doc_status["delay_time"] = delaytime;
  doc_status["phase_num"] = phase_num;
  serializeJson(doc_status, Serial);

  Serial.println();
  doc_status.clear();
  doc_status.garbageCollect();
}

void data_timer_callback() {
  //Serial.println(data_json);
  String temp_variables = "";

  doc_iforce["round"] = round_num;
  //doc["mac"] = WiFi.macAddress();
  //doc["rssi"] = WiFi.RSSI();
  status = "publish sensors";

  serializeJson(doc_iforce, temp_variables);
  state = SENDING;
  //trigger_puplish_effect();
  request = String("POST ") + azure_endpoint.c_str() + " HTTP/1.1\r\n" + "Host: " + azure_host.c_str() + "\r\n" + "Authorization: SharedAccessSignature " + fullSas + "\r\n" + "Content-Type: application/atom+xml;type=entry;charset=utf-8\r\n" + "Content-Length: " + temp_variables.length() + "\r\n\r\n" + temp_variables;
  content_length = temp_variables.length();
  Serial.println(temp_variables);

  client.print(request);
  timer_idle.setTimeout(200);
  timer_idle.start();
}

void firmwareUpdate(void) {
  WiFiClientSecure client_ota_upd;
  client_ota_upd.setCACert(rootCACertificate);
  //httpUpdate.setLedPin(LED_BUILTIN, LOW);
  t_httpUpdate_return ret = httpUpdate.update(client_ota_upd, URL_fw_Bin);

  switch (ret) {
    case HTTP_UPDATE_FAILED:
      status = "OTA update failed";
      state = ERROR;
      Serial.printf("HTTP_UPDATE_FAILD Error (%d): %s\n", httpUpdate.getLastError(), httpUpdate.getLastErrorString().c_str());
      break;

    case HTTP_UPDATE_NO_UPDATES:
      status = "no new OTA update";
      state = IDLE;
      Serial.println("HTTP_UPDATE_NO_UPDATES");
      break;

    case HTTP_UPDATE_OK:
      status = "OTA success";
      state = UPDATE;
      Serial.println("HTTP_UPDATE_OK");
      break;
  }
}
int FirmwareVersionCheck(void) {
  String payload;
  int httpCode;
  String fwurl = "";
  fwurl += URL_fw_Version;
  fwurl += "?";
  fwurl += String(rand());
  Serial.println(fwurl);
  WiFiClientSecure* client_ota_upd = new WiFiClientSecure;

  if (client_ota_upd) {
    client_ota_upd->setCACert(rootCACertificate);

    // Add a scoping block for HTTPClient https to make sure it is destroyed before WiFiClientSecure *client is
    HTTPClient https;

    if (https.begin(*client_ota_upd, fwurl)) {  // HTTPS
      Serial.print("[HTTPS] GET...\n");
      // start connection and send HTTP header
      delay(100);
      httpCode = https.GET();
      delay(100);
      if (httpCode == HTTP_CODE_OK)  // if version received
      {
        payload = https.getString();  // save received version
        state = IDLE;
      } else {
        Serial.print("error in downloading version file:");
        state = ERROR;
        Serial.println(httpCode);
      }
      https.end();
    }
    delete client_ota_upd;
  }

  if (httpCode == HTTP_CODE_OK)  // if version received
  {
    payload.trim();
    if (payload.equals(FirmwareVer)) {
      Serial.printf("\nDevice already on latest firmware version:%s\n", FirmwareVer);
      return 0;
    } else {
      Serial.println(payload);
      preferences.begin("Iforce", false);
      preferences.putString("sw_version", payload);
      preferences.end();
      Serial.println("New firmware detected");
      state = UPDATE;
      return 1;
    }
  }
  return 0;
}

void setup() {
  state = CONNECTING;
  Serial.begin(115200);
  Serial.setRxBufferSize(2048);
  Serial2.begin(9600, SERIAL_8N1, RXD2, TXD2);
  //preferences.begin("Iforce", false);
  Serial.setDebugOutput(false);
  strip.Begin();
  strip.Show();  //wm.setEnableConfigPortal(false);
  wm.setDebugOutput(false);
  if (wm_nonblocking) wm.setConfigPortalBlocking(false);
  wm.setAPCallback(configModeCallback);
  //wm.setWebServerCallback(bindServerCallback);
  wm.setSaveConfigCallback(saveWifiCallback);
  wm.setSaveParamsCallback(saveParamCallback);
  get_parameters();
  // add all your parameters here

  wm.addParameter(&custom_azure_host);
  wm.addParameter(&custom_azure_endpoint);
  wm.addParameter(&custom_azure_key);
  wm.addParameter(&custom_azure_keyname);
  wm.addParameter(&custom_azure_pubspeed);
  new (&custom_options) WiFiManagerParameter(generate_option_html_str());   // custom html input
  new (&custom_options_i2c) WiFiManagerParameter(generate_i2c_html_str());  // custom html input

  wm.addParameter(&custom_options);
  wm.addParameter(&custom_options_i2c);




  custom_azure_host.setValue(azure_host.c_str(), 80);
  custom_azure_endpoint.setValue(azure_endpoint.c_str(), 80);
  custom_azure_keyname.setValue(azure_keyname.c_str(), 50);
  custom_azure_key.setValue(azure_key.c_str(), 100);
  char buffer_float[4];
  dtostrf(azure_publish_speed, 6, 2, buffer_float);

  custom_azure_pubspeed.setValue((const char*)buffer_float, 4);


  wm.setDarkMode(true);

  std::vector<const char*> menu = { "wifi", "wifinoscan", "info", "param", "close", "sep", "erase", "update", "restart", "exit" };
  wm.setMenu(menu);  // custom menu, pass vector

  Serial.println(wm.getWiFiSSID(true));



  wm.setConfigPortalTimeout(120);
  wm.setConnectTimeout(10);
  wm.setConnectRetries(2);
  for (int i = 0; i < 17; i = i + 8) {
    chipId |= ((ESP.getEfuseMac() >> (40 - i)) & 0xff) << i;
  }
  deviceName += String(chipId);



  // END: Wifi connection

  // res = wm.autoConnect(); // auto generated AP name from chipid
  // res = wm.autoConnect("AutoConnectAP"); // anonymous ap

  bool res = wm.autoConnect(deviceName.c_str());  // password protected ap

  if (!res) {
    Serial.println("Failed to connect or hit timeout");
    // ESP.restart();
    state = ERROR;
  } else {
    //if you get here you have connected to the WiFi
    Serial.println("connected...yeey :)");
    state = IDLE;
  }
//if(FirmwareVersionCheck())firmwareUpdate(); //check for new firmware in github iforce once at startup
#ifdef USEOTA
  ArduinoOTA.begin();
#endif



  // START: Naive URL Encode
  String url = "https://" + (String)azure_host + (String)azure_endpoint;
  url.replace(":", "%3A");
  url.replace("/", "%2F");
  Serial.println(url);
  // END: Naive URL Encode

  // START: Create SAS
  // https://azure.microsoft.com/en-us/documentation/articles/service-bus-sas-overview/
  // Where to get secods since the epoch: local service, SNTP, RTC
  int expire = 1711104241;
  String stringToSign = url + "\n" + expire;

  // START: Create signature
  //uint8_t key[azure_key.length()];
  //azure_key.toCharArray(key, azure_key.length());

  Sha256.initHmac((const uint8_t*)azure_key.c_str(), 44);
  Sha256.print(stringToSign);
  char* sign = (char*)Sha256.resultHmac();


  //Serial.println(String (Sha256.resultHmac));
  int signLen = 32;
  // END: Create signature

  // START: Get base64 of signature
  int encodedSignLen = base64_enc_len(signLen);
  char encodedSign[encodedSignLen];
  base64_encode(encodedSign, sign, signLen);
  String encodedSas = (String)encodedSign;

  // Naive URL encode
  encodedSas.replace("=", "%3D");
  //Serial.println(encodedSas);
  // END: Get base64 of signature

  // SharedAccessSignature
  fullSas = "sr=" + url + "&sig=" + encodedSas + "&se=" + expire + "&skn=" + azure_keyname;
  // END: create SAS
  Serial.println("SAS below");
  Serial.println(fullSas);
  Serial.println();
  // START: Wifi connection
  WiFi.mode(WIFI_STA);  // explicitly set mode, esp defaults to STA+AP
  wm.setHostname(deviceName.c_str());
  WiFi.setHostname(deviceName.c_str());

  client.setInsecure();
  if (!client.connect(azure_host.c_str(), httpsPort)) {
    Serial.println("error connecting to Azure, launching Config");
    //res = wm.autoConnect(deviceName.c_str());  // password protected ap
  }

  //pinMode(CONFIG_PIN, INPUT_PULLUP);
  button_config.begin();

  // Attach callback.
  button_config.onPressed(on_pressed_button_config);
  button_config.onPressedFor(10000, on_pressed_button_config_reset);
  //button_config.onSequence(5 /* number of presses */, 1000 /* timeout */, start_http_server /* callback */);
  //start_http_server();


  //Set our callback function

  timer_update.setCallback(check_update);
  timer_idle.setCallback(trigger_idle_state);

  timer_update.setInterval(upd_interval * 1000);
  //Start the timer

  if (upd_interval > 0) timer_update.start();
  data_timer.setInterval(azure_publish_speed * 1000);
  data_timer.setCallback(data_timer_callback);
  status_timer.setInterval(3000);
  status_timer.setCallback(status_timer_callback);
  //Start the timer
  data_timer.start();
  status_timer.start();
  doc_iforce["node"] = deviceName;
  status = "ready";
  status_serial = "ready";

  if (FirmwareVersionCheck()) {
    status = "OTA update";
    state = UPDATE;
    firmwareUpdate();
  }
  queue = xQueueCreate(20, sizeof(float[MAX_SENSORS][2]));
  xTaskCreatePinnedToCore(
    Task_sensor_data_gen_code, /* Task function. */
    "Task_Sensor_data",        /* name of task. */
    10000,                     /* Stack size of task */
    NULL,                      /* parameter of the task */
    1,                         /* priority of the task */
    &Task_sensor_data_gen,     /* Task handle to keep track of created task */
    1);                        /* pin task to core 1 */



  xTaskCreatePinnedToCore(
    Task_sensor_data_send_code, /* Task function. */
    "Task_Sensor_data_send",    /* name of task. */
    20000,                      /* Stack size of task */
    NULL,                       /* parameter of the task */
    1,                          /* priority of the task */
    &Task_sensor_data_send,     /* Task handle to keep track of created task */
    1);                         /* pin task to core 1 */
}

void check_update() {
  if (FirmwareVersionCheck()) {
    status = "OTA update";
    state = UPDATE;
    firmwareUpdate();
  }
}
void trigger_idle_state() {
  state = IDLE;
}

void do_animations() {
  if (animations.IsAnimating()) {
    // the normal loop just needs these two to run the active animations
    animations.UpdateAnimations();
    strip.Show();
  } else {

    //Serial.println("Setup Next Set: "+String(animation.state));
    // example function that sets up some animations
    SetupAnimationSet();
  }
  switch (state) {
    case IDLE:
      trigger_idle_effect();
      break;
    case CONFIG:
      trigger_config_effect();
      break;
    case CONNECTING:
      trigger_connect_effect();
      break;
    case SENDING:
      trigger_puplish_effect();
      break;
    case UPDATE:
      trigger_update_effect();
      break;
    default: trigger_idle_effect();
  }
}
void Task_sensor_data_gen_code(void* pvParameters) {
  Serial.print("Task_sensor_data_gen_code running on core ");
  Serial.println(xPortGetCoreID());

  pinMode(4, OUTPUT);  //to PD12,14,15,13
  pinMode(5, OUTPUT);
  pinMode(14, OUTPUT);  //4 5 8: 3 input of multiplexer,
  pinMode(13, OUTPUT);  //multiplexer enable

  Wire.begin();  //I2C: between Ardunio and ADC/DAC, default speed: 100kHz,modified:400kHz
  ads.begin();
  //Bridge.begin();

  digitalWrite(4, LOW);  //unselcet all sensors
  digitalWrite(5, LOW);
  digitalWrite(14, LOW);
  digitalWrite(13, LOW);

  DAC_command(DAC_D_WRITE_TO_AND_UPDATE_CHANNEL, 2650);
  float sensor_readings[MAX_SENSORS][2];
  for (;;) {





    for (int sensor_switch = sensor_start; sensor_switch <= sensor_num; sensor_switch++) {
      //JsonObject sensor = doc.createNestedObject("Sensor"+String(sensor_switch+1));

      DAC_command(DAC_D_WRITE_TO_AND_UPDATE_CHANNEL, 0);
      DAC_command(DAC_B_WRITE_TO_AND_UPDATE_CHANNEL, 0);
      DAC_command(DAC_C_WRITE_TO_AND_UPDATE_CHANNEL, 0);
      digitalWrite(4, LOW);
      digitalWrite(5, LOW);
      digitalWrite(14, LOW);
      digitalWrite(13, LOW);

      switch (sensor_switch) {
        case 1:
          digitalWrite(14, LOW);
          break;

        case 2:
          digitalWrite(4, HIGH);
          break;

        case 3:
          digitalWrite(5, HIGH);
          break;

        case 4:
          digitalWrite(4, HIGH);
          digitalWrite(5, HIGH);
          break;

        case 5:
          digitalWrite(14, HIGH);
          break;

        case 6:
          digitalWrite(14, HIGH);
          digitalWrite(4, HIGH);
          break;

        case 7:
          digitalWrite(14, HIGH);
          digitalWrite(5, HIGH);
          break;

        case 8:
          digitalWrite(4, HIGH);
          digitalWrite(5, HIGH);
          digitalWrite(14, HIGH);
          break;
      }

      digitalWrite(13, HIGH);
      DAC_command(DAC_D_WRITE_TO_AND_UPDATE_CHANNEL, level_high);

      for (int phase_switch = 0; phase_switch < phase_num; phase_switch++) {
        DAC_command(DAC_D_WRITE_TO_AND_UPDATE_CHANNEL, level_medium);
        DAC_command(DAC_D_WRITE_TO_AND_UPDATE_CHANNEL, level_high);
      }
      DAC_command(DAC_B_WRITE_TO_AND_UPDATE_CHANNEL, 0);
      DAC_command(DAC_C_WRITE_TO_AND_UPDATE_CHANNEL, 1600);
      delay(delaytime);


      //sensor_readings[sensor_switch][0] = (float)ads.readADC_SingleEnded(0)*multiplier;
      //sensor_readings[sensor_switch][1] = (float)ads.readADC_SingleEnded(1)*multiplier;
      sensor_readings[sensor_switch][0] = (float)random(2500, 3000) * multiplier;
      sensor_readings[sensor_switch][1] = (float)random(2500, 3000) * multiplier;

      if (sensor_switch == sensor_num) {

        digitalWrite(4, LOW);
        digitalWrite(5, LOW);
        digitalWrite(14, LOW);
        digitalWrite(13, LOW);
      }
    }

    round_num++;

    //serializeJson(doc, out_json);
    xQueueSend(queue, &sensor_readings, portMAX_DELAY);
    //delay(50);//delete for final use
  }
}


void Task_sensor_data_send_code(void* pvParameters) {
  Serial.print("Task_sensor_data_send_code running on core ");
  Serial.println(xPortGetCoreID());
  float sensor_readings[MAX_SENSORS][2];
  for (;;) {

    doc_iforce["round"] = round_num;
    xQueueReceive(queue, &sensor_readings, portMAX_DELAY);
    //xSemaphoreTake(Semaphore,portMAX_DELAY);
    for (int i = 0; i < MAX_SENSORS; i++) {
      if (sensor_readings[i][0] != 0 || sensor_readings[i][1] != 0) {
        doc_iforce["Sensor" + String(i + 1)]["Curr1"] = sensor_readings[i][0];
        doc_iforce["Sensor" + String(i + 1)]["Curr2"] = sensor_readings[i][1];
      }
    }
    delay(10);
  }
}

void loop() {
  button_config.read();
  do_animations();

  if (wm_nonblocking) wm.process();  // avoid delays() in loop when non-blocking and other long running code

  #ifdef USEOTA
    ArduinoOTA.handle();
  #endif

  data_timer.update();
  status_timer.update();
  timer_update.update();
  timer_idle.update();

  if (!client.connected()) client.connect(azure_host.c_str(), httpsPort);  //reconnection of azure cloud if connection is lost

  if (client.available()) {
    http_response = client.readStringUntil('\n');
    //Serial.println(http_response);
    if (http_response.startsWith("HTTP")) {
      http_status_code = http_response.substring(9, 13).toInt();
    }
  }

  while (Serial.available()) {

    String raw_json = Serial.readString();  // read the incoming data as string
    //Serial.println(raw_json);
    DynamicJsonDocument doc_command(9000);
    DeserializationError error = deserializeJson(doc_command, raw_json);

    // Test if parsing succeeds.
    if (error) {
      Serial.print(F("deserializeJson() failed inside Serial command: "));
      Serial.println(error.f_str());
      status = "deserialization error";
      return;
    }

    if (doc_command["command"].as<int>() == 1) {  //LOAD
      i2c_scan();
      get_parameters();
    }
    if (doc_command["command"].as<int>() == 2) {  //SAVE

      status = "save settings";
      preferences.begin("Iforce", false);
      preferences.putString("azure_host", doc_command["data"]["host"].as<String>());
      preferences.putString("azure_endpoint", doc_command["data"]["endp"].as<String>());
      preferences.putString("azure_key", doc_command["data"]["key"].as<String>());
      preferences.putString("azure_keyname", doc_command["data"]["keyn"].as<String>());
      preferences.putFloat("logspeed_data", doc_command["data"]["spd"].as<float>());
      preferences.end();
      get_parameters();
    }
    if (doc_command["command"].as<int>() == 3) {  //preview settings without permanent save in spiffs
      status = "preview settings";
      azure_host = doc_command["data"]["host"].as<String>();
      azure_endpoint = doc_command["data"]["endp"].as<String>();
      azure_key = doc_command["data"]["key"].as<String>();
      azure_keyname = doc_command["data"]["keyn"].as<String>();
      azure_publish_speed = doc_command["data"]["spd"].as<float>();

      //get_parameters();
    }
  }
}
