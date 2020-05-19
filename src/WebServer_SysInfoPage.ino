#include "src/Globals/RTC.h"
#include "src/DataStructs/RTCStruct.h"
#include "src/Globals/CRCValues.h"
#include "src/Static/WebStaticData.h"

#ifdef WEBSERVER_NEW_UI

// ********************************************************************************
// Web Interface sysinfo page
// ********************************************************************************
void handle_sysinfo_json() {
  checkRAM(F("handle_sysinfo"));

  if (!isLoggedIn()) { return; }
  TXBuffer.startJsonStream();
  json_init();
  json_open();
  json_open(false, F("general"));
  json_number(F("unit"), String(Settings.Unit));
  json_prop(F("time"),   node_time.getDateTimeString('-', ':', ' '));
  json_prop(F("uptime"), getExtendedValue(LabelType::UPTIME));
  json_number(F("cpu_load"),   String(getCPUload()));
  json_number(F("loop_count"), String(getLoopCountPerSec()));
  json_close();

  int freeMem = ESP.getFreeHeap();
  json_open(false, F("mem"));
  json_number(F("free"),    String(freeMem));
  json_number(F("low_ram"), String(lowestRAM));
  json_prop(F("low_ram_fn"), String(lowestRAMfunction));
  json_number(F("stack"),     String(getCurrentFreeStack()));
  json_number(F("low_stack"), String(lowestFreeStack));
  json_prop(F("low_stack_fn"), lowestFreeStackfunction);
  json_close();
  json_open(false, F("boot"));
  json_prop(F("last_cause"), getLastBootCauseString());
  json_number(F("counter"), String(RTC.bootCounter));
  json_prop(F("reset_reason"), getResetReasonString());
  json_close();
  json_open(false, F("wifi"));

  # if defined(ESP8266)
  byte PHYmode = wifi_get_phy_mode();
  # endif // if defined(ESP8266)
  # if defined(ESP32)
  byte PHYmode = 3; // wifi_get_phy_mode();
  # endif // if defined(ESP32)

  switch (PHYmode)
  {
    case 1:
      json_prop(F("type"), F("802.11B"));
      break;
    case 2:
      json_prop(F("type"), F("802.11G"));
      break;
    case 3:
      json_prop(F("type"), F("802.11N"));
      break;
  }
  json_number(F("rssi"), String(WiFi.RSSI()));
  json_prop(F("dhcp"),          useStaticIP() ? getLabel(LabelType::IP_CONFIG_STATIC) : getLabel(LabelType::IP_CONFIG_DYNAMIC));
  json_prop(F("ip"),            formatIP(WiFi.localIP()));
  json_prop(F("subnet"),        formatIP(WiFi.subnetMask()));
  json_prop(F("gw"),            formatIP(WiFi.gatewayIP()));
  json_prop(F("dns1"),          formatIP(WiFi.dnsIP(0)));
  json_prop(F("dns2"),          formatIP(WiFi.dnsIP(1)));
  json_prop(F("allowed_range"), describeAllowedIPrange());


  uint8_t  mac[]   = { 0, 0, 0, 0, 0, 0 };
  uint8_t *macread = WiFi.macAddress(mac);
  char     macaddress[20];
  formatMAC(macread, macaddress);

  json_prop(F("sta_mac"), macaddress);

  macread = WiFi.softAPmacAddress(mac);
  formatMAC(macread, macaddress);

  json_prop(F("ap_mac"), macaddress);
  json_prop(F("ssid"),   WiFi.SSID());
  json_prop(F("bssid"),  WiFi.BSSIDstr());
  json_number(F("channel"), String(WiFi.channel()));
  json_prop(F("connected"), format_msec_duration(timeDiff(lastConnectMoment, millis())));
  json_prop(F("ldr"),       getLastDisconnectReason());
  json_number(F("reconnects"), String(wifi_reconnects));
  json_close();

  json_open(false, F("firmware"));
  json_prop(F("build"),       String(BUILD));
  json_prop(F("notes"),       F(BUILD_NOTES));
  json_prop(F("libraries"),   getSystemLibraryString());
  json_prop(F("git_version"), F(BUILD_GIT));
  json_prop(F("plugins"),     getPluginDescriptionString());
  json_prop(F("md5"),         String(CRCValues.compileTimeMD5[0], HEX));
  json_number(F("md5_check"), String(CRCValues.checkPassed()));
  json_prop(F("build_time"), get_build_time());
  json_prop(F("filename"),   getValue(LabelType::BINARY_FILENAME));
  json_prop(F("build_platform")), getValue(LabelType::BUILD_PLATFORM);
  json_prop(F("git_head")), getValue(LabelType::GIT_HEAD);
  json_close();

  json_open(false, F("esp"));

  # if defined(ESP8266)
  json_prop(F("chip_id"), String(ESP.getChipId(), HEX));
  json_number(F("cpu"), String(ESP.getCpuFreqMHz()));
  # endif // if defined(ESP8266)
  # if defined(ESP32)


  uint64_t chipid  = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
  uint32_t ChipId1 = (uint16_t)(chipid >> 32);
  String   espChipIdS(ChipId1, HEX);
  espChipIdS.toUpperCase();

  json_prop(F("chip_id"), espChipIdS);
  json_prop(F("cpu"),     String(ESP.getCpuFreqMHz()));

  String espChipIdS1(ChipId1, HEX);
  espChipIdS1.toUpperCase();
  json_prop(F("chip_id1"), espChipIdS1);

  # endif // if defined(ESP32)
  # ifdef ARDUINO_BOARD
  json_prop(F("board"), ARDUINO_BOARD);
  # endif // ifdef ARDUINO_BOARD
  json_close();
  json_open(false, F("storage"));

  # if defined(ESP8266)
  uint32_t flashChipId = ESP.getFlashChipId();

  // Set to HEX may be something like 0x1640E0.
  // Where manufacturer is 0xE0 and device is 0x4016.
  json_number(F("chip_id"), String(flashChipId));

  if (flashChipVendorPuya())
  {
    if (puyaSupport()) {
      json_prop(F("vendor"), F("puya, supported"));
    } else {
      json_prop(F("vendor"), F("puya, error"));
    }
  }
  uint32_t flashDevice = (flashChipId & 0xFF00) | ((flashChipId >> 16) & 0xFF);
  json_number(F("device"),    String(flashDevice));
  # endif // if defined(ESP8266)
  json_number(F("real_size"), String(getFlashRealSizeInBytes() / 1024));
  json_number(F("ide_size"),  String(ESP.getFlashChipSize() / 1024));

  // Please check what is supported for the ESP32
  # if defined(ESP8266)
  json_number(F("flash_speed"), String(ESP.getFlashChipSpeed() / 1000000));

  FlashMode_t ideMode = ESP.getFlashChipMode();

  switch (ideMode) {
    case FM_QIO:   json_prop(F("mode"), F("QIO"));  break;
    case FM_QOUT:  json_prop(F("mode"), F("QOUT")); break;
    case FM_DIO:   json_prop(F("mode"), F("DIO"));  break;
    case FM_DOUT:  json_prop(F("mode"), F("DOUT")); break;
    default:
      json_prop(F("mode"), getUnknownString()); break;
  }
  # endif // if defined(ESP8266)

  json_number(F("writes"),        String(RTC.flashDayCounter));
  json_number(F("flash_counter"), String(RTC.flashCounter));
  json_number(F("sketch_size"),   String(ESP.getSketchSize() / 1024));
  json_number(F("sketch_free"),   String(ESP.getFreeSketchSpace() / 1024));

  json_number(F("spiffs_size"),   String(SpiffsTotalBytes() / 1024));
  json_number(F("spiffs_free"),   String(SpiffsFreeSpace() / 1024));
  json_close();
  json_close();

  TXBuffer.endStream();
}

