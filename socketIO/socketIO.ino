/*
 * WebSocketClientSocketIOack.ino
 *
 *  Created on: 20.07.2019
 *
 */
#define HAS_SSL 0

#include <Arduino.h>

#include <WiFi.h>
//#include <WiFiMulti.h>
//#include <WiFiClientSecure.h>
#include <WiFiClient.h>
#include "SerialUI.h"
#include "Settings.h"
#include "SocketIOmanager.h"
#include "wifi_custom.h"
#include <ArduinoJson.h>


#include <SocketIOclient.h>
//#include <ArduinoJson.h>
//WiFiMulti WiFiMulti;
SocketIOclient socketIO;


#include <Wire.h>
#include "MAX30105.h" //MAX30102 and MAX30105 share the same code
#include "heartRate.h"

MAX30105 particleSensor;
const byte RATE_SIZE = 4; //Increase this for more averaging. 4 is good.
byte rates[RATE_SIZE]; //Array of heart rates
byte rateSpot = 0;
long lastBeat = 0; //Time at which the last beat occurred

float beatsPerMinute;
int beatAvg;



#define N_of_sample 5
#define NUM_SENSOR_CH 4
// LED
#define PIN_LED0 16
#define PIN_LED1 17

unsigned long sensor_sampling_interval[NUM_SENSOR_CH]={40,40,0,0}; // ms
unsigned long sensor_sampling_BufSize[NUM_SENSOR_CH]={10,10,1,1}; // ms
bool flag_packet_ready = false;
bool flag_param_changed = false;
unsigned long StartingTime=0;
uint64_t current_time;
String txPacket;
String txPacket_param;
String rxPacket;

uint32_t ppg[2][20];
float heartRate[10];
float temperatureC[10];
unsigned long timeStamp=0;





void socketIOEvent(socketIOmessageType_t type, uint8_t * payload, size_t length) {
    switch(type) {
        case sIOtype_DISCONNECT:
            Serial.printf("[IOc] Disconnected!\n");
            break;
        case sIOtype_CONNECT:
            Serial.printf("[IOc] Connected to url: %s\n", payload);

            // join default namespace (no auto join in Socket.IO V3)
            socketIO.send(sIOtype_CONNECT, "/");
            break;
        case sIOtype_EVENT:
        {
            char * sptr = NULL;
            int id = strtol((char *)payload, &sptr, 10);
            Serial.printf("[IOc] get event: %s id: %d\n", payload, id);
            if(id) {
                payload = (uint8_t *)sptr;
            }
            DynamicJsonDocument doc(1024);
            DeserializationError error = deserializeJson(doc, payload, length);
            if(error) {
                Serial.print(F("deserializeJson() failed: "));
                Serial.println(error.c_str());
                return;
            }

            String eventName = doc[0];
            Serial.printf("[IOc] event name: %s\n", eventName.c_str());

            String str_buf;
            //parsing by socket tag
            if(eventName=="DevReset"){
              DynamicJsonDocument doc(1024);
              JsonArray array = doc.to<JsonArray>();
              array.add("DevResp");  // add evnet name
              // add payload (parameters) for the event
              JsonObject param1 = array.createNestedObject();
              param1["result"] = "Restarting the device..";
              serializeJson(doc, str_buf);
              socketIO.sendEVENT(str_buf);
              Serial.print("[Socket] transmit:");
              Serial.println(str_buf);
              ESP.restart();              
            }else if(eventName=="Greetings"){
              str_buf = GetJsonString_ClientType();
              socketIO.sendEVENT(str_buf);
              Serial.print("[Socket] transmit:");
              Serial.println(str_buf);

            }else if(eventName=="DevInfo"){
              str_buf = GetJsonString_DevInfo();
              socketIO.sendEVENT(str_buf);
              Serial.print("[Socket] transmit:");
              Serial.println(str_buf);

            }else if(eventName=="DevParam"){
              str_buf = GetJsonString_DevParams();
              socketIO.sendEVENT(str_buf);
              Serial.print("[Socket] transmit:");
              Serial.println(str_buf);
              
            }else if(eventName==""){
              
            }else if(eventName==""){
              
            }else if(eventName==""){
              
            }


            // Message Includes a ID for a ACK (callback)
            if(id) {
                // creat JSON message for Socket.IO (ack)
                DynamicJsonDocument docOut(1024);
                JsonArray array = docOut.to<JsonArray>();

                // add payload (parameters) for the ack (callback function)
                JsonObject param1 = array.createNestedObject();
                param1["now"] = millis();

                // JSON to String (serializion)
                String output;
                output += id;
                serializeJson(docOut, output);

                // Send event
                socketIO.send(sIOtype_ACK, output);
            }
        }
            break;
        case sIOtype_ACK:
            Serial.printf("[IOc] get ack: %u\n", length);
            break;
        case sIOtype_ERROR:
            Serial.printf("[IOc] get error: %u\n", length);
            break;
        case sIOtype_BINARY_EVENT:
            Serial.printf("[IOc] get binary: %u\n", length);
            break;
        case sIOtype_BINARY_ACK:
            Serial.printf("[IOc] get binary ack: %u\n", length);
            break;
        default:
            Serial.printf("[IOc] greet!!!: %s id: %d\n", payload, 123);
            break;
    }
}




