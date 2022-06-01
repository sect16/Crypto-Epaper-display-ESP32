#ifndef BOARD_HAS_PSRAM
#error "Please enable PSRAM !!!"
#endif


#include <WiFi.h>
#include <HTTPClient.h>
#include <WifiClientSecure.h>
#include <ArduinoJson.h>
#include "cryptos.h"
#include "coingecko-api.h"
#include <Arduino.h>
#include <esp_task_wdt.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "epd_driver.h"
#include "firasans.h"
#include "esp_adc_cal.h"
#include <Wire.h>
#include <SPI.h>
#include <SD.h>
#include <hal/rmt_ll.h>


#define BATT_PIN            36
#define SD_MISO             12
#define SD_MOSI             13
#define SD_SCLK             14
#define SD_CS               15

int cursor_x ;
int cursor_y ;

uint8_t *framebuffer;
int vref = 1100;

String date;

// ----------------------------
// Configurations - Update these
// ----------------------------

const char *ssid = "YOUR_SSID";
const char *password = "YOUR_PASSWORD";
const boolean staticIp = 1;

IPAddress local_IP(192, 168, 31, 115);
IPAddress gateway(192, 168, 31, 254);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(192, 168, 31, 254); //optional
//IPAddress secondaryDNS(8, 8, 4, 4); //optional

// ----------------------------
// End of area you need to change
// ----------------------------

void setup()
{
  char buf[128];

  Serial.begin(115200);
  // Set WiFi to station mode and disconnect from an AP if it was previously connected
  WiFi.mode(WIFI_STA);
  WiFi.disconnect();
  delay(1000);

  Serial.println("Setup done");

  connectToWifi();

  // Correct the ADC reference voltage
  esp_adc_cal_characteristics_t adc_chars;
  esp_adc_cal_value_t val_type = esp_adc_cal_characterize(ADC_UNIT_1, ADC_ATTEN_DB_11, ADC_WIDTH_BIT_12, 1100, &adc_chars);
  if (val_type == ESP_ADC_CAL_VAL_EFUSE_VREF) {
    Serial.printf("eFuse Vref:%u mV", adc_chars.vref);
    Serial.println("");
    vref = adc_chars.vref;
  }

  epd_init();

  framebuffer = (uint8_t *)ps_calloc(sizeof(uint8_t), EPD_WIDTH * EPD_HEIGHT / 2);
  if (!framebuffer) {
    Serial.println("alloc memory failed !!!");
    while (1);
  }
  memset(framebuffer, 0xFF, EPD_WIDTH * EPD_HEIGHT / 2);

  epd_poweron();
  epd_clear();
  epd_poweroff();
  epd_poweron();

}

void loop()
{
  downloadBaseData("usd");
  delay(1000);
  downloadBtcAndEthPrice();
  title();
  for (int i = 0; i < cryptosCount; i++)
  {
    cursor_y = (50 * (i + 2));
    renderCryptoCard(cryptos[i]);
  }
  footer();
  delay(5000);
}

void footer()
{

  //Serial.println(EPD_HEIGHT);
  //Serial.println(EPD_WIDTH);
  date = date.substring(0,26);
  date = "Last update: " + date;
  Serial.println(date);
  char ts[date.length()];
  date.toCharArray(ts, date.length());
  Serial.println("Date length = " + String(date.length()));
  String str = String(ts[30]) + String(ts[31]);
  Serial.println("Old hour = " + str);
  int hh = str.toInt() + 8;
  Serial.println("New hour = " + String(hh));
  char strnew[1];
  sprintf(strnew, "%02d", hh);
  Serial.println("First digit = " + String(strnew[0]));
  Serial.println("Second digit = " + String(strnew[1]));

  if (hh > 23) {
    hh = hh - 24;
  }
  sprintf(strnew, "%02d", hh);
  ts[30] = strnew[0];
  ts[31] = strnew[1];

  Rect_t area = {
    .x = 1 ,
    .y = 520 ,
    .width = 370,
    .height = 18,
  };
  
  for (int i = 0; i < 2; i++)
  {
    epd_clear_area(area);
    cursor_y = 520 + 15;
    cursor_x = 20;
    writeln((GFXfont *)&OpenSans8B, ts, &cursor_x, &cursor_y, NULL);
  }
}