#endif // WEBSERVER_NEW_UI

#ifdef WEBSERVER_SYSINFO

void handle_sysinfo() {
  checkRAM(F("handle_sysinfo"));

  if (!isLoggedIn()) { return; }
  navMenuIndex = MENU_INDEX_TOOLS;
  html_reset_copyTextCounter();
  TXBuffer.startStream();
  sendHeadandTail_stdtemplate();

  addHtml(printWebString);
  addHtml(F("<form>"));

  // the table header
  html_table_class_normal();


  # ifdef WEBSERVER_GITHUB_COPY

  // Not using addFormHeader() to get the copy button on the same header line as 2nd column
  html_TR();
  html_table_header(F("System Info"), 225);
  addHtml(F("<TH>")); // Needed to get the copy button on the same header line.
  addCopyButton(F("copyText"), F("\\n"), F("Copy info to clipboard"));

  TXBuffer += githublogo;
  html_add_script(false);
  TXBuffer += DATA_GITHUB_CLIPBOARD_JS;
  html_add_script_end();
  # else // ifdef WEBSERVER_GITHUB_COPY
  addFormHeader(F("System Info"));

  # endif // ifdef WEBSERVER_GITHUB_COPY

  handle_sysinfo_basicInfo();

  handle_sysinfo_Network();

  handle_sysinfo_WiFiSettings();

  handle_sysinfo_Firmware();

  handle_sysinfo_SystemStatus();

  handle_sysinfo_ESP_Board();

  handle_sysinfo_Storage();


  html_end_table();
  html_end_form();
  sendHeadandTail_stdtemplate(true);
  TXBuffer.endStream();
}

