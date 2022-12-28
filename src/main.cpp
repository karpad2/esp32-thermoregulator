#include <Arduino.h>
#include <SPI.h>
#include <math.h>
#include "thermistor.h"
#include <Adafruit_SSD1306.h>
#include <Wire.h>

#include <String.h>
#include <SPIFFS.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <WiFiClient.h>
#include <WiFi.h>

#define FILESYSTEM SPIFFS
#define FORMAT_FILESYSTEM false
#define FREQ 50
#define RESOLUTION 8

#define SCREEN_WIDTH 128
#define SCREEN_HEIGHT 64
#define OLED_RESET   -1
#define relayPin 16
#define thermistorPin 36


const char *ssid = "ESP";  // Enter SSID here
const char *password = "12345678";  //Enter Password here
const char *host = "ESP";
const char *ok = "[OK]";


IPAddress local_ip_server(192, 168, 1, 1);
IPAddress local_ip_camera(192, 168, 1, 200);
IPAddress gateway(192, 168, 1, 1);
IPAddress subnet(255, 255, 255, 0);



int histerisis=4, set_temp=0;
String a="";
volatile bool computeNow = false;
boolean relayStatus = false;

File fsUploadFile;
WebServer server(80);
String reverse_text;
String s_histerisis = "4";
String s_settemp = "0";
String json = "";
String dp="";
long long pre=0;

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);
void wifi_setup();
void website_setup();
void check_fs();

void setup() {
  Serial.begin(115200);
  Wire.begin(5, 4);
  pinMode(relayPin, OUTPUT);
  pinMode(LED_BUILTIN, OUTPUT);

  
    Serial.print("\n Filesystem:");
    check_fs();
    //WIFI INIT
    Serial.println(ok);
    website_setup();
    Serial.print("Wifi:");
    wifi_setup();
    Serial.println(ok);
    Serial.print("Server:");
    server.begin();
    Serial.println(ok);
    Serial.println("System started!");

  if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C, false, false)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;);
  }
  delay(1000); // Pause for 2 seconds

 display.clearDisplay();

  Serial.println(F("Start display><"));
  thermistor_setup();
  // put your setup code here, to run once:
}

void loop() {

  server.handleClient();

  Serial.println(F("Start loop"));
  display.clearDisplay();
  long int value=0;
  calculate_temperature(analogRead(thermistorPin));
  display.setTextSize(4);
  //display.setTextSize(1);
  display.setTextColor(WHITE);
  if(pre==0)
  {
    pre=millis();
  }
  if(millis()-pre>1000)
  {
    pre=millis();
    int tmp=floor(Tc);
    dp="";
    dp+=String((int)floor(Tc));
    dp+="C";
    Serial.println(Tc);
  }
  
  
  
  
  
  int set_temp=s_settemp.toInt();
  int histerisis=s_histerisis.toInt();

    display.setCursor(0, 28);
    display.println(dp.c_str());
    Serial.print("Setted_temp: ");
    Serial.println(set_temp);
    if(Tc<0)
    {
        display.println("Error");
        digitalWrite(relayPin,LOW);
        display.display();
        return;
    }

  if(set_temp-histerisis<=Tc)
  {
    relayStatus=false;
  }
  else 
  {
     relayStatus=true;
  }
 
  display.setCursor(0, 0);
  display.setTextSize(1);
  if(relayStatus)
  {
    display.println("On");
    digitalWrite(relayPin,HIGH);
  }
  else {
    display.println("Off");
    digitalWrite(relayPin,LOW);
  }
  display.display();
    
  
 // delay(2000);
  

  // put your main code here, to run repeatedly:
}

void runPid() {
  computeNow = true;
}


void wifi_setup() {
    WiFi.enableAP(true);
    WiFi.softAP(ssid, password);
    WiFi.softAPConfig(local_ip_server, gateway, subnet);
    MDNS.begin(host);
    Serial.print("Open http://");
    Serial.print(WiFi.softAPIP());
    Serial.println("/edit to see the file browser");
}

