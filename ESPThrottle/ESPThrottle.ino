
/*  ESP8266-12E / NodeMCU LocoNet Gateway

 *  Source code can be found here: https://github.com/tanner87661/LocoNet-MQTT-Gateway
 *  Copyright 2018  Hans Tanner IoTT
 *
 *  Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except
 *  in compliance with the License. You may obtain a copy of the License at:
 *      http://www.apache.org/licenses/LICENSE-2.0
 *  Unless required by applicable law or agreed to in writing, software distributed under the License is distributed
 *  on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. See the License
 *  for the specific language governing permissions and limitations under the License.
 */

#define sendLogMsg

#define useIOTAppStory   //uncomment this to use OTA mechanism via IOTAppStory.com. If commented out, WifiManager will be used instead

//#define debugMode

#ifdef debugMode
  #define DEBUG_BUFFER_SIZE 1024
  bool doDebug = true;
#else
  #define DEBUG_BUFFER_SIZE 0
  bool doDebug = false;
#endif

#ifdef sendLogMsg
  #if defined(ESP8266)
    #include "EspSaveCrash.h" //standard library, install using library manager
    //https://github.com/espressif/arduino-esp32/issues/449
  #else
    #include <rom/rtc.h>
  #endif
#endif

#if defined(ESP8266)
  #define espButton 0 //onboard Button on Sonoff Basic
  #define esp12LED 13 //onboard LED on ESP32 Chip Board
#else
  error
#endif

//#ifdef useIOTAppStory
  #define APPNAME "ESPThrottle"
  #define VERSION "1.0.0"
  #define COMPDATE __DATE__ __TIME__
  #define MODEBUTTON espButton
//#endif

#include <PubSubClient.h> //standard library, install using library manager

char mqtt_server[100] = "https://broker.hivemq.com"; // = Mosquitto Server IP "192.168.xx.xx" as loaded from mqtt.cfg
uint16_t mqtt_port = 1883; // = Mosquitto port number, standard is 1883, 8883 for SSL connection;
char mqtt_user[100] = "";
char mqtt_password[100] = "";

char* mqtt_server_field = "https://broker.hivemq.com";
char* mqtt_port_field = "1883";
char* mqtt_user_field = "User Name";
char* mqtt_password_field = "User Password";

//Incoming Topics
char lnEchoTopic[] = "lnEcho";  //data topic, do not change. 

//Outgping Topics
char lnPing[] = "lnPing";
char lnOutTopic[] = "lnOut";  //broadcast data topic, do not change. 

//Broadcast Topic (In and Out)
char lnBCTopic[] = "lnIn";  //broadcast data topic, do not change. 

bool useNetworkMode = false;
bool useTimeStamp = true;

#include <ArduinoJson.h> //standard JSON library, can be installed in the Arduino IDE

#include <FS.h> //standard File System library, can be installed in the Arduino IDE

#include <TimeLib.h> //standard library, can be installed in the Arduino IDE
#include <Ticker.h>

#include <NTPtimeESP.h> //NTP time library from Andreas Spiess, download from https://github.com/SensorsIot/NTPtimeESP
#include <EEPROM.h> //standard library, can be installed in the Arduino IDE

#if defined(ESP8266)
  #include <ESP8266WiFi.h>
  #include <ESPAsyncWebServer.h>
#else
  #include <WiFi.h>
  #include <ESPAsyncWebServer.h>
#endif
//#include <WiFiClientSecure.h>
//#include <WiFiClient.h>
#include <DNSServer.h>

#if defined(ESP8266)
  #define ESP_getChipId()   (ESP.getChipId())
#else
  #include <esp_wifi.h>
  #define ESP_getChipId()   ((uint32_t)ESP.getEfuseMac())
#endif

byte throttleID[2];

#include <IOTAppStory.h> //standard library, can be installed in the Arduino IDE. See https://github.com/iotappstory for more information
IOTAppStory IAS(COMPDATE, MODEBUTTON);    // Initialize IOTAppStory

// SET YOUR NETWORK MODE TO USE WIFI
const char* ssid = ""; //add your ssid and password. If left blank, ESP8266 will enter AP mode and let you enter this information from a browser
const char* password = "";
String NetBIOSName = "SimpleThrottle";

#ifdef sendLogMsg
char lnLogMsg[] = "lnLog";
#endif

char mqttMsg[800]; //buffer used to publish messages via mqtt

String ntpServer = "us.pool.ntp.org"; //default server for US. Change this to the best time server for your region
NTPtime NTPch(ntpServer); 
int ntpTimeout = 5000; //ms timeout for NTP update request

WiFiClientSecure wifiClientSec;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);

//Display variables
#include <Nextion.h>
#include <NextionPage.h>
#include <NextionButton.h>
#include <NextionGauge.h>
#include <NextionDualStateButton.h>
//#include <NextionRadioButton.h>
#include <NextionHotspot.h>
#include <NextionNumber.h>
#include <NextionText.h>
#include <NextionPicture.h>
#include <NextionVariableNumeric.h>

#include <SoftwareSerial.h>
SoftwareSerial nextionSerial(14,12); // RX, TX, D5, D6


Nextion nex(nextionSerial);

NextionText    txtStatMsg(nex, 0, 2, "t0");

NextionPage pgThrottle(nex, 1, 0, "pgThrottle");
NextionButton  btnF0(nex, 1, 4, "b1");
NextionButton  btnF1(nex, 1, 5, "b2");
NextionButton  btnF2(nex, 1, 6, "b3");
NextionButton  btnF3(nex, 1, 7, "b4");
NextionButton  btnF4(nex, 1, 8, "b5");
NextionButton  btnF5(nex, 1, 9, "b6");
NextionButton  btnF6(nex, 1, 10, "b7");
NextionButton  btnF7(nex, 1, 11, "b8");
NextionDualStateButton  btnShift(nex, 1, 32, "b9");
NextionButton  btnDisp(nex, 1, 25, "b10");
NextionButton  btnSwitch(nex, 1, 26, "b11");
NextionButton  btnStop(nex, 1, 27, "b12");
NextionText    txtLocoAddr(nex, 1, 31, "t0");
NextionButton  btnForward(nex, 1, 29, "b13");
NextionButton  btnBackward(nex, 1, 28, "b0");
NextionButton  btnBrake(nex, 1, 30, "b14");
NextionHotspot hspSpeed0(nex, 1, 3, "m0");
NextionHotspot hspSpeed1(nex, 1, 12, "m1");
NextionHotspot hspSpeed2(nex, 1, 13, "m2");
NextionHotspot hspSpeed3(nex, 1, 14, "m3");
NextionHotspot hspSpeed4(nex, 1, 15, "m4");
NextionHotspot hspSpeed5(nex, 1, 16, "m5");
NextionHotspot hspSpeed6(nex, 1, 17, "m6");
NextionHotspot hspSpeed7(nex, 1, 18, "m7");
NextionHotspot hspSpeed8(nex, 1, 19, "m8");
NextionHotspot hspSpeed9(nex, 1, 20, "m9");
NextionHotspot hspSpeed10(nex, 1, 21, "m10");
NextionHotspot hspSpeed11(nex, 1, 22, "m11");
NextionHotspot hspSpeed12(nex, 1, 23, "m12");
NextionHotspot hspSpeed13(nex, 1, 24, "m13");
NextionHotspot hspSpeed14(nex, 1, 24, "m14");
NextionHotspot hspSpeed15(nex, 1, 43, "m15");
NextionHotspot hspSpeed16(nex, 1, 44, "m16");
NextionHotspot hspSpeed17(nex, 1, 45, "m17");
NextionHotspot hspSpeed18(nex, 1, 46, "m18");
NextionGauge   ggSpeedNeedle(nex, 1, 2, "z0");
NextionVariableNumeric varSpeed(nex, 1, 34, "vaSpeed");
NextionVariableNumeric varAddr(nex, 1, 37, "vaAddr");
NextionVariableNumeric varDirF(nex, 1, 40, "vaDIRF");

NextionPage pgSwitchBoard(nex, 2, 0, "pgSwitchBoard");

NextionPicture picSwitch1(nex, 2, 2, "p1");
NextionPicture picSwitch2(nex, 2, 3, "p2");
NextionPicture picSwitch3(nex, 2, 4, "p3");
NextionPicture picSwitch4(nex, 2, 42, "p10");
NextionPicture picSwitch5(nex, 2, 5, "p4");
NextionPicture picSwitch6(nex, 2, 6, "p5");
NextionPicture picSwitch7(nex, 2, 7, "p6");
NextionPicture picSwitch8(nex, 2, 44, "p11");
NextionPicture picSwitch9(nex, 2, 8, "p7");
NextionPicture picSwitch10(nex, 2, 9, "p8");
NextionPicture picSwitch11(nex, 2, 10, "p9");
NextionPicture picSwitch12(nex, 2, 46, "p12");
NextionPicture picSwitch13(nex, 2, 47, "p13");
NextionPicture picSwitch14(nex, 2, 48, "p14");
NextionPicture picSwitch15(nex, 2, 49, "p15");
NextionPicture picSwitch16(nex, 2, 50, "p16");

NextionNumber  numSwitch1(nex, 2, 20, "n0");
NextionNumber  numSwitch2(nex, 2, 21, "n1");
NextionNumber  numSwitch3(nex, 2, 22, "n2");
NextionNumber  numSwitch4(nex, 2, 23, "n3");
NextionNumber  numSwitch5(nex, 2, 24, "n4");
NextionNumber  numSwitch6(nex, 2, 25, "n5");
NextionNumber  numSwitch7(nex, 2, 26, "n6");
NextionNumber  numSwitch8(nex, 2, 27, "n7");
NextionNumber  numSwitch9(nex, 2, 28, "n8");
NextionButton  btnAddress(nex, 2, 11, "b0");
NextionButton  btnAddrMinus(nex, 2, 12, "b2");
NextionButton  btnAddrPlus(nex, 2, 13, "b1");
NextionButton  btnExit(nex, 2, 14, "b3");
NextionButton  btnSwiBackward(nex, 2, 18, "b5");
NextionButton  btnSwiForward(nex, 2, 19, "b6");
NextionButton  btnSwiSpeedMinus(nex, 2, 15, "b4");
NextionButton  btnSwiSpeedPlus(nex, 2, 17, "b13");
NextionButton  btnSwiBrake(nex, 2, 16, "b14");

NextionPage pgCTCPanel(nex, 3, 0, "pgCTCPanel");
NextionPicture picCTCSwitch1(nex, 3, 3, "p2");
NextionPicture picCTCSwitch2(nex, 3, 4, "p3");
NextionPicture picCTCSwitch3(nex, 3, 5, "p4");
NextionPicture picCTCSwitch4(nex, 3, 6, "p5");
NextionPicture picLeftLED1(nex, 3, 7, "p6");
NextionPicture picLeftLED2(nex, 3, 16, "p15");
NextionPicture picLeftLED3(nex, 3, 18, "p17");
NextionPicture picLeftLED4(nex, 3, 20, "p19");
NextionPicture picRightLED1(nex, 3, 15, "p14");
NextionPicture picRightLED2(nex, 3, 17, "p16");
NextionPicture picRightLED3(nex, 3, 19, "p18");
NextionPicture picRightLED4(nex, 3, 21, "p20");

NextionPicture picInpRep1(nex, 3, 9, "p8");
NextionPicture picInpRep2(nex, 3, 10, "p9");
NextionPicture picInpRep3(nex, 3, 29, "p22");
NextionPicture picInpRep4(nex, 3, 8, "p7");
NextionPicture picInpRep5(nex, 3, 30, "p23");
NextionPicture picInpRep6(nex, 3, 14, "p13");
NextionPicture picInpRep7(nex, 3, 13, "p12");
NextionPicture picInpRep8(nex, 3, 31, "p24");
NextionPicture picInpRep9(nex, 3, 2, "p1"); 
NextionPicture picInpRep10(nex, 3, 32, "p25");
NextionPicture picInpRep11(nex, 3, 12, "p11");
NextionPicture picInpRep12(nex, 3, 11, "p10");
NextionButton  btnCTCBackward(nex, 3, 26, "m2");
NextionButton  btnCTCForward(nex, 3, 24, "m0");
NextionButton  btnCTCSpeedMinus(nex, 3, 27, "m3");
NextionButton  btnCTCSpeedPlus(nex, 3, 25, "m1");
NextionButton  btnCTCBrake(nex, 3, 28, "m4");