void setup() {
  //Serial.begin(921600);
  Serial.begin(115200);

  //Serial.setDebugOutput(true);
//    Serial.setDebugOutput(true);

  Serial.println();
  Serial.println("Max30102/Max30105 Example");
  Serial.println("Welcome. Input 'help' to get information");
  delay(100);

  // Initialize sensor
  if (particleSensor.begin() == false){
    Serial.println("MAX30105 was not found. Please check wiring/power. ");
    while (1);
  }
  particleSensor.setup(); //Configure sensor. Use 6.4mA for LED drive

  // Initialize EEPROM
  init_eeprom();
  char buf_eeprom[32];


  INIT_FLAG.get(0,buf_eeprom);
  String init_flag=String(buf_eeprom);
  Serial.println("--flag--");
  Serial.println(init_flag);
  Serial.println("--------");

  if(init_flag!="EEPROM_INITIATED"){
  // Write: Variables ---> EEPROM stores
    DEV_NAME.writeString(0, Dev_name);
    DEV_NAME.commit();
    SERVER_IP.writeString(0, Server_ip);
    SERVER_IP.commit();
    SERVER_PORT.put(0, Server_port);
    SERVER_PORT.commit();
    AP_NAME.writeString(0, AP_id);
    AP_NAME.commit();
    AP_PASSWORD.writeString(0, AP_pw);
    AP_PASSWORD.commit();

    INIT_FLAG.writeString(0, "EEPROM_INITIATED");
    INIT_FLAG.commit();
    Serial.print("device name: ");   Serial.println(Dev_name);
    Serial.print("target server ip: ");   Serial.println(Server_ip);
    Serial.print("target server port: ");   Serial.println(Server_port);
    Serial.print("AP name: ");   Serial.println(AP_id);
    Serial.print("AP password: ");   Serial.println(AP_pw);
    Serial.println("EEPROM now initiated.");
    Serial.println("");
  }else{
    Serial.println("EEPROM already initiated!");
    Serial.println("");
  }
  // Clear variables
//  buf_eeprom[0]='\0';

  Serial.println("");
  DEV_NAME.get(0, buf_eeprom);
  Dev_name=String(buf_eeprom);
  SERVER_IP.get(0, buf_eeprom);
  Server_ip=String(buf_eeprom);
  SERVER_PORT.get(0, Server_port);
//  Server_port=String(buf_eeprom);
  AP_NAME.get(0,buf_eeprom);
  AP_id=String(buf_eeprom);
  AP_PASSWORD.get(0,buf_eeprom);
  AP_pw=String(buf_eeprom);

  setDevInfo();
  setDevParams();
  setSensorParams();

  print_settings();

  delay(1000);
  Serial.println("connecting to AP");
  bool isAPconnected=false;
  for(int i=0;i<3;i++){
    isAPconnected=ConnectToRouter(AP_id.c_str(), AP_pw.c_str());
    if(isAPconnected){
      break;
    }
  }
  if(isAPconnected){
    Serial.println("AP connected");
  }else{
    Serial.println("AP not connected. Proceed anyway..");
  }

  bool isSSL=true;
  // server address, port and URL
  if(isSSL){// (SSL) https:// or wss://
    socketIO.beginSSL(Server_ip, Server_port, "/socket.io/?EIO=3"); //confirmed
  }else{ //http:// or ws://
//  socketIO.begin(Server_ip, 3000, "/socket.io/?EIO=4");
    socketIO.begin(Server_ip, 3000, "/socket.io/?EIO=3"); // maybe 3..?  
  }
  // event handler
  socketIO.onEvent(socketIOEvent);


  // Double-core processing
  // Now set up two tasks to run independently.
  xTaskCreatePinnedToCore(
    TaskSensor
    ,  "TaskSensor"   // A name just for humans
    ,  4096  // This stack size can be checked & adjusted by reading the Stack HighwaterMaskMonitor5
    ,  NULL
    ,  2  // Priority, with 3 (configMAX_PRIORITIES - 1) being the highest, and 0 being the lowest.
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

  xTaskCreatePinnedToCore(
    TaskSocketIO
    ,  "TaskSocketIO"
    ,  16384  // Stack size
    ,  NULL
    ,  1  // Priority
    ,  NULL 
    ,  ARDUINO_RUNNING_CORE);

};



unsigned long messageTimestamp = 0;
String readString;

void loop() {
//do nothing here.
}


void TaskSocketIO(void *pvParameters){  // This is a task.
  (void) pvParameters;
  //wait for device warming up

  vTaskDelay(1000);  // ms
  // bool b_serverTimeReceived=true;
  // while(false){
  //   Serial.println("thread2:Requesting Server time");
  //   Serial.println("(not supported for now. We don't get server time.)");
  //   digitalWrite(PIN_LED1, HIGH);   // turn the LED on (HIGH is the voltage level)
  //   digitalWrite(PIN_LED1, LOW);   // turn the LED on (HIGH is the voltage level)
  //   vTaskDelay(1000);  // ms
  // }

  enroll_serviceProfile();

  while(true){
    vTaskDelay(10);  // ms
    while(Serial.available()) {
      vTaskDelay(1);  //delay to allow buffer to fill 
      if (Serial.available() >0) {
        char c = Serial.read();  //gets one byte from serial buffer
        readString += c; //makes the string readString
      }
    }
    if (readString.length() >0) {
      Serial.println(readString); //see what was received
      parse_packet(readString);
      readString="";
    }    
    socketIO.loop();

    uint64_t now = millis();

    if(flag_param_changed){
      // Serial.println("param change transmitted");
      socketIO.sendEVENT(txPacket_param);
      flag_param_changed=false;
    }

    if(flag_packet_ready){
      reportParamChange();
      flag_param_changed=true;
      if(true){
        enroll_serviceProfile();
      }

      DynamicJsonDocument doc(1024);
      JsonArray array = doc.to<JsonArray>();
      array.add("msg-v0");
      JsonObject root = array.createNestedObject();
      root["_H"]="DUP";

      // flag_param_changed=true;

      JsonObject sensor_ch[4];
      sensor_ch[0] = root.createNestedObject("ch0"); //ppg-red
      sensor_ch[1] = root.createNestedObject("ch1"); //ppg-ir
      sensor_ch[2] = root.createNestedObject("ch2"); //temperature
      sensor_ch[3] = root.createNestedObject("ch3"); //heart rate

      sensor_ch[0]["t0"]=timeStamp;
      sensor_ch[1]["t0"]=timeStamp;
      sensor_ch[2]["t0"]=timeStamp;
      sensor_ch[3]["t0"]=timeStamp;

      JsonArray sensor_data[2];
      sensor_data[0] = sensor_ch[0].createNestedArray("data");
      sensor_data[1] = sensor_ch[1].createNestedArray("data");
      sensor_data[2] = sensor_ch[2].createNestedArray("data");
      sensor_data[3] = sensor_ch[3].createNestedArray("data");

      int i;
      for(i=0;i<2;i++){
        for(int j=0;j<sensor_sampling_BufSize[i];j++){
          sensor_data[i].add(ppg[i][j]);
        }
      }

      i=2;
      for(int j=0;j<sensor_sampling_BufSize[i];j++){
        sensor_data[i].add(temperatureC[j]);
      }
      i=3;
      for(int j=0;j<sensor_sampling_BufSize[i];j++){
        sensor_data[i].add(heartRate[j]);
      }


      txPacket="";
      serializeJson(doc, txPacket);
      Serial.print("DATA:");
      Serial.println(txPacket);

      // Serial.println("socket tx triggered");
      digitalWrite(PIN_LED1, HIGH);   // turn the LED on (HIGH is the voltage level)
      socketIO.sendEVENT(txPacket);
      digitalWrite(PIN_LED1, LOW);   // turn the LED on (HIGH is the voltage level)
      flag_packet_ready=false;

      // // creat JSON message for Socket.IO (event)
      // DynamicJsonDocument doc(1024);
      // JsonArray array = doc.to<JsonArray>();
      // // Add evnet name. Hint: socket.on('event_name', ....  --> array.add("event_name");
      // array.add("msg-v0");//header:message-version-0 (test header)
      // JsonObject param1 = array.createNestedObject();
      // param1["now"] = (uint32_t) now;
      // // JSON to String (serializion)
      // String output;
      // serializeJson(doc, output);
      // Serial.print("transmitting:");
      // Serial.println(output);
      // // Send event
      // socketIO.sendEVENT(output);
    }
  }
}

void TaskSensor(void *pvParameters) { // This is a task.
  (void) pvParameters;

  Wire.begin();
  particleSensor.enableDIETEMPRDY(); //Enable the temp ready interrupt. This is required.

  digitalWrite(PIN_LED0, HIGH);   // turn the LED on (HIGH is the voltage level)
  digitalWrite(PIN_LED1, HIGH);   // turn the LED on (HIGH is the voltage level)

  vTaskDelay(500);  // ms

  main_task();
  // current_time = ws_serverTime_ms +millis();
  // str_timestamp = timeClient.getFormattedDate((unsigned long)(current_time/1000))+"."+String((unsigned int)(current_time%1000));
}


void main_task(void){

  static unsigned long SensorTimeLog0;
  uint32_t iter[NUM_SENSOR_CH]={0,0};
  uint8_t sensorChannel=0;
  SensorTimeLog0 = millis();
  // t0, t1, dt is adopted to survey the quality of sensor sampling rate
  unsigned long t0,t1,dt;

  while(true){
    digitalWrite(PIN_LED0, LOW);   // turn the LED on (HIGH is the voltage level)
    vTaskDelay(2);  // ms
    digitalWrite(PIN_LED0, HIGH);   // turn the LED on (HIGH is the voltage level)
    sensorChannel=0;
    if(millis()-SensorTimeLog0>(sensor_sampling_interval[sensorChannel]*iter[sensorChannel])){ // unit: ms
      ppg[0][iter[0]%sensor_sampling_BufSize[0]]=particleSensor.getRed();
      ppg[1][iter[1]%sensor_sampling_BufSize[1]]=particleSensor.getIR();
      timeStamp=millis();
      long irValue = ppg[1][iter[1]%sensor_sampling_BufSize[1]];
      if (checkForBeat(irValue) == true){
        //We sensed a beat!
        long delta = millis() - lastBeat;
        lastBeat = millis();
        beatsPerMinute = 60 / (delta / 1000.0);
      }
      temperatureC[0] = particleSensor.readTemperature();
      heartRate[0]=beatsPerMinute;
      // Serial.print(", Temp=");
      // Serial.print(temperatureC[0]);
      // Serial.print(", BPM=");
      // Serial.println(heartRate[0]);

      // dt= t0-t1;
      // t1= t0;
      // t0= millis();
      // Serial.print("  dT==");
      // Serial.print(dt);

      iter[0]++;
      iter[1]++;
      if(iter[0]%sensor_sampling_BufSize[0]==0){
        Serial.println("");
        flag_packet_ready = true;
      }
    }
  }
  return; 
}

void reportParamChange(void){
    DynamicJsonDocument doc(1024);
    JsonArray array = doc.to<JsonArray>();
    array.add("msg-v0");
    JsonObject root = array.createNestedObject();
    root["_H"]="PARAM";

    JsonObject sensor_info[NUM_SENSOR_CH];
    sensor_info[0]= root.createNestedObject("ch0");
    sensor_info[1]= root.createNestedObject("ch1");
    sensor_info[2]= root.createNestedObject("ch2");
    sensor_info[3]= root.createNestedObject("ch3");
    sensor_info[0]["name"]="PPR_red";
    sensor_info[0]["dtype"]="uint32_t";
    sensor_info[0]["period"]="20ms";
    sensor_info[0]["unit"]="raw";

    sensor_info[1]["name"]="PPR_ir";
    sensor_info[1]["dtype"]="uint32_t";
    sensor_info[1]["period"]="20ms";
    sensor_info[1]["unit"]="raw";

    sensor_info[2]["name"]="Temperature";
    sensor_info[2]["dtype"]="uint16_t";
    sensor_info[2]["period"]="0";
    sensor_info[2]["unit"]="C";

    sensor_info[3]["name"]="HeartRate";
    sensor_info[3]["dtype"]="float";
    sensor_info[3]["period"]="0";
    sensor_info[3]["unit"]="BPM";

    txPacket_param="";
    serializeJson(doc, txPacket_param);
}


void enroll_serviceProfile(void){
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  array.add("Start_Service");
  JsonObject root = array.createNestedObject();
  root["sid"]="test-0000-0000-0000";
  root["type"]="tsSensor"; //Time Series
  root["nickname"]="sensor_0000";
  root["description"]="ppg sensor";
  root["owner"]="Hong Gil-dong";
  root["state"]="undefined";
  root["room"]="my-little-tiny-room";
  JsonObject contents = root.createNestedObject("contents");
  JsonObject channel[NUM_SENSOR_CH];  
  channel[0]=contents.createNestedObject("0");
  channel[1]=contents.createNestedObject("1");
  channel[2]=contents.createNestedObject("2");
  channel[3]=contents.createNestedObject("3");
  channel[0]["name"] = "ppg-red";
  channel[0]["type"] = "TS";
  channel[1]["name"] = "ppg-ir";
  channel[1]["type"] = "TS";
  channel[2]["name"] = "temperature";
  channel[2]["type"] = "TS";
  channel[3]["name"] = "heart-rate";
  channel[3]["type"] = "TS";
  // JSON to String (serializion)
  String str;
  serializeJson(doc, str);
  socketIO.sendEVENT(str);
  Serial.println("ENROLL:");
  Serial.println(str);
  return;
}



/*
void SensorInit_DPS310(){
  int32_t Pressure[N_of_sample];
  int32_t Temperature[N_of_sample];
  //DPS310 barometer
  Serial.print("Connecting to DPS310...");
  if (! dps.begin_I2C(0x76)) { //0x77 or 76
  }else{
    Serial.println("DPS OK!");
  }
  // Setup highest precision
//  dps.configurePressure(DPS310_64HZ, DPS310_64SAMPLES);
//  dps.configureTemperature(DPS310_64HZ, DPS310_64SAMPLES);
  dps.configurePressure(DPS310_64HZ, DPS310_32SAMPLES);
  dps.configureTemperature(DPS310_32HZ, DPS310_32SAMPLES);

  dps_temp->printSensorDetails();
  dps_pressure->printSensorDetails();
}

*/
/*
void main_task(void){
  DynamicJsonDocument doc(1024);
  JsonArray array = doc.to<JsonArray>();
  array.add("msg-v0");
  JsonObject root = array.createNestedObject();

  JsonArray sensor_data[2];
  sensor_data[0] = root.createNestedArray("sensor_0");
  sensor_data[1] = root.createNestedArray("sensor_1");

  sensor_data[0].add(1);
  sensor_data[0].add(2);
  sensor_data[0].add(3);
  sensor_data[0].add(4);
  
  sensor_data[1].add(10);
  sensor_data[1].add(20);
  sensor_data[1].add(30);
  sensor_data[1].add(40);

  debug.print(" R[");
  debug.print(particleSensor.getRed());
  debug.print("] IR[");
  debug.print(particleSensor.getIR());
  debug.print("] G[");
  debug.print(particleSensor.getGreen());
  debug.print("]");


  // JSON to String (serializion)
  String str;
  serializeJson(doc, str);
  socketIO.sendEVENT(str);
  Serial.print("DATA:");
  Serial.println(str);
  return; 
}
*/