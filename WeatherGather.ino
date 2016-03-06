#include <Wire.h>
#include "BME280/BME280_MOD-1022.h"
#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

void setup() {
  Serial.begin(74880);
  Wire.begin(13, 14); // SDA=GPIO_13,SCL=GPIO_14
  WiFi.mode(WIFI_STA);

  Serial.println("Booting");
  BME280.readCompensationParams();           // read the NVM compensation parameters
  
  BME280.writeStandbyTime(tsb_0p5ms);        // tsb = 0.5ms
  BME280.writeFilterCoefficient(fc_16);      // IIR Filter coefficient 16

  BME280.writeOversamplingTemperature(os1x); // 1x over sampling
  BME280.writeOversamplingHumidity(os1x);    // 1x over sampling
  BME280.writeOversamplingPressure(os1x);    // 1x over sampling

  BME280.writeMode(smNormal);
  
  WiFi.begin("",""); 
  while (WiFi.waitForConnectResult() != WL_CONNECTED) {
    Serial.println("Attempting Wi-Fi cofiguration using WPS Push button...");
    WiFi.beginWPSConfig();
    delay(3000);
    if (WiFi.status() == WL_CONNECTED) {
      break;
    }
  }
  ArduinoOTA.onStart([]() {
    Serial.println("Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    if(0){
      Serial.print("Progress: ");
      Serial.print((progress / (total / 100)));
      Serial.println("%");
    }
    else{
      Serial.print(".");
    }
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();

  Serial.print("Connected to ");
  Serial.println(WiFi.SSID());
  Serial.print("My IP address: ");
  Serial.println(WiFi.localIP());
  Serial.print("My MAC address: ");
  Serial.println(WiFi.macAddress());
  Serial.println("Ready");
}

void loop() {
  ArduinoOTA.handle();
  while (BME280.isMeasuring()){
    Serial.println("Measuring...");
    delay(1000);
    ArduinoOTA.handle();
    }
  Serial.println("Done!");
  BME280.readMeasurements();                     // read out the data

  float temp=BME280.getTemperature();
  float humi=BME280.getHumidity();
  float preshigh=BME280.getPressureMoreAccurate();

  BME280.writeMode(smSleep);

  Serial.println(temp);
  Serial.println(humi);
  Serial.println(preshigh);
  
  ArduinoOTA.handle();
  WiFiClient client;
  const char* host = "WeatherGather.appspot.com";
  const int httpPort = 80;
  
  if (!client.connect(host,httpPort)){
    Serial.println("connection failed");
    return;
  }
 
  Serial.print("Connected to ");Serial.println(host);
  String url = "temperature=";url += String(temp,2);                 // Temp
  url += "&humidity=";url += String(humi,2);                 // Humidity
  url += "&pressure=";url += String(preshigh,2);                 // Pressure  
  
  client.print(String("POST /upload") + " HTTP/1.1\r\n"
  + "Host: " + host + "\r\n"
  + "X-WeatherGather-Device-ID: " + WiFi.macAddress() + "\r\n"
  + "User-Agent: ESP8266/1.0" +  "\r\n"
  + "Content-length: " + url.length() +"\r\n"
  + "Connection: close\r\n\r\n"
  + url);

  String line = client.readStringUntil('\r');
  int status = atoi(line.c_str()+9);
  Serial.println(line);
  Serial.print("Status = "); Serial.println(status);
  int sleeptime=60;
  do{
    String line = client.readStringUntil('\n');
    Serial.println(line);
    if(line.startsWith("Retry-After:")){
      sleeptime = line.substring(13).toInt();
    }
    ArduinoOTA.handle();
  } while(client.available());
  Serial.print("Sleep time is ");Serial.println(sleeptime);
    
  Serial.print("Sent! ");
  ArduinoOTA.handle();

  if(sleeptime > 200){
    Serial.println("Go to deepSleep!");
    ESP.deepSleep(sleeptime * 1000 * 1000 , WAKE_RF_DEFAULT);
    delay(1000); // zzzzz
  }
  Serial.println("Go to Sleep!");
  if(sleeptime < 20) sleeptime=20;
  for(int i=sleeptime;i>0;i--){
    if(i==20){
      Serial.println("Wakeup sensor!");
      BME280.writeMode(smNormal);
    }
    ArduinoOTA.handle();
    delay(1000);
  }
}
