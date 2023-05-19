
#include "eepromCustom.h"
#include "wifi_custom.h"
#include "SerialUI.h"
#include "spiffsCustom.h"
#include "ble_custom.h"
#include "sensorControl.h"

#define uS_TO_S_FACTOR 1000000ULL  /* Conversion factor for micro seconds to seconds */
#define TIME_TO_SLEEP  5        /* Time ESP32 will go to sleep (in seconds) */

RTC_DATA_ATTR int bootCount = 0;


uint8_t txValue = 0;

/*
Method to print the reason by which ESP32
has been awaken from sleep
*/
void print_wakeup_reason(){
  esp_sleep_wakeup_cause_t wakeup_reason;

  wakeup_reason = esp_sleep_get_wakeup_cause();

  switch(wakeup_reason)
  {
    case ESP_SLEEP_WAKEUP_EXT0 : Serial.println("Wakeup caused by external signal using RTC_IO"); break;
    case ESP_SLEEP_WAKEUP_EXT1 : Serial.println("Wakeup caused by external signal using RTC_CNTL"); break;
    case ESP_SLEEP_WAKEUP_TIMER : Serial.println("Wakeup caused by timer"); break;
    case ESP_SLEEP_WAKEUP_TOUCHPAD : Serial.println("Wakeup caused by touchpad"); break;
    case ESP_SLEEP_WAKEUP_ULP : Serial.println("Wakeup caused by ULP program"); break;
    default : Serial.printf("Wakeup was not caused by deep sleep: %d\n",wakeup_reason); break;
  }
}



/***********************************************************************
  Adafruit MQTT Library ESP32 Adafruit IO SSL/TLS example

  Use the latest version of the ESP32 Arduino Core:
    https://github.com/espressif/arduino-esp32

  Works great with Adafruit Huzzah32 Feather and Breakout Board:
    https://www.adafruit.com/product/3405
    https://www.adafruit.com/products/4172

  Adafruit invests time and resources providing this open source code,
  please support Adafruit and open-source hardware by purchasing
  products from Adafruit!

  Written by Tony DiCola for Adafruit Industries.
  Modified by Brent Rubell for Adafruit Industries
  MIT license, all text above must be included in any redistribution
 **********************************************************************/
#include <MQTT.h>



// WiFiClient net;
MQTTClient MQTTclient;

unsigned long lastMillis = 0;

#define LEN_ACCSMPL 30
#define LEN_PRSSMPL 20
#define LEN_TRHSMPL 20
int8_t acc[LEN_ACCSMPL+2][3];
uint32_t prs[LEN_PRSSMPL+6];
int16_t trh[LEN_TRHSMPL+6];

/************************* WiFi Access Point *********************************/
/// MQTT

void connect() {
  Serial.print("checking wifi...");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(1000);
  }

  Serial.print("\nconnecting...");
  while (!MQTTclient.connect("arduino", "public", "public")) {
    Serial.print(".");
    delay(1000);
  }

  Serial.println("\nconnected!");

  MQTTclient.subscribe("perpet/SerialNumber/acc");
  MQTTclient.subscribe("perpet/SerialNumber/prs");
  MQTTclient.subscribe("perpet/SerialNumber/trh");
  MQTTclient.subscribe("perpet/SerialNumber/CMD");
  // client.unsubscribe("/hello");
}


// void messageReceivedAdvanced(MQTTClient *client, char topic[], char bytes[], int length) {
  // String topic2 = topic;
  // String payload = bytes;
  // Serial.println("topic:"+topic2);
  // Serial.print("len:");
  // Serial.println(length);

void messageReceived(String &topic, String &payload) {
  // Serial.println("incoming: " + topic + " - " + payload);
  if(topic == "perpet/SerialNumber/acc"){
    // const char* rxPacket=payload.c_str();
    char rxPacket[LEN_ACCSMPL+2];
    payload.toCharArray(rxPacket,(LEN_ACCSMPL+2),0);
    Serial.println("******");      
    Serial.println("decoded:");
    for(int i=0;i<(LEN_ACCSMPL+2);i++){
      for(int j=0;j<3;j++){
        Serial.print((int8_t)rxPacket[i*3+j]);
        Serial.print(",");
      }
      Serial.println();
    }
    Serial.println("******");      
  }
  if(topic == "perpet/SerialNumber/trh"){
    const char* rxPacket=payload.c_str();
    Serial.println("******");      
    Serial.println("decoded:");
    for(int i=0;i<LEN_TRHSMPL;i++){
      Serial.print((int8_t)rxPacket[i]);
      Serial.print(",");
    }
    Serial.println("******");      
  }

  if(topic == "perpet/SerialNumber/CMD"){
    if((payload.toInt())%3==0){
      Serial.println("shutdown");
    }
  }
  // Note: Do not use the client in the callback to publish, subscribe or
  // unsubscribe as it may cause deadlocks when other things arrive while
  // sending and receiving acknowledgments. Instead, change a global variable,
  // or push to a queue and handle it in the loop after calling `client.loop()`.
}


