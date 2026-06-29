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
#include "esp_wifi.h"
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
const char* wifi_password = "12345678";//"ASD@123asd";//
char local_time_str[64];
time_t now = time(NULL);
struct tm timeinfo;

RTC_DATA_ATTR uint8_t wifi_active_cycles = 0;
RTC_DATA_ATTR bool ntpDone = false;

RTC_DATA_ATTR bool enableNetwork = false;
RTC_DATA_ATTR camera_config_t config;
RTC_DATA_ATTR bool config_ready = false;
RTC_DATA_ATTR bool camera_alive = false;

//#define HALOW //if not defined  than WIFI
#define WIFI
#define LED_PIN 19
#define REC_SCHEDUALE_FILTER
#define DELETE_SD
#ifdef REC_SCHEDUALE_FILTER
#define  START_HOUR 6    // 06:00
#define  STOP_HOUR  22  // 20:00
//#define  START_MINUTE  42
//#define  STOP_MINUTE   42
#endif
#define WIFI_ACTIVE_CYCLES_NUM 10
#define SNAPSHOT_TIMER 100 // in milli seconds

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

static const camera_config_t camera_config = {
    .pin_pwdn      = PWDN_GPIO_NUM,
    .pin_reset     = RESET_GPIO_NUM,
    .pin_xclk      = XCLK_GPIO_NUM,
    .pin_sccb_sda  = SIOD_GPIO_NUM,
    .pin_sccb_scl  = SIOC_GPIO_NUM,

    .pin_d7        = Y9_GPIO_NUM,
    .pin_d6        = Y8_GPIO_NUM,
    .pin_d5        = Y7_GPIO_NUM,
    .pin_d4        = Y6_GPIO_NUM,
    .pin_d3        = Y5_GPIO_NUM,
    .pin_d2        = Y4_GPIO_NUM,
    .pin_d1        = Y3_GPIO_NUM,
    .pin_d0        = Y2_GPIO_NUM,

    .pin_vsync     = VSYNC_GPIO_NUM,
    .pin_href      = HREF_GPIO_NUM,
    .pin_pclk      = PCLK_GPIO_NUM,

    .xclk_freq_hz  = 40000000,

    .ledc_timer    = LEDC_TIMER_0,
    .ledc_channel  = LEDC_CHANNEL_0,

    .pixel_format  = PIXFORMAT_JPEG,
    .frame_size    = FRAMESIZE_QSXGA,

    .jpeg_quality  = 10,
    .fb_count      = 2,
    .fb_location   = CAMERA_FB_IN_PSRAM,
    .grab_mode     = CAMERA_GRAB_LATEST,

    .sccb_i2c_port = -1
};
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

void setup() 
{
  setCpuFrequencyMhz(80);  
  esp_sleep_wakeup_cause_t wakeup_reason;
  Serial.begin(115200);
  //pinMode(LED_PIN, OUTPUT);

  // Turn LED off
 
  //  digitalWrite(LED_PIN, HIGH);
 
  wakeup_reason = esp_sleep_get_wakeup_cause();
  //Serial.printf("Wake reason: %d\n", wakeup_reason);
  Serial.printf("wifi_active_cycles = %d\n", wifi_active_cycles);
  //Serial.printf("ntpDone = %d\n", ntpDone);
  bool isWakeFromSleep = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
  bool coldBoot = !isWakeFromSleep;

  // Init SD
  sdmmcInit();
  //Serial.println("SD initialized\n");

  esp_err_t err = esp_camera_init(&camera_config);
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
  startCameraServer();

  
  Serial.print("Camera Ready! Use 'http://");
  Serial.print(HaLow.localIP());
  Serial.println("' to connect");

#elif defined(WIFI) //WIFI

  configTzTime("IST-2IDT,M3.4.4/26,M10.5.0",
              "pool.ntp.org",
              "time.nist.gov");

  //IPAddress ip(10,232,76,67);
  //IPAddress gateway(10,232,76,237);
  //IPAddress subnet(255,255,255,0);

  // ALWAYS FORCE FIRST BOOT OR RESET CASE
  if (wifi_active_cycles == 0)
  {
      Serial.println("BOOT 0 → WiFi + NTP + Server");
      enableNetwork = true;

#ifdef DELETE_SD
      deleteAllFiles(SD, "/");
      //Serial.println("SD deleted\n");
#endif  
  }
  else if (wifi_active_cycles < WIFI_ACTIVE_CYCLES_NUM)
  {
      Serial.printf("CYCLE %d → WiFi ACTIVE\n", wifi_active_cycles);
      enableNetwork = true;
  }
  else
  {
      Serial.println("CYCLE >= WIFI_ACTIVE_CYCLES_NUM → WiFi OFF MODE");

      enableNetwork = false;

      WiFi.disconnect(true, true);
      WiFi.mode(WIFI_OFF);
      esp_wifi_stop();
  }
  // -------------------- WIFI + NTP --------------------
  if (enableNetwork)
  {
      WiFi.mode(WIFI_STA);
      WiFi.begin(wifi_ssid, wifi_password);
      Serial.print("Connecting");
      for (int j=0;j<=10;j++) 
      {
        if (WiFi.status() != WL_CONNECTED)
        {
          delay(500);
          Serial.print(".");
        }
        else
        {
          Serial.println("\nWiFi Connected");
          Serial.println(WiFi.localIP());

          // NTP only once
          if (!ntpDone)
          {
              Serial.println("Waiting for NTP...");

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
              Serial.println("NTP OK");

              ntpDone = true;
            }
          break;
        }
      } 
      // START SERVER ONLY WHEN NETWORK ENABLED
      if (enableNetwork && ntpDone)
      {
          startCameraServer();
          
          Serial.print("Camera Ready! Use 'http://");
          Serial.print(WiFi.localIP());
          Serial.println("' to connect");
      }
  }
  

#endif

#ifdef HALOW
    // to turn off halow ?
#elif defined(WIFI)

  if( WiFi.status() == WL_CONNECTED)
  {
   // WiFi.disconnect(true);
   // WiFi.mode(WIFI_OFF);
  }
#endif
  if (isWakeFromSleep && ntpDone)
  {
      wifi_active_cycles++;
  }
}


void deep_delay()
{
  esp_sleep_enable_timer_wakeup(SNAPSHOT_TIMER * 1000ULL);
  esp_deep_sleep_start();
}

void loop()
{
  if(ntpDone)
  {
    now = time(NULL);
    localtime_r(&now, &timeinfo);

    strftime(local_time_str,sizeof(local_time_str),"/%H-%M-%S__%d_%m_%Y.jpg",&timeinfo);
   // sprintf(local_time_str,"%s.jpg",local_time_str);
    
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

  if (WiFi.status() != WL_CONNECTED)
  {
    deep_delay();
  }

}