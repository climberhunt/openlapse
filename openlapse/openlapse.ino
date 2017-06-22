/*
 * OpenLapse by David Hunt
 * Based on Sonoff-Tasmota by Theo Arends
*/

#define VERSION                0x000100  // 0.1.0

enum log_t   {LOG_LEVEL_NONE, LOG_LEVEL_ERROR, LOG_LEVEL_INFO, LOG_LEVEL_DEBUG, LOG_LEVEL_DEBUG_MORE, LOG_LEVEL_ALL};

#include "user_config.h"
#include "WEMOS_Motor.h"

/*********************************************************************************************\
 * No user configurable items below
\*********************************************************************************************/

#define MODULE                  WEMOS

//#define DEBUG_THEO                          // Add debug code

#ifdef BE_MINIMAL
#ifdef USE_I2C
#undef USE_I2C                              // Disable all I2C sensors
#endif
#ifdef USE_IR_REMOTE
#undef USE_IR_REMOTE                        // Disable IR driver
#endif
#ifdef DEBUG_THEO
#undef DEBUG_THEO                           // Disable debug code
#endif
#endif  // BE_MINIMAL

#ifndef SWITCH_MODE
#define SWITCH_MODE            TOGGLE       // TOGGLE, FOLLOW or FOLLOW_INV (the wall switch state)
#endif

#define WIFI_HOSTNAME          "openlapse"
#define CONFIG_FILE_SIGN       0xA5         // Configuration file signature
#define CONFIG_FILE_XOR        0x5A         // Configuration file xor (0 = No Xor)

#define HLW_PREF_PULSE         12530        // was 4975us = 201Hz = 1000W
#define HLW_UREF_PULSE         1950         // was 1666us = 600Hz = 220V
#define HLW_IREF_PULSE         3500         // was 1666us = 600Hz = 4.545A

#define APP_POWER              0            // Default saved power state Off
#define MAX_DEVICE             1            // Max number of devices
#define MAX_PULSETIMERS        4            // Max number of supported pulse timers

#define PWM_RANGE              1023         // 255..1023 needs to be devisible by 256
//#define PWM_FREQ               1000         // 100..1000 Hz led refresh
#define PWM_FREQ               910          // 100..1000 Hz led refresh (iTead value)

#define MAX_POWER_HOLD         10           // Time in SECONDS to allow max agreed power (Pow)
#define MAX_POWER_WINDOW       30           // Time in SECONDS to disable allow max agreed power (Pow)
#define SAFE_POWER_HOLD        10           // Time in SECONDS to allow max unit safe power (Pow)
#define SAFE_POWER_WINDOW      30           // Time in MINUTES to disable allow max unit safe power (Pow)
#define MAX_POWER_RETRY        5            // Retry count allowing agreed power limit overflow (Pow)

#define STATES                 10           // loops per second
#define SYSLOG_TIMER           600          // Seconds to restore syslog_level
#define SERIALLOG_TIMER        600          // Seconds to disable SerialLog
#define OTA_ATTEMPTS           10           // Number of times to try fetching the new firmware

#define INPUT_BUFFER_SIZE      100          // Max number of characters in serial buffer
#define TOPSZ                  60           // Max number of characters in topic string
#define LOGSZ                  128          // Max number of characters in log string
#define MAX_LOG_LINES        20           // Max number of lines in weblog

#define APP_BAUDRATE           115200       // Default serial baudrate
#define MAX_STATUS             11           // Max number of status lines

enum butt_t {PRESSED, NOT_PRESSED};

#include "support.h"                        // Global support

#define MESSZ                  360          // Max number of characters in JSON message string (4 x DS18x20 sensors)

#include <Ticker.h>                         // RTC, HLW8012, OSWatch
#include <ESP8266WiFi.h>                    // Ota, WifiManager
#include <ESP8266HTTPClient.h>              // Ota
#include <ESP8266httpUpdate.h>              // Ota
#include <ArduinoJson.h>                    // WemoHue, IRremote
#include <ESP8266WebServer.h>             // WifiManager, Webserver
#include <DNSServer.h>                    // WifiManager
  #include <ESP8266mDNS.h>                  // Webserver
#ifdef USE_I2C
  #include <Wire.h>                         // I2C support library
#endif  // USE_I2C
#ifdef USI_SPI
  #include <SPI.h>                          // SPI, TFT
#endif  // USE_SPI
#include "settings.h"

typedef void (*rtcCallback)();

