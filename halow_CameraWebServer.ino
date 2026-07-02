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
#include <Adafruit_NeoPixel.h>

// ===========================
// Enter your Halow credentials
// ===========================
#include <WiFi.h>


const char* wifi_ssid = "OnePlus 5" ;//"ORBI79";
const char* wifi_password = "12345678";//"ASD@123asd";//
char local_time_str[64];
time_t now = time(NULL);
struct tm timeinfo;

RTC_DATA_ATTR long wifi_active_cycles = 0;
RTC_DATA_ATTR bool ntpDone = false;

RTC_DATA_ATTR bool enableNetwork = false;
RTC_DATA_ATTR camera_config_t config;
RTC_DATA_ATTR bool config_ready = false;
RTC_DATA_ATTR bool camera_alive = false;

//#define HALOW //if not defined  than WIFI
#define WIFI
#define RGB_PIN 19
#define NUMPIXELS       1   // 1 on-board WS2812
// Define the color choices
enum PixelColor {
  COLOR_OFF,
  COLOR_RED,
  COLOR_GREEN,
  COLOR_BLUE,
  COLOR_WHITE,
  COLOR_ORANGE,
  COLOR_YELLOW,
  COLOR_CYAN,
  COLOR_MAGENTA,
  COLOR_PURPLE,
  COLOR_PINK,
  COLOR_TURQUOISE
};

enum PixelBrightness {
  BRIGHTNESS_LOW=1,
  BRIGHTNESS_NORMAL,
  BRIGHTNESS_HIGH
};
// 
// THIS IS THE MISSING LINE:
Adafruit_NeoPixel pixel(NUMPIXELS, RGB_PIN, NEO_GRB + NEO_KHZ800);

#define REC_SCHEDUALE_FILTER
//#define DELETE_SD
#ifdef REC_SCHEDUALE_FILTER

#define  WAKING_HOUR  6  
#define  SLEEPING_HOUR  22  
//#define  START_MINUTE  42
//#define  STOP_MINUTE   42
#endif
#define WIFI_ACTIVE_CYCLES_NUM 2
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

void pixel_led_change(PixelBrightness level, PixelColor color)
{
  uint8_t base_r = 0, base_g = 0, base_b = 0;

  // 1. Get the full-intensity base color values (0-255)
  switch (color) {
    case COLOR_RED:
      base_r = 255; base_g = 0; base_b = 0;
      break;
    case COLOR_GREEN:
      base_r = 0; base_g = 255; base_b = 0;
      break;
    case COLOR_BLUE:
      base_r = 0; base_g = 0; base_b = 255;
      break;
    case COLOR_WHITE:
      base_r = 255; base_g = 255; base_b = 255;
      break;
    case COLOR_ORANGE:
      base_r = 255; base_g = 100; base_b = 0;
      break;
    case COLOR_YELLOW:
      base_r = 255; base_g = 80; base_b = 0;
      break;
    case COLOR_CYAN:
      base_r = 0; base_g = 255; base_b = 255;
      break;
    case COLOR_MAGENTA:
      base_r = 255; base_g = 0; base_b = 255;
      break;
    case COLOR_PURPLE:
      base_r = 128; base_g = 0; base_b = 128;
      break;
    case COLOR_PINK:
      base_r = 255; base_g = 105; base_b = 180;
      break;
    case COLOR_TURQUOISE:
      base_r = 64; base_g = 224; base_b = 208;
      break;
    case COLOR_OFF:
    default:
      base_r = 0; base_g = 0; base_b = 0;
      break;
  }

  // 2. Map the PixelBrightness enum to actual numeric PWM limits
  int target_brightness = 0;
  switch (level) {
    case BRIGHTNESS_LOW:
      target_brightness = 15;   // Very dim, perfect for battery saving
      break;
    case BRIGHTNESS_HIGH:
      target_brightness = 255;  // Quite bright, highly visible
      break;
    case BRIGHTNESS_NORMAL:
    default:
      target_brightness = 50;   // Your sweet-spot standard brightness
      break;
  }

  // 3. Scale each channel dynamically based on the mapped brightness value
  uint8_t r = (base_r * target_brightness) / 255;
  uint8_t g = (base_g * target_brightness) / 255;
  uint8_t b = (base_b * target_brightness) / 255;

  // 4. Update the pixel
  pixel.setPixelColor(0, r, g, b); 
  pixel.show();
}
  
