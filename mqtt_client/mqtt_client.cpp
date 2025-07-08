#include <Arduino.h>
/*
  HARDWARE:
    MCU: ESP32 DEVKIT V1 (similar clone)                                          (https://it.aliexpress.com/item/32959541446.html (ESP32 38 pin Micro-USB CP2102))
    LEDS: strip of SK6812 RGBNW  (RGB + neutral white) -> 4 color: red, green, blue, neutral white. Addressable one by one       (https://it.aliexpress.com/item/1005002632400121.html (Black PCB, 1m 60 IP30, RGB NW))
    Sensors:
      TEMT6000 -> analog sensor for light level                                     (https://it.aliexpress.com/item/1005006582698654.html)
      Capacitive Soil Moisture Sensor -> analog sensor for soil moisture level      (https://it.aliexpress.com/item/1005007475368069.html)
      Module (uses I2C for communication):                                          (https://it.aliexpress.com/item/1005004761725413.html)
        AHT21 -> temperature + humidity
        ENS160 -> AQI (Air Quality Index (UBA)) sensor
*/


/*
    MQTT MESSAGES: (MAC address is without the ':' between the numbers)

        PUBLISH:  pot/<MAC Address>
          message: "<temperature> <humidity> <light> <moisture> <aqi>"

        SUBSCIBE: pot/<MAC Address>/leds
              n [W R G B]
          where n:
            0 = turn lights off  (WRGB not needed)
            1 = turn lights to still WRGB
            2 = breathing light effect loop(WRGB -> off -> WRGB)
            3 = circling light effect (WRGB single led circling around)
          W = white level (0-255)
          R = red lelvel  (0-255)
          G = green level (0-255)
          B = blue level (0-255)
*/

// publishes on pot/<MAC address without :> 
// subscribes on pot/<MAC Address without :>/leds
// uses LWT (Last Will  and Testament), which publishes a message when keep alive is missed, signaling disconnection



/*
IMPLEMENT SPEED 


*/
/* Noi usiamo esp32 quindi come hai scritto dobbiamo usare Wifi*/
#include <WiFi.h>           // For ESP32/ESP8266 use <WiFi.h>, for Arduino Uno WiFi use <WiFiNINA.h>
#include <PubSubClient.h>   // LIbrary for MQTT on ESP32
#include "LedStrip.h"       // Class defined by me to manage led strip
#include "PotSensors.h"     // Class created by me to manage all sensors stuff
#include <esp_wifi.h>       // Needed to be able to get the MAC address (esp_wifi_get_mac)


unsigned long timer_sensors;    // timer used to check if sensors need to be read in loop()
unsigned long timer_mqtt_check;       // timer used to delay the call of client.loop() as explained online https://github.com/knolleary/pubsubclient/issues/756

// === CONFIG RETE WiFi ===
const char* ssid = "Mi9T";      // ssif (name) of the wifi
const char* password = "ciaociao";  // password of said wifi

// === CONFIG MQTT === 
IPAddress ip_server;        // stores the IP of the MQTT server
char mqtt_server[16];       // sotes the IP of MQTT server inf form of string (to use with mqtt library)
const char* mqtt_server_hostname = "raspberrypi.local";  // Hostname del Raspberry Pi
const int mqtt_port = 1883;       // port for MQTT on the server (bridge)
String mqtt_topic= "pot/";      // where the publish mqtt topic is stored

uint8_t MAC_ADDRESS[6];       // used to read the MAC address
String DEVICE_ID="";          // where the MAC address is stored to easily put into other strings

// === CONFIG PINS ===          // Pinout available on https://mischianti.org/doit-esp32-dev-kit-v1-high-resolution-pinout-and-specs/
// Note that for the module that uses I2C no PIN is specified as the standard I2C pins are used (SDA(42) + SCL(39))
#define PIN_LIGHT 35          // pin used for the light sensor
#define PIN_MOISTURE 32       // pin of the soil moisture sensor
#define STRIP_PIN 25
#define NUM_PIXELS 40