NextionButton  btnExitCTC(nex, 3, 22, "b3");

//Keypad
NextionPage    pgKeypad(nex, 4, 0, "pgKeypad");
NextionButton  btnCancel(nex, 4, 15, "bCancel");
NextionButton  btnEnter(nex, 4, 16, "bEnter");
NextionText    txtKeypadField(nex, 4, 2, "t0");
NextionText    txtKeypadInfo(nex, 4, 17, "t1");
NextionText    txtKeypadError(nex, 4, 18, "t2");

NextionPage    pgLocoSelect(nex, 5, 0, "pgLocoSelect");
NextionButton  btnNum1(nex, 5, 3, "b0");
NextionButton  btnNum2(nex, 5, 4, "b1");
NextionButton  btnNum3(nex, 5, 5, "b2");
NextionButton  btnNum4(nex, 5, 6, "b3");
NextionButton  btnNum5(nex, 5, 7, "b4");
NextionButton  btnNum6(nex, 5, 8, "b5");
NextionButton  btnNum7(nex, 5, 9, "b6");
NextionButton  btnNum8(nex, 5, 10, "b7");
NextionButton  btnNum9(nex, 5, 11, "b8");
NextionButton  btnNum0(nex, 5, 13, "b9");
NextionButton  btnNumBack(nex, 5, 12, "bBack");
NextionButton  btnNumClear(nex, 5, 14, "bClear");
NextionButton  btnLocoCancel(nex, 5, 15, "bCancel");
NextionButton  btnLocoSelect(nex, 5, 16, "bSelect");
NextionButton  btnLocoSteal(nex, 5, 19, "bSteal");
NextionText    txtLocoKeypadField(nex, 5, 2, "t0");
NextionText    txtLocoKeypadInfo(nex, 5, 17, "t1");
NextionText    txtLocoKeypadError(nex, 5, 18, "t2");

NextionPage    pgConsistMgmt(nex, 6, 0, "pgConsistMgmt");

NextionButton  btnRelease0(nex, 6, 3, "b0");
NextionButton  btnF00(nex, 6, 4, "b1");
NextionButton  btnF10(nex, 6, 5, "b2");
NextionButton  btnF20(nex, 6, 6, "b3");
NextionButton  btnRelease1(nex, 6, 13, "b4");
NextionButton  btnF01(nex, 6, 14, "b6");
NextionButton  btnF11(nex, 6, 15, "b7");
NextionButton  btnF21(nex, 6, 16, "b8");
NextionButton  btnRelease2(nex, 6, 18, "b10");
NextionButton  btnF02(nex, 6, 19, "b11");
NextionButton  btnF12(nex, 6, 20, "b12");
NextionButton  btnF22(nex, 6, 21, "b13");
NextionButton  btnRelease3(nex, 6, 23, "b14");
NextionButton  btnF03(nex, 6, 24, "b15");
NextionButton  btnF13(nex, 6, 25, "b16");
NextionButton  btnF23(nex, 6, 26, "b17");
NextionButton  btnRelease4(nex, 6, 28, "b18");
NextionButton  btnF04(nex, 6, 29, "b19");
NextionButton  btnF14(nex, 6, 30, "b20");
NextionButton  btnF24(nex, 6, 31, "b21");

NextionButton  btnConsistClose(nex, 6, 2, "bBack");
NextionDualStateButton  btnShiftCn(nex, 6, 9, "b9");
NextionButton  btnPgDn(nex, 6, 41, "b23");
NextionButton  btnPgUp(nex, 6, 40, "b22");
NextionButton  btnAddLoco(nex, 6, 8, "b5");
NextionPicture picPos0(nex, 6, 7, "n0");
NextionPicture picPos1(nex, 6, 12, "n1");
NextionPicture picPos2(nex, 6, 17, "n2");
NextionPicture picPos3(nex, 6, 22, "n3");
NextionPicture picPos4(nex, 6, 27, "n4");

Ticker displayDriver;

int switchBoardStartAddr = 1;
//LocoNet Types
typedef struct
{
  byte slotData[11];
} LocoNetSlot;

typedef struct
{
  byte opCode = 0; //copy of OpCode
  long reqTime = millis(); //time stamp of request being sent
  long respDelay(){return (millis() - reqTime);}
  int  reqID = 0;
} ThrottleResponse; //this is updated with every command sent

//Display Constants & Vars
const int passiveDirCol = 0; //black
const int activeDirCol = 63488; //red, check individual locations if changin
const int invalidLocoCol = 50712;
bool displayReady = true; //blocks ticker from reentry into display routine

//LocoNet  Constants
const int maxSwiAddr = 2048;
const int maxLocoAddr = 9983;
const uint32_t throttlePingInterval = 100000;
const float dispDriverTickTime = 0.5;
const long  respTimeout = 1000; //ms

//LocoNet Variables
bool cfgInstantDir=false;
byte swPos[511]; //current status of switches, 4 per byte. First bit indicates correct state, second is position
byte bdStatus[512]; //4096 input bits
bool pwrStatus = false;
bool pwrStatusCopy = false;
bool validLoco = false;
bool stealOK = false;
int  targetSpeed = 0;
int  currentSpeed = 0;
int  targetDir = 0x20;
bool btnShiftState = false;
int  currentPage = 0;
byte cnDisplayStart = 0; //first slot to display in consist manager
byte cnNumEntries = 0; //number of consisted locos linked down
word cnDispSlots[] = {0x00FF,0x00FF,0x00FF,0x00FF,0x00FF};
LocoNetSlot workSlot;
LocoNetSlot tempSlot;
LocoNetSlot displaySlot;
LocoNetSlot configSlot;
LocoNetSlot arraySlots[128]; //copy of all system slots
int  numSysSlots = 0;
int  cnSelSlot = -1;


ThrottleResponse commResponse;


//keypad data entry support
int numValBuffer;
int slotReqAddr; //temporary storage of requested address to decide if display is updated when slot info comes back

//intervall NTP server is contacted to update time/date
const uint32_t ntpIntervallDefault = 1800000; //half hour in milliseconds
const uint32_t ntpIntervallShort = 10000; //10 Seconds
//const uint32_t lnLoadIntervall = 1000; //1 Seconds to measure network load
const uint32_t lnPingIntervall = 60000; //30 Seconds to send ping string to MQTT server on Topic lnPing
//const uint32_t lnDataIntervall = 5000; //3 Seconds to send ping string to MQTT server on Topic lnPing
const uint32_t lnBrakeTimeout = 500; //0.5 Seconds to read thermocouple

strDateTime dateTime;
int timeZone = -5;

//Ajax Command sends a JSON with information about internal status
char ajaxCmdStr[] = "ajax_input";
char ajaxPingStr[] = "/ajax_ping";
char ajaxDataStr[] = "/ajax_data";
char ajaxCurveStr[] = "/ajax_curves";
//const char* PARAM_MESSAGE = "message";

// OTHER VARIALBES
uint32_t ntpTimer = millis();
uint32_t lnLoadTimer = millis();
uint32_t lnPingTimer = millis();
uint32_t lnDataTimer = millis();
uint32_t lnThrottlePingTimer = millis();
uint32_t lnBrakeOneShotTimer = millis();
uint32_t pwmHeaterStart = millis();


int millisRollOver = 0; //used to keep track of system uptime after millis rollover
unsigned long lastMillis = 0;

File uploadFile;

#if defined(ESP8266)
  AsyncWebServer server(80);
#else
  AsyncWebServer server(80);
#endif

bool    ntpOK = false;
bool    useNTP = true;

void setup()
{
  Serial.begin(115200);

  for (int i = 0; i < 127; i++)
    arraySlots[i].slotData[0] = 0xFF; //set the slot number to FF to indicate the slot was never updated

  nextionSerial.begin(9600);
  nex.init();
  nextionSetup();
  
#if defined(ESP8266)
  WiFi.hostname(NetBIOSName + "-" + ESP_getChipId());
#else
  WiFi.softAP(NetBIOSName.c_str());
#endif

  throttleID[0] = (byte)(ESP_getChipId() & 0x00FF);
  throttleID[1] = (byte)(ESP_getChipId() & 0xFF00) >> 8;

  IAS.preSetDeviceName(NetBIOSName + String(ESP_getChipId()));        // preset deviceName this is also your MDNS responder: http://deviceName.local
  IAS.preSetAppName(F(APPNAME));     // preset appName
  IAS.preSetAppVersion(VERSION);       // preset appVersion
  IAS.preSetAutoUpdate(false);            // automaticUpdate (true, false)
  IAS.preSetWifi(ssid, password);

//  IAS.addField(mqtt_server_field, "Textarea", 80, 'T');            // reference to org variable | field label value | max char return | Optional "special field" char
//  IAS.addField(mqtt_port_field, "Number", 8, 'I');                     // Find out more about the optional "special fields" at https://iotappstory.com/wiki
//  IAS.addField(mqtt_user_field, "textLine", 20);                        // These fields are added to the "App Settings" page in config mode and saved to eeprom. Updated values are returned to the original variable.
//  IAS.addField(mqtt_user_field, "textLine", 20);                        // These fields are added to the "App Settings" page in config mode and saved to eeprom. Updated values are returned to the original variable.
  
#ifdef useIOTAppStory
  IAS.begin('P');                                     // Optional parameter: What to do with EEPROM on First boot of the app? 'F' Fully erase | 'P' Partial erase(default) | 'L' Leave intact
//  IAS.setCallHome(true);                              // Set to true to enable calling home frequently (disabled by default)
  IAS.setCallHome(false);                              // Set to true to enable calling home frequently (disabled by default)
  IAS.setCallHomeInterval(300);                        // Call home interval in seconds, use 60s only for development. Please change it to at least 2 hours in production
  IAS.loop();
#endif
  #ifdef debugMode
    Serial.println("Init SPIFFS");
  #endif
  SPIFFS.begin(); //File System. Size is set to 1 MB during compile time

  readNodeConfig(); //Reading configuration files from File System
  readMQTTConfig();
  
#ifdef sendLogMsg
  File dataFile = SPIFFS.open("/crash.txt", "a");
  if (dataFile)
  {
    #if defined(ESP8266)   
      SaveCrash.print(dataFile);
      dataFile.close();
      SaveCrash.clear();
    #else
      //add code for ESP32
    #endif
    Serial.println("Writing Crash Dump File complete");
  }
#endif

  pinMode(esp12LED, OUTPUT);
  digitalWrite(esp12LED, true);

//start Watchdog Timer
#if defined(ESP8266)
#ifdef debugMode
  Serial.println("Init WDT");
#endif
  ESP.wdtDisable();
  ESP.wdtEnable(WDTO_8S);
  ESP.wdtFeed();
#else
  //add E
#endif

  startWebServer();
  pgThrottleSetup();
  displayDriver.attach(dispDriverTickTime, adjustSpeed);
  sendRequestSlotByNr(127); //getting system info
  randomSeed(micros());
}

