/*
 * Motorized Water Valve Controlled by Sunlight intensity
 *  Purpose: 
 *    Allow to interface with the "control of a water motorized valve" module
 *    By default or if wifi connection fails, the module is in AP (Access Point) mode
 *    The wifi SSID is ESPXXXXXXX where x is a number. You hahve to connect to it (IP is http://192.168.4.1).
 *    Connected to the AP you can set your SSID and password. then the module will reset in server mode.
 *    log to the assigned IP adress to get the help and interfac access of the module.
 *    This module communicate to arduino through UART/SERIAL protocol in 9600bauds from GPIO 0&2
 *  Author:
 *    Vincent(.)Cruvellier(@]gmail(.)com
 *  Date of release:
 *     13-MAY-2017
 *  Hardware:
 *    MCU : ESP-01
  *  Interface:
 *    Main Serial at 9600 bauds
 *    Software Serial to connect to Arduino is GPIO 0 & GPIO 2.
 */ 

 /* 
 *  Include SECTION
 */
#include <ESP8266WiFi.h>          //https://github.com/esp8266/Arduino
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <ESP8266HTTPClient.h>
#include "WiFiManager.h"          //https://github.com/tzapu/WiFiManager
#include <SoftwareSerial.h>       //ESP8266 Software Serial CommunicationArduino

/*
 * GLOBAL VARIABLES
 */
SoftwareSerial swSer(0, 2, false, 256); //Define hardware connections
WiFiClient _client;
ESP8266WebServer server(80);
WiFiManager wifiManager;

// Debug Section definition
#define DEBUG_PROG 
#ifdef DEBUG_PROG
  #define DEBUG_PRINTLN(x)  Serial.println(x)
  #define DEBUG_PRINT(x)    Serial.print(x)
#else
  #define DEBUG_PRINTLN(x) 
  #define DEBUG_PRINT(x)
#endif

String form_html_str; // Variable for html reply

/*************************************************************
 * *configModeCallback*
 */
void configModeCallback (WiFiManager *myWiFiManager) {
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
}

/*************************************************************
 * *handle_root*
 */
void handle_root() {
  String answerxx = F("<!DOCTYPE html><html><body>");
  answerxx += F("<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>");
  answerxx += F("<h1>Sun Water Flow Control Esp8266</h1><p> Acces aux commandes: <a href='/cmd'>/cmd.</a><br> Reset settings - /reset.</P><br><br>");
  answerxx += form_html_str;
  answerxx += F("<p> Vincent Cruvellier(c) May, 2017</p></body></html>");
  server.send(200,"text/html",answerxx);
  delay(100);        
}

/*************************************************************
 * *getQueryValue*
 * Returns value of the specified GET query param
 */
String getQueryValue(String paramName){
  for (uint8_t i=0; i<server.args(); i++){
    if (server.argName(i) == paramName) {
      return server.arg(i);
    }
  }
  return "";
}

/*
 * handles communication between arduino and computer over wifi
 * @param requestCode: unique code for arduino to perform action
 * @param ansBuffer: predicted size of reply
 * @param justOk: true  send ok reply when serial data received
 *                false send the message received over serial 
 * @return void
 */
void relayData(String strcmd, int ansBuffer, boolean justOk){

  char outstr[strcmd.length() + 1];
  strcmd.toCharArray(outstr, strcmd.length() + 1);
  swSer.flush();
  
  // Send request code to arduino
  swSer.write(outstr);
  swSer.write("\n");
  
  
  // Store the answer from arduino
  char answer[ansBuffer];
  String answerStr = "";

  // Monitor the amount of chars
  int i=0;
  //DEBUG_PRINT("<==");
  bool timeout = true;
  unsigned long now = millis();
  while (( millis()-now) < 5000) {
    if (swSer.available() > 0){ timeout=false; break; }
    delay(100);
  }
  if (timeout) {DEBUG_PRINTLN("Serial Software read reached TimeOut");} else
  {
      unsigned long startloop = millis();
      bool notcompleted = true;
      while (( millis()- startloop) < 10000 && (notcompleted)) { // Max 10 sec to get the answer
          while( (swSer.available()>0) && i<5000){
            char inChar = (char)swSer.read();
            if (inChar == '\n') answerStr += "<br>";
            answerStr += inChar;
            //DEBUG_PRINT(inChar);
            if (inChar == '>'){ notcompleted=false; break; }
            //answer[i++] = inChar;
            i++;
            delay(1);
          }
          delay(1);
       }
  }
  //answer[i++]='\0';
  DEBUG_PRINTLN("==>");
  // Handle the reply to the client
  if(i>0){
    if(justOk){
      server.send(200, "text/plain", "ok");
    }
    else{
      server.send(200, "text/html", "<!DOCTYPE html><html><body>" + form_html_str + "<b><u>Server reply:</b></u><p style=\"color: red;\">" + answerStr+"</p>");
    }
  }
  else{
    server.send(200, "text/plain", "No Serial available!");
  }
}