void title()
{
  cursor_y = 50;

  cursor_x = 20;
  char *sym = "Symbol";
  writeln((GFXfont *)&FiraSans, sym, &cursor_x, &cursor_y, NULL);

  cursor_x = 290;
  char *prc = "Price";
  writeln((GFXfont *)&FiraSans, prc, &cursor_x, &cursor_y, NULL);

  cursor_x = 520;
  char *da = "Day(%)";
  writeln((GFXfont *)&FiraSans, da, &cursor_x, &cursor_y, NULL);

  cursor_x = 790;
  char *we = "Week(%)";
  writeln((GFXfont *)&FiraSans, we, &cursor_x, &cursor_y, NULL);

}

void renderCryptoCard(Crypto crypto)
{
  Serial.print("Crypto Name  - "); Serial.println(crypto.symbol);

  cursor_x = 50;

  char *string1 = &crypto.symbol[0];

  writeln((GFXfont *)&FiraSans, string1, &cursor_x, &cursor_y, NULL);

  cursor_x = 220;

  Serial.print("price usd (double) - ");
  Serial.println(crypto.price.usd, 10);
  String Str;
  if (crypto.price.usd < 0.00001) {
    Str = String(crypto.price.usd, 10);
  }
  else if (crypto.price.usd < 1) {
    Str = String(crypto.price.usd, 5);
  }
  else {
    Str = (String)(crypto.price.usd);
  }
  //String Str = (String)(crypto.price.usd);
  char* string2 = &Str[0];

  //Serial.print("price usd - "); Serial.println(Str);

  Rect_t area = {
    .x = cursor_x,
    .y = cursor_y - 40,
    .width = 300,
    .height = 50,
  };

  epd_clear_area(area);

  writeln((GFXfont *)&FiraSans, string2, &cursor_x, &cursor_y, NULL);

  Serial.print("Day change - "); Serial.println(formatPercentageChange(crypto.dayChange));

  cursor_x = 530;

  Rect_t area1 = {
    .x = cursor_x,
    .y = cursor_y - 40,
    .width = 150,
    .height = 50,
  };

  epd_clear_area(area1);
  Str = (String)(crypto.dayChange);
  char* string3 = &Str[0];

  writeln((GFXfont *)&FiraSans, string3, &cursor_x, &cursor_y, NULL);

  Serial.print("Week change - "); Serial.println(formatPercentageChange(crypto.weekChange));

  cursor_x = 800;

  Rect_t area2 = {
    .x = cursor_x,
    .y = cursor_y - 40,
    .width = 150,
    .height = 50,
  };

  epd_clear_area(area2);

  Str = (String)(crypto.weekChange);
  char* string4 = &Str[0];

  writeln((GFXfont *)&FiraSans, string4, &cursor_x, &cursor_y, NULL);

}


void connectToWifi()
{

  Serial.println("scan start");

  // WiFi.scanNetworks will return the number of networks found
  int n = WiFi.scanNetworks();
  Serial.println("scan done");
  if (n == 0) {
    Serial.println("no networks found");
  } else {
    Serial.print(n);
    Serial.println(" networks found");
    for (int i = 0; i < n; ++i) {
      // Print SSID and RSSI for each network found
      Serial.print(i + 1);
      Serial.print(": ");
      Serial.print(WiFi.SSID(i));
      Serial.print(" (");
      Serial.print(WiFi.RSSI(i));
      Serial.print(")");
      Serial.println((WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " " : "*");
      delay(10);
    }
  }
  Serial.println("");

  // Wait a bit before scanning again
  delay(1000);

  if (staticIp && !WiFi.config(local_IP, gateway, subnet, primaryDNS)) {
    Serial.println("STA Failed to configure");
  }

  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED)
  {
    Serial.print("Connecting to WiFi: "); Serial.println(ssid);
    delay(300);
  }

  Serial.println("Connected!!!_______________");
  Serial.println("");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());

  Serial.println("");
  Serial.println("WiFi connected!");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("ESP Mac Address: ");
  Serial.println(WiFi.macAddress());
  Serial.print("Subnet Mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("Gateway IP: ");
  Serial.println(WiFi.gatewayIP());
  Serial.print("DNS: ");
  Serial.println(WiFi.dnsIP());

}


String formatPercentageChange(double change)
{

  double absChange = change;

  if (change < 0)
  {
    absChange = -change;
  }

  if (absChange > 100) {
    return String(absChange, 0) + "%";
  } else if (absChange >= 10) {
    return String(absChange, 1) + "%";
  } else {
    return String(absChange) + "%";
  }
}