void startWebServer()
{
    server.on("/heap", HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(ESP.getFreeHeap()));
    });

    server.on(ajaxDataStr, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(handleJSON_Data()));
    });

    server.on(ajaxPingStr, HTTP_GET, [](AsyncWebServerRequest *request){
        request->send(200, "text/plain", String(handleJSON_Ping()));
    });

    // Send a GET request to <IP>/get?message=<message>
    server.on("/get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        String message;
        if (request->hasParam(ajaxCmdStr)) {
            message = request->getParam(ajaxCmdStr)->value();
            #ifdef debugMode
              Serial.println(message);
            #endif
            byte newCmd[message.length()+1];
            message.getBytes(newCmd, message.length()+1);     //add terminating 0 to make it work
//            if (processRflCommand(newCmd))          
//              request->send(200, "text/plain", String(handleJSON_Data()));
//            else
//              request->send(400, "text/plain", "Invalid Command");
        } else {
            message = "No message sent";
//            request->send(200, "text/plain", String(handleJSON_Ping()));
        }
    });

    server.onFileUpload([](AsyncWebServerRequest *request, const String& filename, size_t index, uint8_t *data, size_t len, bool final){
      String hlpStr = "/" + filename;
      if(!index)
      {
        uploadFile = SPIFFS.open(hlpStr.c_str(), "w");
        Serial.printf("UploadStart: %s\n", filename.c_str());
      }
      int byteOK = uploadFile.write(data, len);
      Serial.printf("writing %i, %i bytes to: %s\n", len, byteOK, hlpStr.c_str());
      if(final)
      {
        uploadFile.close();
        Serial.printf("Upload Complete: %s\n", hlpStr.c_str());
      }
    });



    
    server.onRequestBody([](AsyncWebServerRequest *request, uint8_t *data, size_t len, size_t index, size_t total){
      if(!index)
        Serial.printf("BodyStart: %u\n", total);
      Serial.printf("%s", (const char*)data);
      if(index + len == total)
        Serial.printf("BodyEnd: %u\n", total);
    });
    // Send a POST request to <IP>/post with a form field message set to <message>
/*    server.on("/post", HTTP_POST, [](AsyncWebServerRequest *request){
        String message;
        if (request->hasParam("/delete", true)) {
            message = request->getParam("/delete", true)->value();
            request->send(200, "text/plain", "Hello, POST: " + message);
        } 
        else if (request->hasParam("/upload", true)) {
            message = request->getParam("/upload", true)->value();
            request->send(200, "text/plain", "Hello, POST: " + message);
        } 
        else 
          request->send(200, "text/plain", "No Params ");
    });
*/
    server.onNotFound([](AsyncWebServerRequest *request)
    {
        Serial.printf("NOT_FOUND: ");
      if(request->method() == HTTP_GET)
        Serial.printf("GET");
      else if(request->method() == HTTP_POST)
        Serial.printf("POST");
      else if(request->method() == HTTP_DELETE)
        Serial.printf("DELETE");
      else if(request->method() == HTTP_PUT)
        Serial.printf("PUT");
      else if(request->method() == HTTP_PATCH)
        Serial.printf("PATCH");
      else if(request->method() == HTTP_HEAD)
        Serial.printf("HEAD");
      else if(request->method() == HTTP_OPTIONS)
        Serial.printf("OPTIONS");
      else
        Serial.printf("UNKNOWN");
      Serial.printf(" http://%s%s\n", request->host().c_str(), request->url().c_str());

      if(request->contentLength()){
        Serial.printf("_CONTENT_TYPE: %s\n", request->contentType().c_str());
        Serial.printf("_CONTENT_LENGTH: %u\n", request->contentLength());
      }
    });
    server.serveStatic("/", SPIFFS, "/").setDefaultFile("index.htm");
    server.begin();
}

void getInternetTime()
{
  int thisIntervall = ntpIntervallDefault;
  if (!ntpOK)
    thisIntervall = ntpIntervallShort;
  if (millis() > (ntpTimer + thisIntervall))
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      #ifdef debugMode
        Serial.println("getInternetTime");
      #endif
      uint32_t NTPDelay = millis();
      dateTime = NTPch.getNTPtime(timeZone, 2);
      ntpTimer = millis();
      while (!dateTime.valid)
      {
        delay(100);
//        Serial.println("waiting for Internet Time");
        dateTime = NTPch.getNTPtime(timeZone, 2);
        if (millis() > NTPDelay + ntpTimeout)
        {
          ntpOK = false;
//          Serial.println("Getting NTP Time failed");
          return;
        }
      }
      NTPDelay = millis() - NTPDelay;
      setTime(dateTime.hour, dateTime.minute, dateTime.second, dateTime.day, dateTime.month, dateTime.year);
      ntpOK = true;
      NTPch.printDateTime(dateTime);

      String NTPResult = "NTP Response Time [ms]: ";
      NTPResult += NTPDelay;
//      Serial.println(NTPResult);
    }
    else
    {
#ifdef sendLogMsg
//      Serial.println("No Internet when calling getInternetTime()");
#endif
      
    }
  }
}

String handleJSON_Data()
{
  String response;
  float float1;
  long curTime = now();
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root.printTo(response);
//  Serial.println(response);
  return response;
}

void handlePingMessage()
{
  String hlpStr = handleJSON_Ping();
  hlpStr.toCharArray(mqttMsg, hlpStr.length()+1);
  if (!mqttClient.connected())
    MQTT_connect();
  if (mqttClient.connected())
  {
    if (!mqttClient.publish(lnPing, mqttMsg)) 
    {
    } else {
    }
  }
}

void handleDataMessage()
{
  String hlpStr = handleJSON_Data();
  hlpStr.toCharArray(mqttMsg, hlpStr.length()+1);
  if (!mqttClient.connected())
    MQTT_connect();
  if (mqttClient.connected())
  {
    if (!mqttClient.publish(lnBCTopic, mqttMsg)) 
    {
    } else {
    }
  }
}

void loop()
{
#if defined(ESP8266)
  ESP.wdtFeed();
#else
  //add ESP32 code
#endif
  if (millis() < lastMillis)
  {
    millisRollOver++;
  //in case of Rollover, update all other affected timers
    ntpTimer = millis();
    lnLoadTimer = millis();
    lnPingTimer = millis();
    lnDataTimer = millis();
  }
  else
    lastMillis = millis(); 
     
#ifdef useIOTAppStory
  IAS.loop();                                                 // this routine handles the reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration
#endif
  //-------- Your Sketch starts from here ---------------
  nex.poll();

  extDisplayUpdate(); //used to update display in case of external mqtt events
  
  if (useNTP)
    getInternetTime();

  if (mqttClient.connected())
    mqttClient.loop();
  else
    MQTT_connect();

  if (millis() > lnThrottlePingTimer + throttlePingInterval)
    if (validLoco)
      sendSpeedCommand();

  if (millis() > lnPingTimer + lnPingIntervall)                           // only for development. Please change it to longer interval in production
  {
    handlePingMessage();
    lnPingTimer = millis();
  }
//  if (millis() > lnDataTimer + lnDataIntervall)                           // only for development. Please change it to longer interval in production
//  {
//    handleDataMessage();
//    lnDataTimer = millis();
//  }
} //loop

// function to check existence of nested key see https://github.com/bblanchon/ArduinoJson/issues/322
bool containsNestedKey(const JsonObject& obj, const char* key) {
    for (const JsonPair& pair : obj) {
        if (!strcmp(pair.key, key))
            return true;

        if (containsNestedKey(pair.value.as<JsonObject>(), key)) 
            return true;
    }

    return false;
}

//read the MQTT config file with server address etc.
int readMQTTConfig()
{
  StaticJsonBuffer<800> jsonBuffer;
  if (SPIFFS.exists("/mqtt.cfg"))
  {
    File dataFile = SPIFFS.open("/mqtt.cfg", "r");
    if (dataFile)
    {
      String jsonData;
      while (dataFile.position()<dataFile.size())
      {
        jsonData = dataFile.readStringUntil('\n');
        jsonData.trim();
      } 
      dataFile.close();
      
      JsonObject& root = jsonBuffer.parseObject(jsonData);
      if (root.success())
      {
        if (root.containsKey("lnBCTopic"))
        {
          strcpy(lnBCTopic, root["lnBCTopic"]);
        }
        if (root.containsKey("lnEchoTopic"))
        {
          strcpy(lnEchoTopic, root["lnEchoTopic"]);
        }
        if (root.containsKey("lnOutTopic"))
        {
          strcpy(lnOutTopic, root["lnOutTopic"]);
        }
        if (root.containsKey("modeNetwork"))
        {
            useNetworkMode = bool(root["modeNetwork"]);       
        }
        if (root.containsKey("useTimeStamp"))
          useTimeStamp = bool(root["useTimeStamp"]);
        if (root.containsKey("mqttServer"))
        {
          if (containsNestedKey(root, "ip"))
            strcpy(mqtt_server, root["mqttServer"]["ip"]);
          if (containsNestedKey(root, "port"))
            mqtt_port = uint16_t(root["mqttServer"]["port"]);
          if (containsNestedKey(root, "user"))
            strcpy(mqtt_user, root["mqttServer"]["user"]);
          if (containsNestedKey(root, "password"))
            strcpy(mqtt_password, root["mqttServer"]["password"]);
          Serial.print(mqtt_server);
          Serial.print(" Port ");
          Serial.println(mqtt_port);
        }
      }
      else
        Serial.println("Error Parsing JSON");
    }
  }
}

int writeMQTTConfig()
{
  StaticJsonBuffer<800> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  String newMsg = "";
  root.printTo(newMsg);
//  Serial.println(newMsg);
//  Serial.println("Writing MQTT Config File");
  File dataFile = SPIFFS.open("/mqtt.cfg", "w");
  if (dataFile)
  {
    dataFile.println(newMsg);
    dataFile.close();
//    Serial.println("Writing Config File complete");
  }
}


//read node config file with variable settings
int readNodeConfig()
{
  StaticJsonBuffer<500> jsonBuffer;
  if (SPIFFS.exists("/node.cfg"))
  {
    File dataFile = SPIFFS.open("/node.cfg", "r");
    if (dataFile)
    {
//      Serial.print("Reading Node Config File ");
//      Serial.println(dataFile.size());
      String jsonData;
      while (dataFile.position()<dataFile.size())
      {
        jsonData = dataFile.readStringUntil('\n');
        jsonData.trim();
//        Serial.println(jsonData);
      } 
      dataFile.close();
      
      JsonObject& root = jsonBuffer.parseObject(jsonData);
      if (root.success())
      {
        if (root.containsKey("NetBIOSName"))
        {
          String hlpStr = root["NetBIOSName"];
          NetBIOSName = hlpStr;
        }
        if (root.containsKey("useNTP"))
          useNTP = bool(root["useNTP"]);
        if (root.containsKey("NTPServer"))
        {
          String hlpStr = root["NTPServer"];
          ntpServer = hlpStr;
          NTPch.setNTPServer(ntpServer);
        }
        if (root.containsKey("ntpTimeZone"))
          timeZone = int(root["ntpTimeZone"]);
      }
    }
  } 
}

int writeNodeConfig()
{
  StaticJsonBuffer<800> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["NetBIOSName"] = NetBIOSName;
  root["useNTP"] = int(useNTP);
  root["NTPServer"] = ntpServer;
  root["ntpTimeZone"] = timeZone;
  //add code to save Kx values
  String newMsg = "";
  root.printTo(newMsg);
//  Serial.println(newMsg);
//  Serial.println("Writing Node Config File");
  File dataFile = SPIFFS.open("/node.cfg", "w");
  if (dataFile)
  {
    dataFile.println(newMsg);
    dataFile.close();
//    Serial.println("Writing Config File complete");
  }
}

//read node config file with variable settings
int readProfileConfig()
{
  StaticJsonBuffer<800> jsonBuffer;
  if (SPIFFS.exists("/curves.cfg"))
  {
    File dataFile = SPIFFS.open("/curves.cfg", "r");
    if (dataFile)
    {
      String jsonData;
      while (dataFile.position()<dataFile.size())
      {
        jsonData += dataFile.readStringUntil('\n');
      } 
      jsonData.trim();
      dataFile.close();
      
      JsonObject& root = jsonBuffer.parseObject(jsonData);
      if (root.success())
      {
      }
    }
    #ifdef debugMode
      else
        Serial.println("not opened");
    #endif
    }  
}

