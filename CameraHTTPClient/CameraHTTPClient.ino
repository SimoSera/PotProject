#include "esp_camera.h"
#include <WiFi.h>
#include <HTTPClient.h>
#include <base64.h>
#include <ArduinoJson.h>
#include <string>
#include <esp_wifi.h>       // Needed to be able to get the MAC address (esp_wifi_get_mac)

//
// WARNING!!! PSRAM IC required for UXGA resolution and high JPEG quality
//            Ensure ESP32 Wrover Module or other board with PSRAM is selected
//            Partial images will be transmitted if image exceeds buffer size
//
//            You must select partition scheme from the board menu that has at least 3MB APP space.
//            Face Recognition is DISABLED for ESP32 and ESP32-S2, because it takes up from 15
//            seconds to process single frame. Face Detection is ENABLED if PSRAM is enabled as well

// ===================
// PINS for this camera model
// ===================
#define PWDN_GPIO_NUM     -1
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM     10
#define SIOD_GPIO_NUM     40
#define SIOC_GPIO_NUM     39

#define Y9_GPIO_NUM       48
#define Y8_GPIO_NUM       11
#define Y7_GPIO_NUM       12
#define Y6_GPIO_NUM       14
#define Y5_GPIO_NUM       16
#define Y4_GPIO_NUM       18
#define Y3_GPIO_NUM       17
#define Y2_GPIO_NUM       15
#define VSYNC_GPIO_NUM    38
#define HREF_GPIO_NUM     47
#define PCLK_GPIO_NUM     13

#define LED_GPIO_NUM      21

// ===========================
// Enter your WiFi credentials
// ===========================
const char *ssid = "Mi9T";//"FRITZ!Box 7530 GR";
const char *password = "ciaociao";//"05768323608271690844";
const char *url = "http://raspberrypi.local:80/camera";

String DEVICE_ID="";

// Setup of the camera
void setupCam(){
   camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sccb_sda = SIOD_GPIO_NUM;
  config.pin_sccb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.frame_size = FRAMESIZE_UXGA;
  config.pixel_format = PIXFORMAT_JPEG;  // for streaming
  //config.pixel_format = PIXFORMAT_RGB565; // for face detection/recognition
  config.grab_mode = CAMERA_GRAB_WHEN_EMPTY;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 1;

  // if PSRAM IC present, init with UXGA resolution and higher JPEG quality
  //                      for larger pre-allocated frame buffer.
  if (config.pixel_format == PIXFORMAT_JPEG) {
    if (psramFound()) {
      config.jpeg_quality = 10;
      config.fb_count = 2;
      config.grab_mode = CAMERA_GRAB_LATEST;
    } else {
      // Limit the frame size when PSRAM is not available
      config.frame_size = FRAMESIZE_SVGA;
      config.fb_location = CAMERA_FB_IN_DRAM;
    }
  } else {
    // Best option for face detection/recognition
    config.frame_size = FRAMESIZE_240X240;
  #if CONFIG_IDF_TARGET_ESP32S3
      config.fb_count = 2;
  #endif
    }

  #if defined(CAMERA_MODEL_ESP_EYE)
    pinMode(13, INPUT_PULLUP);
    pinMode(14, INPUT_PULLUP);
  #endif

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t *s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  if (s->id.PID == OV3660_PID) {
    s->set_vflip(s, 1);        // flip it back
    s->set_brightness(s, 1);   // up the brightness just a bit
    s->set_saturation(s, -2);  // lower the saturation
  }
  // drop down frame size for higher initial frame rate
  if (config.pixel_format == PIXFORMAT_JPEG) {
    s->set_framesize(s, FRAMESIZE_QVGA);
  }

  #if defined(CAMERA_MODEL_M5STACK_WIDE) || defined(CAMERA_MODEL_M5STACK_ESP32CAM)
    s->set_vflip(s, 1);
    s->set_hmirror(s, 1);
  #endif

  #if defined(CAMERA_MODEL_ESP32S3_EYE)
    s->set_vflip(s, 1);
  #endif

}

// Establish wifi connection
void setupWifi(){
  WiFi.begin(ssid, password);
  WiFi.setSleep(false);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("");
  Serial.println("WiFi connected");
  Serial.print("Camera Ready! IP: ");
  Serial.println(WiFi.localIP());

  uint8_t MAC_ADDRESS[6];       // used to read the MAC address
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


void setup() {
  Serial.begin(115200);
  Serial.setDebugOutput(true);
  Serial.println();

  


  setupCam();
  Serial.println("Camera initialization succefull");

  setupWifi();
}


int encodedLength(int plainLength) {
	int n = plainLength;
	return (n + 2 - ((n + 2) % 3)) / 3 * 4;
}
// Method to POST an image to url
int sendImage() {
  camera_fb_t * fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("Camera capture failed");
    return -1;
  }
  auto inputLength = fb->len;
  String output;
  output=base64::encode(fb->buf, inputLength);
  
  
  DynamicJsonDocument doc(output.length()+100); // Adjust size as needed
  doc["id"] = DEVICE_ID;
  doc["image"] = output;

  String jsonString;
  serializeJson(doc, jsonString);


  HTTPClient http;
  http.begin(url);
  http.addHeader("Content-Type", "application/json");

  int httpResponseCode = http.POST(jsonString);

  if (httpResponseCode > 0) {
    String response = http.getString();
    Serial.printf("Image sent, server responded: %d\n", httpResponseCode);
    Serial.println(response);
  } else {
    Serial.printf("Error sending image: %s\n", http.errorToString(httpResponseCode).c_str());
    http.end();
    esp_camera_fb_return(fb);
    return -1;
  } 

  http.end();
  esp_camera_fb_return(fb);
  return 0;
}

void loop() {
  sendImage();
  // Instead of delay consider shutting down the esp32 and waking it up after (forx example after 1 day)
  esp_sleep_enable_timer_wakeup(30000000); //30 seconds
  esp_deep_sleep_start();
  //delay(60000);
}