#define SENSOR_SAMPLING_PERIOD 10000    // milliseconds to wait between sensor reads
#define MQTT_LOOP_CHECK_PERIOD 500      // milliseconds between calls to client.loop(), used together with timer_mqtt_check


// === Objects ===

PotSensors sensors(PIN_LIGHT,PIN_MOISTURE);       // Object of type PotSensors, used to manage all the sensors 

LedStrip leds(NUM_PIXELS,STRIP_PIN,0x80700000);

WiFiClient espClient;   // Object used by the PubSubClient library/object

PubSubClient client(espClient);   // Object that manages MQTT connections






// Function to setup the Wifi Connection
void setup_wifi() {
  delay(10);
  Serial.println();
  Serial.print("Connecting to WiFi...");
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());
  
  // Code to get the device id ax the mac address
  while(esp_wifi_get_mac(WIFI_IF_STA,MAC_ADDRESS)!=ESP_OK){
    Serial.print("Error getting ESP32 MAC address(Device ID)");
    delay(1000);
  }
  DEVICE_ID="";
  for (int i = 0; i < 6; ++i) {
    if (MAC_ADDRESS[i] < 0x10) {
      DEVICE_ID += "0"; // Add leading zero for single hex digit
    }
    DEVICE_ID += String(MAC_ADDRESS[i], HEX);
  }

  DEVICE_ID.toUpperCase(); // make uppercase
  Serial.println("DEVICE_ID: "+DEVICE_ID);

}


// Funzione per connettersi al broker MQTT
void reconnect_mqtt() {
  while (!client.connected()) {
    if(WiFi.status() != WL_CONNECTED){
      setup_wifi();
    }
    Serial.print("Connecting to MQTT broker...");
    String clientId = "Pot-"+DEVICE_ID;
    clientId += String(random(0xffff), HEX);
    String LWTtopic="pot/"+DEVICE_ID+"/disconnect";
    String LWTmessage="1";
    if (client.connect(clientId.c_str(), LWTtopic.c_str() , 2, true, LWTmessage.c_str())) {
      Serial.println("Connected to broker!");
    } else {
      Serial.print("Connection to broker Failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in in 5 seconds");
      delay(5000);
    }
  }
  client.subscribe(("pot/"+DEVICE_ID+"/leds").c_str());
  Serial.println(("pot/"+DEVICE_ID+"/leds").c_str());
}


/*
message to action:

"0"  -> Turn off leds
"1"  -> Turn on leds
"2 W R G B"  -> Set light mode to still with color WRGB
"3 W R G B"  -> Set light mode to breathing woth color WRGB
"4 W R G B"  -> Set light mode to circling woth color WRGB
"5 int"    -> Set brightness to val (0-255)
"6 int"     -> Set light threshold to val (0-1000 lx) 
"7 bool"   -> Set auto brightness on/off (bool val)   /currently removed
"8 float"   -> Set auto brightness "gamma" value      /currently removed
*/


