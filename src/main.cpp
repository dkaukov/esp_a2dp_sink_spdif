#include "WiFi.h"
#include "debug.h"
#include "secrets.h"
#include <Arduino.h>
#include <ArduinoOTA.h>
#include <spdif.h>

#define CLIENT_ID_TEMPLATE "ATU-%06X"
#define CLIENT_ID_SIZE (sizeof(CLIENT_ID_TEMPLATE) + 5)

String deviceId;

void InitDeviceId() {
#if defined(DEVICE_ID)
  deviceId = DEVICE_ID;
#else
  char clientId[CLIENT_ID_SIZE];
  uint32_t nic = ESP.getEfuseMac() >> 24;
  uint32_t chipid = nic & 0xFF;
  chipid = chipid << 8;
  nic = nic >> 8;
  chipid = chipid | (nic & 0xFF);
  chipid = chipid << 8;
  nic = nic >> 8;
  chipid = chipid | (nic & 0xFF);
  snprintf(clientId, CLIENT_ID_SIZE, CLIENT_ID_TEMPLATE, chipid);
  deviceId = String(clientId);
#endif
}

const char *getDeviceId() {
  return deviceId.c_str();
}

void WiFiSTAConnect() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
#if (!defined(IP_CONFIGURATION_TYPE) || (IP_CONFIGURATION_TYPE == IP_CONFIGURATION_TYPE_DHCP))
  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE);
#elif (defined(IP_CONFIGURATION_TYPE) && (IP_CONFIGURATION_TYPE == IP_CONFIGURATION_TYPE_STATIC))
#define IP(name, value) IPAddress name(value);
  IP(local_ip, IP_CONFIGURATION_ADDRESS);
  IP(local_mask, IP_CONFIGURATION_MASK);
  IP(gw, IP_CONFIGURATION_GW);
  WiFi.config(local_ip, gw, local_mask);
#elif
#error "Incorrect IP_CONFIGURATION_TYPE."
#endif
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  WiFi.setHostname(getDeviceId());
  WiFi.setAutoReconnect(true);
  WiFi.setTxPower(WIFI_POWER_19_5dBm);
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  _LOGI("main", "Connected to AP successfully!");
}

void wiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  WiFiSTAConnect();
}

void wiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  _LOGI("main", "WiFi connected!");
  _LOGI("main", "IP address: %s", WiFi.localIP().toString().c_str());
  _LOGI("main", "IP hostname: %s", WiFi.getHostname());
}

void setupWiFi() {
  btStop();
  WiFiSTAConnect();
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(wiFiStationDisconnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.onEvent(wiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
#if defined(NTP_SERVER)
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, (char *)NTP_SERVER);
  sntp_init();
#endif
  if (WiFi.waitForConnectResult() != WL_CONNECTED) {
    _LOGE("main", "Wifi connection failed!");
  }
}

uint16_t data[32];

void setup() {
  InitDeviceId();
  pinMode(BUILTIN_LED, OUTPUT);
  Serial.begin(115200);
  digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
  setupWiFi();
  digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
  debugInit();
  ArduinoOTA.setMdnsEnabled(true);
  ArduinoOTA.setHostname(getDeviceId());
  ArduinoOTA.begin();

  spdif_init(44100); // initailize S/PDIF driver
}

void loop() {
  spdif_write(&data, sizeof(data));
  debugLoop();
  ArduinoOTA.handle();
  digitalWrite(BUILTIN_LED, !digitalRead(BUILTIN_LED));
}