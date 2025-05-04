
/* Noi usiamo esp32 quindi come hai scritto dobbiamo usare Wifi*/
#include <WiFi.h>           // Per ESP32/ESP8266 usa <WiFi.h>, per Arduino Uno WiFi usa <WiFiNINA.h>
#include <PubSubClient.h>   // Libreria MQTT
#include "PotSensors.h"

#define SAMPLING_PERIOD  600// seconds

// === CONFIG RETE WiFi ===
const char* ssid = "SSID";
const char* password = "PASSWORD";

// === CONFIG MQTT ===
IPAddress ip_server;
char* mqtt_server;  // IP del Raspberry Pi
const char* mqtt_server_hostname = "raspberrypi";  // Hostname del Raspberry Pi
const int mqtt_port = 1883;
String mqtt_topic= "pot/";

char MAC_ADDRESS[6];
String DEVICE_ID="";

#define PIN_LIGHT 35
#define PIN_MOISTURE 32

PotSensors sensors(PIN_LIGHT,PIN_MOISTURE); 



WiFiClient espClient;
/* 
Questa riga crea un oggetto client TCP/IP che serve per connettersi al broker MQTT su una porta specifica (tipicamente la 1883).
A cosa serve?
È una specie di "canale di comunicazione" su cui il client MQTT può appoggiarsi per inviare e ricevere dati.
Fa parte delle librerie WiFi di Arduino/ESP.
Viene passato all'oggetto PubSubClient subito dopo.
*/
PubSubClient client(espClient);
/*
Questa riga crea l’oggetto MQTT vero e proprio, e gli dice di usare espClient (il client TCP/IP creato sopra) per parlare con il broker.
Cosa fa?
Crea un client MQTT.
Specifica il mezzo fisico di trasporto, cioè: "usa espClient per inviare/ricevere messaggi".
È con questo oggetto client che chiami metodi come:
client.connect(...)
client.publish(...)
client.subscribe(...)
client.loop()
*/

/* La parte di setup del Wifi sembra corretta, però serve anche la parte per i sensori*/
// Funzione per inizializzare tutti sensori






// Funzione per connettersi al WiFi
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
  while (!client.connected()) {
    Serial.print("Connecting to MQTT broker...");
    String clientId = "ArduinoClient-";
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
}

void setup() {
  Serial.begin(9600);
  setup_wifi();
  
  
  WiFi.hostByName(mqtt_server_hostname, ip_server);
  mqtt_server=(char*)malloc(ip_server.toString().length() +1);
  strcpy(mqtt_server, ip_server.toString().c_str());
  Serial.print("MQTT Server: ");
  Serial.print(mqtt_server);
  Serial.println(ip_server.toString());
  
  mqtt_topic+=DEVICE_ID;
  client.setServer(mqtt_server, mqtt_port);
  // è come se dicesse: “Ehi client MQTT, il broker lo trovi all’indirizzo mqtt_server, e ascolta sulla porta mqtt_port.” indirizzo? quello del RASPBERRY PI
  sensors.init();
}

void loop() {
  if (!client.connected()) {
    reconnect_mqtt();
  }
  client.loop();
  /* Questa riga è fondamentale per far funzionare correttamente il client MQTT.
  Mantiene viva la connessione MQTT.
  Gestisce il "keep-alive" e riceve eventuali messaggi MQTT in arrivo.
  Deve essere chiamata spesso, idealmente in ogni ciclo del loop().
  Senza questa riga, non riceveresti messaggi MQTT, e potresti anche venire disconnesso automaticamente dal broker. */

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
  client.publish(mqtt_topic, tempStr.c_str());



  /* Nel protocollo MQTT:
  I topic NON si devono pre-creare.
  Non esistono realmente sul broker finché non vengono usati.
  Il broker è stateless rispetto ai topic: appena pubblichi o sottoscrivi un topic, quello topic "esiste".
  Quindi se nel tuo Arduino scrivi:
  client.publish("sensori/temperatura", "23.5");
  Il topic sensori/temperatura “nasce” nel momento in cui:
  qualcuno ci pubblica sopra (publish)
  o qualcuno si iscrive (subscribe)
  Puoi usare qualsiasi nome di topic (es. "casa/salone/temperatura"), basta che sia coerente tra chi pubblica e chi riceve.
  N.B. 
  Usa / per separare categorie: stanza/sensore/tipo
  Evita spazi
  Tutto minuscolo è più leggibile: cucina/luce/stato
  Puoi anche usare wildcard + e # per sottoscrizioni flessibili
  */

  delay(5000); // attesa 5 secondi tra le letture
}