void handle_sysinfo_basicInfo() {
  addRowLabelValue(LabelType::UNIT_NR);

  if (node_time.systemTimePresent())
  {
    addRowLabelValue(LabelType::LOCAL_TIME);
  }

  addRowLabel(getLabel(LabelType::UPTIME));
  {
    addHtml(getExtendedValue(LabelType::UPTIME));
  }

  addRowLabel(getLabel(LabelType::LOAD_PCT));

  if (wdcounter > 0)
  {
    String html;
    html.reserve(32);
    html += getCPUload();
    html += F("% (LC=");
    html += getLoopCountPerSec();
    html += ')';
    addHtml(html);
  }
  addRowLabelValue(LabelType::CPU_ECO_MODE);

  int freeMem = ESP.getFreeHeap();
  addRowLabel(F("Free Mem"));
  {
    String html;
    html.reserve(64);

    html += freeMem;
    html += " (";
    html += lowestRAM;
    html += F(" - ");
    html += lowestRAMfunction;
    html += ')';
    addHtml(html);
  }
  addRowLabel(F("Free Stack"));
  {
    String html;
    html.reserve(64);
    html += getCurrentFreeStack();
    html += " (";
    html += lowestFreeStack;
    html += F(" - ");
    html += lowestFreeStackfunction;
    html += ')';
    addHtml(html);
  }
# ifdef CORE_POST_2_5_0
  addRowLabelValue(LabelType::HEAP_MAX_FREE_BLOCK);
  addRowLabelValue(LabelType::HEAP_FRAGMENTATION);
  addHtml("%");
# endif // ifdef CORE_POST_2_5_0


  addRowLabel(F("Boot"));
  {
    String html;
    html.reserve(64);

    html += getLastBootCauseString();
    html += " (";
    html += RTC.bootCounter;
    html += ')';
    addHtml(html);
  }
  addRowLabelValue(LabelType::RESET_REASON);
  addRowLabelValue(LabelType::LAST_TASK_BEFORE_REBOOT);
  addRowLabelValue(LabelType::SW_WD_COUNT);
}

