#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
#include <ESP8266mDNS.h>

#include <ArduinoJson.h>
// AWS IoT
#include <ESP8266AWSIoTMQTTWS.h>
// Sensor Librarys
#include <Wire.h>
#include "SH1106Wire.h"
#include "SH1106.h"
#include "DHTesp.h"

#define EEPROM_LENGTH 1024
// Display and DHT11
SH1106 display(0x3c, D3, D5);
DHTesp dht;

//const char *ssid = "EH717";//"U+Net9618";
//const char *password = "0318041176600";//"1157001488";
char ssid[30];
char password[30];

char eRead[30];
byte len;

char device_id[20];
char led_topic[50];
int loop_count = 0;
bool captive = true;

// See `src/aws_iot_config.h` for formatting
char *region = (char *) "ap-northeast-2";
char *endpoint = (char *) "a1nac6uq3iham1-ats";
char *mqttHost = (char *) "a1nac6uq3iham1-ats.iot.ap-northeast-2.amazonaws.com";
int mqttPort = 443;
char *iamKeyId = (char *) "AKIAJOHWOO5AAOXZLYBQ";
char *iamSecretKey = (char *) "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";
const char* aws_topic  = "$aws/things/ESP8266/shadow/update";
 
ESP8266DateTimeProvider dtp;
AwsIotSigv4 sigv4(&dtp, region, endpoint, mqttHost, mqttPort, iamKeyId, iamSecretKey);
AWSConnectionParams cp(sigv4);
AWSWebSocketClientAdapter adapter(cp);
AWSMqttClient client(adapter, cp);


WiFiClient espClient;
const byte DNS_PORT = 53;
IPAddress apIP(192, 168, 1, 1);
DNSServer dnsServer;
ESP8266WebServer webServer(80);

String responseHTML = ""
    "<!DOCTYPE html><html><head><title>IoT Flowerpot Setting Page</title></head><body><center>"
    "<p>IoT Flowerpot Setting Page</p>"
    "<form action='/button'>"
    "<p><input type='text' name='ssid' placeholder='SSID' onblur='this.value=removeSpaces(this.value);'></p>"
    "<p><input type='text' name='password' placeholder='WLAN Password'></p>"
    "<p><input type='submit' value='Submit'></p></form>"
    "<p>This is IoT Flowerpot Setting Page</p></center></body>"
    "<script>function removeSpaces(string) {"
    "   return string.split(' ').join('');"
    "}</script></html>";


// DIsplay Function
void drawTempHumi(String temperature, String humidity, String dirt) 
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(2, 1, "- Temperature");
  display.drawString(30, 11, temperature + " oC");
  display.drawString(2, 22, "- Humidity");
  display.drawString(30, 32, humidity + " %");
  display.drawString(2, 43, "- Dirt Humidity");
  display.drawString(30, 53, dirt + " %");
  display.display();
}

void drawSubInfo(String topic, String msg) 
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_16);
  display.drawString(2, 0, "- Topic");
  display.setFont(ArialMT_Plain_10);
  display.drawString(15, 16, topic);
  display.setFont(ArialMT_Plain_16);
  display.drawString(2, 25, "- Message");
  display.drawString(15, 41, msg);
  display.display();
}

void drawCaptiveMode() 
{
  display.clear();
  display.setTextAlignment(TEXT_ALIGN_LEFT);
  display.setFont(ArialMT_Plain_10);
  display.drawString(2, 1, "1. Connect Wi-Fi");
  display.drawString(17, 11, "SSID : " + String(device_id));
  display.drawString(2, 22, "2. Enter this URL");
  display.drawString(17, 32, "http://192.168.1.1");
  display.drawString(2, 43, "3. Input Your Wi-Fi Info");
  display.drawString(17, 53, "[SSID], [Passwd]");
  display.display();
}


void drawProgressBar(int progress) 
{
  int temp = progress;
  if(temp >= 100)
    temp = 100;
  display.clear();
  display.drawProgressBar(0, 32, 120, 10, temp);
  display.setTextAlignment(TEXT_ALIGN_CENTER);
  display.drawString(64, 15, String(temp) + "%");
  display.drawString(64, 49, "Connecting : " + String(ssid));
  display.display();
}

void displayInit()
{
  display.init();
  display.flipScreenVertically();
  display.setFont(ArialMT_Plain_10);
//  drawProgressBar();
}