void check_fs() {
    if (FORMAT_FILESYSTEM) FILESYSTEM.format();
    FILESYSTEM.begin();
    {
        File root = FILESYSTEM.open("/");
        File file = root.openNextFile();
        while (file) {
            String fileName = file.name();
            size_t fileSize = file.size();
           // Serial.printf("FS File: %s, size: %s\n", fileName.c_str(), formatBytes(fileSize).c_str());
            file = root.openNextFile();
        }
        Serial.printf("\n");
    }
}


bool exists(String path) {
    bool yes = false;
    File file = FILESYSTEM.open(path, "r");
    if (!file.isDirectory()) {
        yes = true;
    }
    file.close();
    return yes;
}

String formatBytes(size_t bytes) {
    if (bytes < 1024) {
        return String(bytes) + "B";
    } else if (bytes < (1024 * 1024)) {
        return String(bytes / 1024.0) + "KB";
    } else {
        return String(bytes / 1024.0 / 1024.0) + "MB";
    }
}

String getContentType(String filename) {
    if (server.hasArg("download")) {
        return "application/octet-stream";
    } else if (filename.endsWith(".htm")) {
        return "text/html";
    } else if (filename.endsWith(".html")) {
        return "text/html";
    } else if (filename.endsWith(".css")) {
        return "text/css";
    } else if (filename.endsWith(".js")) {
        return "application/javascript";
    } else if (filename.endsWith(".png")) {
        return "image/png";
    } else if (filename.endsWith(".gif")) {
        return "image/gif";
    } else if (filename.endsWith(".jpg")) {
        return "image/jpeg";
    } else if (filename.endsWith(".ico")) {
        return "image/x-icon";
    } else if (filename.endsWith(".xml")) {
        return "text/xml";
    } else if (filename.endsWith(".pdf")) {
        return "application/x-pdf";
    } else if (filename.endsWith(".zip")) {
        return "application/x-zip";
    } else if (filename.endsWith(".gz")) {
        return "application/x-gzip";
    }
    return "text/plain";
}

bool handleFileRead(String path) {
    Serial.println("handleFileRead: " + path);
    if (path.endsWith("/")) {
        path += "index.html";
    }
    String contentType = getContentType(path);
    String pathWithGz = path + ".gz";
    if (exists(pathWithGz) || exists(path)) {
        if (exists(pathWithGz)) {
            path += ".gz";
        }
        File file = FILESYSTEM.open(path, "r");
        server.streamFile(file, contentType);
        file.close();
        return true;
    }
    return false;
}

void handleFileUpload() {
    if (server.uri() != "/edit") {
        return;
    }
    HTTPUpload &upload = server.upload();
    if (upload.status == UPLOAD_FILE_START) {
        String filename = upload.filename;
        if (!filename.startsWith("/")) {
            filename = "/" + filename;
        }
        Serial.print("handleFileUpload Name: ");
        Serial.println(filename);
        fsUploadFile = FILESYSTEM.open(filename, "w");
        filename = String();
    } else if (upload.status == UPLOAD_FILE_WRITE) {
        //Serial.print("handleFileUpload Data: "); Serial.println(upload.currentSize);
        if (fsUploadFile) {
            fsUploadFile.write(upload.buf, upload.currentSize);
        }
    } else if (upload.status == UPLOAD_FILE_END) {
        if (fsUploadFile) {
            fsUploadFile.close();
        }
        Serial.print("handleFileUpload Size: ");
        Serial.println(upload.totalSize);
    }
}

void handleFileDelete() {
    if (server.args() == 0) {
        return server.send(500, "text/plain", "BAD ARGS");
    }
    String path = server.arg(0);
    Serial.println("handleFileDelete: " + path);
    if (path == "/") {
        return server.send(500, "text/plain", "BAD PATH");
    }
    if (!exists(path)) {
        return server.send(404, "text/plain", "FileNotFound");
    }
    FILESYSTEM.remove(path);
    server.send(200, "text/plain", "");
    path = String();
}