//========================================================= Nextion Display Handler Code starts here==============================================================================
void nextionSetup()
{
  //page 1
  btnSwitch.attachCallback(cb_btnSwitch);
  btnShift.attachCallback(cb_btnShift);
  btnStop.attachCallback(cb_btnStop);
  txtLocoAddr.attachCallback(cb_txtAddr);
  btnForward.attachCallback(cb_btnForward);
  btnBackward.attachCallback(cb_btnBackward);
  btnF0.attachCallback(cb_btnF0);
  btnF1.attachCallback(cb_btnF0);
  btnF2.attachCallback(cb_btnF0);
  btnF3.attachCallback(cb_btnF0);
  btnF4.attachCallback(cb_btnF0);
  btnF5.attachCallback(cb_btnF0);
  btnF6.attachCallback(cb_btnF0);
  btnF7.attachCallback(cb_btnF0);
  btnBrake.attachCallback(cb_btnBrake);
  hspSpeed0.attachCallback(cb_hspSpeed);
  hspSpeed1.attachCallback(cb_hspSpeed);
  hspSpeed2.attachCallback(cb_hspSpeed);
  hspSpeed3.attachCallback(cb_hspSpeed);
  hspSpeed4.attachCallback(cb_hspSpeed);
  hspSpeed5.attachCallback(cb_hspSpeed);
  hspSpeed6.attachCallback(cb_hspSpeed);
  hspSpeed7.attachCallback(cb_hspSpeed);
  hspSpeed8.attachCallback(cb_hspSpeed);
  hspSpeed9.attachCallback(cb_hspSpeed);
  hspSpeed10.attachCallback(cb_hspSpeed);
  hspSpeed11.attachCallback(cb_hspSpeed);
  hspSpeed12.attachCallback(cb_hspSpeed);
  hspSpeed13.attachCallback(cb_hspSpeed);
  hspSpeed14.attachCallback(cb_hspSpeed);
  hspSpeed15.attachCallback(cb_hspSpeed);
  hspSpeed16.attachCallback(cb_hspSpeed);
  hspSpeed17.attachCallback(cb_hspSpeed);
  hspSpeed18.attachCallback(cb_hspSpeed);
  ggSpeedNeedle.attachCallback(cb_ggSpeedNeedle);

  //page 2
  btnAddress.attachCallback(cb_btnAddress);
  btnAddrMinus.attachCallback(cb_btnMinus);
  btnAddrPlus.attachCallback(cb_btnPlus);
  btnExit.attachCallback(cb_btnExit);
  
  picSwitch1.attachCallback(cb_picSwitch);
  picSwitch2.attachCallback(cb_picSwitch);
  picSwitch3.attachCallback(cb_picSwitch);
  picSwitch4.attachCallback(cb_picSwitch);
  picSwitch5.attachCallback(cb_picSwitch);
  picSwitch6.attachCallback(cb_picSwitch);
  picSwitch7.attachCallback(cb_picSwitch);
  picSwitch8.attachCallback(cb_picSwitch);
  picSwitch9.attachCallback(cb_picSwitch);
  picSwitch10.attachCallback(cb_picSwitch);
  picSwitch11.attachCallback(cb_picSwitch);
  picSwitch12.attachCallback(cb_picSwitch);
  picSwitch13.attachCallback(cb_picSwitch);
  picSwitch14.attachCallback(cb_picSwitch);
  picSwitch15.attachCallback(cb_picSwitch);
  picSwitch16.attachCallback(cb_picSwitch);
  btnSwiBackward.attachCallback(cb_btnBackward);
  btnSwiForward.attachCallback(cb_btnForward);
  btnSwiSpeedMinus.attachCallback(cb_btnSpdMinus);
  btnSwiSpeedPlus.attachCallback(cb_btnSpdPlus);
  btnSwiBrake.attachCallback(cb_btnBrake);

  //page 3
  btnExitCTC.attachCallback(cb_btnExitCTC);
  picCTCSwitch1.attachCallback(cb_picCTCSwitch);
  picCTCSwitch2.attachCallback(cb_picCTCSwitch);
  picCTCSwitch3.attachCallback(cb_picCTCSwitch);
  picCTCSwitch4.attachCallback(cb_picCTCSwitch);
  btnCTCBackward.attachCallback(cb_btnBackward);
  btnCTCForward.attachCallback(cb_btnForward);
  btnCTCSpeedMinus.attachCallback(cb_btnSpdMinus);
  btnCTCSpeedPlus.attachCallback(cb_btnSpdPlus);
  btnCTCBrake.attachCallback(cb_btnBrake);

  //page 4

  //page 6
  btnConsistClose.attachCallback(cb_btnConsistClose);
  btnRelease0.attachCallback(cb_btnRelease);
  btnF00.attachCallback(cb_btnCnFct);
  btnF10.attachCallback(cb_btnCnFct);
  btnF20.attachCallback(cb_btnCnFct);
  btnRelease1.attachCallback(cb_btnRelease);
  btnF01.attachCallback(cb_btnCnFct);
  btnF11.attachCallback(cb_btnCnFct);
  btnF21.attachCallback(cb_btnCnFct);
  btnRelease2.attachCallback(cb_btnRelease);
  btnF02.attachCallback(cb_btnCnFct);
  btnF12.attachCallback(cb_btnCnFct);
  btnF22.attachCallback(cb_btnCnFct);
  btnRelease3.attachCallback(cb_btnRelease);
  btnF03.attachCallback(cb_btnCnFct);
  btnF13.attachCallback(cb_btnCnFct);
  btnF23.attachCallback(cb_btnCnFct);
  btnRelease4.attachCallback(cb_btnRelease);
  btnF04.attachCallback(cb_btnCnFct);
  btnF14.attachCallback(cb_btnCnFct);
  btnF24.attachCallback(cb_btnCnFct);
  picPos0.attachCallback(cb_picPos);
  picPos1.attachCallback(cb_picPos);
  picPos2.attachCallback(cb_picPos);
  picPos3.attachCallback(cb_picPos);
  picPos4.attachCallback(cb_picPos);

  btnShiftCn.attachCallback(cb_btnShift);
  btnPgDn.attachCallback(cb_btnPgDn);
  btnPgUp.attachCallback(cb_btnPgUp);
  btnAddLoco.attachCallback(cb_btnAddLoco);
  
}

void extDisplayUpdate() //called from Loop to react to MQTT induced changes
{
  if (pwrStatus != pwrStatusCopy)
  {
    throttlePwrBtnSetup();
    pwrStatusCopy = pwrStatus;
  }
}

void pgThrottleSetup()
{
  nex.sendCommand("page 1");
  currentPage = 1;
  btnShiftState = false;
  throttlePwrBtnSetup();
  throttleDisplaySetup();
}

void pgConsistMgmtSetup()
{
  nex.sendCommand("page 6");
  currentPage = 6;

  cnLoadLocoInfo(&workSlot);

//  Serial.print("vaCnStat ");
//  Serial.println(cnStat);
}

void cnLoadLocoInfoLayer(LocoNetSlot * startSlot, int * cnCounter, int * currDispLine, int currLayer)
{
  LocoNetSlot * tempSlot;
//  Serial.print("Looking for Consists for ");
//  Serial.println((startSlot->slotData[2] & 0x7F) + ((startSlot->slotData[7] & 0x7F) << 7));
  for (int i=0; i < numSysSlots; i++)
  {
    //looking for uplinked slot into currentSlot
    if (((getConsistStatus(arraySlots[i]) & 0x02) > 0) && (arraySlots[i].slotData[3] == startSlot->slotData[0])) //found a slot linked into calling slot, add to list
    {
      tempSlot = &arraySlots[i];
      if ((*cnCounter >= cnDisplayStart) && (*currDispLine < 5)) //add to display if in viewable line number
      {
        cnDispSlots[*currDispLine] = tempSlot->slotData[0] + ((currLayer & 0x00FF) << 8);
//        Serial.print("Disp Line ");
//        Serial.println(*currDispLine);
//        Serial.println(currLayer);
//        Serial.println((tempSlot->slotData[2] & 0x7F) + ((tempSlot->slotData[7] & 0x7F) << 7));
        *currDispLine += 1;
      }
      *cnCounter +=1;
      if ((getConsistStatus(*tempSlot) & 0x01) > 0) //has slots linked into it as well
        cnLoadLocoInfoLayer(tempSlot, cnCounter, currDispLine, currLayer+1);
      if (*currDispLine > 5) //no need to keep loading after all lines used
        break;
    }
  }
}

void cnLoadLocoInfo(LocoNetSlot * startSlot)
{
  LocoNetSlot * tempSlot = startSlot;
  int cnCounter = 0;
  int currDispLine = 0;
  int currLayer = 0;
  char buff[10];

  for (int i=0; i<5;i++)
    cnDispSlots[i] = 0x00FF;
  cnDisplayStart = 0;
  cnNumEntries = 0;
  cnSelSlot = -1;
  btnShiftState = false;

  cnLoadLocoInfoLayer(tempSlot, &cnCounter, &currDispLine, currLayer);
  cnNumEntries = cnCounter;
  
  for (int i=0; i<5;i++)
  {
    String hlpStr = "va" + String(i);
    hlpStr.toCharArray(buff, 10);
    int subStat = -1;
    if ((cnDispSlots[i] & 0x00FF) < 0xFF)
    {
      byte slotNr = cnDispSlots[i] & 0x00FF;
      subStat = (arraySlots[slotNr].slotData[2] & 0x7F) + ((arraySlots[slotNr].slotData[7] & 0x7F) << 7) + ((cnDispSlots[i] & 0xFF00)<<8);// + ((tempSlot->slotData[4] & 0x1F) << 16) + ((tempSlot->slotData[8] & 0x0F) << 24);
    }
    setNumVal(buff, subStat); //triggers the timer update in the display
//    Serial.print(hlpStr);
//    Serial.println(subStat);
  }
  int address = (startSlot->slotData[2] & 0x7F) + ((startSlot->slotData[7] & 0x7F) << 7);
  uint32_t cnStat = (address & 0x00003FFF) + ((cnDisplayStart & 0x00FF) << 16) + ((cnNumEntries & 0x00FF) << 24);
  setNumVal("vaCnStat", cnStat); //triggers the timer update in the display
}

void throttlePwrBtnSetup()
{
  if (pwrStatus)
  {
    btnStop.setText("IDLE");
  }
  else
  {
    btnStop.setText("RUN");
  }
}

void setDirF()
{
  word dirVal;
  if (validLoco)
  {
    dirVal = ((workSlot.slotData[4] & 0x7F) <<8) + (workSlot.slotData[8] & 0x7F);
//    Serial.print("DIRF ");
//    Serial.println(dirVal,16);
  }
  else
    dirVal = 0;
  setNumVal("vaDIRF", dirVal);
}

void setNumVal(String varName, long numVal)
{
  String outStr = varName + ".val=" + String(numVal);
  char buff[25];
  outStr.toCharArray(buff, 25);
  nex.sendCommand(buff);  
}

void throttleDisplaySetup()
{
  char buff[4];
  if (validLoco)
  {
    int address = (workSlot.slotData[2] & 0x7F) + ((workSlot.slotData[7] & 0x7F) << 7);
    setNumVal("vaAddr", address);
    currentSpeed = workSlot.slotData[3] & 0x7F;
    if (currentSpeed == 1)
      currentSpeed = 0;
    targetSpeed = currentSpeed;
    setNumVal("vaSpeed", currentSpeed);
    setDirF();
    targetDir = workSlot.slotData[4] & 0x20;
  }
  else
  {
    setNumVal("vaAddr", -1);
    currentSpeed = 0;
    targetSpeed = 0;
    setNumVal("vaSpeed", 0);
    setDirF();
  }
  displaySlot = workSlot;
}

void pgCTCPanelSetup()
{
  nex.sendCommand("page 3");
  currentPage = 3;
  updateInpRepDisplay();
  setNumVal("vaSwiStat", swPos[0]);
  if (validLoco)
  {
    setNumVal("vaAddr", 1);
    setNumVal("vaSpeed", currentSpeed);
    setDirF();
  }
  else
  {
    setNumVal("vaAddr", -1);
  }
}

