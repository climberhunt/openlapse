/*
Copyright (c) 2017 Theo Arends.  All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

- Redistributions of source code must retain the above copyright notice,
  this list of conditions and the following disclaimer.
- Redistributions in binary form must reproduce the above copyright notice,
  this list of conditions and the following disclaimer in the documentation
  and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
*/

/*********************************************************************************************\
 * RTC memory
\*********************************************************************************************/

#define RTC_MEM_VALID 0xA55A

uint32_t _rtcHash = 0;

uint32_t getRtcHash()
{
  uint32_t hash = 0;
  uint8_t *bytes = (uint8_t*)&rtcMem;

  for (uint16_t i = 0; i < sizeof(RTCMEM); i++) {
    hash += bytes[i]*(i+1);
  }
  return hash;
}

void RTC_Save()
{
  if (getRtcHash() != _rtcHash) {
    rtcMem.valid = RTC_MEM_VALID;
    ESP.rtcUserMemoryWrite(100, (uint32_t*)&rtcMem, sizeof(RTCMEM));
    _rtcHash = getRtcHash();
#ifdef DEBUG_THEO
    addLog_P(LOG_LEVEL_DEBUG, PSTR("Dump: Save"));
    RTC_Dump();
#endif  // DEBUG_THEO
  }
}

void RTC_Load()
{
  ESP.rtcUserMemoryRead(100, (uint32_t*)&rtcMem, sizeof(RTCMEM));
#ifdef DEBUG_THEO
  addLog_P(LOG_LEVEL_DEBUG, PSTR("Dump: Load"));
  RTC_Dump();
#endif  // DEBUG_THEO
  if (rtcMem.valid != RTC_MEM_VALID) {
    memset(&rtcMem, 0x00, sizeof(RTCMEM));
    rtcMem.valid = RTC_MEM_VALID;
    RTC_Save();
  }
  _rtcHash = getRtcHash();
}

boolean RTC_Valid()
{
  return (RTC_MEM_VALID == rtcMem.valid);
}

#ifdef DEBUG_THEO
void RTC_Dump()
{
  #define CFG_COLS 16
  
  char log[LOGSZ];
  uint16_t idx;
  uint16_t maxrow;
  uint16_t row;
  uint16_t col;

  uint8_t *buffer = (uint8_t *) &rtcMem;
  maxrow = ((sizeof(RTCMEM)+CFG_COLS)/CFG_COLS);

  for (row = 0; row < maxrow; row++) {
    idx = row * CFG_COLS;
    snprintf_P(log, sizeof(log), PSTR("%04X:"), idx);
    for (col = 0; col < CFG_COLS; col++) {
      if (!(col%4)) {
        snprintf_P(log, sizeof(log), PSTR("%s "), log);
      }
      snprintf_P(log, sizeof(log), PSTR("%s %02X"), log, buffer[idx + col]);
    }
    snprintf_P(log, sizeof(log), PSTR("%s |"), log);
    for (col = 0; col < CFG_COLS; col++) {
//      if (!(col%4)) {
//        snprintf_P(log, sizeof(log), PSTR("%s "), log);
//      }
      snprintf_P(log, sizeof(log), PSTR("%s%c"), log, ((buffer[idx + col] > 0x20) && (buffer[idx + col] < 0x7F)) ? (char)buffer[idx + col] : ' ');
    }
    snprintf_P(log, sizeof(log), PSTR("%s|"), log);
    addLog(LOG_LEVEL_INFO, log);
  }
}
#endif  // DEBUG_THEO

/*********************************************************************************************\
 * Config - Flash
\*********************************************************************************************/

extern "C" {
#include "spi_flash.h"
}
#include "eboot_command.h"

extern "C" uint32_t _SPIFFS_end;

#define SPIFFS_END          ((uint32_t)&_SPIFFS_end - 0x40200000) / SPI_FLASH_SEC_SIZE

// Version 3.x config
#define CFG_LOCATION_3      SPIFFS_END - 4

// Version 4.2 config = eeprom area
#define CFG_LOCATION        SPIFFS_END  // No need for SPIFFS as it uses EEPROM area

uint32_t _cfgHash = 0;

/********************************************************************************************/
/*
 * Based on cores/esp8266/Updater.cpp
 */
