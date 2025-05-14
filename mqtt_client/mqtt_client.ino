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




/*
IMPLEMENT SPEED 


*/

#include <Adafruit_NeoPixel.h>          // Docs: https://adafruit.github.io/Adafruit_NeoPixel/html/class_adafruit___neo_pixel.html
/* Noi usiamo esp32 quindi come hai scritto dobbiamo usare Wifi*/
#include <WiFi.h>           // For ESP32/ESP8266 use <WiFi.h>, for Arduino Uno WiFi use <WiFiNINA.h>
#include <PubSubClient.h>   // LIbrary for MQTT on ESP32
#include "PotSensors.h"     // Class created by me to manage all sensors stuff
#include <esp_wifi.h>       // Needed to be able to get the MAC address (esp_wifi_get_mac)


unsigned long timer_sensors;    // timer used to check if sensors need to be read in loop()
unsigned long timer_mqtt_check;       // timer used to delay the call of client.loop() as explained online https://github.com/knolleary/pubsubclient/issues/756
unsigned long timer_leds;       // timer used for the leds effects

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
#define PIN_LEDS  33           // pin for the LEDS lights
#define NUMPIXELS 16          // number of leds to be used on the strip

#define SENSOR_SAMPLING_PERIOD 10000    // milliseconds to wait between sensor reads
#define MQTT_LOOP_CHECK_PERIOD 500      // milliseconds between calls to client.loop(), used together with timer_mqtt_check

//=== Define LEDS settings ===
uint32_t W=0xff000000,R=0x00ff0000,G=0x0000ff00,B=0x000000ff;    // used to store the colors values (stored as 32 bit to easily sum them together)
bool breathing=false,circling=false,breathing_inverse=false,light_on=true;    // used to activate effects
size_t leds_timestep=0;                 // used during effects to track current step
#define BREATHING_EFFECT_PERIOD 30      // period between updates of the breathing effect
#define BREATHING_EFFECT_DURATION 3000  // total duration of the breathing effect (brethe in and out)
#define CICLING_EFFECT_PERIOD 100     // period between updates of the cicling effect


// === Objects ===

PotSensors sensors(PIN_LIGHT,PIN_MOISTURE);       // Object of type PotSensors, used to manage all the sensors 
int light_threashold=0;


Adafruit_NeoPixel pixels(NUMPIXELS, PIN_LEDS, NEO_GRBW + NEO_KHZ800);     // object used to manage the led strip

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
  for (int i = 0; i < 6; i++) {
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
  if(WiFi.status() != WL_CONNECTED){
    setup_wifi();
  }
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    String clientId = "Pot-"+DEVICE_ID;
    clientId += String(random(0xffff), HEX);
  // non necessario, visto che ho un solo arduino tutte le volte. la riga di codice sopra mi farebbe ottenere tipo clientId = "ArduinoClient-3f7b"
    if (client.connect(clientId.c_str())) {
      Serial.println("Connected to broker!");
    } else {
      Serial.print("Connection to broker Failed, rc=");
      Serial.print(client.state());
      Serial.println(" retry in in 5 seconds");
      delay(5000);
    }
  }
  client.subscribe(("pot/"+DEVICE_ID+"/leds").c_str());

}


void callbackSubscribe(char* topic, byte* message, unsigned int length){
  circling=false;
  breathing=false;
  char token=message[0];
  if(token=='0'){ // message = 0 -> turn off leds
    light_on=false;
    pixels.fill(0);
    pixels.show();
    return;
  }

  if(token=='4'){  //message = 4 -> change threashold
    int temp=0,i=2;
    while(((char)message[i])!=' ' && i<length){
      temp*=10;
      temp+=message[i]-'0';
      i++;
    }
    light_threshold=temp;
    return;
  }

  // all other messages (1,2,3) -> light effect (still, circling, breathing)

  int count=0,temp=0,speed=0;
  for(unsigned int i=2;i<length;i++){

    while(((char)message[i])!=' ' && i<length){
      temp*=10;
      temp+=message[i]-'0';
      i++;
    }
    count++;
    if(count==1){
      W=temp <<  24;
    } else if(count==2){
      R=temp <<  16;
    }else if(count==3){
      G=temp <<  8;
    }else if(count==4){
      B=temp;
    }else if(count==5){
      speed=temp;
    }
    temp=0;
  }
  switch(token){
    case 0: // should never happen
      break;
    case 1:
      break;
    case 2:
      breathing=true;
      // implment speed
      break;
    case 3:
      circling=true;
      //implment speed
      break;
  }
  leds_timestep=0;
  timer_leds=millis();
  if(light_on){
    pixels.fill(W);
    pixels.show();
  }
}




void setup() {
  Serial.begin(9600);
  pixels.begin();
  pixels.fill(W+R);
  pixels.show();
  setup_wifi();
  
  
  WiFi.hostByName(mqtt_server_hostname, ip_server);
  strcpy(&mqtt_server[0], ip_server.toString().c_str());
  Serial.println("MQTT Server: ");
  Serial.print(mqtt_server);
  Serial.println(ip_server.toString());
  delay(300);
  mqtt_topic+=DEVICE_ID;
  client.setServer(mqtt_server, mqtt_port);
  // è come se dicesse: “Ehi client MQTT, il broker lo trovi all’indirizzo mqtt_server, e ascolta sulla porta mqtt_port.” indirizzo? quello del RASPBERRY PI
  client.setCallback(callbackSubscribe);
  sensors.init();
  pixels.fill(W);
  pixels.show();
  timer_sensors=millis();
  timer_leds=millis();

}

void loop() {

  else{

  }
  
  // LEDS EFFECTS part
  if(light_on){
    if(sensors.getLightLevel() < light_threahold){
      light_on=false;
      pixels.fill(0);
      pixels.show();
    }else{
       if(breathing  &&  (millis()-timer_leds) > BREATHING_EFFECT_PERIOD){  // if the breathing effect is active and at least BREATING_EFFECT_PERIOD milliseconds have elapsed 
        if(!breathing_inverse){
          leds_timestep++;
          if(leds_timestep>=(BREATHING_EFFECT_DURATION/BREATHING_EFFECT_PERIOD)/2){
            breathing_inverse=true;
          }
        }else {
          leds_timestep--;
          if(leds_timestep==0){ //reset effect
            breathing_inverse=false;
          }
        }
        float p=leds_timestep/(BREATHING_EFFECT_DURATION/BREATHING_EFFECT_PERIOD);
        pixels.fill(W*p+R*p+G*p+B*p);
        timer_leds=millis();
        pixels.show();
      }
      if(circling && (millis()-timer_leds) > CICLING_EFFECT_PERIOD){ // if the cicling effect is active and at least CICLING_EFFECT_PERIOD milliseconds have elapsed
        if(leds_timestep==NUMPIXELS){
          leds_timestep=0;   
        }
        pixels.fill(W);
        pixels.fill(R+G+B,leds_timestep,1);
        leds_timestep++;
        timer_leds=millis();
        pixels.show();
      }
    }


  }else{
    if(sensors.getLightLevel() >= light_threahold){
      light_on=true;
      pixels.fill(W);
      pixels.show();
    }
  }
  


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
    while(aqi_uba==255){ // while i can't get a read for the AQI: try to read again
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