void callbackSubscribe(char* topic, byte* payload, unsigned int length) {
  char message[129];
  if (length >= sizeof(message) || length==0) return; // Avoid overflow
  memcpy(message, payload, length);
  message[length] = '\0'; // Null-terminate
  Serial.println(message);
  char* tokenStr = strtok(message, " ");
  if (!tokenStr){
    Serial.println("Message missing??"); 
    return;
  }
  int token = atoi(tokenStr);
  if(token==0 && (tokenStr[0]>'9' || tokenStr[0]<'0' )){
    Serial.println("Wrong type of message");
    return;
  }
  // Token 0: Turn off
  if (token == 0) {
      leds.turn_off();
      return;
  }
  if(token==1){
    leds.turn_on();
  }
  // Token 8: Float case
  if (token == 8) {
      char* arg1 = strtok(nullptr, " ");
      if (!arg1) {
          Serial.println("Error in parsing subscribe message (float)");
          return;
      }
      float f = strtof(arg1, nullptr);
      leds.set_brightness_gamma(f);
      leds.turn_on();
  }
  if(token>=5 && token<=7){
    char* arg1=strtok(nullptr," ");
    uint32_t val = strtoul(arg1, nullptr, 10);
    if(token==5){
      if(val>255){
        return;
      }
      leds.set_brightness(val);
    }else if(token==6){
      leds.set_light_threshold(val);
    }else {
      leds.set_auto_brightness(bool(val));
    }
  }
  // Tokens 2–7: General number cases
  if (token >= 2 && token <= 4) {
      char* rgbwstr[4];
      uint32_t rgbw[4];
      color_t col=0;
      for(int i=0;i<4;++i){
        rgbwstr[i]=strtok(nullptr, " ");
        if (!rgbwstr[i]) {
          Serial.println("Error in parsing subscribe message (arg1)");
          return;
        }
        rgbw[i]=strtoul(rgbwstr[i],nullptr,10);
        col+=rgbw[i]<<(8*(3-i));
      }
      
      if(token == 2){
        leds.set_still(col);
      }
      if (token == 3 || token == 4) {
            char* arg2 = strtok(nullptr, " ");
            if (!arg2) {
                Serial.println("Error in parsing subscribe message (arg2)");
                return;
            }
            uint8_t speed = strtoul(arg2, nullptr, 10);
            if (token == 3) leds.set_breathing(col, speed);
            if (token == 4) leds.set_circling(col, speed);
      }
  }
  if(token==5){

  }

  leds.update(sensors.getLightLevel());

}




void setup() {
    Serial.begin(9600);
    leds.set_still(0x80700000);
    leds.turn_on();
    delay(500);  //?? otherwise can't connect to wifi
    setup_wifi();
    
    WiFi.hostByName(mqtt_server_hostname, ip_server);
    strcpy(&mqtt_server[0], ip_server.toString().c_str());
    Serial.println("MQTT Server: ");
    Serial.println(mqtt_server);
    //Serial.println(ip_server.toString());
    
    mqtt_topic+=DEVICE_ID;
    client.setServer(mqtt_server, mqtt_port);
    client.setCallback(callbackSubscribe);
    reconnect_mqtt();
    //client.publish((mqtt_topic+"/new_conn").c_str(), "");   PUT THIS INSIDE THE PART FOR FIRST EVER CONNECTION
    
    sensors.init();   
    timer_sensors=millis();
    leds.set_breathing(0x008000ff,60);
    client.loop();
}

void loop() {

  // LEDS update
  leds.update(sensors.getLightLevel());
  if (!client.connected()) {
    reconnect_mqtt();
  }


// SENSORS SAMPLING AND MQTT COMMUNICATION



  if (millis()-timer_sensors > SENSOR_SAMPLING_PERIOD){
    /*
    String(tempC) converte il float in stringa.
    .c_str() serve perché client.publish() vuole un const char*, non un oggetto String.
    */
    // sia tempC,humidity,moisture,light li avevo già calcolati quindi le prime righe di ogni blocco sono da togliere
    String tempStr;
    String temperature =String(sensors.getTemperature());
    String humidity=String(sensors.getHumidity());
    String light=String(sensors.getLightLevel()); 
    //non c'è da specificare il DEC per decimale perchè Arduino già converte automaticamente il numero in una stringa in formato decimale (DEC). Quindi non devi preoccuparti di specificarlo!
    String moisture=String(sensors.getSoilMoisture());
    uint8_t aqi_uba = sensors.getAQI();
    while(aqi_uba==255){ // (255=error), while i can't get a read for the AQI: try to read again
      aqi_uba = sensors.getAQI();
    }
    String aqi =String(aqi_uba);
    tempStr=temperature+" "+humidity+" "+light+" " +moisture+" "+aqi;
    client.publish(mqtt_topic.c_str(), tempStr.c_str());


    timer_sensors=millis();
  }

  // CHECK FOR PENDING MQTT MESSAGES and MQTT STATUS
  if(millis()-timer_mqtt_check > MQTT_LOOP_CHECK_PERIOD){
    client.loop();
    timer_mqtt_check=millis();
  }

  
}

  



