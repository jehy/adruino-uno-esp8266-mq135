#include <doxygen.h>
#include <ESP8266AT.h>
#include <SoftwareSerial.h>

#include "mq135-data.h"
#include "MemoryFree.h"
#include "wifi-creds.h"

SoftwareSerial sSerial(3, 2); // RX, TX

ESP8266 wifi(sSerial, 9600);

void checkAlive()
{
  while (!wifi.kick())
  {
    Serial.println("esp not alive, restarting it");
    wifi.restart();
    delay(20000);
  }
}
void wifi_connect()
{
  bool joined = wifi.joinAP(WIFI_SSID, WIFI_PASS);
  while (!joined)  {
    Serial.print("Join AP failure\r\n");
    joined = wifi.joinAP(WIFI_SSID, WIFI_PASS);
  }
}

void setup()
{
  Serial.begin(9600);
  Serial.println("setup begin");

  checkAlive();

  Serial.print("FW Version: ");
  Serial.println(wifi.getVersion().c_str());



  if (wifi.setOprToStationSoftAP()) {
    Serial.println("mode to station + softap ok");
  } else {
    Serial.println("mode to station + softap err");
  }


  wifi_connect();
  Serial.println("Join AP success");
  Serial.print("IP: ");
  Serial.println(wifi.getLocalIP().c_str());

  Serial.println("WiFi status: " + wifi.getIPStatus());
  if (wifi.disableMUX()) {
    Serial.print("single ok\r\n");
  } else {
    Serial.print("single err\r\n");
  }
  Serial.print("setup end\r\n");
}

void loop()
{
  //  char macStr[20];
  int temp = 21;
  int humidity = 26;

  Serial.println("loop started");
  delay(1000);

  checkAlive();
  Serial.println("reading data");
  long valr = analogRead(A0);
  if (valr == 0)
  {
    Serial.println("Sensor returned 0, smth is not right. Skipping loop.");
    return;
  }

  long val =  ((float)22000 * (1023 - valr) / valr);
  long mq135_ro = mq135_getro(58378, 1050);//8000;//mq135_getro(val, 500);
  //convert to ppm (using default ro)
  float valAIQ = mq135_getppm(val, mq135_ro);

  float ppm_corrected = getCorrectedPPM(val, temp, humidity, mq135_ro);

  Serial.println("val raw = " + String(valr) + ",val = " + String(val) + ",ro = " + String(mq135_ro)
                 + " ppm = " + String(valAIQ) + " corrected ppm = " + String(ppm_corrected) + " free RAM: " + String(freeMemory()));
  if (valAIQ <= 0)
  {
    Serial.println("Function mq135_getppm returned non valid interval! Data will not be sent.");
    return;
  }
  Serial.println("Checking connection...");
  if (wifi.connected())
    Serial.println("Wifi connected");
  else
    Serial.println("Wifi not connected!");

  Serial.println("WiFi status: " + wifi.getIPStatus());
  wifi.setTCPServerTimeout(5000);
  if (wifi.createTCP(String("co2.jehy.ru"), 80)) {
    Serial.println("create tcp ok");
  } else {
    Serial.println("create tcp err");
    wifi.releaseTCP();
    return;
  }


  wifi.send((const uint8_t*)"GET /send.php?data={\"id\":1,\"val\":", strlen("GET /send.php?data={\"id\":1,\"val\":"));

  String mac;
  if (wifi.getMac(mac))
  {
    Serial.print("MAC: ");
    Serial.println(mac);
  }
  else
  {
    Serial.println("Could not get MAC");
  }
  mac = "";

  char *charBuf = new char[10];

  String(valr).toCharArray(charBuf, 10);
  wifi.send((const uint8_t*)charBuf, String(valr).length());
  wifi.send((const uint8_t*)",\"ppm\":", strlen(",\"ppm\":"));


  String((int)ppm_corrected).toCharArray(charBuf, 10);
  wifi.send((const uint8_t*)charBuf, String((int)ppm_corrected).length());
  wifi.send((const uint8_t*)",\"mac\":\"0\",\"SSID\":\"dunno\", \"FreeRAM\":\"", strlen(",\"mac\":\"0\",\"SSID\":\"dunno\", \"FreeRAM\":\""));

  int mem = freeMemory();
  String(mem).toCharArray(charBuf, 10);
  wifi.send((const uint8_t*)charBuf, String(mem).length());
  wifi.send((const uint8_t*)"\"} HTTP/1.1", strlen("\"} HTTP/1.1"));
  wifi.send((const uint8_t*)"\r\nHost: co2.jehy.ru\r\nConnection: close\r\n\r\n", strlen("\r\nHost: co2.jehy.ru\r\nConnection: close\r\n\r\n"));

  delete[] charBuf;

  uint8_t* buffer = new uint8_t[200];
  //uint8_t buffer[200] = {0};
  uint32_t len = wifi.recv(buffer, sizeof(buffer), 10000);
  if (len > 0) {
    Serial.print("Received:[");
    for (uint32_t i = 0; i < len; i++) {
      Serial.print((char)buffer[i]);
    }
    Serial.print("]\r\n");
  }
  delete[] buffer;

  if (wifi.releaseTCP()) {
    Serial.print("release tcp ok\r\n");
  } else {
    Serial.print("release tcp err\r\n");
  }
}