void handle_sysinfo_Network() {
  addTableSeparator(F("Network"), 2, 3, F("Wifi"));

  if (WiFiConnected())
  {
    addRowLabel(F("Wifi"));
    # if defined(ESP8266)
    byte PHYmode = wifi_get_phy_mode();
    # endif // if defined(ESP8266)
    # if defined(ESP32)
    byte PHYmode = 3; // wifi_get_phy_mode();
    # endif // if defined(ESP32)

    {
      String html;
      html.reserve(64);

      switch (PHYmode)
      {
        case 1:
          html += F("802.11B");
          break;
        case 2:
          html += F("802.11G");
          break;
        case 3:
          html += F("802.11N");
          break;
      }
      html += F(" (RSSI ");
      html += WiFi.RSSI();
      html += F(" dB)");
      addHtml(html);
    }
  }
  addRowLabelValue(LabelType::IP_CONFIG);
  addRowLabelValue(LabelType::IP_ADDRESS_SUBNET);
  addRowLabelValue(LabelType::GATEWAY);
  addRowLabelValue(LabelType::CLIENT_IP);
  addRowLabelValue(LabelType::DNS);
  addRowLabelValue(LabelType::ALLOWED_IP_RANGE);
  addRowLabel(getLabel(LabelType::STA_MAC));

  {
    uint8_t  mac[]   = { 0, 0, 0, 0, 0, 0 };
    uint8_t *macread = WiFi.macAddress(mac);
    char     macaddress[20];
    formatMAC(macread, macaddress);
    addHtml(macaddress);

    addRowLabel(getLabel(LabelType::AP_MAC));
    macread = WiFi.softAPmacAddress(mac);
    formatMAC(macread, macaddress);
    addHtml(macaddress);
  }

  addRowLabel(getLabel(LabelType::SSID));
  {
    String html;
    html.reserve(64);

    html += WiFi.SSID();
    html += " (";
    html += WiFi.BSSIDstr();
    html += ')';
    addHtml(html);
  }

  addRowLabelValue(LabelType::CHANNEL);
  addRowLabelValue(LabelType::CONNECTED);
  addRowLabel(getLabel(LabelType::LAST_DISCONNECT_REASON));
  addHtml(getValue(LabelType::LAST_DISC_REASON_STR));
  addRowLabelValue(LabelType::NUMBER_RECONNECTS);
}

void handle_sysinfo_WiFiSettings() {
  addTableSeparator(F("WiFi Settings"), 2, 3);
  addRowLabelValue(LabelType::FORCE_WIFI_BG);
  addRowLabelValue(LabelType::RESTART_WIFI_LOST_CONN);
# ifdef ESP8266
  addRowLabelValue(LabelType::FORCE_WIFI_NOSLEEP);
# endif // ifdef ESP8266
# ifdef SUPPORT_ARP
  addRowLabelValue(LabelType::PERIODICAL_GRAT_ARP);
# endif // ifdef SUPPORT_ARP
  addRowLabelValue(LabelType::CONNECTION_FAIL_THRESH);
}

void handle_sysinfo_Firmware() {
  addTableSeparator(F("Firmware"), 2, 3);

  addRowLabelValue_copy(LabelType::BUILD_DESC);
  addHtml(" ");
  addHtml(F(BUILD_NOTES));

  addRowLabelValue_copy(LabelType::SYSTEM_LIBRARIES);
  addRowLabelValue_copy(LabelType::GIT_BUILD);
  addRowLabelValue_copy(LabelType::PLUGIN_COUNT);
  addHtml(" ");
  addHtml(getPluginDescriptionString());

  addRowLabel(F("Build Origin"));
  addHtml(get_build_origin());
  addRowLabelValue_copy(LabelType::BUILD_TIME);
  addRowLabelValue_copy(LabelType::BINARY_FILENAME);
  addRowLabelValue_copy(LabelType::BUILD_PLATFORM);
  addRowLabelValue_copy(LabelType::GIT_HEAD);
}

void handle_sysinfo_SystemStatus() {
  addTableSeparator(F("System Status"), 2, 3);

  // Actual Loglevel
  addRowLabelValue(LabelType::SYSLOG_LOG_LEVEL);
  addRowLabelValue(LabelType::SERIAL_LOG_LEVEL);
  addRowLabelValue(LabelType::WEB_LOG_LEVEL);
    # ifdef FEATURE_SD
  addRowLabelValue(LabelType::SD_LOG_LEVEL);
    # endif // ifdef FEATURE_SD
}