void pgSwitchBoardSetup()
{
  nex.sendCommand("page 2");
  currentPage = 2;
  setNumVal("vaBaseAddr", switchBoardStartAddr);
  updateSwiStatDisplay();
  if (validLoco)
  {
    setNumVal("vaAddr",1);
    setNumVal("vaSpeed", currentSpeed);
    setDirF();
  }
  else
  {
    setNumVal("vaAddr",-1);
  }
}

void updateInpRepDisplay()
{
  if (currentPage==3)
  {
    long inpStatus=bdStatus[0] + (bdStatus[1]<<8) + (bdStatus[6]<<16) + (bdStatus[7]<<24);
    setNumVal("vaInpStat", inpStatus);
//    Serial.println(inpStatus, 16);
  }
}

void updateSwiStatDisplay()
{
  if (currentPage==2)
  {
    uint32_t swiStatus=0;
    uint16_t thisAddress;
    byte byteIndex = (switchBoardStartAddr-1)>>2;  
    byte bitMask;
    for (int i=0;i<16;i++)
    {
      thisAddress = switchBoardStartAddr-1 + i;
      byteIndex = thisAddress>>2;
      bitMask = 0x01 << (2*((byte)thisAddress & 0x03));
      bool currStatus = swPos[byteIndex] & bitMask;
      if (currStatus)
        swiStatus = swiStatus | (0x01<<(2*i));
    }
//    Serial.print("Swi Stat: ");
//    Serial.println(swiStatus,16);
    setNumVal("vaSwiStat", swiStatus);
  }
   
  if (currentPage==3)
      setNumVal("vaSwiStat", swPos[0]);
}

void cb_btnAddress(NextionEventType type, INextionTouchable *widget)
{
  char buff[4];
  if (type == NEX_EVENT_PUSH)
  {
    lnBrakeOneShotTimer = millis();
    btnCancel.attachCallback(cb_btnCancelSwi);
    btnEnter.attachCallback(cb_btnEnterSwi);
    numValBuffer = switchBoardStartAddr;
    nex.sendCommand("page 4");
    currentPage = 4;
    ltoa(numValBuffer, buff, 10);
    txtKeypadField.setText(buff);
    txtKeypadInfo.setText("Enter Addr from 1 - 2048");
    txtKeypadError.setText("");
  }
  else if (type == NEX_EVENT_POP)
  { 
    if (lnBrakeOneShotTimer + lnBrakeTimeout < millis() && validLoco) //Long press while loco assigned. Call consist dialog
    {
    }
    else //short press or no loco assigned, call address selector dialog
    {
    }
  }
}

