
/* This is an initial sketch to be used as a "blueprint" to create apps which can be used with IOTappstory.com infrastructure
  Your code can be filled wherever it is marked.


  Copyright (c) [2016] [Andreas Spiess]

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"), to deal
  in the Software without restriction, including without limitation the rights
  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
  copies of the Software, and to permit persons to whom the Software is
  furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
  SOFTWARE.

*/

#define APPNAME "Checkens"
#define VERSION "V1.1.3"
#define COMPDATE __DATE__ __TIME__
#define MODEBUTTON 0

#include <WiFiClient.h>
#include <ESP8266WebServer.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <PubSubClient.h>
#include <ESP8266WiFi.h>
#include <IOTAppStory.h>

#include <TimeLib.h> 

#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

#include <math.h>

#include <DHT.h>
#include <DHT_U.h>

#include <Wire.h>
#include <BH1750FVI.h> // BH1750FVI Light sensor library

#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme; // I2C

BH1750FVI LightSensor;
int lux;

#define DHTPIN            D5        // Pin which is connected to the DHT sensor.
#define DHTTYPE           DHT22     // DHT 22 (AM2302)

//#include <IOTAppStory.h>
//include <IAS_Xtra_Func.h>
//IOTAppStory IAS(SKETCH,VERSION,MODEBUTTON);

IOTAppStory IAS(APPNAME, VERSION, COMPDATE, MODEBUTTON);


// See guide for details on sensor wiring and usage:
//   https://learn.adafruit.com/dht/overview

DHT_Unified dht(DHTPIN, DHTTYPE);


//#define LEDS_INVERSE          // LEDS on = GND

const char* mqtt_server = "192.168.1.222";

const int DoorOPEN = D4;
const int DoorCLOSE = D2;
const int PIR = D1;

const int icSDA =  D6;
const int icSCL =  D7;
ESP8266WebServer server ( 80 );

// NTP Servers:

IPAddress timeServer(132, 163, 4, 102); // time-b.timefreq.bldrdoc.gov
IPAddress timeServerIP; // time.nist.gov NTP server address
const char* ntpServerName = "pool.ntp.org";



const int timeZone = 1;     // Central European Time

WiFiUDP Udp;
unsigned int localPort = 2390;  // local port to listen for UDP packets

String webPage = "";
time_t  PIRtrigger = 0;
byte OpenH = 6;
byte OpenM = 10;
byte FlagO = true;
byte CloseH = 22;
byte CloseM = 10;
byte FlagC = true;
byte PIRdelay = 3;
byte PIRentry = 2;
byte FlagDelay = false;
byte FlagReopen = false;
byte door_Timer = 40;
byte Door_Operating = 0;
byte Door_Status = 2;
byte prevStatus = false;
boolean TrigPRIV = LOW;
boolean rc = LOW;
float batt;
char buf[30];
char buf2[10];
char buf3[10];

// ================================================ PIN DEFINITIONS ======================================
#ifdef ARDUINO_ESP8266_ESP01  // Generic ESP's 
  #define MODEBUTTON 0
  #define LEDgreen 13
  //#define LEDred 12
#else
  #define MODEBUTTON D3
  #define LEDgreen D7
  //#define LEDred D6
#endif
  

WiFiClient espClient;
PubSubClient MQTTclient(espClient);