void handle_sysinfo_ESP_Board() {
  addTableSeparator(F("ESP Board"), 2, 3);

  addRowLabel(getLabel(LabelType::ESP_CHIP_ID));
  # if defined(ESP8266)
  {
    String html;
    html.reserve(32);
    html += ESP.getChipId();
    html += F(" (0x");
    String espChipId(ESP.getChipId(), HEX);
    espChipId.toUpperCase();
    html += espChipId;
    html += ')';
    addHtml(html);
  }

  addRowLabel(getLabel(LabelType::ESP_CHIP_FREQ));
  addHtml(String(ESP.getCpuFreqMHz()));
  addHtml(F(" MHz"));
  # endif // if defined(ESP8266)
  # if defined(ESP32)
  {
    String html;
    html.reserve(64);
    html += F(" (0x");
    uint64_t chipid  = ESP.getEfuseMac(); // The chip ID is essentially its MAC address(length: 6 bytes).
    uint32_t ChipId1 = (uint16_t)(chipid >> 32);
    String   espChipIdS(ChipId1, HEX);
    espChipIdS.toUpperCase();
    html   += espChipIdS;
    ChipId1 = (uint32_t)chipid;
    String espChipIdS1(ChipId1, HEX);
    espChipIdS1.toUpperCase();
    html += espChipIdS1;
    html += ')';
    addHtml(html);
  }

  addRowLabel(getLabel(LabelType::ESP_CHIP_FREQ));
  addHtml(String(ESP.getCpuFreqMHz()));
  addHtml(F(" MHz"));
  # endif // if defined(ESP32)
  # ifdef ARDUINO_BOARD
  addRowLabel(getLabel(LabelType::ESP_BOARD_NAME));
  addHtml(ARDUINO_BOARD);
  # endif // ifdef ARDUINO_BOARD
}

