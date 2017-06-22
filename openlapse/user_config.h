/*********************************************************************************************\
 * User specific configuration parameters
 *
 * ATTENTION: Changes to most PARAMETER defines will only override flash settings if you change
 *            define CFG_HOLDER.
 *            Most parameters can be changed online using commands via WebConsole or serial
 *
 * Corresponding Serial/Console commands in [brackets]
\*********************************************************************************************/

// -- Project -------------------------------------
#define PROJECT                "openlapse"       // PROJECT is used as the default topic delimiter and OTA file name
                                                 //   As an IDE restriction it needs to be the same as the main .ino file
#define MODULE_NAME            "WeMos D1 mini"

#define CFG_HOLDER             0x2016120a        // [Reset 1] Change this value to load following default configuration parameters
#define SAVE_DATA              1                 // [SaveData] Save changed parameters to Flash (0 = disable, 1 - 3600 seconds)
#define SAVE_STATE             1                 // [SaveState] Save changed power state to Flash (0 = disable, 1 = enable)

// -- Wifi ----------------------------------------
#define WIFI_IP_ADDRESS        "192.168.1.1"     // [IpAddress1] Set to 0.0.0.0 for using DHCP or IP address
#define WIFI_GATEWAY           "192.168.1.254"   // {IpAddress2] If not using DHCP set Gateway IP address
#define WIFI_SUBNETMASK        "255.255.255.0"   // [IpAddress3] If not using DHCP set Network mask
#define WIFI_DNS               "192.168.1.254"   // [IpAddress4] If not using DHCP set DNS IP address (might be equal to WIFI_GATEWAY)

#define STA_SSID1              "MATRIX7"         // [Ssid1] Wifi SSID
#define STA_PASS1              "1hxgi3tzw1"      // [Password1] Wifi password
#define STA_SSID2              "NETGEAR"         // [Ssid2] Optional alternate AP Wifi SSID
#define STA_PASS2              "Heaven17"        // [Password2] Optional alternate AP Wifi password
                                                 
// -- Syslog --------------------------------------
#define SYS_LOG_HOST           "10.4.11.82"      // [LogHost] (Linux) syslog host
#define SYS_LOG_PORT           514               // [LogPort] default syslog UDP port
#define SYS_LOG_LEVEL          LOG_LEVEL_NONE    // [SysLog]
#define SERIAL_LOG_LEVEL       LOG_LEVEL_INFO    // [SerialLog]
#define WEB_LOG_LEVEL          LOG_LEVEL_INFO    // [WebLog]

// -- Ota -----------------------------------------
#define OTA_URL                "http://domus1:80/api/arduino/" PROJECT ".ino.bin"  // [OtaUrl]

// -- HTTP ----------------------------------------
#define WEB_SERVER           2                 // [WebServer] Web server (0 = Off, 1 = Start as User, 2 = Start as Admin)
#define WEB_PORT             80                // Web server Port for User and Admin mode
#define WEB_USERNAME         "admin"           // Web server Admin mode user name
#define WEB_PASSWORD         ""                // [WebPassword] Web server Admin mode Password for WEB_USERNAME (empty string = Disable)
#define FRIENDLY_NAME        "Sonoff"          // [FriendlyName] Friendlyname up to 32 characters used by webpages and Alexa

// -- Application ---------------------------------
#define APP_TIMEZONE           1                 // [Timezone] +1 hour (Amsterdam) (-12 .. 12 = hours from UTC, 99 = use TIME_DST/TIME_STD)
#define APP_PULSETIME          0                 // [PulseTime] Time in 0.1 Sec to turn off power for relay 1 (0 = disabled)
#define APP_POWERON_STATE      3                 // [PowerOnState] Power On Relay state (0 = Off, 1 = On, 2 = Toggle Saved state, 3 = Saved state)
#define APP_BLINKTIME          10                // [BlinkTime] Time in 0.1 Sec to blink/toggle power for relay 1
#define APP_BLINKCOUNT         10                // [BlinkCount] Number of blinks (0 = 32000)
#define APP_SLEEP              0                 // [Sleep] Sleep time to lower energy consumption (0 = Off, 1 - 250 mSec)

#define SWITCH_MODE            TOGGLE            // [SwitchMode] TOGGLE, FOLLOW, FOLLOW_INV, PUSHBUTTON or PUSHBUTTON_INV (the wall switch state)

#define TEMP_CONVERSION        0                 // Convert temperature to (0 = Celsius or 1 = Fahrenheit)
#define TEMP_RESOLUTION        1                 // Maximum number of decimals (0 - 3) showing sensor Temperature
#define HUMIDITY_RESOLUTION    1                 // Maximum number of decimals (0 - 3) showing sensor Humidity
#define PRESSURE_RESOLUTION    1                 // Maximum number of decimals (0 - 3) showing sensor Pressure
#define ENERGY_RESOLUTION      3                 // Maximum number of decimals (0 - 5) showing energy usage in kWh

// -- Sensor code selection -----------------------
#define USE_ADC_VCC                              // Display Vcc in Power status. Disable for use as Analog input on selected devices

//#define USE_I2C                                  // I2C using library wire (+10k code, 0.2k mem) - Disable by //
  #define USE_BH1750                             // Add I2C code for BH1750 sensor
  #define USE_BMP                                // Add I2C code for BMP/BME280 sensor
  #define USE_HTU                                // Add I2C code for HTU21/SI7013/SI7020/SI7021 sensor
  #define USE_SHT                                // Add I2C emulating code for SHT1X sensor

//#define USE_IR_REMOTE                            // Send IR remote commands using library IRremoteESP8266 and ArduinoJson (+3k code, 0.3k mem)
//  #define USE_IR_HVAC                            // Support for HVAC system using IR (+2k code)

#define STOPPED       0
#define PULSE_ON      1
#define PULSE_OFF     2
#define SHUTTER_OPEN  3
#define SHUTTER_CLOSE 4


/*********************************************************************************************\
 * Compile a minimal version if upgrade memory gets tight ONLY TO BE USED FOR UPGRADE STEP 1!
 *   To be used as step 1 during upgrade. 
 *   Step 2 is re-compile with option BE_MINIMAL commented out.
 *   !!! Needed for next release of Arduino/ESP8266 (+22k code, +2k mem) !!!
\*********************************************************************************************/

//#define BE_MINIMAL                               // Minimal version if upgrade memory gets tight (-45k code, -2k mem)

/*********************************************************************************************\
 * No user configurable items below
\*********************************************************************************************/

#if (ARDUINO < 10610)
  #error "This software is supported with Arduino IDE starting from 1.6.10 and ESP8266 Release 2.3.0"
#endif