void handleFileCreate() {
    if (server.args() == 0) {
        return server.send(500, "text/plain", "BAD ARGS");
    }
    String path = server.arg(0);
    Serial.println("handleFileCreate: " + path);
    if (path == "/") {
        return server.send(500, "text/plain", "BAD PATH");
    }
    if (exists(path)) {
        return server.send(500, "text/plain", "FILE EXISTS");
    }
    File file = FILESYSTEM.open(path, "w");
    if (file) {
        file.close();
    } else {
        return server.send(500, "text/plain", "CREATE FAILED");
    }
    server.send(200, "text/plain", "");
    path = String();
}

void handleFileList() {
    if (!server.hasArg("dir")) {
        server.send(500, "text/plain", "BAD ARGS");
        return;
    }

    String path = server.arg("dir");
    Serial.println("handleFileList: " + path);
    File root = FILESYSTEM.open(path);
    path = String();
    String output = "[";
    if (root.isDirectory()) {
        File file = root.openNextFile();
        while (file) {
            if (output != "[") {
                output += ',';
            }
            output += "{\"type\":\"";
            output += (file.isDirectory()) ? "dir" : "file";
            output += "\",\"name\":\"";
            output += String(file.name()).substring(1);
            output += "\"}";
            file = root.openNextFile();
        }
    }
    output += "]";
    server.send(200, "text/json", output);
}


void website_setup() {
    Serial.print("Website:");

    server.on("/list", HTTP_GET, handleFileList);
    //load editor
    server.on("/edit", HTTP_GET, []() {
        if (!handleFileRead("/edit.htm")) {
            server.send(404, "text/plain", "FileNotFound");
        }
    });
    //create file
    server.on("/edit", HTTP_PUT, handleFileCreate);
    //delete file
    server.on("/edit", HTTP_DELETE, handleFileDelete);

    server.on("/edit", HTTP_POST, []() {
        server.send(200, "text/plain", "");
    }, handleFileUpload);

    server.onNotFound([]() {
        if (!handleFileRead(server.uri())) {
            server.send(404, "text/plain", "FileNotFound");
        }
    });
/*
    //get heap status, analog input value and all GPIO statuses in one json call
    server.on("/set", []() {
        server.send(200, "text/plain", reverse_text);
        if  (server.hasArg("speed") && server.hasArg("steering")) {
            s_steering=server.arg("steering");
            sa_speed = server.arg("speed");
           // steering = s_steering.toInt();
           // s_speed = sa_speed.toInt();
           // Serial.println("Intreperted..."+s_steering+","+sa_speed+";integer:"+ String(s_speed)+", "+ String(steering));
            s_steering=String();
            sa_speed=String();
          
        }
        //console="Server_moving, speed:" + String(s_speed) + ", steering:" + String(steering));
        delay(10);
    });
*/
    server.on("/gettemp",HTTP_GET, []() {

         if  (server.hasArg("histerisis") && server.hasArg("set_value")) {
            s_histerisis=server.arg("histerisis");
            s_settemp=server.arg("set_value");
          }   
          
        String servo_text="{\"temperature\":";
          int tmp=floor(Tc);
        servo_text=servo_text+tmp;
        servo_text=servo_text+"}";
         server.send(200, "text/plain", servo_text);
     


         
          

    });


   /* server.on("/all", HTTP_GET, []() {
        String json = "{";
        json += "\"heap\":" + String(ESP.getFreeHeap());
        json += ", \"analog\":" + String(analogRead(A0));
        json += ", \"gpio\":" + String((uint32_t)(0));
        json += "}";
        server.send(200, "text/json", json);
        json = String();
    });*/
    Serial.println(ok);
}