void setup() 
{
  setCpuFrequencyMhz(80);  
  esp_sleep_wakeup_cause_t wakeup_reason;
  Serial.begin(115200);

  mmwlan_shutdown();
  // Turn LED off
  pixel.begin();            // Initialize the NeoPixel strip/LED
  pixel.setBrightness(50);  // Set brightness (0 to 255) to protect your eyes

  wakeup_reason = esp_sleep_get_wakeup_cause();
  //Serial.printf("Wake reason: %d\n", wakeup_reason);
  Serial.printf("wifi_active_cycles = %d\n", wifi_active_cycles);
  //Serial.printf("ntpDone = %d\n", ntpDone);
  bool isWakeFromSleep = (wakeup_reason == ESP_SLEEP_WAKEUP_TIMER);
  bool coldBoot = !isWakeFromSleep;

  if (coldBoot)
  {
    pixel_led_change(BRIGHTNESS_HIGH,COLOR_RED);
  }
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
  else
  {
      Serial.println("CYCLE >=  WiFi OFF MODE");

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
          pixel_led_change(BRIGHTNESS_LOW,COLOR_OFF);

          delay(500);
          Serial.print(".");
          pixel_led_change(BRIGHTNESS_HIGH,COLOR_RED);

        }
        else
        {
          Serial.println("\nWiFi Connected");
          Serial.println(WiFi.localIP());

          // NTP only once
          if (!ntpDone)
          {
             Serial.println("Waiting for NTP...");

              while (!getLocalTime(&timeinfo, 10000))
              {
                  pixel_led_change(BRIGHTNESS_HIGH,COLOR_YELLOW);

                  Serial.println("NTP FAILED");
                  delay(200);
                  pixel_led_change(BRIGHTNESS_HIGH,COLOR_RED);
                  delay(200);
              }

              pixel_led_change(BRIGHTNESS_HIGH,COLOR_YELLOW);
              delay(2000);
              //NTP succeed
              {
                strftime(local_time_str,
                      sizeof(local_time_str),
                      "%H-%M-%S__%d_%m_%Y",
                      &timeinfo);

                Serial.print("TIME=");
                Serial.println(local_time_str);
                Serial.println("NTP OK");
                pixel_led_change(BRIGHTNESS_HIGH,COLOR_GREEN);
                delay(2000);


                ntpDone = true;
              }
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
  if (ntpDone)
  {
      wifi_active_cycles++;
  }
}


void deep_delay()
{
    time_t now;
    struct tm timeinfo;

    time(&now);
    localtime_r(&now, &timeinfo);

    uint64_t sleep_us;

    // Night sleep: 23:00 -> 06:00
    if (timeinfo.tm_hour >= SLEEPING_HOUR || timeinfo.tm_hour < WAKING_HOUR)
    {
        struct tm wakeTime = timeinfo;

        wakeTime.tm_hour = WAKING_HOUR;
        wakeTime.tm_min  = 0;
        wakeTime.tm_sec  = 0;

        // If after 23:00, wake tomorrow morning
        if (timeinfo.tm_hour >= SLEEPING_HOUR)
            wakeTime.tm_mday++;

        time_t wakeEpoch = mktime(&wakeTime);

        sleep_us = (uint64_t)(wakeEpoch - now) * 1000000ULL;

        Serial.printf("Night sleep for %.1f hours\n",
                      (wakeEpoch - now) / 3600.0);
    }
    else
    {
        // Normal snapshot interval
        sleep_us = SNAPSHOT_TIMER * 1000ULL;
    }
   
    esp_sleep_enable_timer_wakeup(sleep_us);
    esp_deep_sleep_start();
}

void loop()
{
  pixel_led_change(BRIGHTNESS_LOW,COLOR_BLUE);
  if(ntpDone)
  {
    now = time(NULL);
    localtime_r(&now, &timeinfo);

    strftime(local_time_str,sizeof(local_time_str),"/%H-%M-%S__%d_%m_%Y.jpg",&timeinfo);
    sprintf(local_time_str,"%s.jpg",local_time_str);
    
    

    #ifdef REC_SCHEDUALE_FILTER
      if (timeinfo.tm_hour >= WAKING_HOUR &&
          timeinfo.tm_hour <= SLEEPING_HOUR)
      {
        savePhotoToSD();
      }
    #else
        savePhotoToSD ();
    #endif
  }

  if (WiFi.status() != WL_CONNECTED)
  {
    pixel_led_change(BRIGHTNESS_LOW,COLOR_OFF);
    deep_delay();
  }
}
