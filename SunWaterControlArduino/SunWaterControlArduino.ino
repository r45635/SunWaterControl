/*
 * Motorized Water Valve Controlled by Sunlight intensity
 *  Purpose: 
 *    Allow control of a water motorized valve in manual or automatic mode.
 *    In Automatic mode, a trigger is defined by the intensity of the Sun Light (In Lux)
 *    This program can work in Standalone way but it is much more convenient to use an ESP01
 *    connected to it through (TX/RX Urats interfaces) in order to manage it.
 *  Author:
 *    Vincent(.)Cruvellier(@]gmail(.)com
 *  Date of release:
 *     13-MAY-2017
 *  Hardware:
 *    MCU : Arduino (Nano for my purpose)
 *    L9110S : Motor DC Driver Module
 *    BH1750 : Digital Lux Light Meter Module
 *  Interface:
 *    Serial at 9600 bauds
 */  

// Debug Section definition
//#define DEBUG
#ifdef DEBUG
 #define DEBUG_PRINT(x)  Serial.print (x)
 #define DEBUG_PRINTLN(x)  Serial.println (x)
#else
 #define DEBUG_PRINT(x)
 #define DEBUG_PRINTLN(x)
#endif

/* 
 *  Include SECTION
 */
#include <Wire.h>
#include <BH1750.h>          // facilitate the use of B1750 Lux light module through I2C
#include <avr/eeprom.h>      // use of native EEPROM Function

/*
 * GLOBAL VARIABLES
 */
BH1750 lightMeter; /* BH1750 usage.
  This example initalises the BH1750 object using the default
  high resolution mode and then makes a light level reading every second.
  Connection:
    VCC -> 3.3V (3V3 on Arduino Due, Zero, MKR1000, etc)
    GND -> GND
    SCL -> SCL (A5 on Arduino Uno, Leonardo, etc or 21 on Mega and Due)
    SDA -> SDA (A4 on Arduino Uno, Leonardo, etc or 20 on Mega and Due)
    ADD -> (not connected) or GND
  ADD pin uses to set sensor I2C address. If it has voltage greater or equal to
  0.7VCC voltage (as example, you've connected it to VCC) - sensor address will be
  0x5C. In other case (if ADD voltage less than 0.7 * VCC) - sensor address will
  be 0x23 (by default).
*/


// L9110 motor driver control
const int AIA = 9;  // (pwm) pin 9 connected to pin A-IA 
const int AIB = 5;  // (pwm) pin 5 connected to pin A-IB 
byte speed = 255;  // change this (0-255) to control the speed of the motors 

// Application global variable structured to stored in Eeprom
uint16_t lux_trigger = 200; // Triger value to turn on the valve when in automatice
bool water_flow_status = false; // Status of the valve, applied at the first execution
bool application_mode_manual = true; // Application mode: use automatic trigger or only manual;
bool EE_initiated = false; // Check if eeprom has been initiated or not.

// SETUP
#define VERSION        (1)  // firmware version
#define BAUD           (9600)  // How fast is the Arduino talking?
#define MAX_BUF        (64)  // What is the longest message Arduino can store?
#define BASE_EEPROM    (2)       // Base Adress for EEPROM

// GLOBALS
//------------------------------------------------------------------------------

char  buffer[MAX_BUF];  // where we store the message until we get a newline
int   sofar;            // how much is in the buffer
unsigned long blink_start = millis(); // use to define interface regular communication and check loop

// CONSTANTS DEFINITION
const byte valid_signature[5] = "ESP=";
const int blink_interval = 10000; // Loop interval in ms 10000 = 10s

// STRUCTURE DEFINITION
struct settings_t
{
  uint16_t  lux_trigger;
  bool      water_flow_status;
  bool      application_mode_manual;
} settings, lastSettings;

/**************************************************************************************************
 * read_eeprom():
 * check the eeprom, initialized it if needed.
 */
void read_eeprom() {
  
  char signature[5] ;             // Temp String
  
  // EEprom Configuration check
  eeprom_read_block((void*)&signature, (void*)BASE_EEPROM, sizeof(signature)); // read EEPROM
  EE_initiated = true; // Will determine if EEPROM contains valid Signature
  for (int i = 0; i < 4; i++) {
    #ifdef DEBUG
    Serial.Print("EE Read ="); Serial.Print(signature[i], HEX);
    Serial.Print(" / Expect ="); Serial.Print(valid_signature[i], HEX);
    Serial.Print("\n");
    #endif
    if (valid_signature[i] != signature[i]) {
      EE_initiated = false; // Content not expected => EEPROM not initialized
    }
  }
  if (EE_initiated) { // EEPROM Contains valid data => Read of counter values
    eeprom_read_block((void*)&settings, (void*)BASE_EEPROM + 4, sizeof(settings));
    Serial.print("EE_initiated Ok\n");
    lux_trigger = settings.lux_trigger;
    water_flow_status = settings.water_flow_status;
    application_mode_manual = settings.application_mode_manual;
    //Serial.print("Compteur 1 : %lu,", settings.compteur_1); printf("Compteur 2 : %lu \n", settings.compteur_2);
  } else { // EEPROM to be initialized at 0 : default values
    Serial.print("EE_initiated NOK\n");
    strcpy(signature, "ESP="); // set the signature
    eeprom_write_block((const void*)&signature, (void*)BASE_EEPROM, sizeof(signature));
    settings.lux_trigger = lux_trigger;
    settings.water_flow_status = water_flow_status;
    settings.application_mode_manual = application_mode_manual;
    eeprom_write_block((const void*)&settings, (void*)BASE_EEPROM + 4, sizeof(settings));
  }
}

