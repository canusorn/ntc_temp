#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClient.h>
#include <IotWebConf.h>
#include <IotWebConfUsing.h>
#include <ESP8266HTTPUpdateServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <Ticker.h>
#include <EEPROM.h>
#include <TM1637.h>
#include <TridentTD_LineNotify.h>
#include <iotbundle.h>

// 1 สร้าง object ชื่อ iot และกำหนดค่า(project)
#define PROJECT "DHT"
Iotbundle iot(PROJECT);

const char thingName[] = "ntc";
const char wifiInitialApPassword[] = "iotbundle";

#define STRING_LEN 128
#define NUMBER_LEN 32

// timer interrupt
Ticker timestamp;

TM1637 tm(D0, D5);

unsigned long previousMillis = 0;

// -- Method declarations.
void handleRoot();
// -- Callback methods.
void wifiConnected();
void configSaved();
bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper);

DNSServer dnsServer;
WebServer server(80);
ESP8266HTTPUpdateServer httpUpdater;

char emailParamValue[STRING_LEN];
char passParamValue[STRING_LEN];
char serverParamValue[STRING_LEN];

char tokenParamValue[STRING_LEN];
char lowerTempParamValue[NUMBER_LEN];
char upperTempParamValue[NUMBER_LEN];

IotWebConf iotWebConf(thingName, &dnsServer, &server, wifiInitialApPassword, VERSION); // version defind in iotbundle.h file
// -- You can also use namespace formats e.g.: iotwebconf::TextParameter
IotWebConfParameterGroup login = IotWebConfParameterGroup("login", "ล็อกอิน(สมัครที่เว็บก่อนนะครับ)");

IotWebConfTextParameter emailParam = IotWebConfTextParameter("อีเมลล์", "emailParam", emailParamValue, STRING_LEN);
IotWebConfPasswordParameter passParam = IotWebConfPasswordParameter("รหัสผ่าน", "passParam", passParamValue, STRING_LEN);
IotWebConfTextParameter serverParam = IotWebConfTextParameter("เซิฟเวอร์", "serverParam", serverParamValue, STRING_LEN, "https://iotkiddie.com");

IotWebConfParameterGroup line_notify = IotWebConfParameterGroup("Line Notify", "แจ้งเตือน");
IotWebConfTextParameter tokenParam = IotWebConfTextParameter("Token", "tokenParam", tokenParamValue, STRING_LEN, "");
IotWebConfTextParameter upperTempParam = IotWebConfTextParameter("แจ้งเตือนสูงกว่า", "upperTempParam", upperTempParamValue, NUMBER_LEN, "8");
IotWebConfTextParameter lowerTempParam = IotWebConfTextParameter("แจ้งเตือนต่ำกว่า", "lowerTempParam", lowerTempParamValue, NUMBER_LEN, "4");

uint8_t t_connecting;
iotwebconf::NetworkState prev_state = iotwebconf::Boot;
uint8_t displaytime;
String noti;
bool ota_updated = false;
uint16_t timer_nointernet;

uint16_t Vt;
uint8_t Vt_index;
float temp;
int8_t upperTemp = 8, lowerTemp = 4;
uint8_t stateTemp = 3; // 0-lower  1-normal  2-high  3-line not config
bool line_flag = 0;

// timer interrupt every 1 second
void time1sec()
{
  iot.interrupt1sec();

  // if can't connect to network
  if (iotWebConf.getState() == iotwebconf::OnLine)
  {
    if (iot.serverConnected)
    {
      timer_nointernet = 0;
    }
    else
    {
      timer_nointernet++;
      if (timer_nointernet > 30)
        Serial.println("No connection time : " + String(timer_nointernet));
    }
  }

  // reconnect wifi if can't connect server
  if (timer_nointernet == 60)
  {
    Serial.println("Can't connect to server -> Restart wifi");
    iotWebConf.goOffLine();
    timer_nointernet++;
  }
  else if (timer_nointernet >= 65)
  {
    timer_nointernet = 0;
    iotWebConf.goOnLine(false);
  }
  else if (timer_nointernet >= 61)
    timer_nointernet++;

  if (iot.getTodayTimestamp() == 28800) //28800
  {
    line_flag = 1;
  }
}