const char* path = "/acc.txt";

// ref: (13:04) https://www.youtube.com/watch?v=JFDiqPHw3Vc&list=PL4s_3hkDEX_0YpVkHRY3MYHfGxxBNyKkj&index=100&t=590s
// clock | current wifi(mA) | current no wifi(mA) | Serial(baudrate) | i2c clock
//  240  |      157         |            69       |     115200       | 350kHz
//  160  |      131         |            46       |     115200       | 350kHz
//   80  |      119         |            32       |     115200       | 350kHz
//   40  |       82         |            18       |      57600       | 100kHz
//   20  |       77         |            13       |      28800       | 100kHz
//   10  |       74         |            11       |      14400       | 100kHz
void setup() {
  Serial.begin(230400);

  // SPIFFS setup
  if(!SPIFFS.begin(FORMAT_SPIFFS_IF_FAILED)){
      Serial.println("SPIFFS Mount Failed");
      return;
  }    
  listDir(SPIFFS, "/", 0);
  deleteFile(SPIFFS, "/acc.txt");
  int8_t accdum[1];
  Serial.println("writing data");
  writeFileBytes(SPIFFS, path, (uint8_t*)accdum,0);


  // EEPROM setup
  init_eeprom();
  eepromSetup_custom();
  // print params via serial port
  print_settings();

  // BLE setup
  bool bBLEactivated=true;
  if(bBLEactivated){
    ble_setup_custom();
  }
  delay(1000);

  // WiFi connection
  Serial.println("connecting to AP");
  bool isAPconnected=false;
  // WiFi.begin(ssid, pass);
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

  // MQTT setup
  MQTTclient.begin("jayutest.best", net);
  MQTTclient.onMessage(messageReceived);
  // MQTTclient.onMessageAdvanced(messageReceivedAdvanced);

  connect();


  // light_sleep_purpet();
}

void light_sleep_purpet(){
  //Increment boot number and print it every reboot
  ++bootCount;
  Serial.println("Boot number: " + String(bootCount));
  //Print the wakeup reason for ESP32
  print_wakeup_reason();

  /*
  First we configure the wake up source
  We set our ESP32 to wake up every 5 seconds
  */
  esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
  Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");

  Serial.println("Going to sleep now");
  Serial.flush();
  // WiFi.disconnect(true);
  // WiFi.mode(WIFI_OFF);

  esp_light_sleep_start();
  Serial.println("WiFi turning on!");
  // WiFi.begin(ssid, pass);
  // Serial.println("WiFi turned on!");
}

int8_t accbuf[90]; //3*1*30 Hz * 1 sec
int32_t prsbuf[80];   //4* 10 Hz * 2 sec
int16_t tempbuf[20]; // 2* 0.1 Hz * 100sec

uint32_t timestamp[3] = {2000,2000,2000};
uint32_t dt[3]={33,100,10000};
int8_t idx[3]={0,0,0};
uint16_t iter=0;

bool bViewerActive=false;
bool bPetActive=true;