#define MAX_BUTTON_COMMANDS    5            // Max number of button commands supported
const char commands[MAX_BUTTON_COMMANDS][14] PROGMEM = {
  {"wificonfig 1"},   // Press button three times
  {"wificonfig 2"},   // Press button four times
  {"wificonfig 3"},   // Press button five times
  {"restart 1"},      // Press button six times
  {"upgrade 1"}};     // Press button seven times

const char wificfg[5][12] PROGMEM = { "Restart", "Smartconfig", "Wifimanager", "WPSconfig", "Retry" };

struct TIME_T {
  uint8_t       Second;
  uint8_t       Minute;
  uint8_t       Hour;
  uint8_t       Wday;      // day of week, sunday is day 1
  uint8_t       Day;
  uint8_t       Month;
  char          MonthName[4];
  uint16_t      DayOfYear;
  uint16_t      Year;
  unsigned long Valid;
} rtcTime;

struct TimeChangeRule
{
  uint8_t       week;      // 1=First, 2=Second, 3=Third, 4=Fourth, or 0=Last week of the month
  uint8_t       dow;       // day of week, 1=Sun, 2=Mon, ... 7=Sat
  uint8_t       month;     // 1=Jan, 2=Feb, ... 12=Dec
  uint8_t       hour;      // 0-23
  int           offset;    // offset from UTC in minutes
};

int Baudrate = APP_BAUDRATE;          // Serial interface baud rate
byte SerialInByte;                    // Received byte
int SerialInByteCounter = 0;          // Index in receive buffer
char serialInBuf[INPUT_BUFFER_SIZE + 2];  // Receive buffer
byte Hexcode = 0;                     // Sonoff dual input flag
uint16_t ButtonCode = 0;              // Sonoff dual received code
int16_t savedatacounter;              // Counter and flag for config save to Flash
char Version[16];                     // Version string from VERSION define
char Hostname[33];                    // Composed Wifi hostname
unsigned long timerxs = 0;            // State loop timer
int state = 0;                        // State per second flag
int otaflag = 0;                      // OTA state flag
int otaok = 0;                        // OTA result
byte otaretry = OTA_ATTEMPTS;         // OTA retry counter
int restartflag = 0;                  // Sonoff restart flag
int uptime = 0;                       // Current uptime in hours
boolean uptime_flg = true;            // Signal latest uptime
int tele_period = 0;                  // Tele period timer
String Log[MAX_LOG_LINES];            // Web log buffer
byte logidx = 0;                      // Index in Web log buffer
byte logajaxflg = 0;                  // Reset web console log
byte Maxdevice = MAX_DEVICE;          // Max number of devices supported
int status_update_timer = 0;          // Refresh initial status
uint16_t pulse_timer[MAX_PULSETIMERS] = { 0 }; // Power off timer
uint16_t blink_timer = 0;             // Power cycle timer
uint16_t blink_counter = 0;           // Number of blink cycles
uint8_t blink_power;                  // Blink power state
uint8_t blink_mask = 0;               // Blink relay active mask
uint8_t blink_powersave;              // Blink start power save state
uint8_t latching_power = 0;           // Power state at latching start
uint8_t latching_relay_pulse = 0;     // Latching relay pulse timer

WiFiClient espClient;               // Wifi Client
WiFiUDP portUDP;                      // UDP Syslog and Alexa

uint8_t power;                        // Current copy of sysCfg.power
byte syslog_level;                    // Current copy of sysCfg.syslog_level
uint16_t syslog_timer = 0;            // Timer to re-enable syslog_level
byte seriallog_level;                 // Current copy of sysCfg.seriallog_level
uint16_t seriallog_timer = 0;         // Timer to disable Seriallog
uint8_t sleep;                        // Current copy of sysCfg.sleep

int blinks = 201;                     // Number of LED blinks
uint8_t blinkstate = 0;               // LED state

uint8_t lastbutton[4] = { NOT_PRESSED, NOT_PRESSED, NOT_PRESSED, NOT_PRESSED };     // Last button states
uint8_t holdcount = 0;                // Timer recording button hold
uint8_t multiwindow = 0;              // Max time between button presses to record press count
uint8_t multipress = 0;               // Number of button presses within multiwindow
uint8_t lastwallswitch[4];            // Last wall switch states

uint8_t rel_inverted[4] = { 0 };      // Relay inverted flag (1 = (0 = On, 1 = Off))
uint8_t led_inverted[4] = { 0 };      // LED inverted flag (1 = (0 = On, 1 = Off))
uint8_t swt_flg = 0;                  // Any external switch configured
uint8_t dht_type = 0;                 // DHT type (DHT11, DHT21 or DHT22)
uint8_t i2c_flg = 0;                  // I2C configured
uint8_t spi_flg = 0;                  // SPI configured
uint8_t pwm_flg = 0;                  // PWM configured
uint8_t pwm_idxoffset = 0;            // Allowed PWM command offset (change for Sonoff Led)