/**************************************************************************************************
 * write_eeprom():
 * write the application variable to the eeprom, to be used nex restart.
 */
void write_eeprom() {
    settings.lux_trigger = lux_trigger;
    settings.water_flow_status = water_flow_status;
    settings.application_mode_manual = application_mode_manual;
    eeprom_write_block((const void*)&settings, (void*)BASE_EEPROM + 4, sizeof(settings));  
}

/**
 * Display the help support through Serial interface
 */
void help() {
  Serial.print(F("VANNE CONTROL V"));
  Serial.println(VERSION);
  Serial.println(F("\nCommands:"));
  Serial.println(F("G01; - GET The Lux Value Trigger"));
  Serial.println(F("|---- Example: G02;"));
  Serial.println(F("S01 L(lux); - SET The Lux Value Trigger"));
  Serial.println(F("|---- Example: S01 L300;"));
  Serial.println(F("G02; - GET The Application Data"));
  Serial.println(F("|---- Example: G02;")); 
  Serial.println(F("G03; - GET The Actual Lux Value "));
  Serial.println(F("|---- Example: G03;"));
  Serial.println(F("G04; - GET mode (Manual/Auto)"));
  Serial.println(F("|---- Example: G04;"));  
  Serial.println(F("S04; - SET mode (0 Manual / 1 Auto)"));
  Serial.println(F("|---- Example: S04 X0;"));  
  Serial.println("");
  Serial.println(F("M10; - Switch ON the water"));
  Serial.println(F("M11; - Switch OFF the water"));
  Serial.println(F("M18; - disable motor control"));
  Serial.println(F("M19; - enable motor control"));
  Serial.println(F("M100; - this help message"));
  Serial.println(F("M500; - Write data to EEPROM"));
  Serial.println(F("\nAll commands must end with a newline."));
}

/**
 * prepares the input buffer to receive a new message and tells the serial connected device it is ready for more.
 */
uint16_t getLux() {
  uint16_t lux = lightMeter.readLightLevel();
  Serial.print("ACTUAL LIGHT=");
  Serial.print(lux);
  Serial.println(" lux");
  return lux;
}
/**
 * Look for character /code/ in the buffer and read the float that immediately follows it.
 * @return the value found.  If nothing is found, /val/ is returned.
 * @input code the character to look for.
 * @input val the return value if /code/ is not found.
 **/
float parsenumber(char code,float fallback) {
  char *ptr=buffer;
  while(ptr && *ptr && ptr<buffer+sofar) {
    if(*ptr==code) {
      return atof(ptr+1);
    }
    ptr=strchr(ptr,' ')+1;
  }
  return fallback;
}

/**
 * prepares the input buffer to receive a new message and tells the serial connected device it is ready for more.
 */
void ready() {
  sofar=0;  // clear input buffer
  Serial.print(F("-->\n"));  // signal ready to receive input
}

/**
 * Read the input buffer and find any recognized commands.  One G or M command per line.
 */
void processCommand() {
  
  // General Commands
  int cmd = parsenumber('G',-1);
  //Serial.println("G cmd:"+cmd);
  switch(cmd) {
  case  1: { // LUX trigger
    DEBUG_PRINT("cmd G01");
    Serial.print("LUX TRIGGER=");
    Serial.println(lux_trigger);    
    break;
    }
  case  2: { // display application data
    DEBUG_PRINT("cmd G02");
    display_application_data();
    break;
  }
  case  3: { // LUX
    DEBUG_PRINT("cmd G03");
    uint16_t temp=getLux();
    break;
    }
  case  4: { // GET mode
    DEBUG_PRINT("cmd G04");
    if (application_mode_manual) {
      Serial.println(F("Application mode = MANUAL"));
    } else {
      Serial.println(F("Application mode = AUTO"));
    }
    break;
    }
  default:  break;
  }
  // LUX Trigger
  cmd = parsenumber('L',-1);
  if (cmd > -1) {
    DEBUG_PRINT("cmd LXX");
    lux_trigger = cmd;
    DEBUG_PRINT(" L:"); DEBUG_PRINT(x);
    Serial.print(F("LUX TRIGGER="));
    Serial.println(lux_trigger);
  }
  
  // SETUP Commands
  cmd = parsenumber('S',-1);
  switch(cmd) {
  case  1: { // LUX Trigger
    DEBUG_PRINT("cmd S01");
    int x = parsenumber('L',-1);
    if (x==-1) {
      Serial.println(F("Not applicable Trigger Value - Error."));     
    } else lux_trigger = x;  
    DEBUG_PRINT(" L:"); DEBUG_PRINT(x);
    Serial.print(F("LUX TRIGGER="));
    Serial.println(lux_trigger);
    break;
    }
  case  4: { // SET mode
    DEBUG_PRINT("cmd S04");
    int x = parsenumber('X',0);
    DEBUG_PRINT(" X:"); DEBUG_PRINT(x);
    if (x==0) { application_mode_manual=true; Serial.println(F("Application mode=MANUAL")); }
    if (x==1) { application_mode_manual=false; Serial.println(F("Application mode=AUTO")); }
    break;
    }
  default:  break;
  }

  cmd = parsenumber('M',-1);
  switch(cmd) {
  case 10:  // enable water flow
    water_flow_on();
    Serial.println(F("Water Flow=ON"));
    break;
  case 11:  // disable water flow
    water_flow_off();
    Serial.println(F("Water Flow=OFF"));
    break;    
  case 100:  help();  break;
  case 500:  {
    write_eeprom();
    Serial.println(F("Eeprom UPDATED."));
    break;
  }
  default:  break;
  }
}

