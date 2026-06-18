/**********************************************************************
  Filename    : Camera Web Server
  Description : The camera images captured by the ESP32S3 are displayed on the web page through WiFi Halow.
  Auther      : www.freenove.com, heltec_automation
  Modification: 2022/10/31
**********************************************************************/
#include "esp_camera.h"
#include "mmiperf.h"
#include "HaLow.h"
#include "sd_read_write.h"

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_event_base.h"
#include "esp_event.h"
#include "esp_timer.h"

#include "FS.h"
#include "esp_camera.h"


// ===========================
// Enter your Halow credentials
// ===========================
#include <WiFi.h>


const char* wifi_ssid = "OnePlus 5" ;//"ORBI79";
const char* wifi_password = "12345678";
char local_time_str[32];
time_t now = time(NULL);
struct tm timeinfo;


//#define HALOW //if not defined  than WIFI
#define WIFI

//#define REC_SCHEDUALE_FILTER
//#define DELETE_SD
#ifdef REC_SCHEDUALE_FILTER
#define  START_HOUR 6    // 06:00
#define  STOP_HOUR  22  // 20:00
//#define  START_MINUTE  42
//#define  STOP_MINUTE   42
#endif

#define SNAPSHOT_TIMER 1000 // in milli seconds

const char* halow_ssid     = "NEW4";
const char* halow_password = "Aa123456";

ESP_EVENT_DECLARE_BASE(ESP_HTTP_SERVER_EVENT);
//ESP_EVENT_DEFINE_BASE(ESP_HTTP_SERVER_EVENT);
typedef enum {
    HTTP_SERVER_EVENT_ERROR = 0,       /*!< This event occurs when there are any errors during execution */
    HTTP_SERVER_EVENT_START,           /*!< This event occurs when HTTP Server is started */
    HTTP_SERVER_EVENT_ON_CONNECTED,    /*!< Once the HTTP Server has been connected to the client, no data exchange has been performed */
    HTTP_SERVER_EVENT_ON_HEADER,       /*!< Occurs when receiving each header sent from the client */
    HTTP_SERVER_EVENT_HEADERS_SENT,     /*!< After sending all the headers to the client */
    HTTP_SERVER_EVENT_ON_DATA,         /*!< Occurs when receiving data from the client */
    HTTP_SERVER_EVENT_SENT_DATA,       /*!< Occurs when an ESP HTTP server session is finished */
    HTTP_SERVER_EVENT_DISCONNECTED,    /*!< The connection has been disconnected */
    HTTP_SERVER_EVENT_STOP,            /*!< This event occurs when HTTP Server is stopped */
} esp_http_server_event_id_t;

static const char *TAG1 = "camera_test";


void startCameraServer();


void savePhotoToSD()
{

    camera_fb_t *fb = esp_camera_fb_get();

    if (!fb)
    {
        Serial.println("Camera capture failed");
        return;
    }

    writejpg(SD, local_time_str, fb->buf, fb->len);
    esp_camera_fb_return(fb);
}

void get_time()
{
    Serial.println("Getting time...");

    if (!getLocalTime(&timeinfo, 10000))
    {
        Serial.println("NTP FAILED");
        return;
    }

    strftime(local_time_str,
             sizeof(local_time_str),
             "%H-%M-%S__%d_%m_%Y",
             &timeinfo);

    Serial.print("TIME=");
    Serial.println(local_time_str);
}

void setup() {
  Serial.begin(115200);

//  Serial.setDebugOutput(true);
   
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
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 40000000;
  config.frame_size = FRAMESIZE_QSXGA;//FRAMESIZE_QSXGA;//FRAMESIZE_FHD;
  config.pixel_format = PIXFORMAT_JPEG; // for streaming
  config.grab_mode = CAMERA_GRAB_LATEST;
  config.fb_location = CAMERA_FB_IN_PSRAM;
  config.jpeg_quality = 12;
  config.fb_count = 2;
  //config.frame_size = FRAMESIZE_SVGA;
  //config.fb_location = CAMERA_FB_IN_PSRAM;

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK) {
    Serial.printf("Camera init failed with error 0x%x", err);
    return;
  }

  sensor_t * s = esp_camera_sensor_get();
  // initial sensors are flipped vertically and colors are a bit saturated
  s->set_vflip(s, 0); // flip it back
  s->set_hmirror(s, 1); // flip it back
  s->set_brightness(s, 1); // up the brightness just a bit
  s->set_saturation(s, 0); // lower the saturation
//  s->set_framesize(s,FRAMESIZE_FHD);
#ifdef HALOW
  Serial.println("Start WiFi HaLow");
  
#ifdef HT_RC3268
  //enable WiFiHalow LDO
  pinMode(HALOW_LDO_CTRL,OUTPUT);
  digitalWrite(HALOW_LDO_CTRL,HALOW_LDO_ENABLE);
#endif

  HaLow.init("US");
  HaLow.begin(halow_ssid, halow_password);
  Serial.print("MAC:");
  Serial.print(HaLow.macAddress());
  Serial.println();
  while (HaLow.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  Serial.println("");
  Serial.println("HaLow connected");

  startCameraServer();
  
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(HaLow.localIP());
  Serial.println("' to connect");

#elif defined(WIFI) //WIFI


  WiFi.mode(WIFI_STA);      // Enable Wi-Fi Station mode
  WiFi.begin(wifi_ssid, wifi_password);

  Serial.print("Connecting");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }
  Serial.println("Connected");

  configTzTime("IST-2IDT,M3.4.4/26,M10.5.0",
              "pool.ntp.org",
              "time.nist.gov");

  Serial.println("Waiting for NTP...");

  while (!getLocalTime(&timeinfo, 5000))
  {
      Serial.println("Retry...");
      delay(1000);
  }

  Serial.println("NTP OK");
  startCameraServer();
  
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(WiFi.localIP());
  Serial.println("' to connect");
#endif

  // Init SD
  sdmmcInit();
  Serial.println("SD initialized\n");

#ifdef DELETE_SD
  deleteAllFiles(SD, "/");
  Serial.println("SD deleted\n");
#endif  

  get_time();

  Serial.println("NTP Done\n");

#ifdef HALOW
    // to turn off halow ?
#elif defined(WIFI)

  if( WiFi.status() == WL_CONNECTED)
  {
   // WiFi.disconnect(true);
   // WiFi.mode(WIFI_OFF);
  }
#endif
}




void loop()
{
  delay(SNAPSHOT_TIMER);
  now = time(NULL);
  localtime_r(&now, &timeinfo);

  strftime(local_time_str,sizeof(local_time_str),"/%H-%M-%S__%d_%m_%Y",&timeinfo);
  sprintf(local_time_str,"%s_%d.jpg",local_time_str,millis());
  
#ifdef REC_SCHEDUALE_FILTER
  if (timeinfo.tm_hour >= START_HOUR &&
      timeinfo.tm_hour <= STOP_HOUR)
  {
    savePhotoToSD();
  }
#else
    savePhotoToSD ();
#endif
}