void setup() {
  sprintf(device_id, "ESP_%08X", ESP.getChipId());
  sprintf(led_topic, "ESP8266/%s/LED", device_id);
  Serial.begin(115200);
  // EEPROM Init
  EEPROM.begin(EEPROM_LENGTH);
  // Flash button -> Init button
  pinMode(D1, INPUT_PULLUP);
//  digitalWrite(D1, HIGH);
  attachInterrupt(D1, initDevice, FALLING);
  // LED Init
  pinMode(D8, OUTPUT);
  // DHT11 Init
  dht.setup(2);
  while(!Serial) {
      yield();
  }
  displayInit();
  ReadString(0, 30);
  if (!strcmp(eRead, "")) // EEPROM에 아무것도 없는 경우
  {
    setup_captive();
    drawCaptiveMode();
  }
  else // EEPROM에 데이터가 있는 경우
  {
    captive = false;
    strcpy(ssid, eRead);
    ReadString(30, 30);
    strcpy(password, eRead);
    ReadString(60, 30);

    WiFi.mode(WIFI_STA);
    WiFi.begin(ssid, password);
    int prog_per = 0;
    while (WiFi.status() != WL_CONNECTED) 
    {
      drawProgressBar(prog_per);
      delay(25);
      prog_per++;
      Serial.print(".");
      if(prog_per > 1500)
        initDevice();
    }
    drawProgressBar(100);
    if (MDNS.begin(device_id))
      Serial.println("MDNS responder started");
    webServer.onNotFound(handleNotFound);
    webServer.begin();
    
    int res = client.connect();
    Serial.printf("mqtt connect=%d\n", res);
    
    if (res == 0) {

      Serial.println(led_topic);
      client.subscribe(led_topic, 1,[](const char* topic, const char* msg)
        { 
          Serial.println("Subscribe!");
          if(strcmp(msg, "LED_ON") == 0)
          {
            digitalWrite(D8, HIGH);
            drawSubInfo(String(topic), String(msg));
            delay(3000);
            digitalWrite(D8, LOW);
          }
          else
            digitalWrite(D8, LOW);
          


        }

      );
    }
  }
}

void loop() {

  if(!captive) // Not Captive
  {
    String humidity = String(dht.getHumidity());
    String temperature = String(dht.getTemperature());
    String dirt = String( ((1024 - analogRead(A0)) * 100) / 1024 );
    drawTempHumi(temperature, humidity, dirt);
    if(loop_count >= 15)
    {
      if (client.isConnected()) 
      {
        String shadow = "{\"state\": {\"reported\": {\"foo\" : \"bar\"}}, \"device_id\": \"" + String(device_id) + "\", \"temperature\" : " + temperature + ", \"humidity\" : " + humidity + ", \"dirt\" : " + dirt + "}";
        client.publish(aws_topic, shadow.c_str(), 0, false);
        client.yield();
      }
      else 
      {
        Serial.println("Not connected...");
        delay(2000);
      }
      loop_count = 0;
    }
    delay(2000);
    loop_count++;
  }
  else // Captive
  {
    dnsServer.processNextRequest();
    webServer.handleClient();
//    Serial.println("captive!");
    yield;
  }
}

void setup_captive() {    
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, IPAddress(255, 255, 255, 0));
  WiFi.softAP(device_id);
  
  dnsServer.start(DNS_PORT, "*", apIP);

  webServer.on("/button", button);

  webServer.onNotFound([]() {
    webServer.send(200, "text/html", responseHTML);
  });
  webServer.begin();
  Serial.println("Captive Portal Started");
}

void button()
{
  Serial.println("button pressed");
  Serial.println(webServer.arg("ssid"));
  SaveString( 0, (webServer.arg("ssid")).c_str());
  SaveString(30, (webServer.arg("password")).c_str());
  webServer.send(200, "text/plain", "OK");
  ESP.restart();
}

void SaveString(int startAt, const char* id) 
{ 
  for (byte i = 0; i <= strlen(id); i++)
    EEPROM.write(i + startAt, (uint8_t) id[i]);
  EEPROM.commit();
}

void ReadString(byte startAt, byte bufor) 
{
  for (byte i = 0; i <= bufor; i++) {
    eRead[i] = (char)EEPROM.read(i + startAt);
  }
  len = bufor;
}

void handleNotFound()
{
  String message = "File Not Found\n";
  webServer.send(404, "text/plain", message);
}

void initDevice() {
  Serial.println("Flushing EEPROM....");
  SaveString(0, "");
  ESP.restart();
}