void setup()
{
  Serial.begin(115200);

  // for clear eeprom jump D5 to GND
  pinMode(D5, INPUT_PULLUP);
  if (digitalRead(D5) == false)
  {
    delay(1000);
    if (digitalRead(D5) == false)
    {
      Serial.println("Reset all data");
      delay(1000);
      clearEEPROM();
    }
  }

  digitalWrite(D6, HIGH);
  digitalWrite(D7, LOW);
  pinMode(D6, OUTPUT);
  pinMode(D7, OUTPUT);

  // timer interrupt every 1 sec
  timestamp.attach(1, time1sec);

  tm.begin();
  tm.setBrightness(4);

  login.addItem(&emailParam);
  login.addItem(&passParam);
  login.addItem(&serverParam);

  line_notify.addItem(&tokenParam);
  line_notify.addItem(&upperTempParam);
  line_notify.addItem(&lowerTempParam);

  //  iotWebConf.setStatusPin(STATUS_PIN);
  // iotWebConf.setConfigPin(CONFIG_PIN);
  //  iotWebConf.addSystemParameter(&stringParam);
  iotWebConf.addParameterGroup(&login);
  iotWebConf.addParameterGroup(&line_notify);
  iotWebConf.setConfigSavedCallback(&configSaved);
  iotWebConf.setFormValidator(&formValidator);
  iotWebConf.getApTimeoutParameter()->visible = false;
  iotWebConf.setWifiConnectionCallback(&wifiConnected);

  // -- Define how to handle updateServer calls.
  iotWebConf.setupUpdateServer(
    [](const char *updatePath)
  {
    httpUpdater.setup(&server, updatePath);
  },
  [](const char *userName, char *password)
  {
    httpUpdater.updateCredentials(userName, password);
  });

  // -- Initializing the configuration.
  iotWebConf.init();

  // -- Set up required URL handlers on the web server.
  server.on("/", handleRoot);
  server.on("/config", []
  { iotWebConf.handleConfig(); });
  server.on("/cleareeprom", clearEEPROM);
  server.on("/reboot", reboot);
  server.onNotFound([]()
  {
    iotWebConf.handleNotFound();
  });

  Serial.println("ESPID: " + String(ESP.getChipId()));
  Serial.println("Ready.");
}

void loop()
{
  // 3 คอยจัดการ และส่งค่าให้เอง
  iot.handle();

  iotWebConf.doLoop();
  server.handleClient();
  MDNS.update();

  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= 100)
  { // run every 0.1 second
    previousMillis = currentMillis;
    Vt += analogRead(A0);
    Vt_index++;
  }

  if (Vt_index >= 50)
  {
    float Vtf = (float)Vt / Vt_index;
    Vt = 0;
    Vt_index = 0;

    float Rt = (float)(1023 * 9900) / Vtf - 9900;
    float ln = log(Rt / 10000);
    temp = (1 / ((ln / 3950) + (1 / (25 + 273.15))));
    temp -= 273.15;
    Serial.println(temp);
    tm.display(String(temp, 0) + " C");

    iot.update(NAN, temp);

    // check need ota update flag from server
    if (iot.need_ota)
      iot.otaUpdate("NCT-14298243");

    // แจ้งเตือนไลน์
    if (stateTemp == 0)
    {
      if (temp > lowerTemp + 1)
      {
        LINE.notify("อุณหภูมิปกติ " + String(temp) + " C");
        stateTemp = 1;
      }
      else if (temp > upperTemp + 0.5)
      {
        LINE.notify("อุณหภูมิมากกว่ากำหนด " + String(temp) + " C");
        stateTemp = 2;
      }
    }
    else if (stateTemp == 1)
    {
      if (temp < lowerTemp - 0.5)
      {
        LINE.notify("อุณหภูมิต่ำกว่ากำหนด " + String(temp) + " C");
        stateTemp = 0;
      }
      else if (temp > upperTemp + 0.5)
      {
        LINE.notify("อุณหภูมิมากกว่ากำหนด " + String(temp) + " C");
        stateTemp = 2;
      }
    }
    else if (stateTemp == 2)
    {
      if (temp < upperTemp - 1)
      {
        LINE.notify("อุณหภูมิปกติ " + String(temp) + " C");
        stateTemp = 1;
      }
      else if (temp < lowerTemp - 0.5)
      {
        LINE.notify("อุณหภูมิต่ำกว่ากำหนด " + String(temp) + " C");
        stateTemp = 0;
      }
    }
  }

  if (line_flag) {
    LINE.notify("อุณหภูมิตอนนี้ " + String(temp) + " C");
    line_flag = 0;
  }
}