void cb_btnCancelSwi(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    pgSwitchBoardSetup();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnEnterSwi(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    char buff[4];
    int buffLen;
    txtKeypadField.getText(buff, 4);
    numValBuffer = atoi(buff);
    if ((numValBuffer > 0) && (numValBuffer <= maxSwiAddr))
    {
      switchBoardStartAddr = numValBuffer;
      pgSwitchBoardSetup();
    }
    else
    {
      txtKeypadError.setText("INVALID ENTRY");
//      txtKeypadError.show();
    }
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnPlus(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    switchBoardStartAddr = min(switchBoardStartAddr+16, maxSwiAddr-15);
    updateSwiStatDisplay();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnMinus(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    switchBoardStartAddr = max(switchBoardStartAddr-16, 1);
    updateSwiStatDisplay();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnExit(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    pgThrottleSetup();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnExitCTC(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    pgThrottleSetup();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnF0(NextionEventType type, INextionTouchable *widget)
{
  int funcNr = widget->getComponentID()-4; //ID Offset is 4
  if ((type == NEX_EVENT_PUSH) && validLoco)
  {
    if (btnShiftState)
      funcNr += 8;

    switch (funcNr)
    {
      case 0: workSlot.slotData[4] = workSlot.slotData[4] ^ 0x10; sendDIRF0to4Command(); break;
      case 1: workSlot.slotData[4] = workSlot.slotData[4] ^ 0x01; sendDIRF0to4Command(); break;
      case 2: workSlot.slotData[4] = workSlot.slotData[4] ^ 0x02; sendDIRF0to4Command(); break;
      case 3: workSlot.slotData[4] = workSlot.slotData[4] ^ 0x04; sendDIRF0to4Command(); break;
      case 4: workSlot.slotData[4] = workSlot.slotData[4] ^ 0x08; sendDIRF0to4Command(); break;
      case 5: workSlot.slotData[8] = workSlot.slotData[8] ^ 0x01; sendDIRF5to8Command(); break;
      case 6: workSlot.slotData[8] = workSlot.slotData[8] ^ 0x02; sendDIRF5to8Command(); break;
      case 7: workSlot.slotData[8] = workSlot.slotData[8] ^ 0x04; sendDIRF5to8Command(); break;
      case 8: workSlot.slotData[8] = workSlot.slotData[8] ^ 0x08; sendDIRF5to8Command(); break;
      //missing: F9-F15, using IMM_PACK Commands
      default: return;
    }
    setDirF(); //update display variable
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnSwitch(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (!btnShiftState)
    {
      pgSwitchBoardSetup();
    }
    else
    {
      pgCTCPanelSetup();
    }
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnShift(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    switch (currentPage)
    {
      case 1: btnShiftState = btnShift.isActive(); break;
      case 6: btnShiftState = btnShiftCn.isActive(); break;
    }
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnForward(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (validLoco)
    {
      if (!cfgInstantDir)
      {
        targetDir = 0x20;
        lnBrakeOneShotTimer = millis();
      }
      else
      {
        targetSpeed=0;
        currentSpeed=0;
        targetDir = 0x20;
        workSlot.slotData[3] = 0x00;
        setNumVal("vaSpeed", currentSpeed);
        sendSpeedCommand();
        workSlot.slotData[4] |= 0x20;
        setDirF();
        sendDIRF0to4Command();
      }
    }
  }
  else if (type == NEX_EVENT_POP)
  {
    if (validLoco)
      if (lnBrakeOneShotTimer + lnBrakeTimeout < millis())
      {
        targetSpeed=0;
        currentSpeed=0;
        workSlot.slotData[3] = 0x00;
        setNumVal("vaSpeed", currentSpeed);
        sendSpeedCommand();
        workSlot.slotData[4] |= 0x20;
        setDirF();
        sendDIRF0to4Command();
      }
  }
}

void cb_btnBackward(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (validLoco)
    {
      if (!cfgInstantDir)
      {
        targetDir = 0x00;
        lnBrakeOneShotTimer = millis();
      }
      else
      {
        targetSpeed=0;
        currentSpeed=0;
        targetDir = 0x00;
        workSlot.slotData[3] = 0x00;
        setNumVal("vaSpeed", currentSpeed);
        sendSpeedCommand();
        workSlot.slotData[4] &= ~0x20;
        setDirF();
        sendDIRF0to4Command();
      }
    }
  }
  else if (type == NEX_EVENT_POP)
  {
    if (validLoco)
      if (lnBrakeOneShotTimer + lnBrakeTimeout < millis())
      {
        targetSpeed=0;
        currentSpeed=0;
        workSlot.slotData[3] = 0x00;
        setNumVal("vaSpeed", currentSpeed);
        sendSpeedCommand();
        workSlot.slotData[4] &= ~0x20;
        setDirF();
        sendDIRF0to4Command();
      }
  }
}


void cb_btnSpdMinus(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (targetSpeed > 0)
      targetSpeed-=5;
    else
      targetSpeed=0;
  }
}

void cb_btnSpdPlus(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (targetSpeed < 122)
      targetSpeed+=5;
    else
      targetSpeed=127;
  }
}

void cb_btnBrake(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (validLoco)
    {
      lnBrakeOneShotTimer = millis();
      targetSpeed = 0;
    }
  }
  else if (type == NEX_EVENT_POP)
  {
    if (validLoco)
      if (lnBrakeOneShotTimer + lnBrakeTimeout < millis())
      {
        targetSpeed = 0;
        currentSpeed = 0;
        workSlot.slotData[3] = 0x01;
        setNumVal("vaSpeed", currentSpeed);
        sendSpeedCommand();
      } 
  }
}

void adjustSpeed()
{
  bool lnUsed = false;
  if (displayReady && validLoco)
  {
    displayReady = false;
    byte tempTargetSpeed=0;
    byte currentDir = workSlot.slotData[4] & 0x20;

    if ((cfgInstantDir) || (currentDir==targetDir))
      tempTargetSpeed = targetSpeed;
    else
    {
      tempTargetSpeed = 0;
      if (currentSpeed==0)
      {
        workSlot.slotData[4] ^= 0x20;
        setDirF();
        sendDIRF0to4Command();
        lnUsed = true;
      }
    }
//    Serial.println(currentDir);
//    Serial.println(targetDir);
//    Serial.println(targetSpeed);
//    Serial.println(tempTargetSpeed);
    
    if (tempTargetSpeed != currentSpeed)
    {
      int dispDifference = tempTargetSpeed - currentSpeed; 
      int dispSignum = 0;
      if (dispDifference != 0)
      {
        dispSignum = round(dispDifference / abs(dispDifference)); //positive if current < target
        dispDifference = abs(dispDifference);
      }
      dispDifference = min(max(round(dispDifference/10),(long)1),(long)5); //change 1/10 of difference but min 1 and not more than 5 points
      currentSpeed = min(max(currentSpeed + (dispSignum * dispDifference),0), 126);
      switch (currentSpeed)
      {
        case 0:;
        case 1: workSlot.slotData[3] = 0; break;
        default: workSlot.slotData[3] = currentSpeed + 1; break;
      }
      switch (currentPage)
      {
        case 1:; //for pages 1-3 we send a speed update. All variables have the same name
        case 2:;
        case 3: setNumVal("vaSpeed", currentSpeed); break;
        default: break;
      }
      sendSpeedCommand();
      lnUsed = true;
    }
    displayReady = true;
  }
// this is experimental. All slots are read to allow for building up the consist list as consists are downlinked
// if activated, the throttle should not be actively used for about 1 Min after start up to allow for reading all slots
// the better way would be to implement a slot monitor in either the MQTT gateway or in Node RED, combined with a Consist Service that would request SL_RD as soon as 
// it sees a consist link or unlink command coming along the network

//  if (!lnUsed)
//    updateSlotArray();
}

void updateSlotArray()
{
  if (arraySlots[0].slotData[127] < 0xFF)
  {
    if ((arraySlots[127].slotData[9] & 0x08) == 0)
      numSysSlots = 22;
    else
      numSysSlots = 120;
    for (int i=0; i<numSysSlots; i++)
    {
      if (arraySlots[i].slotData[0] != i)
      {
        sendRequestSlotByNr(i);
        return;
      }
    }
    if (arraySlots[0x7B].slotData[0] != 0x7B) //Fast Clock
    {
      sendRequestSlotByNr(0x7B);
      return;
    }
    if (arraySlots[0x7C].slotData[0] != 0x7C) //Programmer
    {
      sendRequestSlotByNr(0x7C);
      return;
    }
  }
}

void cb_hspSpeed(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (validLoco)
    {
      lnBrakeOneShotTimer = millis();
      int touchPoint = max(widget->getComponentID() - 11, 0); //compensate for the first touchpoint with ID 3, bring them in a row from 0 - 13
      if (touchPoint > 13)
        touchPoint -= 17; //compensation of second part of hsp array starting at 42-46
      Serial.println(touchPoint);
      targetSpeed = round(touchPoint * 126 / 18);
      Serial.println(targetSpeed);
    }
    else
    {
      targetSpeed = 0;
      currentSpeed = 0;
    }
  }
  else if (type == NEX_EVENT_POP)
  {
    if (validLoco)
      if (lnBrakeOneShotTimer + lnBrakeTimeout < millis())
      {
        int touchPoint = max(widget->getComponentID() - 11, 0); //compensate for the first touchpoint with ID 3, bring them in a row from 0 - 13
        if (touchPoint > 13)
          touchPoint -= 17; //compensation of second part of hsp array starting at 42-46
        targetSpeed = round(touchPoint * 126 / 18);
        currentSpeed=targetSpeed;
        workSlot.slotData[3] = currentSpeed;
        setNumVal("vaSpeed", currentSpeed);
        sendSpeedCommand();
      }
  }
}

void cb_ggSpeedNeedle(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (validLoco && (currentSpeed != targetSpeed))
    {
      targetSpeed = currentSpeed;
    }
  }
  
}

void cb_btnStop(NextionEventType type, INextionTouchable *widget)
{
  sendPowerCommand(!pwrStatus);
  pwrStatus = !pwrStatus;
  throttlePwrBtnSetup();
}

void cb_txtAddr(NextionEventType type, INextionTouchable *widget)
{
  char buff[4];
  if (type == NEX_EVENT_PUSH)
  {
    lnBrakeOneShotTimer = millis();
  }
  else if (type == NEX_EVENT_POP)
  {
    if (((lnBrakeOneShotTimer + lnBrakeTimeout) < millis()) && validLoco && ((getConsistStatus(workSlot) & 0x01) == 1 )) //Long press while consist loco assigned. Call consist dialog
    {
      //add code to call consist dialog
      pgConsistMgmtSetup();
      nex.sendCommand("page6");
    }
    else //short press or no loco assigned, call address selector dialog
    {
      uint32_t currentAddress;
      if (validLoco)
        currentAddress = (workSlot.slotData[2] & 0x7F) + ((workSlot.slotData[7] & 0x7F) << 7);
      else
        currentAddress = 0;
      sendReleaseSlot();
      nex.sendCommand("page 5");
      currentPage = 5;
      btnLocoCancel.attachCallback(cb_btnCancelAddr);
      btnLocoSelect.attachCallback(cb_btnSelectAddr);
      btnLocoSteal.attachCallback(cb_btnStealAddr);
      btnDisp.attachCallback(cb_btnDisp);
      btnNum1.attachCallback(cb_btnNumKeys);
      btnNum2.attachCallback(cb_btnNumKeys);
      btnNum3.attachCallback(cb_btnNumKeys);
      btnNum4.attachCallback(cb_btnNumKeys);
      btnNum5.attachCallback(cb_btnNumKeys);
      btnNum6.attachCallback(cb_btnNumKeys);
      btnNum7.attachCallback(cb_btnNumKeys);
      btnNum8.attachCallback(cb_btnNumKeys);
      btnNum9.attachCallback(cb_btnNumKeys);
      btnNum0.attachCallback(cb_btnNumKeys);
      btnNumBack.attachCallback(cb_btnNumKeys);
      btnNumClear.attachCallback(cb_btnNumKeys);
      ltoa(currentAddress, buff, 10);
      txtLocoKeypadField.setText(buff);
      txtLocoKeypadInfo.setText("Enter Addr from 0 - 9983");
      txtLocoKeypadError.setText("");
      updateAddrKeypad();
      sendRequestSlot(currentAddress);
    }
  }
}

void cb_btnConsistClose(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    pgThrottleSetup();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnRelease(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    int row = 0;
    int thisButtonID = widget->getComponentID();
    switch (thisButtonID)
    {
      case 3: row = 0; break;
      case 13: row = 1; break;
      case 18: row = 2; break;
      case 23: row = 3; break;
      case 28: row = 4; break;
    }
    int releaseThis = cnDispSlots[row] & 0x00FF;
    int releaseFrom = arraySlots[releaseThis].slotData[3];
    sendUnlinkSlotsCommand(releaseThis, releaseFrom);
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnCnFct(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    int col = 0, row = 0;
    int thisButtonID = widget->getComponentID();
    switch (thisButtonID)
    {
      case 4: col = 0; row = 0; break;
      case 5: col = 1; row = 0; break;
      case 6: col = 2; row = 0; break;
      case 14: col = 0; row = 1; break;
      case 15: col = 1; row = 1; break;
      case 16: col = 2; row = 1; break;
      case 19: col = 0; row = 2; break;
      case 20: col = 1; row = 2; break;
      case 21: col = 2; row = 2; break;
      case 24: col = 0; row = 3; break;
      case 25: col = 1; row = 3; break;
      case 26: col = 2; row = 3; break;
      case 29: col = 0; row = 4; break;
      case 30: col = 1; row = 4; break;
      case 31: col = 2; row = 4; break;
    }
    if (btnShiftState)
      col += 3;
    LocoNetSlot * thisLoco = &arraySlots[cnDispSlots[row] & 0x00FF];
    int currentAddress = (thisLoco->slotData[2] & 0x7F) + ((thisLoco->slotData[7] & 0x7F) << 7);
    //prepare function data
    byte bitMask = 0x01;
    switch (col)
    {
      case 0: bitMask = 0x10; break;
      case 5: bitMask = 0x10; break; //should not happen
      default: bitMask = 0x01 << (col-1); break;
    }
    Serial.println(col);
    Serial.println(bitMask);
    thisLoco->slotData[4] ^= bitMask;
    //send function data
    sendDIRFConsistCommand(thisLoco);
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_picPos(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    int thisPicID = widget->getComponentID();
    int newSel = -1;
    switch (thisPicID)
    {
      case 7: newSel = 0; break;
      case 12: newSel = 1; break;
      case 16: newSel = 2; break;
      case 22: newSel = 3; break;
      case 27: newSel = 4; break;
    }
    if (cnSelSlot==newSel)
      cnSelSlot = -1;
    else
      cnSelSlot = newSel;
  }
}

void cb_btnPgDn(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    cnDisplayStart += 5;
    pgConsistMgmtSetup();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnPgUp(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    cnDisplayStart = max(0, cnDisplayStart - 5);
    pgConsistMgmtSetup();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnAddLoco(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (cnSelSlot >= 0)
    {
      int linkTo = (cnDispSlots[cnSelSlot] & 0x7F);
      int linkThis;
      sendLinkSlotsCommand(linkThis, linkTo);
    }
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnNumKeys(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    char buff[4];
    int buffLen;
    if (txtLocoKeypadField.getText(buff, 4) > 0)
    {
      numValBuffer = atoi(buff);
//      Serial.println(numValBuffer);
      if ((numValBuffer >= 0) && (numValBuffer <= maxLocoAddr))
      {
        sendRequestSlot(numValBuffer);
      }
      else
        txtLocoKeypadError.setText("");
    }
    else
    {
      txtLocoKeypadError.setText("INVALID ENTRY");
    }
  }
  else if (type == NEX_EVENT_POP)
  {
  }
  
}

byte getConsistStatus(LocoNetSlot ofSlot)
{
  return ((ofSlot.slotData[1] & 0x08) >> 3) + ((ofSlot.slotData[1] & 0x40)>>5);
}

void updateAddrKeypad()
{
//  Serial.println(tempSlot.slotData[1], 16);
  char buff[25];
  byte speedSteps = tempSlot.slotData[1] & 0x07;
  byte consistStatus = getConsistStatus(tempSlot);
  byte opsStatus = (tempSlot.slotData[1] & 0x30) >> 4;
  String myStr;
  switch (opsStatus)
  {
    case 0: myStr = "FREE "; break;
    case 1: myStr = "COMMON "; break;
    case 2: myStr = "IDLE "; break;
    case 3: myStr = "IN USE "; break;
  }
  switch (consistStatus)
  {
    //case 0: myStr = myStr + ""; break; // not in a consist
    case 1: myStr = myStr + "CN TOP "; break;
    case 2: myStr = myStr + "CN BOT "; break;
    case 3: myStr = myStr + "CN MID "; break;
  }
  switch (speedSteps)
  {
    case 0: myStr = myStr + "28 Steps"; break;
    case 1: myStr = myStr + "28 Tri "; break;
    case 2: myStr = myStr + "14 Steps"; break;
    case 3: myStr = myStr + "128 Steps"; break;
    case 4: myStr = myStr + "28 Ad Cn"; break;
    case 5: myStr = myStr + "n/a"; break;
    case 6: myStr = myStr + "n/a"; break;
    case 7: myStr = myStr + "128 Ad Cn"; break;
  }
  myStr.toCharArray(buff, 25);
  txtLocoKeypadError.setText(buff);
  bool allowSelect = (consistStatus < 2) && (opsStatus < 3);
  bool allowSteal = (consistStatus < 2) && (opsStatus == 3); 
  if (allowSelect)
    nex.sendCommand("vis bSelect,1");
  else
    nex.sendCommand("vis bSelect,0");
  if (allowSteal)
    nex.sendCommand("vis bSteal,1");
  else
    nex.sendCommand("vis bSteal,0");
  
}

void cb_btnCancelAddr(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    pgThrottleSetup();
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnSelectAddr(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    char buff[4];
    int buffLen;
    txtKeypadField.getText(buff, 4);
    numValBuffer = atoi(buff);
    if ((numValBuffer >= 0) && (numValBuffer <= maxLocoAddr))
    {
      workSlot.slotData[0] = tempSlot.slotData[0];
      stealOK = false;
      sendMoveSlot(tempSlot.slotData[0], tempSlot.slotData[0]);
    }
    else
    {
      txtKeypadError.setText("INVALID ENTRY");
    }
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnStealAddr(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    char buff[4];
    int buffLen;
    txtKeypadField.getText(buff, 4);
    numValBuffer = atoi(buff);
    if ((numValBuffer >= 0) && (numValBuffer <= maxLocoAddr))
    {
      workSlot.slotData[0] = tempSlot.slotData[0];
      stealOK = true;
      sendMoveSlot(tempSlot.slotData[0], tempSlot.slotData[0]);
    }
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}

void cb_btnDisp(NextionEventType type, INextionTouchable *widget)
{
  if (type == NEX_EVENT_PUSH)
  {
    if (validLoco)
    {
      sendReleaseSlot();
      sendMoveSlot(workSlot.slotData[0], 0);
      validLoco = false;
      workSlot.slotData[0] = 0xFF;
    }
    else
    {
      sendMoveSlot(0, 1); //dispatch get
      validLoco = false;
      workSlot.slotData[0] = 0xFF;
    }
  }
  else if (type == NEX_EVENT_POP)
  {
  }
}


void cb_picSwitch(NextionEventType type, INextionTouchable *widget)
{
  NextionPicture *thisPic = (NextionPicture*)widget;
  int swiAddr;
  int hlpID = widget->getComponentID();
  switch (hlpID) // ID Offset is 2
  {
    case 2:;
    case 3:;
    case 4: swiAddr = hlpID - 2; break;
    case 42: swiAddr = 3; break;
    case 5:;
    case 6:;
    case 7: swiAddr = hlpID - 1; break;
    case 44: swiAddr = 7; break;
    case 8:;
    case 9:;
    case 10: swiAddr = hlpID; break;
    default: swiAddr = hlpID - 35; break;
  }
  Serial.println(hlpID);
  swiAddr += switchBoardStartAddr;
  Serial.println(swiAddr);
  byte swiPos = (thisPic->getPictureID() == 13);
  if (type == NEX_EVENT_PUSH)
  {
    sendSwitchCommand(swiAddr, swiPos, true);
  }
  else if (type == NEX_EVENT_POP)
  {
    sendSwitchCommand(swiAddr, swiPos, false);
  }
}

void cb_picCTCSwitch(NextionEventType type, INextionTouchable *widget)
{
  NextionPicture *thisPic = (NextionPicture*)widget;
  int swiAddr = widget->getComponentID() - 2; // ID Offset is 2
  if (swiAddr > 4)
    return;
  swiAddr = 5 - swiAddr; //Swi 1 is on right on the track 
  byte swiPos = (thisPic->getPictureID() == 16);
  if (type == NEX_EVENT_PUSH)
  {
    sendSwitchCommand(swiAddr, swiPos, true);
  }
  else if (type == NEX_EVENT_POP)
  {
    sendSwitchCommand(swiAddr, swiPos, false);
  }
}


//==============================================================Web Server=================================================

String extractValue(String keyWord, String request)
{
  int startPos = request.indexOf(keyWord);
  int endPos = -1;
  if (startPos >= 0)
  {
    startPos = request.indexOf("=", startPos) + 1;
//    Serial.println(startPos);
    endPos = request.indexOf("&", startPos);
//    Serial.println(endPos);
    if (endPos < 0)
      endPos = request.length();
//    Serial.println(request.substring(startPos, endPos));   
    return request.substring(startPos, endPos);  
  }
  else
    return("");
}


String handleJSON_Ping()
{
  String response;
  float float1;
  long curTime = now();
  StaticJsonBuffer<500> jsonBuffer;
  JsonObject& root = jsonBuffer.createObject();
  root["IP"] = WiFi.localIP().toString();
  if (WiFi.status() == WL_CONNECTED)
  {
    long rssi = WiFi.RSSI();
    root["SigStrength"] = rssi;
  }
  root["SWVersion"] = VERSION;
  root["NetBIOSName"] = NetBIOSName;
  root["useNTP"] = int(useNTP);
  root["NTPServer"] = ntpServer;  
  root["ntpTimeZone"] = timeZone;
  root["mem"] = ESP.getFreeHeap();
  float1 = (millisRollOver * 4294967.296) + millis()/1000;
  root["uptime"] = round(float1);
  if (ntpOK && useNTP)
  {
    if (NTPch.daylightSavingTime(curTime))
      curTime -= (3600 * (timeZone+1));
    else
      curTime -= (3600 * timeZone);
    root["currenttime"] = curTime;  //seconds since 1/1/1970
  }
    
  root.printTo(response);
//  Serial.println(response);
  return response;
}

void MQTT_connect() {
  // Loop until we're reconnected sdk_common.h: No such file or directoryntp--  no, not anymore, see below
  while (!mqttClient.connected()) {
    #ifdef debugMode
      Serial.println("Attempting MQTT connection...");
      Serial.print(mqtt_server);
      Serial.println(mqtt_port);
    #endif
    // Create a random client ID
    String clientId = "LNGW";
    clientId += String(random(0xffff), HEX);
    // Attempt to connect
    mqttClient.setServer(mqtt_server, mqtt_port);
    mqttClient.setCallback(mqttCallback);
    if (mqttClient.connect(clientId.c_str(), mqtt_user, mqtt_password)) 
    {
      #ifdef debugMode
        Serial.println("connected");
      #endif
      // ... and resubscribe
        mqttClient.subscribe(lnBCTopic);
//        mqttClient.subscribe(lnEchoTopic);
    } else {
//      Serial.print("failed, rc=");
//      Serial.print(mqttClient.state());
//      Serial.println(" try again in 5 seconds");
      return; //break the loop to make sure web server can be accessed to enter a valid MQTT server address
    }
  }
#ifdef sendLogMsg
//  sendLogMessage("MQTT Connected");
#endif
#ifdef debugMode
  Serial.println("MQTT Connected!");
#endif
}

//called when mqtt message with subscribed topic is received
void mqttCallback(char* topic, byte* payload, unsigned int length) 
{
  if (String(topic) == lnBCTopic)
  {
    processLNInCommand(payload);
  }
  if (String(topic) == lnEchoTopic)
  {
  }
}

void sendLocoNetCommand(byte *OpCode)
{
  StaticJsonBuffer<800> jsonBuffer;
  String jsonOut = "";
  JsonObject& root = jsonBuffer.createObject();
  root["From"] = NetBIOSName + "-" + ESP_getChipId();
  root["Valid"] = 1;
  if (useTimeStamp)
    root["Time"] = millis();
  JsonArray& data = root.createNestedArray("Data");
  byte opCodeLen;
  switch (OpCode[0] & 0x60)
  {
    case 0x00: opCodeLen = 2; break;
    case 0x20: opCodeLen = 4; break;
    case 0x40: opCodeLen = 6; break;
    case 0x60: opCodeLen = OpCode[1] & 0x7F; break;
  }
  for (byte i=0; i < opCodeLen; i++)
  {
    data.add(OpCode[i]);
  }
  if ((OpCode[0] & 0x08) > 0)
  {
    commResponse.opCode = OpCode[0];
    commResponse.reqID = random(10000);
    commResponse.reqTime = millis();
    root["ReqID"] = commResponse.reqID;
  }
  root.printTo(mqttMsg);
  Serial.println(mqttMsg);
  if (!mqttClient.connected())
    MQTT_connect();
  if (mqttClient.connected())
  {
    if (!mqttClient.publish(lnBCTopic, mqttMsg))
    {
      Serial.println(F("lnIn Failed"));
    } else 
    {
//      Serial.println(F("lnIn OK!"));
    }
  }
}

void sendRequestSlotByNr(byte slotNr)
{
  byte OpCode[] = {0xBB, (byte)slotNr, 0x00, 0x00};
  OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
  sendLocoNetCommand(OpCode);
}

void sendWriteSlot(LocoNetSlot thisSlot)
{
//  if (validLoco)
  {
    byte OpCode[] = {0xEF, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, (throttleID[1] & 0x7F), (throttleID[0] & 0x7F), 0x00};
    OpCode[13] = OpCode[0] ^ OpCode[1];
    for (int i = 0; i < 9; i++)
    {
      OpCode[i+2] = thisSlot.slotData[i];
      OpCode[13] = OpCode[13] ^ thisSlot.slotData[i];
    }
    OpCode[13] = OpCode[13] ^ OpCode[11] ^ OpCode[12] ^ 0xFF;
    sendLocoNetCommand(OpCode);
  }
}
void sendReleaseSlot()
{
  if (validLoco)
  {
    byte OpCode[] = {0xB5, 0x00, 0x00, 0x00};
    workSlot.slotData[1] = workSlot.slotData[1] & (~0x20);
    OpCode[1] = workSlot.slotData[0];
    OpCode[2] = workSlot.slotData[1];
    OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
    validLoco = false;
    sendLocoNetCommand(OpCode);
  }
}

void sendMoveSlot(byte Source, byte Dest)
{
  byte OpCode[] = {0xBA, 0x00, 0x00, 0x00}; //no answer
  OpCode[1] = Source; //tempSlot.slotData[0];
  OpCode[2] = Dest; //tempSlot.slotData[0];
  OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
  sendLocoNetCommand(OpCode);
}

void sendRequestSlot(int locoAddr)
{
//  Serial.println("Request Slot");
  slotReqAddr = locoAddr;
  byte OpCode[] = {0xBF, 0x00, 0x00, 0x00};
  OpCode[2] = locoAddr & 0x7F;
  OpCode[1] = (locoAddr >> 7) & 0x7F;
  OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
  sendLocoNetCommand(OpCode);
}

void sendSpeedCommand()
{
  if (validLoco)
  {
    lnThrottlePingTimer = millis();
    byte OpCode[] = {0xA0, workSlot.slotData[0], workSlot.slotData[3], 0x00};
    OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
    sendLocoNetCommand(OpCode);
  }
}

void sendDIRF0to4Command()
{
  if (validLoco)
  {
    lnThrottlePingTimer = millis();
    byte OpCode[] = {0xA1, workSlot.slotData[0], workSlot.slotData[4], 0x00};
    OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
    sendLocoNetCommand(OpCode);
  }
}

void sendDIRF5to8Command()
{
  if (validLoco)
  {
    lnThrottlePingTimer = millis();
    byte OpCode[] = {0xA2, workSlot.slotData[0], workSlot.slotData[8], 0x00};
    OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
    sendLocoNetCommand(OpCode);
  }
}

void sendDIRFConsistCommand(LocoNetSlot * cnSlot)
{
  if (validLoco)
  {
    lnThrottlePingTimer = millis();
    byte OpCode[] = {0xB6, cnSlot->slotData[0], cnSlot->slotData[4], 0x00};
    OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
    sendLocoNetCommand(OpCode);
  }
}

void sendUnlinkSlotsCommand(byte releaseThis, byte releaseFrom)
{
  byte OpCode[] = {0xB8, releaseThis, releaseFrom, 0x00};
  OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
  sendLocoNetCommand(OpCode);
}

void sendLinkSlotsCommand(byte linkThis, byte linkTo)
{
  byte OpCode[] = {0xB9, linkThis, linkTo, 0x00};
  OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
  sendLocoNetCommand(OpCode);
}

void sendSwitchCommand(int swiAddr, byte swiPos, bool pwrOn)
{
  swiAddr -=1; //Swi 1 = Addr 0
  word byteIndex = swiAddr>>2;
  byte bitMask = 0x01 << (2*((byte)swiAddr & 0x03));
  if (swiPos > 0) //update internal status buffer for display on switchboard
    swPos[byteIndex] |= bitMask;
  else
    swPos[byteIndex] &= (~bitMask);
  swPos[byteIndex] |= (bitMask<<1); //set status bit
  updateSwiStatDisplay();
  byte OpCode[] = {0xB0, 0x00, 0x00, 0x00};
  OpCode[1] = swiAddr & 0x7F;
  OpCode[2] = ((swiAddr & 0x0780) >> 7);
  if (swiPos > 0)
    OpCode[2] |= 0x20;
  if (pwrOn > 0)
    OpCode[2] |= 0x10;
  OpCode[3] = (OpCode[0] ^ OpCode[1] ^ OpCode[2] ^ 0xFF);
  sendLocoNetCommand(OpCode);
}

void sendPowerCommand(bool pwrStatus)
{
  byte OpCode[] = {0x85, 0x7A}; //OPC_IDLE
  if (pwrStatus)
  {
    OpCode[0] = 0x83;
    OpCode[1] = 0x7C; //OPC_GPON
  }
  sendLocoNetCommand(OpCode);
}

bool processLNInCommand(byte* newCmd)
{
    word posIndex = 0;
    word address = 0;
    byte bitMask = 0;
    byte varVal = 0;
    long respID = 0;
    long respTime = 0;
    StaticJsonBuffer<1000> jsonBuffer;
    JsonObject& root = jsonBuffer.parseObject(newCmd);

    if (!root.containsKey("From")) 
      return false;
    if (root["From"] == NetBIOSName + "-" + ESP_getChipId()) //ignore commands sent by the throttle itself
    {
      //Serial.println("My own command");      
      return false;
    }
    if (!root.success())
    {
      #ifdef debugMode
        Serial.println("invalid payload");
      #endif
      return false;
    }
    if (root.containsKey("Valid")) //LocoNet OpCode
    {
      bool valStat = root["Valid"];
      if (valStat)
      {
        if (root.containsKey("RespID"))
        {
          respID = root["RespID"]; 
        }
        if (root.containsKey("RespTime"))
        {
          respTime = root["respTime"];
        }
        if (root.containsKey("Data")) //LocoNet OpCode
        {
          long respDelay = commResponse.respDelay();
//          Serial.print("delay: ");
//          Serial.println(respDelay);
          byte opCode = root["Data"][0];
//          Serial.println(opCode,16);
          switch (opCode)
          {
            case 0x82:;
            case 0x85: pwrStatus = false; break;
            case 0x83: pwrStatus = true; break;
            case 0xB0:;
            case 0xB1:;
            case 0xBD:
              {
                address = (((byte)root["Data"][1] & 0x7F) + (((byte)root["Data"][2] & 0x0F) << 7));
                posIndex = address >> 2;
                bitMask = 0x01 << (2*((byte)address & 0x03));
                byte statMask = bitMask << 1;
                varVal = ((byte)root["Data"][2] & 0x20) >> 5;
                if (varVal > 0)
                  swPos[posIndex] = swPos[posIndex] | bitMask;
                else
                  swPos[posIndex] = swPos[posIndex] & ~bitMask;
                swPos[posIndex] = swPos[posIndex] | statMask; //has been updated, so we know the status is correct
                updateSwiStatDisplay();
                break;
              }
            case 0xB2:
              {
                address = (((byte)root["Data"][1] & 0x7F) << 1) + (((byte)root["Data"][2] & 0x0F) << 8) + (((byte)root["Data"][2] & 0x20) >> 5);
//                Serial.print("Input Rep.: ");
//                Serial.println(address);
                posIndex = trunc(address / 8);
                bitMask = 0x01 << (address % 8);
                varVal = ((byte)root["Data"][2] & 0x10) >> 4;
                if (varVal > 0)
                  bdStatus[posIndex] = bdStatus[posIndex] | bitMask;
                else
                  bdStatus[posIndex] = bdStatus[posIndex] & ~bitMask;
                updateInpRepDisplay();
                break;
              }
            case 0xB4: //LACK
              {
                if ((respID > 0) && (millis() - commResponse.reqTime < respTimeout)) //this is atimely answer from a request
                  switch (commResponse.opCode)
                  {
                    case 0xBF: //LACK B4, 3F, 0
                    {
                      //no free slot
                      slotReqAddr = -1;
                      break;
                    }
                    case 0xBA: //LACK BA, 3A, 0 //illegal slot move
                    {
                      if (stealOK)
                      {
                        Serial.println("Steal this");
                        workSlot = tempSlot;
                        sendWriteSlot(workSlot);
                      }
                      else
                      {
                        //no free slot
                        slotReqAddr = -1;
                        workSlot.slotData[0] = 0xFF;
                        validLoco = false;
                        throttleDisplaySetup();
                      }
                    }
                    case 0xEF: //SLOT_WR
                    {
                      if (!validLoco)
                        if ((byte)root["Data"][2] == 0x7F)
                        {
                          Serial.println("Stolen Loco assigned");
                          validLoco = (workSlot.slotData[1] & 0x30) == 0x30;
                          pgThrottleSetup();
                        }
                      break;
                    }  
                  }
                  break;  
                }
              case 0xE7: //slot read, response to request slot. Store in temp slot and update Address Keypad
                {
                  if ((byte)root["Data"][1] == 0x0E) //Slot Read
                  {
                    LocoNetSlot evalSlot;
                    for (int i = 0; i < 11; i++)
                       evalSlot.slotData[i] = (byte)root["Data"][i+2];
                    address = (evalSlot.slotData[2] & 0x7F) + ((evalSlot.slotData[7] & 0x7F) << 7);
                    if ((respID > 0) && (millis() - commResponse.reqTime < respTimeout)) //this is atimely answer from a request
                    {
                      switch (commResponse.opCode)
                      {
                        case 0xBF: //LOCO_ADR
                        {
                          if (address == slotReqAddr)
                          {
                            tempSlot = evalSlot;
                            updateAddrKeypad();
                            slotReqAddr = -1;
                          }
                          break;
                        }
                        case 0xBA: //MOVE_SLOT
                        {
                          if (evalSlot.slotData[0] == workSlot.slotData[0])
                          {
                            Serial.println("Loco assigned");
                            workSlot = evalSlot;
                            validLoco = (workSlot.slotData[1] & 0x30) == 0x30;
                            pgThrottleSetup();
                            sendWriteSlot(workSlot);
                          }
                          else
                          {
                            if ((evalSlot.slotData[1] & 0x30) == 0x30) //dispatch GET
                            {
                              Serial.println("Loco dispatch GET");
                              workSlot = evalSlot;
                              validLoco = (workSlot.slotData[1] & 0x30) == 0x30;
                              pgThrottleSetup();
                              sendWriteSlot(workSlot);
                            }
                            else
                            {
                              Serial.println("Loco dispatch PUT");
                              validLoco = false; //should already be false here, just to be sure
                              if (workSlot.slotData[0] = 0xFF)
                                pgThrottleSetup();
                            }
                          }
                          break;
                        }
                        case 0xBB: //RQ_SL_DATA
                        {
                          arraySlots[evalSlot.slotData[0]] = evalSlot;
                          Serial.println("Updating Slot Info");
                          break;
                        }
                        case 0xEF: //SLOT_WR
                        {
                          break;
                        }
                      }
                    }
                    else //nothing to do with a request, just update if the same as work slot -> slot follow mode
                    {
                      arraySlots[evalSlot.slotData[0]] = evalSlot; //unrequested, from someone else. Just update internal buffer
                      Serial.println("Possible Message Timeout");
                      if (evalSlot.slotData[0] == workSlot.slotData[0])
                      {
                        Serial.println("Slot follow mode update");
                        workSlot = evalSlot;
                        validLoco = (workSlot.slotData[1] & 0x30) == 0x30;
                        throttleDisplaySetup();
                      }
                    }
                }
                break;  
              }
          }
          return true;
        }
      }
    }
    return false;
}


//========================================================= Standard Web Server Code starts here==============================================================================

void returnOK() {
//  server.send(200, "text/plain", "");
}

void returnFail(String msg) {
//  server.send(500, "text/plain", msg + "\r\n");
}

//loading of web pages available in SPIFFS. By default, this is upload.htm and delete.htm. You can add other pages as needed
bool loadFromSdCard(String path){
  String dataType = "text/plain";
  if(path.endsWith("/")) path += "index.htm";

//  Serial.print("Load from SPIFFS - Path: ");
//  Serial.println(path);

  
  if(path.endsWith(".src")) path = path.substring(0, path.lastIndexOf("."));
  else if(path.endsWith(".htm")) dataType = "text/html";
  else if(path.endsWith(".css")) dataType = "text/css";
  else if(path.endsWith(".js")) dataType = "application/javascript";
  else if(path.endsWith(".png")) dataType = "image/png";
  else if(path.endsWith(".gif")) dataType = "image/gif";
  else if(path.endsWith(".jpg")) dataType = "image/jpeg";
  else if(path.endsWith(".ico")) dataType = "image/x-icon";
  else if(path.endsWith(".xml")) dataType = "text/xml";
  else if(path.endsWith(".pdf")) dataType = "application/pdf";
  else if(path.endsWith(".zip")) dataType = "application/zip";

  File dataFile = SPIFFS.open(path.c_str(), "r");

  if (!dataFile)
  {
//    Serial.println("File not found");
    return false;
  }

//  if (server.hasArg("download")) dataType = "application/octet-stream";
//  Serial.println(dataFile.size());
  int siz = dataFile.size();

//  int i = server.streamFile(dataFile, dataType);
//  if (i != dataFile.size()) 
  {
//    Serial.println(i);
//    Serial.println("Sent less data than expected!");
  }
//    Serial.println("all sent");
  dataFile.close();
  return true;
}

void handleFileUpload()
{
//  Serial.println("Handle Upload");
//  Serial.println(server.uri());
//  if(server.uri() != "/edit") 
//    return;
//  HTTPUpload& upload = server.upload();
//  if(upload.status == UPLOAD_FILE_START)
//  {
//    if(SPIFFS.exists((char *)upload.filename.c_str())) 
//    {
//      Serial.print("Upload selected file "); Serial.println(upload.filename);
//    }
//    String hlpStr = "/" + upload.filename;
//    uploadFile = SPIFFS.open(hlpStr, "w");
//    if (!uploadFile)
//      Serial.print("Upload of file failed");
//    else
//      Serial.print("Upload: START, filename: "); Serial.println(upload.filename);
//  } 
//  else 
//    if(upload.status == UPLOAD_FILE_WRITE)
//    {
//      if (uploadFile) 
//      {
//        uploadFile.write(upload.buf, upload.currentSize);
//        Serial.print("Upload: WRITE, Bytes: "); Serial.println(upload.currentSize);
//      }
//      else
//        Serial.println("Write operation failed");
//    } 
//    else 
//      if(upload.status == UPLOAD_FILE_END)
//      {
//        if (uploadFile)
//        { 
//          uploadFile.close();
//          Serial.print("Upload: END, Size: "); Serial.println(upload.totalSize);
//        }
//        else
//          Serial.println("Closing failed");
//      }
//      else
//      {
//        Serial.print("Unknown File Status "); Serial.println(upload.status);
//      }
}

//recuersive deletion not implemented
/*

void deleteRecursive(String path){
  File file = SPIFFS.open((char *)path.c_str(), "r");
  if(!file.isDirectory()){
    file.close();
    SPIFFS.remove((char *)path.c_str());
    return;
  }

  file.rewindDirectory();
  while(true) {
    File entry = file.openNextFile();
    if (!entry) break;
    String entryPath = path + "/" +entry.name();
    if(entry.isDirectory()){
      entry.close();
      deleteRecursive(entryPath);
    } else {
      entry.close();
      SPIFFS.remove((char *)entryPath.c_str());
    }
    yield();
  }

  SD.rmdir((char *)path.c_str());
  file.close();
  
}
*/

void deleteFile(fs::FS &fs, const char * path){
//    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
//        Serial.println("- file deleted");
    } else {
//        Serial.println("- delete failed");
    }
}

void handleDelete()
{
//  Serial.println("Handle Delete");
//  Serial.println(server.uri());
//  if(server.uri() != "/delete") 
//    return;
//  String path = server.arg(0);
//  Serial.print("Trying to delete ");
//  Serial.println((char *)path.c_str());
//  if(server.args() == 0) return returnFail("BAD ARGS");
//  if(path == "/" || !SPIFFS.exists((char *)path.c_str())) {
//    returnFail("BAD PATH");
//    return;
//  }
//  deleteFile(SPIFFS, (char *)path.c_str());
//  deleteRecursive(path);
  returnOK();
}

//file creation not implemented
void handleCreate(){
/*
  if(server.args() == 0) return returnFail("BAD ARGS");
  String path = server.arg(0);
  if(path == "/" || SD.exists((char *)path.c_str())) {
    returnFail("BAD PATH");
    return;
  }

  if(path.indexOf('.') > 0){
    File file = SD.open((char *)path.c_str(), FILE_WRITE);
    if(file){
      file.write((const char *)0);
      file.close();
    }
  } else {
    SD.mkdir((char *)path.c_str());
  }
  */
  returnOK();
}

//print directory not implemented
void printDirectory() {
/*
  if(!server.hasArg("dir")) return returnFail("BAD ARGS /list");
  String path = server.arg("dir");
  if(path != "/" && !SD.exists((char *)path.c_str())) return returnFail("BAD PATH");
  File dir = SD.open((char *)path.c_str());
  path = String();
  if(!dir.isDirectory()){
    dir.close();
    return returnFail("NOT DIR");
  }
  dir.rewindDirectory();
  server.setContentLength(CONTENT_LENGTH_UNKNOWN);
  server.send(200, "text/json", "");
  WiFiClient client = server.client();

  server.sendContent("[");
  for (int cnt = 0; true; ++cnt) {
    File entry = dir.openNextFile();
    if (!entry)
    break;

    String output;
    if (cnt > 0)
      output = ',';

    output += "{\"type\":\"";
    output += (entry.isDirectory()) ? "dir" : "file";
    output += "\",\"name\":\"";
    output += entry.name();
    output += "\"";
    output += "}";
    server.sendContent(output);
    entry.close();
 }
 server.sendContent("]");
 dir.close();
 */
}

//all calls for web pages start here
void handleNotFound(){
//this is the hook to handle async requests
//  Serial.println(server.uri());
//  if ((server.uri().indexOf(ajaxCmdStr) != -1) && handleAjaxCommand(server.uri())) {return; }
//this is the default file handler
//  if(loadFromSdCard(server.uri())) {return;}
  String message = "SPIFFS Not available or File not Found\n\n";
//  message += "URI: ";
//  message += server.uri();
//  message += "\nMethod: ";
//  message += (server.method() == HTTP_GET)?"GET":"POST";
//  message += "\nArguments: ";
//  message += server.args();
  message += "\n";
//  for (uint8_t i=0; i<server.args(); i++){
//    message += " NAME:"+server.argName(i) + "\n VALUE:" + server.arg(i) + "\n";
//  }
//  server.send(404, "text/plain", message);
}


char* dbgprint ( const char* format, ... )
{
  static char sbuf[DEBUG_BUFFER_SIZE] ;                // For debug lines
  va_list varArgs ;                                    // For variable number of params

  va_start ( varArgs, format ) ;                       // Prepare parameters
  vsnprintf ( sbuf, sizeof(sbuf), format, varArgs ) ;  // Format the message
  va_end ( varArgs ) ;                                 // End of using parameters
  if ( doDebug )                                         // DEBUG on?
  {
    Serial.print ( "D: " ) ;                           // Yes, print prefix
    Serial.println ( sbuf ) ;                          // and the info
  }
  return sbuf ;                                        // Return stored string
}