boolean mDNSbegun = false;

/********************************************************************************************/

void getClient(char* output, const char* input, byte size)
{
  char *token;
  uint8_t digits = 0;

  if (strstr(input, "%")) {
    strlcpy(output, input, size);
    token = strtok(output, "%");
    if (strstr(input, "%") == input) {
      output[0] = '\0';
    } else {
      token = strtok(NULL, "");
    }
    if (token != NULL) {
      digits = atoi(token);
      if (digits) {
        snprintf_P(output, size, PSTR("%s%c0%dX"), output, '%', digits);
        snprintf_P(output, size, output, ESP.getChipId());
      }
    }
  }
  if (!digits) {
    strlcpy(output, input, size);
  }
}

void setLatchingRelay(uint8_t power, uint8_t state)
{
  power &= 1;
  if (2 == state) {           // Reset relay
    state = 0;
    latching_power = power;
    latching_relay_pulse = 0;
  }
  else if (state && !latching_relay_pulse) {  // Set port power to On
    latching_power = power;
    latching_relay_pulse = 2; // max 200mS (initiated by stateloop())
  }
}


/********************************************************************************************/

void json2legacy(char* stopic, char* svalue)
{
  char *p;
  char *token;
  uint16_t i;
  uint16_t j;

  if (!strstr(svalue, "{\"")) {
    return;  // No JSON
  }

// stopic = stat/sonoff/RESULT
// svalue = {"POWER2":"ON"}
// --> stopic = "stat/sonoff/POWER2", svalue = "ON"
// svalue = {"Upgrade":{"Version":"2.1.2", "OtaUrl":"%s"}}
// --> stopic = "stat/sonoff/UPGRADE", svalue = "2.1.2"
// svalue = {"SerialLog":2}
// --> stopic = "stat/sonoff/SERIALLOG", svalue = "2"
// svalue = {"POWER":""}
// --> stopic = "stat/sonoff/POWER", svalue = ""

  token = strtok(svalue, "{\"");      // Topic
  p = strrchr(stopic, '/') +1;
  i = p - stopic;
  for (j = 0; j < strlen(token)+1; j++) {
    stopic[i+j] = toupper(token[j]);
  }
  token = strtok(NULL, "\"");         // : or :3} or :3, or :{
  if (strstr(token, ":{")) {
    token = strtok(NULL, "\"");       // Subtopic
    token = strtok(NULL, "\"");       // : or :3} or :3,
  }
  if (strlen(token) > 1) {
    token++;
    p = strchr(token, ',');
    if (!p) {
      p = strchr(token, '}');
    }
    i = p - token;
    token[i] = '\0';                  // Value
  } else {
    token = strtok(NULL, "\"");       // Value or , or }
    if ((token[0] == ',') || (token[0] == '}')) {  // Empty parameter
      token = NULL;
    }
  }
  if (token == NULL) {
    svalue[0] = '\0';
  } else {
    memcpy(svalue, token, strlen(token)+1);
  }
}

char* getStateText(byte state)
{
  if (state > 2) {
    state = 1;
  }
  return sysCfg.state_text[state];
}

/********************************************************************************************/

void stateloop()
{
  uint8_t button = NOT_PRESSED;
  uint8_t flag;
  uint8_t switchflag;
  uint8_t power_now;
  char scmnd[20];
  char log[LOGSZ];
  char svalue[80];  // was MESSZ

  timerxs = millis() + (1000 / STATES);
  state++;
  if (STATES == state) {             // Every second
    state = 0;
  }



  switch (state) {
  case (STATES/10)*2:
    if (otaflag) {
      otaflag--;
      if (2 == otaflag) {
        otaretry = OTA_ATTEMPTS;
        ESPhttpUpdate.rebootOnUpdate(false);
      }
      if (otaflag <= 0) {
        if (sysCfg.webserver) {
          stopWebserver();
        }
        otaflag = 92;
        otaok = 0;
        otaretry--;
        if (otaretry) {
//          snprintf_P(log, sizeof(log), PSTR("OTA: Attempt %d"), OTA_ATTEMPTS - otaretry);
//          addLog(LOG_LEVEL_INFO, log);
          otaok = (HTTP_UPDATE_OK == ESPhttpUpdate.update(sysCfg.otaUrl));
          if (!otaok) {
            otaflag = 2;
          }
        }
      }
      if (90 == otaflag) {  // Allow MQTT to reconnect
        otaflag = 0;
        if (otaok) {
          setModuleFlashMode(1);  // QIO - ESP8266, DOUT - ESP8285 (Sonoff 4CH and Touch)
          snprintf_P(svalue, sizeof(svalue), PSTR("Successful. Restarting"));
        } else {
          snprintf_P(svalue, sizeof(svalue), PSTR("Failed %s"), ESPhttpUpdate.getLastErrorString().c_str());
        }
        restartflag = 2;  // Restart anyway to keep memory clean webserver
      }
    }
    break;
  case (STATES/10)*4:
    if (restartflag) {
      if (211 == restartflag) {
        CFG_Default();
        restartflag = 2;
      }
      if (212 == restartflag) {
        CFG_Erase();
        CFG_Default();
        restartflag = 2;
      }
      CFG_Save();
      restartflag--;
      if (restartflag <= 0) {
        addLog_P(LOG_LEVEL_INFO, PSTR("APP: Restarting"));
        ESP.restart();
      }
    }
    break;
  case (STATES/10)*8:
    break;
  }
}