void handleRoot()
{
  // -- Let IotWebConf test and handle captive portal requests.
  if (iotWebConf.handleCaptivePortal())
  {
    // -- Captive portal request were already served.
    return;
  }
  String s = "<!DOCTYPE html><html lang=\"en\"><head><meta charset=\"utf-8\"><meta name=\"viewport\" content=\"width=device-width, initial-scale=1, user-scalable=no\"/>";
  s += "<title>Iotkiddie AC Powermeter config</title></head><body>IoTkiddie config data";
  s += "<ul>";
  s += "<li>Device name : ";
  s += String(iotWebConf.getThingName());
  s += "<li>อีเมลล์ : ";
  s += emailParamValue;
  s += "<li>WIFI SSID : ";
  s += String(iotWebConf.getSSID());
  s += "<li>RSSI : ";
  s += String(WiFi.RSSI()) + " dBm";
  s += "<li>ESP ID : ";
  s += ESP.getChipId();
  s += "<li>Server : ";
  s += serverParamValue;
  s += "</ul>";
  s += "<button style='margin-top: 10px;' type='button' onclick=\"location.href='/reboot';\" >รีบูทอุปกรณ์</button><br><br>";
  s += "<a href='config'>configure page</a> เพื่อแก้ไขข้อมูล wifi และ user";
  s += "</body></html>\n";

  server.send(200, "text/html", s);
}

void configSaved()
{
  Serial.println("Configuration was updated.");
}

void wifiConnected()
{

  Serial.println("WiFi was connected.");
  MDNS.begin(iotWebConf.getThingName());
  MDNS.addService("http", "tcp", 80);

  Serial.printf("Ready! Open http://%s.local in your browser\n", String(iotWebConf.getThingName()));
  if ((String)emailParamValue != "" && (String)passParamValue != "")
  {
    Serial.println("login");

    // 2 เริ่มเชื่อมต่อ หลังจากต่อไวไฟได้
    if ((String)passParamValue != "")
      iot.begin((String)emailParamValue, (String)passParamValue, (String)serverParamValue);
    else // ถ้าไม่ได้ตั้งค่า server ให้ใช้ค่า default
      iot.begin((String)emailParamValue, (String)passParamValue);
  }

  upperTemp = atoi(upperTempParamValue);
  lowerTemp = atoi(lowerTempParamValue);
  LINE.setToken(tokenParamValue);
  stateTemp = 1;
  Serial.println("token " + String(tokenParamValue) + "\tat " + String(lowerTemp) + " to " + String(upperTemp));
}

bool formValidator(iotwebconf::WebRequestWrapper *webRequestWrapper)
{
  Serial.println("Validating form.");
  bool valid = true;

  /*
    int l = webRequestWrapper->arg(stringParam.getId()).length();
    if (l < 3)
    {
      stringParam.errorMessage = "Please provide at least 3 characters for this test!";
      valid = false;
    }
  */
  return valid;
}

void clearEEPROM()
{
  EEPROM.begin(512);
  // write a 0 to all 512 bytes of the EEPROM
  for (int i = 0; i < 512; i++)
  {
    EEPROM.write(i, 0);
  }

  EEPROM.end();
  server.send(200, "text/plain", "Clear all data\nrebooting");
  delay(1000);
  ESP.restart();
}

void reboot()
{
  server.send(200, "text/plain", "rebooting");
  delay(1000);
  ESP.restart();
}
