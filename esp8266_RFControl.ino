#include <FS.h>                   //this needs to be first, or it all crashes and burns...

#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino

//needed for library
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>          //https://github.com/tzapu/WiFiManager

#include <ArduinoJson.h>          //https://github.com/bblanchon/ArduinoJson

#include <RFControl.h>

const char* config_ssid = "RFControl";
const char* config_password = "lortnoCFR";

//flag for saving data
bool shouldSaveConfig = false;

const int RESET_PIN = 14; //D5 on nodeMCU
const int RECEIVE_PIN = 2; //D4 on nodeMCU
const int SEND_PIN = 16; //D0 on nodeMCU

//array for last received code links
const int MAX_CODE_URLS = 16;
String code_urls[MAX_CODE_URLS];
int current_code_url_index = 0;

WiFiClient espClient;

std::unique_ptr<ESP8266WebServer> server;

void handleRoot() {
  String current_url = "http://" + WiFi.localIP().toString() + "/receive";
  String message = "<html><body>\n\n";
  message += "To receive signals go to <a href='" + current_url + "' location='blank'>" + current_url + "</a>\n";
  message += "</body></html>";
  server->send(200, "text/html", message);
}

void handleNotFound() {
  String message = "File Not Found\n\n";
  message += "URI: ";
  message += server->uri();
  message += "\nMethod: ";
  message += (server->method() == HTTP_GET) ? "GET" : "POST";
  message += "\nArguments: ";
  message += server->args();
  message += "\n";
  for (uint8_t i = 0; i < server->args(); i++) {
    message += " " + server->argName(i) + ": " + server->arg(i) + "\n";
  }
  server->send(404, "text/plain", message);
}

void setup() {
  // put your setup code here, to run once:

  //WiFiManager
  //Local intialization. Once its business is done, there is no need to keep it around
  //needs to be in the beginning for accessing settings reset on reset pin
  WiFiManager wifiManager;
  
  Serial.begin(115200);
  Serial.println();
  //setup gpios
  Serial.println("GPIO setup...");
  
  pinMode(SEND_PIN, OUTPUT);
  pinMode(RECEIVE_PIN, INPUT);
  
  pinMode(RESET_PIN, INPUT_PULLUP);

  //if reset is triggered
  if(digitalRead(RESET_PIN) == LOW){
    Serial.println("RESET triggered...");
    //we format the memory
    SPIFFS.format();
    //and reset the ESP settings
    wifiManager.resetSettings();
  }

  //set static ip
  //wifiManager.setSTAStaticIPConfig(IPAddress(10,0,1,99), IPAddress(10,0,1,1), IPAddress(255,255,255,0));

  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if (!wifiManager.autoConnect(config_ssid,config_password)) {
    Serial.println("failed to connect and hit timeout");
    delay(3000);
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(5000);
  }
  //if you get here you have connected to the WiFi
  Serial.println("connected...yeey :)");

  //we start the interaction webserver
  server.reset(new ESP8266WebServer(WiFi.localIP(), 80));

  server->on("/", handleRoot);
  server->on("/receive", handleReceive);
  server->on("/send", handleSend);
  
  server->onNotFound(handleNotFound);

  server->begin();
  Serial.println("HTTP server started");

  //make sure softap is off
  WiFi.softAPdisconnect(true);


  Serial.println("local ip:");
  Serial.println(WiFi.localIP());
  
}

void loop() {
  server->handleClient();
}


void handleReceive() {
  unsigned int old_millis = millis();
  unsigned int receive_timeout = 5; //timout in seconds waiting for received codes
  Serial.println("handleReceive()");
  String current_url = "http://";
  RFControl::startReceiving(RECEIVE_PIN);
  while(1){
    //keep the watchdog happy
    ESP.wdtFeed();
    
    if(RFControl::hasData()) {
      unsigned int *timings;
      unsigned int timings_size;
      unsigned int pulse_length_divider = RFControl::getPulseLengthDivider();
      
      //prepare URL
      current_url += WiFi.localIP().toString();
      current_url += "/send?buckets=";
      
      RFControl::getRaw(&timings, &timings_size);
      unsigned int buckets[8];
      RFControl::compressTimingsAndSortBuckets(buckets, timings, timings_size);
      Serial.print("buckets: ");
      for(int i=0; i < 8; i++) {
        unsigned long bucket = buckets[i] * pulse_length_divider;
        Serial.print(bucket);
        Serial.write(' ');
        current_url += bucket;
        if(i < 7){
          current_url += ",";
        }
      }
      current_url += "&timings=";
      Serial.print("\ntimings: ");
      for(int i=0; i < timings_size; i++) {
        Serial.write('0' + timings[i]);
        current_url += timings[i];
      }
      Serial.write('\n');
      Serial.write('\n');
  
      current_url += "&repeats=3";
      
      RFControl::stopReceiving();
      break;
    }
    unsigned int current_millis = millis();
    if (current_millis > (old_millis + receive_timeout * 1000)){
      current_url = "Receive timed out. Refresh to start again.";
      break;
    }
  }
  String message = "<html><body>\n\n";
  message += "<a href='" + current_url + "' location='blank'>" + current_url + "</a>\n";
  message += "</body></html>";
  server->send(200, "text/html", message);
}

void handleSend() {
  Serial.println("handleSend()");
  String buckets_string = server->arg("buckets");
  String timings_string = server->arg("timings");
  String repeats_string = server->arg("repeats");

  String buckets_string_tmp = String(buckets_string);

  unsigned long buckets[8] = {0,0,0,0,0,0,0,0};

  char delimiter[] = ",";
  char *ptr;
  // read the delimited parameter
  ptr = strtok((char *)buckets_string_tmp.c_str(), delimiter);
  unsigned int bucket_index = 0;
  while(ptr != NULL) {
    buckets[bucket_index] = atoi(ptr);
    ptr = strtok(NULL, delimiter);
    bucket_index++;
  }
  Serial.println("Buckets:");
  for (int ii = 0; ii < 8; ii++){
    Serial.println(buckets[ii]);
  }
  Serial.println("Repeats:");
  Serial.println(atoi(repeats_string.c_str()));
  
  RFControl::sendByCompressedTimings(SEND_PIN, buckets, (char *)timings_string.c_str(), repeats_string.toInt());
  
  String current_url = "http://" + WiFi.localIP().toString() + "/send?buckets=" + buckets_string + "&timings=" + timings_string + "&repeats=" + repeats_string;
  
  String message = "<html><body>\n\n";
  message += "Sent: <a href='" + current_url + "' location='blank'>" + current_url + "</a>\n";
  message += "</body></html>";
  server->send(200, "text/html", message);
}