typedef struct {
  char ssid[STRUCT_CHAR_ARRAY_SIZE];
  char password[STRUCT_CHAR_ARRAY_SIZE];
  char boardName[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStory1[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStoryPHP1[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStory2[STRUCT_CHAR_ARRAY_SIZE];
  char IOTappStoryPHP2[STRUCT_CHAR_ARRAY_SIZE];
  char automaticUpdate[2];   // right after boot
  // insert NEW CONSTANTS according boardname example HERE!

    char blinkPin[STRUCT_CHAR_ARRAY_SIZE];

  char magicBytes[4];

} strConfig;

strConfig config = {
  "garage",
  "davidburke",
  "chickens",
  "iotappstory.com",
  "/ota/esp8266-v1.php",
  "iotappstory.com",
  "/ota/esp8266-v1.php",
  "0",
  "CFG"  // Magic Bytes
};
// ================================================ EXAMPLE VARS =========================================
char* lbl1 = "Light Show";                                                                                    // default value | this gets stored to eeprom and gets updated if changed from wifi config
char* lbl2 = "Living Room";
char* lbl3 = "Bath Room";
char* lbl4 = "Test Room";
char* lbl5 = "Another Room";

// ================================================ SETUP ================================================

time_t prevDisplay = 0; // when the digital clock was displayed

void ChickDoor(char* stat, char* msg) {

  char fullMSG[30];
  sprintf(fullMSG,"%s:%s", stat, msg);
  rc = MQTTclient.publish("/chicken/status", fullMSG);
   if (stat == "1") {
     Door_Status=1; 
     digitalWrite(DoorOPEN, LOW);
     digitalWrite(DoorCLOSE, HIGH);
     Door_Operating=door_Timer;  
     
   } else {
     Door_Status=0; 
     digitalWrite(DoorOPEN, HIGH);
     digitalWrite(DoorCLOSE, LOW);
     Door_Operating=door_Timer;  
   } 
  prevDisplay=0;
}

void callback(char* topic, byte* payload, unsigned int length) {
  float tempC =0;
  float relH = 0;
 byte rc=0; 
 Serial.print("Message arrived [");
 Serial.print(topic);
 Serial.print("] ");
 for (int i=0;i<length;i++) {
  char receivedChar = (char)payload[i];
  Serial.println(receivedChar);
  
  if (strcmp(topic,"/chicken/door")==0) {
    if (receivedChar == '0') {
      ChickDoor("0","MQTT");
      return;
    }
    if (receivedChar == '1') {
       ChickDoor("1","MQTT");
       return;
   }
   if (receivedChar == 'O') {
    OpenM = (payload[4] - 48) * 10 + (payload[5] - 48); 
    OpenH = (payload[1] - 48) * 10 + (payload[2] - 48); 
   ChickSTAT();
   return;
     
   }
   if (receivedChar == 'C') {
    CloseM = (payload[4] - 48) * 10 + (payload[5] - 48); 
    CloseH = (payload[1] - 48) * 10 + (payload[2] - 48); 
   ChickSTAT();
   return;  
   }
   if (receivedChar =='?') {
      ESP.reset();
      return;
   }
   if (receivedChar == 'G') {
       FlagDelay=true;
       ChickSTAT();
       return;  
   }
   if (receivedChar == 'g') {
       FlagDelay=false;
       ChickSTAT();
       return;  
   }
    if (receivedChar == 'D') {
       FlagReopen=true;
       ChickSTAT();
       return;  
   }
     if (receivedChar == 'd') {
       FlagReopen=false;
       ChickSTAT();
       return;  
   }
 if (receivedChar == 'X') {
       FlagC=true;
       ChickSTAT();
       return;  
   }
    if (receivedChar == 'x') {
       FlagC=false;
       ChickSTAT();
       return;  
   }   
if (receivedChar == 'E') {
       FlagO=true;
       ChickSTAT();
       return;  
   }
    if (receivedChar == 'e') {
       FlagO=false;
       ChickSTAT();
       return;  
   }      
  }
  
  if (strcmp(topic,"/chicken/signal")==0) {
    if (receivedChar == '1')
    {   
       int32_t rssi = WiFi.RSSI();
       batt = analogRead(A0) * 0.04;

     dtostrf(batt,2,2,buf2);      
      sprintf(buf, "%d,%d,%s,%d.%d.%d.%d", rssi, millis(),buf2, WiFi.localIP()[0], WiFi.localIP()[1], WiFi.localIP()[2], WiFi.localIP()[3]);
      rc = MQTTclient.publish("/chicken/strength", buf);  

       sensors_event_t event;  
       dht.temperature().getEvent(&event);

            
       tempC=event.temperature;
       

       // Get humidity event and print its value.
       dht.humidity().getEvent(&event);
       relH=event.relative_humidity;
       
 
       dtostrf(tempC,2,1,buf2); 
       dtostrf(relH,2,0,buf3); 

       lux = LightSensor.getAmbientLight();
      sprintf(buf, "%s,%s,%d", buf2, buf3,lux);

      sprintf(buf,"%d,%d,%d,",bme.readTemperature(),bme.readPressure() / 100.0F,bme.readAltitude(SEALEVELPRESSURE_HPA),bme.readHumidity());
      
      Serial.println(buf);
      rc = MQTTclient.publish("/chicken/env", buf);   
   }
  }
  
  Serial.println();
 }
} 
void ChickSTAT() {
   sprintf(buf, "%d:%d,%d:%d,%d,%d,%d,%d", OpenH,OpenM,CloseH,CloseM,FlagO,FlagC,FlagDelay,FlagReopen);
   Serial.println(buf);
   rc = MQTTclient.publish("/chicken/settings", buf);  

  
}
void reconnect() {
 // Loop until we're reconnected
 unsigned int retry = 5;
 while (!MQTTclient.connected()) {
 Serial.print("Attempting MQTT connection...");
 // Attempt to connect
 if (MQTTclient.connect("ESP8266 MQTTClient")) {
  Serial.println("connected");
  rc = MQTTclient.subscribe("/chicken/door");
  rc = MQTTclient.subscribe("/chicken/signal");
   
  
  Serial.print("Subscribed To= ");
  Serial.println(rc);
  rc = MQTTclient.publish("/chicken/connected", VERSION); 
  
  
  // ... and subscribe to topic
  MQTTclient.subscribe("ledStatus");
 } else {
  retry--;
  Serial.print("failed, rc=");
  Serial.print(MQTTclient.state());
  if (retry<1) return;
  Serial.println(" try again in 5 seconds");
  // Wait 5 seconds before retrying
  delay(5000);
  }
 }
}
void DoorSET() {
  unsigned int OpenMIN;
  unsigned int CloseMIN;
  unsigned int NowMIN;
  
  OpenMIN = (OpenH * 60) + OpenM;
  CloseMIN = (CloseH * 60) + CloseM;
  NowMIN   = (hour() * 60) + minute();
 if (timeStatus() == timeNotSet) {
   prevStatus = false;
   ChickDoor("1","Invalid Time");
   return;
 }
 prevStatus = true;
 if (NowMIN >= OpenMIN && NowMIN < CloseMIN) {
   ChickDoor("1","STARTUP");
   return;

 }
 ChickDoor("0","STARTUP");
 
 return;

 
}

 char* OPEN_TIME = "06:00";
 char* CLOSE_TIME = "20:33"; 
 
void setup() {
  bool status;
  IAS.serialdebug(true);                                                                                      // 1st parameter: true or false for serial debugging. Default: false
  //IAS.serialdebug(true,115200);                                                                             // 1st parameter: true or false for serial debugging. Default: false | 2nd parameter: serial speed. Default: 115200


  
  // IAS.preSetConfig("garage","davidburke","chickens","iotappstory.com","/ota/esp8266-v1.php",true);                                                     // preset Wifi, boardName & automaticUpdate
 
 // IAS.addField(lbl1, "OnTime", "06:20", 6);                                                                // reference to org variable | field name | field label value | max char return
 // IAS.addField(lbl2, "OffTime", "21:44", 6);
 // IAS.addField(lbl3, "DoorACT", "30", 5);
  /* TIP! delete the above lines when not used */
// IAS.writeConfig();  // configuration in EEPROM
// IAS.writeRTCmem();
  IAS.begin(true);                                                                                  // 1st parameter: true or false to view BOOT STATISTICS | 2nd parameter: green feedback led integer | 3rd argument attach interrupt for the mode selection button
IAS.callHome(true);

  //-------- Your Setup starts from here ---------------
  Serial.println(WiFi.SSID());
  Wire.begin(icSDA,icSCL);  
  pinMode ( DoorOPEN, OUTPUT );
  pinMode ( DoorCLOSE, OUTPUT );
  pinMode ( PIR, INPUT_PULLUP );
  
  digitalWrite ( DoorOPEN, 1 );
  digitalWrite ( DoorCLOSE, 1 );


  WiFi.hostname("chickens");

server.on ( "/", handleRoot );
 /* server.on ( "/test.svg", drawGraph ); */
  server.on ( "/inline", []() {
    server.send ( 200, "text/plain", "this works as well" );
  } );
  server.onNotFound ( handleNotFound );
  server.begin();
  Serial.println ( "HTTP server started" );

  
  Serial.print("IP number assigned by DHCP is ");
  Serial.println(WiFi.localIP());
  Serial.println("Starting UDP");
  Udp.begin(localPort);
  Serial.print("Local port: ");
  Serial.println(Udp.localPort());
  Serial.println("waiting for sync");

  status = bme.begin();
    if (!status) {
        Serial.println("Could not find a valid BME280 sensor, check wiring!");
     //   while (1);
    } else {
 Serial.print("Temperature = ");
    Serial.print(bme.readTemperature());
    Serial.println(" *C");

    Serial.print("Pressure = ");

    Serial.print(bme.readPressure() / 100.0F);
    Serial.println(" hPa");

    Serial.print("Approx. Altitude = ");
    Serial.print(bme.readAltitude(SEALEVELPRESSURE_HPA));
    Serial.println(" m");

    Serial.print("Humidity = ");
    Serial.print(bme.readHumidity());
    Serial.println(" %");      
    }
   dht.begin();

   
  setSyncProvider(getNtpTime);  

  MQTTclient.setServer(mqtt_server, 1883);
  MQTTclient.setCallback(callback);
  
  DoorSET(); // Initial door position

  sensor_t sensor;
  dht.temperature().getSensor(&sensor);
  
  // Print humidity sensor details.
  dht.humidity().getSensor(&sensor);
  
  // Set delay between sensor readings based on sensor details.


  LightSensor.begin();
  LightSensor.setMode(Continuously_High_Resolution_Mode); // see datasheet page 5 for modes

  Serial.println("Light sensor BH1750FVI found and running");

  lux = LightSensor.getAmbientLight();
  Serial.print("Ambient light intensity: ");
  Serial.print(lux);
  Serial.println(" lux");
/*
if (!MQTTclient.connected()) {
  reconnect();
 }
  */
  ChickSTAT();
  
//IAS.initWiFiManager();
//IAS.configESP();
Serial.println("COMPLETE");
}




// ================================================ LOOP =================================================
void loop() {
  yield();
  IAS.buttonLoop();                        // this routine handles the reaction of the MODEBUTTON pin. If short press (<4 sec): update of sketch, long press (>7 sec): Configuration

 
  //-------- Your Sketch starts from here ---------------

    if ((Door_Operating > 0) && ((now()-prevDisplay)> 0)) 
    {
      prevDisplay = now();
        Door_Operating--;
        if (Door_Operating==0) {
          digitalWrite(DoorOPEN, HIGH); 
          digitalWrite(DoorCLOSE, HIGH); 
        }

    }
 if ((now()-PIRtrigger) > 4) {
 
   if (digitalRead(PIR)== HIGH){
      PIRtrigger =now();
      if (TrigPRIV == LOW) {
        rc = MQTTclient.publish("/chicken/pir", "A");
        Serial.print("Movement ");
        Serial.println(rc);
        TrigPRIV = HIGH;
      }
   } else {
      if (TrigPRIV == HIGH ) {
        Serial.print("PIR timmer");
        Serial.println(now()-PIRtrigger);
        rc = MQTTclient.publish("/chicken/pir", "P");
        Serial.print("Clear ");
        Serial.println(rc);
        TrigPRIV = LOW;
      }
      
   }
 }
if (FlagO==true && (((hour() * 60 + minute())  >= (OpenH*60 + OpenM)) || (hour() * 60 + minute() > PI  )))
{ 
 if (Door_Status==0 && FlagO==true && hour() == OpenH && minute() == OpenM) 
  {
   ChickDoor("1","AUTO");
  }
}
  if (Door_Status== 1 && FlagC==true && hour() == CloseH && minute() == CloseM) 
  {
   ChickDoor("0","AUTO");
  }

  server.handleClient();
  


 if (!MQTTclient.connected()) {
  reconnect();
 }
 MQTTclient.loop();
}



void handleRoot() {

 ProcessURL();
 webPage= "<!DOCTYPE html><html lang='en'><head><meta charset='utf-8'><div style=' text-align: center; text-indent: 0px; padding: 0px 0px 0px 0px; margin: 0px 0px 0px 0px;'>";
 webPage+="<table width='50%' border='1' cellpadding='2' cellspacing='2'";
 if (timeStatus() == timeNotSet) {
   webPage+="style='background-color: #A53030;'>";
 } else {
   webPage+="style='background-color: #000000;'>";
 } 
 webPage+="<tr valign='top'><td style='border-width : 0px;'><p style=' text-align: left; text-indent: 0px; padding: 0px 0px 0px 0px; margin: 0px 0px 0px 0px;'><span style=' font-size: 10pt; font-family: Arial,Helvetica, sans-serif; font-style: normal; font-weight: normal; color: #00ff00; background-color: transparent; text-decoration: none;'>Chicken Door"; 
 webPage+= VERSION;
 webPage+= "</span></p></td>";
 webPage+="<td style='border-width : 0px;'><p style=' text-align: left; text-indent: 0px; padding: 0px 0px 0px 0px; margin: 0px 0px 0px 0px;'><span style=' font-size: 10pt; font-family: Arial,Helvetica, sans-serif; font-style: normal; font-weight: normal; color: #00ff00; background-color: transparent; text-decoration: none;'>Sensor</span></p></td>";
 webPage+="</tr> <tr valign='top'><td height='108' style='border-width : 0px;'><p style=' text-align: left; text-indent: 0px; padding: 0px 0px 0px 0px; margin: 0px 0px 0px 0px;'><span style=' font-size: 36pt; font-family: Arial,Helvetica, sans-serif; font-style: normal; font-weight: bold; color: #ffffff; background-color: transparent; text-decoration: none;'>";

 if ((digitalRead(DoorOPEN) == HIGH) && (digitalRead(DoorCLOSE) == HIGH)) 
 {
  if (Door_Status==1) webPage+="OPEN";
  if (Door_Status==0) webPage+="CLOSED";
  if (Door_Status==99) webPage+="CLOSED++";
  if (Door_Status==98) webPage+="OPEN++";
  
  if ((Door_Status>1) && (Door_Status <90)) webPage+="UNKNOWN." + String(Door_Status); 
 }
 if ((digitalRead(DoorOPEN) == LOW) && (digitalRead(DoorCLOSE) == LOW))   webPage+="INVALID";
 if ((digitalRead(DoorOPEN) == HIGH) && (digitalRead(DoorCLOSE) == LOW))  webPage+="CLOSING";
 if ((digitalRead(DoorOPEN) == LOW) && (digitalRead(DoorCLOSE) == HIGH))  webPage+="OPENING";


 webPage+="</span></p></td><td height='108' style='border-width : 0px;'><p style=' text-align: left; text-indent: 0px; padding: 0px 0px 0px 0px; margin: 0px 0px 0px 0px;'><span style=' font-size: 48pt; font-family: Arial,Helvetica, sans-serif; font-style: normal; font-weight: normal; color: #ff00ff; background-color: transparent; text-decoration: none;'>";

 if (digitalRead(PIR) == HIGH) {
  webPage+="Movment"; 
  PIRtrigger = now();
 } else webPage+="CLEAR"; 
 

 if (PIRtrigger> 0) {
  webPage+= String(now() - PIRtrigger);
 }
 webPage+="</span></p></td></tr><tr valign='top'><td colspan=2 height='29' style='border-width : 0px;'><p style=' text-align: left; text-indent: 0px; padding: 0px 0px 0px 0px; margin: 0px 0px 0px 0px;'><span style=' font-size: 22pt; font-family: Arial, Helvetica, sans-serif; font-style: normal; font-weight: bold; color: #ffffff; background-color: transparent; text-decoration: none;'>";
 webPage+="</span></p></td></tr></table></div><hr color='#000080' />\
    <title>Chickens</title><style type='text/css'>\
    <!--\
    body {\
      color:#000000;\
      background-color:#FFFFFF;\
      background-image:url('Background Image');\
      background-repeat:no-repeat;\
    }\
    a  { color:#0000FF; }\
    a:visited { color:#800080; }\
    a:hover { color:#008000; }\
    a:active { color:#FF0000; }\
    -->\
    </style>\
  <form>\
  <input type='submit' name='ACTION' value='>'>\
  <input type='submit' name='ACTION' value='<'>\
  <input type='submit' name='ACTION' value='OPEN'><input type='submit' name='ACTION' value='CLOSE'>\
  <br> <hr color='#0000E0' />  <br><table border='0'  width='30%'>";
   webPage+=hour();
   webPage+=":";
   webPage+=minute();
   webPage+=":";
   webPage+=second();
   webPage+="<tr><td>OPEN TIME</td><td>";
   BLD_Select("Ho",3,9,1,OpenH); 
  webPage+="</td><td>";
  BLD_Select("Mo",0,59,1,OpenM);
  
  webPage+="</td><td><input type='checkbox' name='OP' value='auto'";
  if (FlagO == true) webPage+=" checked";
  webPage+="></td></tr><td>CLOSE TIME</td><td>";
    
  BLD_Select("Hc",12,23,1,CloseH);
  webPage+="</td><td>";
  BLD_Select("Mc",0,59,1,CloseM);
  webPage+="</td><td><input type='checkbox' name='CL' value='auto'";
  if (FlagC == true) webPage+=" checked";
  webPage+="></td></tr>";
    
  webPage+="<tr><td>Deleay via PIR</td><td>";
  BLD_Select("Delay",0,20,1,PIRdelay);
  webPage+="</td><td><input type='checkbox' name='PIRc' value='auto'";
  if (FlagDelay == true) webPage+=" checked";
  webPage+="></td></tr>";

  webPage+="<tr><td>Re-Entry via PIR</td><td>";
  BLD_Select("reEnter",0,20,1,PIRentry);
  webPage+="</td><td><input type='checkbox' name='PIRo' value='auto'";
  if (FlagReopen == true) webPage+=" checked";
  webPage+="></td></tr>";

  webPage+="<tr><td>OPERATION MAX</td><td>";
  BLD_Select("opMAX",40,60,1,door_Timer);
  webPage+="</td><td>";
  
  batt= analogRead(A0) * 0.04;
  webPage+="<tr><td>Battry " + String(batt,1) + "v</td></tr>";
  webPage+="</table><input type='submit' name='ACTION' value='Update'></form></head><body></body></html>";  
  

       server.send ( 200, "text/html", webPage );

}

void ProcessURL() {

  if (server.args()==-1) return;

 
  for ( uint8_t i = 0; i < server.args(); i++ ) {
    if (server.argName ( i )=="ACTION" && server.arg ( i ) =="Update") {
      FlagReopen = false;
      FlagDelay = false;
      FlagC = false;
      FlagO = false;   
    }
  }
  for ( uint8_t i = 0; i < server.args(); i++ ) {   
    if (server.argName ( i )=="Ho") OpenH = server.arg ( i ).toInt();
    if (server.argName ( i )=="Mo") OpenM = server.arg ( i ).toInt();
    if (server.argName ( i )=="Hc") CloseH = server.arg ( i ).toInt();
    if (server.argName ( i )=="Mc") CloseM = server.arg ( i ).toInt();

    if (server.argName ( i )=="Delay") PIRdelay = server.arg ( i ).toInt();
    if (server.argName ( i )=="reEnter") PIRentry = server.arg ( i ).toInt();

    if (server.argName ( i )=="PIRo") FlagReopen = true;
    if (server.argName ( i )=="OP") FlagO = true;
    if (server.argName ( i )=="CL") FlagC = true;
    if (server.argName ( i )=="PIRo") FlagDelay = true;

    if (server.argName ( i )=="opMAX") door_Timer = server.arg ( i ).toInt();

    

    if (server.argName ( i )=="ACTION") 
    {
      if (server.arg ( i ) == "OPEN")   ChickDoor("1","WEB");
      if (server.arg ( i ) == "CLOSE")  ChickDoor("0","WEB");

      if (server.arg ( i ) == "<") { Door_Status=99; digitalWrite(DoorOPEN, HIGH); digitalWrite(DoorCLOSE, LOW); delay(1000) ; digitalWrite(DoorCLOSE, HIGH);}    
      if (server.arg ( i ) == ">") { Door_Status=98; digitalWrite(DoorOPEN, LOW); digitalWrite(DoorCLOSE, HIGH); delay(1000) ; digitalWrite(DoorOPEN,  HIGH);}    
      
    }
 
      
    
  }
 ChickSTAT();
}
void handleNotFound() {
 
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server.uri();
  message += "\nMethod: ";
  message += ( server.method() == HTTP_GET ) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server.args();
  message += "\n";

  for ( uint8_t i = 0; i < server.args(); i++ ) {
    message += " " + server.argName ( i ) + ": " + server.arg ( i ) + "\n";
  }

  server.send ( 404, "text/plain", message );
  
}


/*-------- NTP code ----------*/

const int NTP_PACKET_SIZE = 48; // NTP time is in the first 48 bytes of message
byte packetBuffer[NTP_PACKET_SIZE]; //buffer to hold incoming & outgoing packets

time_t getNtpTime()
{
  while (Udp.parsePacket() > 0) ; // discard any previously received packets
  Serial.print("Transmit NTP Request (");
  Serial.print(ntpServerName);
  Serial.print(") IP ");
  WiFi.hostByName(ntpServerName, timeServerIP);
  Serial.println(timeServerIP.toString());
  
  
  sendNTPpacket(timeServerIP);
  //sendNTPpacket(timeServer);
  uint32_t beginWait = millis();
  while (millis() - beginWait < 1500) {
    int size = Udp.parsePacket();
    if (size >= NTP_PACKET_SIZE) {
      Serial.println("Receive NTP Response");
      Udp.read(packetBuffer, NTP_PACKET_SIZE);  // read packet into the buffer
      unsigned long secsSince1900;
      // convert four bytes starting at location 40 to a long integer
      secsSince1900 =  (unsigned long)packetBuffer[40] << 24;
      secsSince1900 |= (unsigned long)packetBuffer[41] << 16;
      secsSince1900 |= (unsigned long)packetBuffer[42] << 8;
      secsSince1900 |= (unsigned long)packetBuffer[43];
      return secsSince1900 - 2208988800UL + timeZone * SECS_PER_HOUR;
    }
  }
  Serial.println("No NTP Response :-(");
  return 0; // return 0 if unable to get the time
}

// send an NTP request to the time server at the given address
void sendNTPpacket(IPAddress &address)
{
  // set all bytes in the buffer to 0
  memset(packetBuffer, 0, NTP_PACKET_SIZE);
  // Initialize values needed to form NTP request
  // (see URL above for details on the packets)
  packetBuffer[0] = 0b11100011;   // LI, Version, Mode
  packetBuffer[1] = 0;     // Stratum, or type of clock
  packetBuffer[2] = 6;     // Polling Interval
  packetBuffer[3] = 0xEC;  // Peer Clock Precision
  // 8 bytes of zero for Root Delay & Root Dispersion
  packetBuffer[12]  = 49;
  packetBuffer[13]  = 0x4E;
  packetBuffer[14]  = 49;
  packetBuffer[15]  = 52;
  // all NTP fields have been given values, now
  // you can send a packet requesting a timestamp:                 
  Udp.beginPacket(address, 123); //NTP requests are to port 123
  Udp.write(packetBuffer, NTP_PACKET_SIZE);
  Udp.endPacket();
}

void BLD_Select(String iName, int iStart, int iEnd, int iStep, int iSELECT)
{
   String vSelect="";
   iEnd++;
 
   webPage+= "<select name='" + iName + "'>";
   for (int j=iStart; j<iEnd; j=j+iStep){
    vSelect="";
    if (j== iSELECT) vSelect=" selected";
    webPage+= "<option" + vSelect + " value'" + String(j) + "'>" + String(j) + "</option>";
   }
   webPage+= "</select>";
  
}