void handle_sysinfo_Storage() {
  addTableSeparator(F("Storage"), 2, 3);

  addRowLabel(getLabel(LabelType::FLASH_CHIP_ID));
  # if defined(ESP8266)
  uint32_t flashChipId = ESP.getFlashChipId();

  // Set to HEX may be something like 0x1640E0.
  // Where manufacturer is 0xE0 and device is 0x4016.
  addHtml(F("Vendor: "));
  addHtml(formatToHex(flashChipId & 0xFF));

  if (flashChipVendorPuya())
  {
    addHtml(F(" (PUYA"));

    if (puyaSupport()) {
      addHtml(F(", supported"));
    } else {
      addHtml(F(HTML_SYMBOL_WARNING));
    }
    addHtml(")");
  }
  addHtml(F(" Device: "));
  uint32_t flashDevice = (flashChipId & 0xFF00) | ((flashChipId >> 16) & 0xFF);
  addHtml(formatToHex(flashDevice));
  # endif // if defined(ESP8266)
  uint32_t realSize = getFlashRealSizeInBytes();
  uint32_t ideSize  = ESP.getFlashChipSize();

  addRowLabel(getLabel(LabelType::FLASH_CHIP_REAL_SIZE));
  addHtml(String(realSize / 1024));
  addHtml(F(" kB"));

  addRowLabel(getLabel(LabelType::FLASH_IDE_SIZE));
  addHtml(String(ideSize / 1024));
  addHtml(F(" kB"));

  // Please check what is supported for the ESP32
  # if defined(ESP8266)
  addRowLabel(getLabel(LabelType::FLASH_IDE_SPEED));
  addHtml(String(ESP.getFlashChipSpeed() / 1000000));
  addHtml(F(" MHz"));

  FlashMode_t ideMode = ESP.getFlashChipMode();
  addRowLabel(getLabel(LabelType::FLASH_IDE_MODE));
  {
    String html;

    switch (ideMode) {
      case FM_QIO:   html += F("QIO");  break;
      case FM_QOUT:  html += F("QOUT"); break;
      case FM_DIO:   html += F("DIO");  break;
      case FM_DOUT:  html += F("DOUT"); break;
      default:
        html += getUnknownString(); break;
    }
    addHtml(html);
  }
  # endif // if defined(ESP8266)

  addRowLabel(getLabel(LabelType::FLASH_WRITE_COUNT));
  {
    String html;
    html.reserve(32);
    html += RTC.flashDayCounter;
    html += F(" daily / ");
    html += RTC.flashCounter;
    html += F(" boot");
    addHtml(html);
  }

  {
    // FIXME TD-er: Must also add this for ESP32.
    addRowLabel(getLabel(LabelType::SKETCH_SIZE));
    {
      String html;
      html.reserve(32);
      html += ESP.getSketchSize() / 1024;
      html += F(" kB (");
      html += ESP.getFreeSketchSpace() / 1024;
      html += F(" kB free)");
      addHtml(html);
    }

    uint32_t maxSketchSize;
    bool     use2step;
    # if defined(ESP8266)
    bool     otaEnabled = 
    #endif
      OTA_possible(maxSketchSize, use2step);
    addRowLabel(getLabel(LabelType::MAX_OTA_SKETCH_SIZE));
    {
      String html;
      html.reserve(32);

      html += maxSketchSize / 1024;
      html += F(" kB (");
      html += maxSketchSize;
      html += F(" bytes)");
      addHtml(html);
    }

    # if defined(ESP8266)
    addRowLabel(getLabel(LabelType::OTA_POSSIBLE));
    addHtml(boolToString(otaEnabled));

    addRowLabel(getLabel(LabelType::OTA_2STEP));
    addHtml(boolToString(use2step));
    # endif // if defined(ESP8266)

  }

  addRowLabel(getLabel(LabelType::SPIFFS_SIZE));
  {
    String html;
    html.reserve(32);

    html += SpiffsTotalBytes() / 1024;
    html += F(" kB (");
    html += SpiffsFreeSpace() / 1024;
    html += F(" kB free)");
    addHtml(html);
  }

  addRowLabel(F("Page size"));
  addHtml(String(SpiffsPagesize()));

  addRowLabel(F("Block size"));
  addHtml(String(SpiffsBlocksize()));

  addRowLabel(F("Number of blocks"));
  addHtml(String(SpiffsTotalBytes() / SpiffsBlocksize()));

  {
  # if defined(ESP8266)
    fs::FSInfo fs_info;
    SPIFFS.info(fs_info);
    addRowLabel(F("Maximum open files"));
    addHtml(String(fs_info.maxOpenFiles));

    addRowLabel(F("Maximum path length"));
    addHtml(String(fs_info.maxPathLength));

  # endif // if defined(ESP8266)
  }

# ifndef BUILD_MINIMAL_OTA

  if (showSettingsFileLayout) {
    addTableSeparator(F("Settings Files"), 2, 3);
    html_TR_TD();
    addHtml(F("Layout Settings File"));
    html_TD();
    getConfig_dat_file_layout();
    html_TR_TD();
    html_TD();
    addHtml(F("(offset / size per item / index)"));

    for (int st = 0; st < SettingsType::SettingsType_MAX; ++st) {
      SettingsType::Enum settingsType = static_cast<SettingsType::Enum>(st);
      html_TR_TD();
      addHtml(SettingsType::getSettingsTypeString(settingsType));
      html_BR();
      addHtml(SettingsType::getSettingsFileName(settingsType));
      html_TD();
      getStorageTableSVG(settingsType);
    }
  }
# endif // ifndef BUILD_MINIMAL_OTA

  # ifdef ESP32
  addTableSeparator(F("Partitions"), 2, 3,
                    F("https://dl.espressif.com/doc/esp-idf/latest/api-guides/partition-tables.html"));

  addRowLabel(F("Data Partition Table"));

  //   TXBuffer += getPartitionTableHeader(F(" - "), F("<BR>"));
  //   TXBuffer += getPartitionTable(ESP_PARTITION_TYPE_DATA, F(" - "), F("<BR>"));
  getPartitionTableSVG(ESP_PARTITION_TYPE_DATA, 0x5856e6);

  addRowLabel(F("App Partition Table"));

  //   TXBuffer += getPartitionTableHeader(F(" - "), F("<BR>"));
  //   TXBuffer += getPartitionTable(ESP_PARTITION_TYPE_APP , F(" - "), F("<BR>"));
  getPartitionTableSVG(ESP_PARTITION_TYPE_APP, 0xab56e6);
  # endif // ifdef ESP32
}

#endif    // ifdef WEBSERVER_SYSINFO