/**
 * void water_flow_on(): Turn On the motorized valve
 */
void water_flow_on()
{
  DEBUG_PRINTLN(F("WATER FLOW=ENABLED"));
  water_flow_status = true;
  analogWrite(AIA, 0);
  analogWrite(AIB, speed);
  delay(1000);
}

/**
 * void water_flow_off(): Turn Off the motorized valve
 */
void water_flow_off()
{
  DEBUG_PRINTLN(F("WATER FLOW=DISABLED"));
  water_flow_status = false;
  analogWrite(AIA, speed);
  analogWrite(AIB, 0);
  delay(1000); // force a 1s delay before letting user or application take another decision
}

/**
 * void water_flow_off(): Turn Off the motorized valve
 */
void display_application_data() {
  Serial.print(F("WATER FLOW / VANNE POSITION="));
  if (water_flow_status) { // First Time set the valve in the expected position
    water_flow_on(); // Valve ON
    Serial.println(F("ON."));
  } else {
    water_flow_off(); // Valve OFF
    Serial.println(F("OFF."));
  }  
  Serial.print(F("APPLICATION MODE="));
  if (application_mode_manual==true) Serial.println(F("AUTOMATIC")); else Serial.println(F("MANUAL"));
  Serial.print(F("LUX TRIGGER / DECLENCHEMENT="));
  Serial.println(lux_trigger);
  uint16_t temp=getLux(); // display Lux Value
}

/**
 * Setup :
 */
void setup() {
  Serial.begin(BAUD); // initialize the Serial interface (BAUD shall be 9600 in order to communicate with ESP-01
  delay(10); 

  Serial.println(F("Hello World."));
  Serial.println();
  // read Eeeprom
  read_eeprom();
  
  // Initialize L9110 motor driver control, Use only one motor
  pinMode(AIA, OUTPUT); // set pins to output
  pinMode(AIB, OUTPUT);
  Serial.print(F("L9110 initialized with position "));
  if (water_flow_status) { // First Time set the valve in the expected position
    water_flow_on(); // Valve ON
    Serial.println(F("ON."));
  } else {
    water_flow_off(); // Valve OFF
    Serial.println(F("OFF."));
  }  
  lightMeter.begin(); // Initialize BH1750
  Serial.println(F("BH1750 initialized."));
  Serial.print(F("Application Mode is "));
  if (application_mode_manual) Serial.println(F("ON")); else Serial.println(F("OFF"));
  Serial.print(F("LUX TRIGGER / DECLENCHEMENT="));
  Serial.println(lux_trigger);
  Serial.println(F("-----------------------------------------"));
  help(); // display the support information
}


/**
 * loop :
 */
void loop() {
   // listen for serial commands
  while(Serial.available() > 0) {  // if something is available
    char c=Serial.read();  // get it
    Serial.print(c);  // repeat it back so I know you got the message
    if(sofar<MAX_BUF-1) buffer[sofar++]=c;  // store it
    if((c=='\n') || (c == '\r')) {
      // entire message received
      buffer[sofar]=0;  // end the buffer so string functions work right
      //Serial.print(F("\r\n"));  // echo a return character for humans
      Serial.println("<--");
      processCommand();  // do something with the command
      ready();
    }
  }
  if (millis() - blink_start > blink_interval) { // every 10 seconds
    //uint16_t lux = lightMeter.readLightLevel();
    uint16_t lux = getLux(); // Display the lux value and blink therefore the led
    blink_start = millis();
    if (!application_mode_manual) { // Only there if we are in Automatice mode.
      if (lux >= lux_trigger) // if the Lux value is greater than the trigger value
      {
        DEBUG_PRINT(F("LUX Trigger ON"));
        water_flow_on();
      } else
      {
        DEBUG_PRINT(F("LUX Trigger OFF"));
        water_flow_off();
      }
    }   
  }
}