/********************************************************************************************/

void serial()
{
  char log[LOGSZ];

  while (Serial.available()) {
    yield();
    SerialInByte = Serial.read();

    // Sonoff dual 19200 baud serial interface
    if (Hexcode) {
      Hexcode--;
      if (Hexcode) {
        ButtonCode = (ButtonCode << 8) | SerialInByte;
        SerialInByte = 0;
      } else {
        if (SerialInByte != 0xA1) {
          ButtonCode = 0;  // 0xA1 - End of Sonoff dual button code
        }
      }
    }
    if (0xA0 == SerialInByte) {                    // 0xA0 - Start of Sonoff dual button code
      SerialInByte = 0;
      ButtonCode = 0;
      Hexcode = 3;
    }

    if (SerialInByte > 127) { // binary data...
      SerialInByteCounter = 0;
      Serial.flush();
      return;
    }
    if (isprint(SerialInByte)) {
      if (SerialInByteCounter < INPUT_BUFFER_SIZE) {  // add char to string if it still fits
        serialInBuf[SerialInByteCounter++] = SerialInByte;
      } else {
        SerialInByteCounter = 0;
      }
    }
  }
}

/********************************************************************************************/


extern "C" {
extern struct rst_info resetInfo;
}


const int LEDPIN = D4;
//const int MOTORPWM_A = D1;
//const int MOTORPWM_B = D2;
//const int MOTORPIN_A = D3;
//const int MOTORPIN_B = D4;
const int SHUTTERPIN = D7;




int pulse = 500;
int pulse_d = 490;
int interval = 500;
int interval_d = 10;
int frames = 20;
int motor_a_cmd = 0;
int motor_b_cmd = 0;

int motor_direction = _CW;
int pwm = 100;

//Motor shiled I2C Address: 0x30
//PWM frequency: 1000Hz(1kHz)
Motor M1(0x30,_MOTOR_A, 1000);//Motor A
Motor M2(0x30,_MOTOR_B, 1000);//Motor B

int next_state = 0;

/*
 * State machine for when we're running a timelapse. 
 * This means we can still handle web requests over Wifi for the GUI while
 * the timelapse is running.
 * Each state sets the timer for the current state and moves onto the next. 
 * When we reach the final state, go back to the beginning. 
 */
void timelapse (int pulse, int interval, int motor) {
  
  int current;

  //current = ESP.getCycleCount() / 160000L;
  current = millis();
  
  if (current < next_state)
    return;

  if (current - next_state > 10)
    Serial.printf("Overshot by %d\n", current - next_state);

  switch(sysCfg.timelapse_running) {

    // Timelapse not running, reset the timer, and return
    case STOPPED:
      next_state = current + 100;
      return;

    // Start the motor
    case PULSE_ON:
      Serial.printf("Pulse On %d\n", current);
      if (motor)
        motor_a_cmd = 1;
      else
        motor_b_cmd = 1;
      next_state = current + sysCfg.timelapse_pulse;
      break;

    // Stop the motor
    case PULSE_OFF:
      Serial.printf("Pulse Off %d\n", current);
      if (motor)
        motor_a_cmd = 2;
      else
        motor_b_cmd = 2;
      next_state = current + pulse_d;
      break;

    // Open the shutter
    case SHUTTER_OPEN:
      sysCfg.timelapse_count++;
      Serial.printf("Shutter On %d\n", current);
      digitalWrite(LEDPIN, LOW);
      digitalWrite(SHUTTERPIN, LOW);
      if (sysCfg.timelapse_interval < (pulse_d + interval_d + sysCfg.timelapse_pulse))
        sysCfg.timelapse_interval = (pulse_d + interval_d + sysCfg.timelapse_pulse);
      next_state = current + sysCfg.timelapse_interval - (pulse_d + interval_d + sysCfg.timelapse_pulse);
      break;

    // Close the shutter
    case SHUTTER_CLOSE:
      Serial.printf("Shutter Off %d\n", current);
      digitalWrite(LEDPIN, HIGH);
      digitalWrite(SHUTTERPIN, HIGH);
      next_state = current + interval_d;
      break;

    // How the heck did we get here? :)
    default:
      //Serial.println("Unknown State");
      break;      
  }

  sysCfg.timelapse_running++;
  if (sysCfg.timelapse_running > SHUTTER_CLOSE)
    sysCfg.timelapse_running = PULSE_ON;

  /* Check the frame limit once each time around the state machine loop. */
  if (sysCfg.timelapse_running == PULSE_ON) {
    Serial.printf("Count %d of %d\n", sysCfg.timelapse_count, sysCfg.timelapse_frames);
    if (sysCfg.timelapse_count >= sysCfg.timelapse_frames) {
      Serial.printf("STOPPING\n");
      next_state = STOPPED;
      sysCfg.timelapse_running =  STOPPED;
    }
  }

}