void setFlashMode(byte option, byte mode)
{
  char log[LOGSZ];
  uint8_t *_buffer;
  uint32_t address;

// option 0 - Use absolute address 0
// option 1 - Use OTA/Upgrade relative address

  if (option) {
    eboot_command ebcmd;
    eboot_command_read(&ebcmd);
    address = ebcmd.args[0];
  } else {
    address = 0;
  }
  _buffer = new uint8_t[FLASH_SECTOR_SIZE];
  if (SPI_FLASH_RESULT_OK == spi_flash_read(address, (uint32_t*)_buffer, FLASH_SECTOR_SIZE)) {
    if (_buffer[2] != mode) {
      _buffer[2] = mode &3;
      noInterrupts();
      if (SPI_FLASH_RESULT_OK == spi_flash_erase_sector(address / FLASH_SECTOR_SIZE)) {
        spi_flash_write(address, (uint32_t*)_buffer, FLASH_SECTOR_SIZE);
      }
      interrupts();
      snprintf_P(log, sizeof(log), PSTR("FLSH: Set Flash Mode to %d"), (option) ? mode : ESP.getFlashChipMode());
      addLog(LOG_LEVEL_DEBUG, log);
    }
  }
  delete[] _buffer;
}

void setModuleFlashMode(byte option)
{
  uint8_t mode = 0;  // QIO - ESP8266
  setFlashMode(option, mode);
}

uint32_t getHash()
{
  uint32_t hash = 0;
  uint8_t *bytes = (uint8_t*)&sysCfg;

  for (uint16_t i = 0; i < sizeof(SYSCFG); i++) {
    hash += bytes[i]*(i+1);
  }
  return hash;
}

/*********************************************************************************************\
 * Config Save - Save parameters to Flash ONLY if any parameter has changed
\*********************************************************************************************/

void CFG_Save()
{
  char log[LOGSZ];

#ifndef BE_MINIMAL
  if (getHash() != _cfgHash) {
    noInterrupts();
    sysCfg.saveFlag++;
    spi_flash_erase_sector(CFG_LOCATION);
    spi_flash_write(CFG_LOCATION * SPI_FLASH_SEC_SIZE, (uint32*)&sysCfg, sizeof(SYSCFG));
    interrupts();
    snprintf_P(log, sizeof(log), PSTR("Config: Saved configuration (%d bytes) to flash at %X and count %d"), sizeof(SYSCFG), CFG_LOCATION, sysCfg.saveFlag);
    addLog(LOG_LEVEL_DEBUG, log);
    _cfgHash = getHash();
  }
#endif  // BE_MINIMAL
  RTC_Save();
}

void CFG_Load()
{
  char log[LOGSZ];

  struct SYSCFGH {
    unsigned long cfg_holder;
    unsigned long saveFlag;
  } _sysCfgH;

  noInterrupts();
  spi_flash_read(CFG_LOCATION * SPI_FLASH_SEC_SIZE, (uint32*)&sysCfg, sizeof(SYSCFG));
  interrupts();
  snprintf_P(log, sizeof(log), PSTR("Config: Loaded configuration from flash at %X and count %d"), CFG_LOCATION, sysCfg.saveFlag);
  addLog(LOG_LEVEL_DEBUG, log);

  if (sysCfg.cfg_holder != CFG_HOLDER) {
    if ((sysCfg.version < 0x04020000) || (sysCfg.version > 0x06000000)) {
      noInterrupts();
      spi_flash_read((CFG_LOCATION_3) * SPI_FLASH_SEC_SIZE, (uint32*)&sysCfg, sizeof(SYSCFG));
      spi_flash_read((CFG_LOCATION_3 + 1) * SPI_FLASH_SEC_SIZE, (uint32*)&_sysCfgH, sizeof(SYSCFGH));
      if (sysCfg.saveFlag < _sysCfgH.saveFlag)
        spi_flash_read((CFG_LOCATION_3 + 1) * SPI_FLASH_SEC_SIZE, (uint32*)&sysCfg, sizeof(SYSCFG));
      interrupts();
      if (sysCfg.cfg_holder != CFG_HOLDER) {
        CFG_Default();
      } else {
        sysCfg.saveFlag = 0;
      }
    } else {
      CFG_Default();
    }
  }
  _cfgHash = getHash();

  RTC_Load();
}