void loop() {
  // ESP.restart();

  // BLE Control
  if (deviceConnected) {
    bViewerActive = true;
    pTxCharacteristic->setValue(&txValue, 1);
    pTxCharacteristic->notify();
    txValue++;
		delay(50); // bluetooth stack will go into congestion, if too many packets are sent
	}else{
    bViewerActive = false;
  }

  // if BLE is disconnected
  if (!deviceConnected && oldDeviceConnected) {
    delay(500); // give the bluetooth stack the chance to get things ready
    pServer->startAdvertising(); // restart advertising
    Serial.println("start advertising");
    oldDeviceConnected = deviceConnected;
  }
  // connecting
  if (deviceConnected && !oldDeviceConnected) {
  // do stuff here on connecting
    oldDeviceConnected = deviceConnected;
  }


  MQTTclient.loop();
  delay(10);  // <- fixes some issues with WiFi stability
  if (!MQTTclient.connected()) {
    connect();
  }

  if(bViewerActive==true){
    // setCpuFrequencyMhz(80); //No BT/Wifi: 10,20,40 MHz, for BT/Wifi, 80,160,240MHz 
    //function - sensing

    // save sensor data
    // IMU: 30Hz, Alt: 10 Hz, RH: 10s, T: 10s
    timestamp[0]=millis(); //check it later
    if(millis()>timestamp[0]){
      timestamp[0]+=dt[0];
      // getSensorDataIMU();
      acc[idx[0]][0]=(int8_t)(idx[0]+1);
      acc[idx[0]][1]=(int8_t)(idx[0]+1);
      acc[idx[0]][2]=-(int8_t)(idx[0]+1);
      idx[0]++;
      if(idx[0]==LEN_ACCSMPL){
        MQTTclient.publish("perpet/SerialNumber/acc", (const char*)acc,LEN_ACCSMPL*3);
        idx[0]=0;
      }
    }     

    if(millis()>timestamp[1]){
      //sensor data sampling
      timestamp[1]+=dt[1];
      if(idx[1]==LEN_PRSSMPL){
        MQTTclient.publish("perpet/SerialNumber/prs", (const char*)prs,LEN_PRSSMPL*4);
      }
    }
    if(millis()>timestamp[2]){
      //sensor data sampling
      timestamp[2]+=dt[2];
      if(idx[2]==LEN_TRHSMPL){
        MQTTclient.publish("perpet/SerialNumber/trh", (const char*)trh,LEN_TRHSMPL*4);
      }
    }

  }else{
    Serial.println("owner:non-active");
    if(bPetActive==true){
      
      Serial.println("pet:active");
      WiFi.disconnect();
      WiFi.mode(WIFI_OFF);
      delay(10);
      setCpuFrequencyMhz(20); //No BT/Wifi: 10,20,40 MHz, for BT/Wifi, 80,160,240MHz 
      delay(10);
      Serial.println("appending data");

      uint32_t timestamp_dive = millis(); 
      idx[0]=2;
      while(true){
        //logging sensor data
        // IMU: 30Hz, Alt: 10 Hz, RH: 10s, T: 10s
        timestamp[0]=millis(); //check it later
        if(millis()>timestamp[0]){
          timestamp[0]+=dt[0];
          if(idx[0]==2){
            acc[0][0]=30;
            acc[0][1]=(int8_t)(iter>>8);
            acc[0][2]=(int8_t)iter;
            acc[1][0]=30;
            acc[1][1]=(int8_t)(iter>>8);
            acc[1][2]=(int8_t)iter;
            // Serial.println(iter);
            iter++;
          }
          // getSensorDataIMU();
          acc[idx[0]][0]=(int8_t)(idx[0]+1);
          acc[idx[0]][1]=(int8_t)(idx[0]+1);
          acc[idx[0]][2]=-(int8_t)(idx[0]+1);
          idx[0]++;
          if(idx[0]==LEN_ACCSMPL+2){
            appendFileBytes(SPIFFS, path, (uint8_t*)acc, LEN_ACCSMPL*3+6);
            idx[0]=2;
          }
        } 

        if(millis()-timestamp_dive>5*1000){
          break;
        }
      }

      setCpuFrequencyMhz(80); //No BT/Wifi: 10,20,40 MHz, for BT/Wifi, 80,160,240MHz 

      Serial.println("connecting to AP");
      bool isAPconnected=false;
      // WiFi.begin(ssid, pass);
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

      //transmitting the log
      if (!MQTTclient.connected()) {
        connect();
      }
      Serial.printf("Reading file: %s\r\n", path);
      File file = SPIFFS.open(path);
      if(!file || file.isDirectory()){
          Serial.println("- failed to open file for reading");
          return;
      }
      Serial.println("- read from file:");
      uint8_t buffer[LEN_ACCSMPL*3+6];
      // while(file.available()){
      if(file.available()){
        for(int i = 0; i<2*60; i++){
          file.seek(i*(LEN_ACCSMPL*3+6));
          file.read(buffer,LEN_ACCSMPL*3+6);
          // for(int i = 0 ; i < LEN_ACCBUF*3 ; i ++){
          //   Serial.print(buffer[i]);
          //   if(i%3==2){
          //     Serial.println();
          //   }else{
          //     Serial.print(",");
          //   }
          // }
          Serial.println(i);
          MQTTclient.publish("perpet/SerialNumber/acc", (const char*)buffer,LEN_ACCSMPL*3+6);
        }
      }
      file.close();

    }else{
      setCpuFrequencyMhz(20); //No BT/Wifi: 10,20,40 MHz, for BT/Wifi, 80,160,240MHz 
      esp_sleep_enable_timer_wakeup(TIME_TO_SLEEP * uS_TO_S_FACTOR);
      Serial.println("Setup ESP32 to sleep for every " + String(TIME_TO_SLEEP) + " Seconds");
      Serial.println("Going to sleep now");
      Serial.flush();
      // WiFi.disconnect(true);
      // WiFi.mode(WIFI_OFF);
      esp_light_sleep_start();
      Serial.println("WiFi turning on!");
    }
  }





  

  // light_sleep_purpet();/
}