void timer0_ISR (void) {
  timelapse(pulse, interval, 0);
}

void timer1_ISR (void) {
  timelapse(pulse, interval, 1);
}

void setup()
{
  char log[LOGSZ];
  byte idx;

  Serial.begin(Baudrate);
  delay(10);
  Serial.println();
  seriallog_level = LOG_LEVEL_INFO;  // Allow specific serial messages until config loaded

  snprintf_P(Version, sizeof(Version), PSTR("%d.%d.%d"), VERSION >> 24 & 0xff, VERSION >> 16 & 0xff, VERSION >> 8 & 0xff);
  if (VERSION & 0x1f) {
    idx = strlen(Version);
    Version[idx] = 96 + (VERSION & 0x1f);
    Version[idx +1] = 0;
  }
  CFG_Load();
  CFG_Delta();


  sysCfg.bootcount++;
  snprintf_P(log, sizeof(log), PSTR("APP: Bootcount %d"), sysCfg.bootcount);
  addLog(LOG_LEVEL_DEBUG, log);
  savedatacounter = sysCfg.savedata;
  seriallog_timer = SERIALLOG_TIMER;
  seriallog_level = sysCfg.seriallog_level;
  syslog_level = sysCfg.syslog_level;
  sleep = sysCfg.sleep;

  if (Serial.baudRate() != Baudrate) {
    if (seriallog_level) {
      snprintf_P(log, sizeof(log), PSTR("APP: Set baudrate to %d"), Baudrate);
      addLog(LOG_LEVEL_INFO, log);
    }
    delay(100);
    Serial.flush();
    Serial.begin(Baudrate);
    delay(10);
    Serial.println();
  }

  if (strstr(sysCfg.hostname, "%")) {
    strlcpy(sysCfg.hostname, WIFI_HOSTNAME, sizeof(sysCfg.hostname));
    snprintf_P(Hostname, sizeof(Hostname)-1, sysCfg.hostname, sysCfg.mqtt_topic, ESP.getChipId() & 0x1FFF);
  } else {
    snprintf_P(Hostname, sizeof(Hostname)-1, sysCfg.hostname);
  }

  beginWifiManager();

  pinMode(LEDPIN, OUTPUT);
  digitalWrite(LEDPIN, HIGH);
  pinMode(SHUTTERPIN, OUTPUT);
  digitalWrite(SHUTTERPIN, HIGH);

  // Make sure stopped at reboot.
  sysCfg.timelapse_running = STOPPED;
  
  if (sysCfg.timelapse_pulse <= 0)
    sysCfg.timelapse_pulse = 80;
  if (sysCfg.timelapse_interval <= 0)
    sysCfg.timelapse_interval = 3000;
}

void loop()
{
  pollDnsWeb();

  if (millis() >= timerxs) {
    stateloop();
  }
  
  timelapse(pulse, interval, 0);

  if (motor_a_cmd == 1) {
    M1.setmotor(motor_direction, 50);
  }
  if (motor_a_cmd == 2) {
    M1.setmotor(_STOP, 50);
  }
  motor_a_cmd = 0;

  if (motor_b_cmd == 1) {
    M2.setmotor(motor_direction, 50);
  }
  if (motor_b_cmd == 2) {
    M2.setmotor(_STOP, 50);
  }
  motor_b_cmd = 0;

  delay(1);  // https://github.com/esp8266/Arduino/issues/2021
}