void CFG_Erase()
{
  char log[LOGSZ];
  SpiFlashOpResult result;

  uint32_t _sectorStart = (ESP.getSketchSize() / SPI_FLASH_SEC_SIZE) + 1;
  uint32_t _sectorEnd = ESP.getFlashChipRealSize() / SPI_FLASH_SEC_SIZE;
  boolean _serialoutput = (LOG_LEVEL_DEBUG_MORE <= seriallog_level);

  snprintf_P(log, sizeof(log), PSTR("Config: Erasing %d flash sectors"), _sectorEnd - _sectorStart);
  addLog(LOG_LEVEL_DEBUG, log);

  for (uint32_t _sector = _sectorStart; _sector < _sectorEnd; _sector++) {
    noInterrupts();
    result = spi_flash_erase_sector(_sector);
    interrupts();
    if (_serialoutput) {
      Serial.print(F("Flash: Erased sector "));
      Serial.print(_sector);
      if (SPI_FLASH_RESULT_OK == result) {
        Serial.println(F(" OK"));
      } else {
        Serial.println(F(" Error"));
      }
      delay(10);
    }
  }
}

void CFG_Dump()
{
  #define CFG_COLS 16
  
  char log[LOGSZ];
  uint16_t idx;
  uint16_t maxrow;
  uint16_t row;
  uint16_t col;

  uint8_t *buffer = (uint8_t *) &sysCfg;
  maxrow = ((sizeof(SYSCFG)+CFG_COLS)/CFG_COLS);

  for (row = 0; row < maxrow; row++) {
    idx = row * CFG_COLS;
    snprintf_P(log, sizeof(log), PSTR("%04X:"), idx);
    for (col = 0; col < CFG_COLS; col++) {
      if (!(col%4)) {
        snprintf_P(log, sizeof(log), PSTR("%s "), log);
      }
      snprintf_P(log, sizeof(log), PSTR("%s %02X"), log, buffer[idx + col]);
    }
    snprintf_P(log, sizeof(log), PSTR("%s |"), log);
    for (col = 0; col < CFG_COLS; col++) {
//      if (!(col%4)) {
//        snprintf_P(log, sizeof(log), PSTR("%s "), log);
//      }
      snprintf_P(log, sizeof(log), PSTR("%s%c"), log, ((buffer[idx + col] > 0x20) && (buffer[idx + col] < 0x7F)) ? (char)buffer[idx + col] : ' ');
    }
    snprintf_P(log, sizeof(log), PSTR("%s|"), log);
    addLog(LOG_LEVEL_INFO, log);
  }
}

/********************************************************************************************/

void CFG_DefaultSet1()
{
  memset(&sysCfg, 0x00, sizeof(SYSCFG));

  sysCfg.cfg_holder = CFG_HOLDER;
  sysCfg.saveFlag = 0;
  sysCfg.version = VERSION;
  sysCfg.bootcount = 0;
}
  
void CFG_DefaultSet2()
{
  sysCfg.savedata = SAVE_DATA;
  sysCfg.savestate = SAVE_STATE;
  sysCfg.timezone = APP_TIMEZONE;
  strlcpy(sysCfg.otaUrl, OTA_URL, sizeof(sysCfg.otaUrl));

  sysCfg.seriallog_level = SERIAL_LOG_LEVEL;
  sysCfg.sta_active = 0;
  strlcpy(sysCfg.hostname, WIFI_HOSTNAME, sizeof(sysCfg.hostname));
  strlcpy(sysCfg.syslog_host, SYS_LOG_HOST, sizeof(sysCfg.syslog_host));
  sysCfg.syslog_port = SYS_LOG_PORT;
  sysCfg.syslog_level = SYS_LOG_LEVEL;
  sysCfg.webserver = WEB_SERVER;
  sysCfg.weblog_level = WEB_LOG_LEVEL;

  sysCfg.value_units = 0;
  sysCfg.button_restrict = 0;

  sysCfg.power = APP_POWER;
  sysCfg.poweronstate = APP_POWERON_STATE;
  sysCfg.sleep = APP_SLEEP;

  CFG_DefaultSet_3_2_4();

  strlcpy(sysCfg.friendlyname[0], FRIENDLY_NAME, sizeof(sysCfg.friendlyname[0]));
  strlcpy(sysCfg.friendlyname[1], FRIENDLY_NAME"2", sizeof(sysCfg.friendlyname[1]));
  strlcpy(sysCfg.friendlyname[2], FRIENDLY_NAME"3", sizeof(sysCfg.friendlyname[2]));
  strlcpy(sysCfg.friendlyname[3], FRIENDLY_NAME"4", sizeof(sysCfg.friendlyname[3]));

  CFG_DefaultSet_3_9_3();

  strlcpy(sysCfg.switch_topic, "0", sizeof(sysCfg.switch_topic));

  strlcpy(sysCfg.web_password, WEB_PASSWORD, sizeof(sysCfg.web_password));

  CFG_DefaultSet_4_0_4();
  sysCfg.pulsetime[0] = APP_PULSETIME;

  // 4.0.7
  for (byte i = 0; i < 5; i++) sysCfg.pwmvalue[i] = 0;

  // 4.0.9
  CFG_DefaultSet_4_0_9();

  // 4.1.1
  CFG_DefaultSet_4_1_1();

}