/*
 * Setup:
 */
void setup() {
  delay(2000);
  Serial.begin(9600);   //Initialize hardware serial with baudrate of 115200
  DEBUG_PRINTLN("Hardware serial test started");

  //reset settings - for testing
  //wifiManager.resetSettings();
  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback(configModeCallback);
  //fetches ssid and pass and tries to connect
  //if it does not connect it starts an access point with the specified name
  //here  "AutoConnectAP"
  //and goes into a blocking loop awaiting configuration
  if(!wifiManager.autoConnect()) {
    Serial.println("failed to connect and hit timeout");
    //reset and try again, or maybe put it to deep sleep
    ESP.reset();
    delay(1000);
  }

  DEBUG_PRINTLN("WIFI Setup and started");

  delay(5000);
  swSer.begin(9600);    //Initialize software serial with baudrate of 115200
  swSer.println("\nSoftware serial test started");

  // Activate Web Server
  server.on("/", handle_root);
  server.onNotFound([](){
      server.send(404, "text/plain", "404 Not found");
  });  
  server.on("/cmd", []() { 
    String strcmd = getQueryValue("CMD");
    if (strcmd=="") {
      String answerxx = "<!DOCTYPE html><html><body><style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>";
      answerxx += "<h1>Sun Water Flow Control Esp8266</h1><br>";
      answerxx += "<h2>Gestion de Controle de Commande</h2><form action=\"/cmd\">Commande:<br>"; 
      answerxx += "<input type=\"text\" name=\"CMD\" value=\"M100;\"><br><br><input type=\"submit\" value=\"Submit\"></form>";
      answerxx += "<p> Vincent (c) 2017</p></body></html>";
      server.send(200,"text/html",answerxx);
      return;
   }
    DEBUG_PRINT("/cmd?CMD=");
    DEBUG_PRINTLN(strcmd);
    //swSer.println(strcmd);
    relayData(strcmd, 1024, false);
    delay(100);
  });
  server.on("/reset", []() {
    wifiManager.resetSettings();
    ESP.reset();
    delay(5000);
  });
  server.begin();
  DEBUG_PRINTLN("WIFI Setup and started");
  // Print the IP address
  DEBUG_PRINTLN("IP Adress:");DEBUG_PRINTLN(WiFi.localIP());
  form_html_str = F("<style> body { background-color: #fffff; font-family: Arial, Helvetica, Sans-Serif; Color: #000088; }</style>");
  form_html_str += "<h1>Sun Water Flow Control Esp8266</h1><br>";
  form_html_str += F("<h1>Gestion de Controle de Commande</h1><form action=\"/cmd\">Commande Libre:<br><input type=\"text\" name=\"CMD\" value=\"M100;\"><input type=\"submit\" value=\"Submit\"></form>");
  form_html_str += F("<br><b><u>Commandes predefinies</u></b>");
  form_html_str += F("<ul><li><a href=\"/cmd?CMD=M10%3B\">OUVERTURE VANNE</a> (Water Flow=ON)</li>");
  form_html_str += F("<li><a href=\"/cmd?CMD=M11%3B\">FERMETURE VANNE</a> (Water Flow=OFF)</li>");
  form_html_str += F("<li><a href=\"/cmd?CMD=G02%3B\">ETAT DES PARAMETRES</a> (Get Application Data)</li>");
  form_html_str += F("<li><a href=\"/cmd?CMD=G01%3B\">VALEUR DECLENCHEMENT</a> (Lux Trigger)</li>");
  form_html_str += F("<li><a href=\"/cmd?CMD=G03%3B\">VALEUR ACTUELLE</a> (Actual Lux Value)</li>");
  form_html_str += F("<li><a href=\"/cmd?CMD=S04+X0%3B\">METTRE MODE MANUEL</a> (Set Manual Mode)</li>");
  form_html_str += F("<li><a href=\"/cmd?CMD=S04+X1%3B\">METTRE MODE AUTO</a> (Set Auto Mode)</li>");
  form_html_str += F("<li><a href=\"/cmd?CMD=M500%3B\">ENREGISTRER LES INFORMATIONS</a> (Eeeprom Update)</li>");
  form_html_str += F("<li><form action=\"/cmd\">Lux Trigger:<input type=\"text\" name=\"CMD\" value=\"L1500;\"><input type=\"submit\" value=\"Submit\"></form>");
  form_html_str += F("</ul><hr style=\"width: 100%; height: 2px;\">");
}

/*
 * Loop:
 */
void loop() {
  while (swSer.available() > 0) {  //wait for data at software serial
    Serial.write(swSer.read()); //Send data recived from software serial to hardware serial    
  }
  delay(1);
  while (Serial.available() > 0) { //wait for data at hardware serial
    char c=Serial.read();  // get it
    swSer.print(c);  // repeat it back so I know you got the message
    //swSer.write(Serial.read());     //send data recived from hardware serial to software serial
  }
  server.handleClient();
}