void CFG_DefaultSet_3_2_4()
{
}

void CFG_DefaultSet_3_9_3()
{

}

void CFG_DefaultSet_4_0_4()
{
}

void CFG_DefaultSet_4_0_9()
{
  parseIP(&sysCfg.ip_address[0], WIFI_IP_ADDRESS);
  parseIP(&sysCfg.ip_address[1], WIFI_GATEWAY);
  parseIP(&sysCfg.ip_address[2], WIFI_SUBNETMASK);
  parseIP(&sysCfg.ip_address[3], WIFI_DNS);
}

void CFG_DefaultSet_4_1_1()
{
}

void CFG_Default()
{
  addLog_P(LOG_LEVEL_NONE, PSTR("Config: Use default configuration"));
  CFG_DefaultSet1();
  CFG_DefaultSet2();
  CFG_Save();
}

/********************************************************************************************/

void CFG_Delta()
{
  if (sysCfg.version != VERSION) {      // Fix version dependent changes
    if (sysCfg.version < 0x03000600) {  // 3.0.6 - Add parameter
      sysCfg.ex_pulsetime = APP_PULSETIME;
    }
    if (sysCfg.version < 0x03010200) {  // 3.1.2 - Add parameter
      sysCfg.poweronstate = APP_POWERON_STATE;
    }
    if (sysCfg.version < 0x03010600) {  // 3.1.6 - Add parameter
      sysCfg.blinktime = APP_BLINKTIME;
      sysCfg.blinkcount = APP_BLINKCOUNT;
    }
    if (sysCfg.version < 0x03020400) {  // 3.2.4 - Add parameter
      CFG_DefaultSet_3_2_4();
    }
    if (sysCfg.version < 0x03020500) {  // 3.2.5 - Add parameter
      getClient(sysCfg.friendlyname[0], sysCfg.mqtt_client, sizeof(sysCfg.friendlyname[0]));
      strlcpy(sysCfg.friendlyname[1], FRIENDLY_NAME"2", sizeof(sysCfg.friendlyname[1]));
      strlcpy(sysCfg.friendlyname[2], FRIENDLY_NAME"3", sizeof(sysCfg.friendlyname[2]));
      strlcpy(sysCfg.friendlyname[3], FRIENDLY_NAME"4", sizeof(sysCfg.friendlyname[3]));
    }      
    if (sysCfg.version < 0x03020800) {  // 3.2.8 - Add parameter
      strlcpy(sysCfg.switch_topic, sysCfg.button_topic, sizeof(sysCfg.switch_topic));
    }
    if (sysCfg.version < 0x03020C00) {  // 3.2.12 - Add parameter
      sysCfg.sleep = APP_SLEEP;
    }
    if (sysCfg.version < 0x03090300) {  // 3.9.2d - Add parameter
      CFG_DefaultSet_3_9_3();
    }
    if (sysCfg.version < 0x03091400) {
      strlcpy(sysCfg.web_password, WEB_PASSWORD, sizeof(sysCfg.web_password));
    }
    if (sysCfg.version < 0x03091500) {
      for (byte i = 0; i < 4; i++) sysCfg.switchmode[i] = sysCfg.ex_switchmode;
    }
    if (sysCfg.version < 0x04000200) {
      sysCfg.button_restrict = 0;
    }
    if (sysCfg.version < 0x04000400) {
      CFG_DefaultSet_4_0_4();
    }
    if (sysCfg.version < 0x04000700) {
      for (byte i = 0; i < 5; i++) {
        sysCfg.pwmvalue[i] = 0;
      }
    }
    if (sysCfg.version < 0x04000804) {
      CFG_DefaultSet_4_0_9();
    }
    if (sysCfg.version < 0x04010100) {
      CFG_DefaultSet_4_1_1();
    }
    sysCfg.version = VERSION;
  }
}


