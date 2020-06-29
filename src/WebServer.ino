#define _HEAD false
#define _TAIL true


#include <WString.h>

#include "ESPEasyNetwork.h"
#include "ESPEasy_common.h"
#include "src/Globals/CPlugins.h"
#include "src/Globals/Device.h"
#include "src/Globals/TXBuffer.h"
#include "src/Static/WebStaticData.h"
#include "src/DataStructs/SettingsType.h"



void sendHeadandTail(const String& tmplName, boolean Tail = false, boolean rebooting = false) {
  // This function is called twice per serving a web page.
  // So it must keep track of the timer longer than the scope of this function.
  // Therefore use a local static variable.
  #ifdef USES_TIMING_STATS
  static unsigned statisticsTimerStart = 0;

  if (!Tail) {
    statisticsTimerStart = micros();
  }
  #endif // ifdef USES_TIMING_STATS

  String pageTemplate = "";
  String fileName     = tmplName;

  fileName += F(".htm");
  fs::File f = tryOpenFile(fileName, "r");

  if (f) {
    pageTemplate.reserve(f.size());

    while (f.available()) { pageTemplate += (char)f.read(); }
    f.close();
  } else {
    // TODO TD-er: Should send data directly to TXBuffer instead of using large strings.
    getWebPageTemplateDefault(tmplName, pageTemplate);
  }
  checkRAM(F("sendWebPage"));

  // web activity timer
  lastWeb = millis();

  if (Tail) {
    addHtml(pageTemplate.substring(
              11 +                                      // Size of "{{content}}"
              pageTemplate.indexOf(F("{{content}}")))); // advance beyond content key
  } else {
    int indexStart = 0;
    int indexEnd   = 0;
    int readPos    = 0; // Position of data sent to TXBuffer
    String varName;     // , varValue;
    String meta;

    if (rebooting) {
      meta = F("<meta http-equiv='refresh' content='10 url=/'>");
    }

    while ((indexStart = pageTemplate.indexOf(F("{{"), indexStart)) >= 0) {
      addHtml(pageTemplate.substring(readPos, indexStart));
      readPos = indexStart;

      if ((indexEnd = pageTemplate.indexOf(F("}}"), indexStart)) > 0) {
        varName    = pageTemplate.substring(indexStart + 2, indexEnd);
        indexStart = indexEnd + 2;
        readPos    = indexEnd + 2;
        varName.toLowerCase();

        if (varName == F("content")) { // is var == page content?
          break;                       // send first part of result only
        } else if (varName == F("error")) {
          getErrorNotifications();
        }
        else if (varName == F("meta")) {
          addHtml(meta);
        }
        else {
          getWebPageTemplateVar(varName);
        }
      } else { // no closing "}}"
        // eat "{{"
        readPos    += 2;
        indexStart += 2;
      }
    }
  }

  if (shouldReboot) {
    // we only add this here as a seperate chunk to prevent using too much memory at once
    html_add_script(false);
    TXBuffer += DATA_REBOOT_JS;
    html_add_script_end();
  }
  STOP_TIMER(HANDLE_SERVING_WEBPAGE);
}

void sendHeadandTail_stdtemplate(boolean Tail = false, boolean rebooting = false) {
  sendHeadandTail(F("TmplStd"), Tail, rebooting);

  if (!Tail) {
    if (!clientIPinSubnet() && WifiIsAP(WiFi.getMode()) && (WiFi.softAPgetStationNum() > 0)) {
      addHtmlError(F("Warning: Connected via AP"));
    }

    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      const int nrArgs = web_server.args();

      if (nrArgs > 0) {
        String log = F(" Webserver args:");

        for (int i = 0; i < nrArgs; ++i) {
          log += ' ';
          log += i;
          log += F(": '");
          log += web_server.argName(i);
          log += F("' length: ");
          log += web_server.arg(i).length();
        }
        addLog(LOG_LEVEL_INFO, log);
      }
    }
  }
}

size_t streamFile_htmlEscape(const String& fileName)
{
  fs::File f    = tryOpenFile(fileName, "r");
  size_t   size = 0;

  if (f)
  {
    String escaped;

    while (f.available())
    {
      char c = (char)f.read();

      if (htmlEscapeChar(c, escaped)) {
        addHtml(escaped);
      } else {
        addHtml(String(c));
      }
      ++size;
    }
    f.close();
  }
  return size;
}

// ********************************************************************************
// Web Interface init
// ********************************************************************************
// #include "core_version.h"
#define HTML_SYMBOL_WARNING "&#9888;"
#define HTML_SYMBOL_INPUT   "&#8656;"
#define HTML_SYMBOL_OUTPUT  "&#8658;"
#define HTML_SYMBOL_I_O     "&#8660;"


#define TASKS_PER_PAGE TASKS_MAX

// Uncrustify must not be used on macros, so turn it off.
// *INDENT-OFF*
#define strncpy_webserver_arg(D, N) safe_strncpy(D, web_server.arg(N).c_str(), sizeof(D));
// Uncrustify must not be used on macros, but we're now done, so turn Uncrustify on again.
// *INDENT-ON*

void WebServerInit()
{
  if (webserver_init) { return; }
  webserver_init = true;

  // Prepare webserver pages
  #ifdef WEBSERVER_ROOT
  web_server.on("/",                 handle_root);
  #endif
  #ifdef WEBSERVER_ADVANCED
  web_server.on(F("/advanced"),      handle_advanced);
  #endif
  #ifdef WEBSERVER_CONFIG
  web_server.on(F("/config"),        handle_config);
  #endif
  #ifdef WEBSERVER_CONTROL
  web_server.on(F("/control"),       handle_control);
  #endif
  #ifdef WEBSERVER_CONTROLLERS
  web_server.on(F("/controllers"),   handle_controllers);
  #endif
  #ifdef WEBSERVER_DEVICES
  web_server.on(F("/devices"),       handle_devices);
  #endif
  #ifdef WEBSERVER_DOWNLOAD
  web_server.on(F("/download"),      handle_download);
  #endif

#ifdef USES_C016
  // web_server.on(F("/dumpcache"),     handle_dumpcache);  // C016 specific entrie
  web_server.on(F("/cache_json"),    handle_cache_json); // C016 specific entrie
  web_server.on(F("/cache_csv"),     handle_cache_csv);  // C016 specific entrie
#endif // USES_C016

  #ifdef WEBSERVER_FACTORY_RESET
  web_server.on(F("/factoryreset"),  handle_factoryreset);
  #endif
  #ifdef USE_SETTINGS_ARCHIVE
  web_server.on(F("/settingsarchive"), handle_settingsarchive);
  #endif
  web_server.on(F("/favicon.ico"),   handle_favicon);
  #ifdef WEBSERVER_FILELIST
  web_server.on(F("/filelist"),      handle_filelist);
  #endif
  #ifdef WEBSERVER_HARDWARE
  web_server.on(F("/hardware"),      handle_hardware);
  #endif
  #ifdef WEBSERVER_I2C_SCANNER
  web_server.on(F("/i2cscanner"),    handle_i2cscanner);
  #endif
  web_server.on(F("/json"),          handle_json);     // Also part of WEBSERVER_NEW_UI
  web_server.on(F("/csv"),           handle_csvval);
  web_server.on(F("/log"),           handle_log);
  web_server.on(F("/login"),         handle_login);
  web_server.on(F("/logjson"),       handle_log_JSON); // Also part of WEBSERVER_NEW_UI
#ifdef USES_NOTIFIER
  web_server.on(F("/notifications"), handle_notifications);
#endif 
  #ifdef WEBSERVER_PINSTATES
  web_server.on(F("/pinstates"),     handle_pinstates);
  #endif
  #ifdef WEBSERVER_RULES
  web_server.on(F("/rules"),         handle_rules_new);
  web_server.on(F("/rules/"),        Goto_Rules_Root);
  web_server.on(F("/rules/add"),     []()
  {
    handle_rules_edit(web_server.uri(), true);
  });
  web_server.on(F("/rules/backup"),      handle_rules_backup);
  web_server.on(F("/rules/delete"),      handle_rules_delete);
  #endif // WEBSERVER_RULES
#ifdef FEATURE_SD
  web_server.on(F("/SDfilelist"),        handle_SDfilelist);
#endif // ifdef FEATURE_SD
#ifdef WEBSERVER_SETUP
  web_server.on(F("/setup"),             handle_setup);
#endif
#ifdef WEBSERVER_SYSINFO
  web_server.on(F("/sysinfo"),           handle_sysinfo);
#endif
#ifdef WEBSERVER_SYSVARS
  web_server.on(F("/sysvars"),           handle_sysvars);
#endif // WEBSERVER_SYSVARS
#ifdef WEBSERVER_TIMINGSTATS
  web_server.on(F("/timingstats"),       handle_timingstats);
#endif // WEBSERVER_TIMINGSTATS
#ifdef WEBSERVER_TOOLS
  web_server.on(F("/tools"),             handle_tools);
#endif
#ifdef WEBSERVER_UPLOAD
  web_server.on(F("/upload"),            HTTP_GET,  handle_upload);
  web_server.on(F("/upload"),            HTTP_POST, handle_upload_post, handleFileUpload);
#endif
#ifdef WEBSERVER_WIFI_SCANNER
  web_server.on(F("/wifiscanner"),       handle_wifiscanner);
#endif

#ifdef WEBSERVER_NEW_UI
  web_server.on(F("/buildinfo"),         handle_buildinfo);     // Also part of WEBSERVER_NEW_UI
  web_server.on(F("/factoryreset_json"), handle_factoryreset_json);
  web_server.on(F("/filelist_json"),     handle_filelist_json);
  web_server.on(F("/i2cscanner_json"),   handle_i2cscanner_json);
  web_server.on(F("/node_list_json"),    handle_nodes_list_json);
  web_server.on(F("/pinstates_json"),    handle_pinstates_json);
  web_server.on(F("/sysinfo_json"),      handle_sysinfo_json);
  web_server.on(F("/timingstats_json"),  handle_timingstats_json);
  web_server.on(F("/upload_json"),       HTTP_POST, handle_upload_json, handleFileUpload);
  web_server.on(F("/wifiscanner_json"),  handle_wifiscanner_json);
#endif // WEBSERVER_NEW_UI

  web_server.onNotFound(handleNotFound);

  #if defined(ESP8266) || defined(ESP32) 
  {
    # ifndef NO_HTTP_UPDATER
    uint32_t maxSketchSize;
    bool     use2step;

    if (OTA_possible(maxSketchSize, use2step)) {
      httpUpdater.setup(&web_server);
    }
    # endif // ifndef NO_HTTP_UPDATER
  }
  #endif    // if defined(ESP8266)

  #if defined(ESP8266)

  # ifdef USES_SSDP

  if (Settings.UseSSDP)
  {
    web_server.on(F("/ssdp.xml"), HTTP_GET, []() {
      WiFiClient client(web_server.client());
      client.setTimeout(CONTROLLER_CLIENTTIMEOUT_DFLT);
      SSDP_schema(client);
    });
    SSDP_begin();
  }
  # endif // USES_SSDP
  #endif  // if defined(ESP8266)
}

void set_mDNS() {
  #ifdef FEATURE_MDNS

  if (webserverRunning) {
    addLog(LOG_LEVEL_INFO, F("WIFI : Starting mDNS..."));
    bool mdns_started = MDNS.begin(NetworkGetHostname().c_str());
    MDNS.setInstanceName(NetworkGetHostname()); // Needed for when the hostname has changed.

    if (loglevelActiveFor(LOG_LEVEL_INFO)) {
      String log = F("WIFI : ");

      if (mdns_started) {
        log += F("mDNS started, with name: ");
        log += getValue(LabelType::M_DNS);
      }
      else {
        log += F("mDNS failed");
      }
      addLog(LOG_LEVEL_INFO, log);
    }

    if (mdns_started) {
      MDNS.addService("http", "tcp", Settings.WebserverPort);
    }
  }
  #endif // ifdef FEATURE_MDNS
}

void setWebserverRunning(bool state) {
  if (webserverRunning == state) {
    return;
  }

  if (state) {
    WebServerInit();
    web_server.begin(Settings.WebserverPort);
    addLog(LOG_LEVEL_INFO, F("Webserver: start"));
  } else {
    web_server.stop();
    addLog(LOG_LEVEL_INFO, F("Webserver: stop"));
  }
  webserverRunning = state;
  set_mDNS(); // Uses webserverRunning state.
}

void getWebPageTemplateDefault(const String& tmplName, String& tmpl)
{
  tmpl.reserve(576);
  const bool addJS   = true;
  const bool addMeta = true;

  if (tmplName == F("TmplAP"))
  {
    getWebPageTemplateDefaultHead(tmpl, !addMeta, !addJS);
    tmpl += F("<body>"
              "<header class='apheader'>"
              "<h1>Welcome to ESP Easy Mega AP</h1>"
              "</header>");
    getWebPageTemplateDefaultContentSection(tmpl);
    getWebPageTemplateDefaultFooter(tmpl);
  }
  else if (tmplName == F("TmplMsg"))
  {
    getWebPageTemplateDefaultHead(tmpl, !addMeta, !addJS);
    tmpl += F("<body>");
    getWebPageTemplateDefaultHeader(tmpl, F("{{name}}"), false);
    getWebPageTemplateDefaultContentSection(tmpl);
    getWebPageTemplateDefaultFooter(tmpl);
  }
  else if (tmplName == F("TmplDsh"))
  {
    getWebPageTemplateDefaultHead(tmpl, !addMeta, addJS);
    tmpl += F(
      "<body>"
      "{{content}}"
      "</body></html>"
      );
  }
  else // all other template names e.g. TmplStd
  {
    getWebPageTemplateDefaultHead(tmpl, addMeta, addJS);
    tmpl += F("<body class='bodymenu'>"
              "<span class='message' id='rbtmsg'></span>");
    getWebPageTemplateDefaultHeader(tmpl, F("{{name}} {{logo}}"), true);
    getWebPageTemplateDefaultContentSection(tmpl);
    getWebPageTemplateDefaultFooter(tmpl);
  }
}

void getWebPageTemplateDefaultHead(String& tmpl, bool addMeta, bool addJS) {
  tmpl += F("<!DOCTYPE html><html lang='en'>"
            "<head>"
            "<meta charset='utf-8'/>"
            "<meta name='viewport' content='width=device-width, initial-scale=1.0'>"
            "<title>{{name}}</title>");

  if (addMeta) { tmpl += F("{{meta}}"); }

  if (addJS) { tmpl += F("{{js}}"); }

  tmpl += F("{{css}}"
            "</head>");
}

void getWebPageTemplateDefaultHeader(String& tmpl, const String& title, bool addMenu) {
  tmpl += F("<header class='headermenu'>"
            "<h1>ESP Easy Mega: ");
  tmpl += title;
  tmpl += F("</h1><BR>");

  if (addMenu) { tmpl += F("{{menu}}"); }
  tmpl += F("</header>");
}

void getWebPageTemplateDefaultContentSection(String& tmpl) {
  tmpl += F("<section>"
            "<span class='message error'>"
            "{{error}}"
            "</span>"
            "{{content}}"
            "</section>"
            );
}

void getWebPageTemplateDefaultFooter(String& tmpl) {
  tmpl += F("<footer>"
            "<br>"
            "<h6>Powered by <a href='http://www.letscontrolit.com' style='font-size: 15px; text-decoration: none'>Let's Control It</a> community</h6>"
            "</footer>"
            "</body></html>"
            );
}

void getErrorNotifications() {
  // Check number of MQTT controllers active.
  int nrMQTTenabled = 0;

  for (controllerIndex_t x = 0; x < CONTROLLER_MAX; x++) {
    if (Settings.Protocol[x] != 0) {
      protocolIndex_t ProtocolIndex = getProtocolIndex_from_ControllerIndex(x);

      if (validProtocolIndex(ProtocolIndex) && Settings.ControllerEnabled[x] && Protocol[ProtocolIndex].usesMQTT) {
        ++nrMQTTenabled;
      }
    }
  }

  if (nrMQTTenabled > 1) {
    // Add warning, only one MQTT protocol should be used.
    addHtmlError(F("Only one MQTT controller should be active."));
  }

  // Check checksum of stored settings.
}

#define MENU_INDEX_MAIN          0
#define MENU_INDEX_CONFIG        1
#define MENU_INDEX_CONTROLLERS   2
#define MENU_INDEX_HARDWARE      3
#define MENU_INDEX_DEVICES       4
#define MENU_INDEX_RULES         5
#define MENU_INDEX_NOTIFICATIONS 6
#define MENU_INDEX_TOOLS         7
static byte navMenuIndex = MENU_INDEX_MAIN;

// See https://github.com/letscontrolit/ESPEasy/issues/1650
String getGpMenuIcon(byte index) {
  switch (index) {
    case MENU_INDEX_MAIN          : return F("&#8962;");  
    case MENU_INDEX_CONFIG        : return F("&#9881;");  
    case MENU_INDEX_CONTROLLERS   : return F("&#128172;");
    case MENU_INDEX_HARDWARE      : return F("&#128204;");
    case MENU_INDEX_DEVICES       : return F("&#128268;");
    case MENU_INDEX_RULES         : return F("&#10740;"); 
    case MENU_INDEX_NOTIFICATIONS : return F("&#9993;");  
    case MENU_INDEX_TOOLS         : return F("&#128295;");
  }
  return "";
}

String getGpMenuLabel(byte index) {
  switch (index) {
    case MENU_INDEX_MAIN          : return F("Main");         
    case MENU_INDEX_CONFIG        : return F("Config");       
    case MENU_INDEX_CONTROLLERS   : return F("Controllers");  
    case MENU_INDEX_HARDWARE      : return F("Hardware");     
    case MENU_INDEX_DEVICES       : return F("Devices");      
    case MENU_INDEX_RULES         : return F("Rules");        
    case MENU_INDEX_NOTIFICATIONS : return F("Notifications");
    case MENU_INDEX_TOOLS         : return F("Tools");        
  }
  return "";
}

String getGpMenuURL(byte index) {
  switch (index) {
    case MENU_INDEX_MAIN          : return F("/");             
    case MENU_INDEX_CONFIG        : return F("/config");       
    case MENU_INDEX_CONTROLLERS   : return F("/controllers");  
    case MENU_INDEX_HARDWARE      : return F("/hardware");     
    case MENU_INDEX_DEVICES       : return F("/devices");      
    case MENU_INDEX_RULES         : return F("/rules");        
    case MENU_INDEX_NOTIFICATIONS : return F("/notifications");
    case MENU_INDEX_TOOLS         : return F("/tools");        
  }
  return "";
}

void getWebPageTemplateVar(const String& varName)
{
  // serialPrint(varName); serialPrint(" : free: "); serialPrint(ESP.getFreeHeap());   serialPrint("var len before:  "); serialPrint
  // (varValue.length()) ;serialPrint("after:  ");
  // varValue = "";

  if (varName == F("name"))
  {
    addHtml(Settings.Name);
  }

  else if (varName == F("unit"))
  {
    addHtml(String(Settings.Unit));
  }

  else if (varName == F("menu"))
  {
    addHtml(F("<div class='menubar'>"));

    for (byte i = 0; i < 8; i++)
    {
      if ((i == MENU_INDEX_RULES) && !Settings.UseRules) { // hide rules menu item
        continue;
      }
#ifndef USES_NOTIFIER

      if (i == MENU_INDEX_NOTIFICATIONS) { // hide notifications menu item
        continue;
      }
#endif

      addHtml(F("<a class='menu"));

      if (i == navMenuIndex) {
        addHtml(F(" active"));
      }
      addHtml(F("' href='"));
      addHtml(getGpMenuURL(i));
      addHtml("'>");
      addHtml(getGpMenuIcon(i));
      addHtml(F("<span class='showmenulabel'>"));
      addHtml(getGpMenuLabel(i));
      addHtml(F("</span></a>"));
    }

    addHtml(F("</div>"));
  }

  else if (varName == F("logo"))
  {
    if (ESPEASY_FS.exists(F("esp.png")))
    {
      addHtml(F("<img src=\"esp.png\" width=48 height=48 align=right>"));
    }
  }

  else if (varName == F("css"))
  {
    if (ESPEASY_FS.exists(F("esp.css"))) // now css is written in writeDefaultCSS() to FS and always present
    // if (0) //TODO
    {
      addHtml(F("<link rel=\"stylesheet\" type=\"text/css\" href=\"esp.css\">"));
    }
    else
    {
      addHtml(F("<style>"));

      // Send CSS per chunk to avoid sending either too short or too large strings.
      TXBuffer += DATA_ESPEASY_DEFAULT_MIN_CSS;
      addHtml(F("</style>"));
    }
  }


  else if (varName == F("js"))
  {
    html_add_autosubmit_form();
    html_add_script(false);
    TXBuffer += jsToastMessageBegin;
    // we can push custom messages here in future releases...
    addHtml(F("Submitted"));
    TXBuffer += jsToastMessageEnd;

    html_add_script_end();
  }

  else if (varName == F("error"))
  {
    // print last error - not implemented yet
  }

  else if (varName == F("debug"))
  {
    // print debug messages - not implemented yet
  }

  else
  {
    if (loglevelActiveFor(LOG_LEVEL_ERROR)) {
      String log = F("Templ: Unknown Var : ");
      log += varName;
      addLog(LOG_LEVEL_ERROR, log);
    }

    // no return string - eat var name
  }
}

void writeDefaultCSS(void)
{
  return; // TODO

  if (!ESPEASY_FS.exists(F("esp.css")))
  {
    String defaultCSS;

    fs::File f = tryOpenFile(F("esp.css"), "w");

    if (f)
    {
      if (loglevelActiveFor(LOG_LEVEL_INFO)) {
        String log = F("CSS  : Writing default CSS file to FS (");
        log += defaultCSS.length();
        log += F(" bytes)");
        addLog(LOG_LEVEL_INFO, log);
      }
      defaultCSS = PGMT(DATA_ESPEASY_DEFAULT_MIN_CSS);
      f.write((const unsigned char *)defaultCSS.c_str(), defaultCSS.length()); // note: content must be in RAM - a write of F("XXX") does
                                                                               // not work
      f.close();
    }
  }
}

// ********************************************************************************
// Functions to stream JSON directly to TXBuffer
// FIXME TD-er: replace stream_xxx_json_object* into this code.
// N.B. handling of numerical values differs (string vs. no string)
// ********************************************************************************

int8_t level     = 0;
int8_t lastLevel = -1;

void json_quote_name(const String& val) {
  String html;

  html.reserve(4 + val.length());

  if (lastLevel == level) {
    html += ",";
  }

  if (val.length() > 0) {
    html += '\"';
    html += val;
    html += '\"';
    html += ':';
  }
  addHtml(html);
}

void json_quote_val(const String& val) {
  String html;

  html.reserve(4 + val.length());
  html += '\"';
  html += val;
  html += '\"';
  addHtml(html);
}

void json_open() {
  json_open(false, String());
}

void json_open(bool arr) {
  json_open(arr, String());
}

void json_open(bool arr, const String& name) {
  json_quote_name(name);
  addHtml(arr ? "[" : "{");
  lastLevel = level;
  level++;
}

void json_init() {
  level     = 0;
  lastLevel = -1;
}

void json_close() {
  json_close(false);
}

void json_close(bool arr) {
  addHtml(arr ? "]" : "}");
  level--;
  lastLevel = level;
}

void json_number(const String& name, const String& value) {
  json_quote_name(name);
  json_quote_val(value);
  lastLevel = level;
}

void json_prop(const String& name, const String& value) {
  json_quote_name(name);
  json_quote_val(value);
  lastLevel = level;
}

void json_prop(LabelType::Enum label) {
  json_prop(getInternalLabel(label, '-'), getValue(label));
}

// ********************************************************************************
// Add a task select dropdown list
// This allows to select a task index based on the existing tasks.
// ********************************************************************************
void addTaskSelect(const String& name,  taskIndex_t choice)
{
  String deviceName;

  addHtml(F("<select id='selectwidth' name='"));
  addHtml(name);
  addHtml(F("' onchange='return dept_onchange(frmselect)'>"));

  for (taskIndex_t x = 0; x < TASKS_MAX; x++)
  {
    const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(x);
    deviceName = getPluginNameFromDeviceIndex(DeviceIndex);
    LoadTaskSettings(x);

    {
      String html;
      html.reserve(32);

      html += F("<option value='");
      html += x;
      html += '\'';

      if (choice == x) {
        html += F(" selected");
      }
      addHtml(html);
    }

    if (!validPluginID_fullcheck(Settings.TaskDeviceNumber[x])) {
      addDisabled();
    }
    {
      String html;
      html.reserve(96);

      html += '>';
      html += x + 1;
      html += F(" - ");
      html += deviceName;
      html += F(" - ");
      html += ExtraTaskSettings.TaskDeviceName;
      html += F("</option>");
      addHtml(html);
    }
  }
}

// ********************************************************************************
// Add a Value select dropdown list, based on TaskIndex
// This allows to select a task value, based on the existing tasks.
// ********************************************************************************
void addTaskValueSelect(const String& name, int choice, taskIndex_t TaskIndex)
{
  if (!validTaskIndex(TaskIndex)) { return; }
  const deviceIndex_t DeviceIndex = getDeviceIndex_from_TaskIndex(TaskIndex);

  if (!validDeviceIndex(DeviceIndex)) { return; }

  {
    String html;
    html.reserve(34 + name.length());

    html += F("<select id='selectwidth' name='");
    html += name;
    html += "'>";
    addHtml(html);
  }

  LoadTaskSettings(TaskIndex);

  for (byte x = 0; x < Device[DeviceIndex].ValueCount; x++)
  {
    String html;
    html.reserve(96);
    html += F("<option value='");
    html += x;
    html += '\'';

    if (choice == x) {
      html += F(" selected");
    }
    html += '>';
    html += ExtraTaskSettings.TaskDeviceValueNames[x];
    html += F("</option>");
    addHtml(html);
  }
}

// ********************************************************************************
// Login state check
// ********************************************************************************
boolean isLoggedIn()
{
  String www_username = F(DEFAULT_ADMIN_USERNAME);

  if (!clientIPallowed()) { return false; }

  if (SecuritySettings.Password[0] == 0) { return true; }

  if (!web_server.authenticate(www_username.c_str(), SecuritySettings.Password))

  // Basic Auth Method with Custom realm and Failure Response
  // return server.requestAuthentication(BASIC_AUTH, www_realm, authFailResponse);
  // Digest Auth Method with realm="Login Required" and empty Failure Response
  // return server.requestAuthentication(DIGEST_AUTH);
  // Digest Auth Method with Custom realm and empty Failure Response
  // return server.requestAuthentication(DIGEST_AUTH, www_realm);
  // Digest Auth Method with Custom realm and Failure Response
  {
#ifdef CORE_PRE_2_5_0

    // See https://github.com/esp8266/Arduino/issues/4717
    HTTPAuthMethod mode = BASIC_AUTH;
#else // ifdef CORE_PRE_2_5_0
    HTTPAuthMethod mode = DIGEST_AUTH;
#endif // ifdef CORE_PRE_2_5_0
    String message = F("Login Required (default user: ");
    message += www_username;
    message += ')';
    web_server.requestAuthentication(mode, message.c_str());
    return false;
  }
  return true;
}

String getControllerSymbol(byte index)
{
  String ret = F("<p style='font-size:20px'>&#");

  ret += 10102 + index;
  ret += F(";</p>");
  return ret;
}

/*
   String getValueSymbol(byte index)
   {
   String ret = F("&#");
   ret += 10112 + index;
   ret += ';';
   return ret;
   }
 */
void addSVG_param(const String& key, float value) {
  String value_str = String(value, 2);

  addSVG_param(key, value_str);
}

void addSVG_param(const String& key, const String& value) {
  String html;

  html.reserve(8 + key.length() + value.length());
  html += ' ';
  html += key;
  html += '=';
  html += '\"';
  html += value;
  html += '\"';
  addHtml(html);
}

void createSvgRect_noStroke(unsigned int fillColor, float xoffset, float yoffset, float width, float height, float rx, float ry) {
  createSvgRect(fillColor, fillColor, xoffset, yoffset, width, height, 0, rx, ry);
}

void createSvgRect(unsigned int fillColor,
                   unsigned int strokeColor,
                   float        xoffset,
                   float        yoffset,
                   float        width,
                   float        height,
                   float        strokeWidth,
                   float        rx,
                   float        ry) {
  addHtml(F("<rect"));
  addSVG_param(F("fill"), formatToHex(fillColor, F("#")));

  if (strokeWidth != 0) {
    addSVG_param(F("stroke"),       formatToHex(strokeColor, F("#")));
    addSVG_param(F("stroke-width"), strokeWidth);
  }
  addSVG_param("x",         xoffset);
  addSVG_param("y",         yoffset);
  addSVG_param(F("width"),  width);
  addSVG_param(F("height"), height);
  addSVG_param(F("rx"),     rx);
  addSVG_param(F("ry"),     ry);
  addHtml(F("/>"));
}

void createSvgHorRectPath(unsigned int color, int xoffset, int yoffset, int size, int height, int range, float SVG_BAR_WIDTH) {
  float width = SVG_BAR_WIDTH * size / range;

  if (width < 2) { width = 2; }
  String html;
  html.reserve(96);
  html += formatToHex(color, F("<path fill=\"#"));
  html += F("\" d=\"M");
  html += toString(SVG_BAR_WIDTH * xoffset / range, 2);
  html += ' ';
  html += yoffset;
  html += 'h';
  html += toString(width, 2);
  html += 'v';
  html += height;
  html += 'H';
  html += toString(SVG_BAR_WIDTH * xoffset / range, 2);
  html += F("z\"/>\n");
  addHtml(html);
}

void createSvgTextElement(const String& text, float textXoffset, float textYoffset) {
  addHtml(F("<text style=\"line-height:1.25\" x=\""));
  addHtml(toString(textXoffset, 2));
  addHtml(F("\" y=\""));
  addHtml(toString(textYoffset, 2));
  addHtml(F("\" stroke-width=\".3\" font-family=\"sans-serif\" font-size=\"8\" letter-spacing=\"0\" word-spacing=\"0\">\n"));
  addHtml(F("<tspan x=\""));
  addHtml(toString(textXoffset, 2));
  addHtml(F("\" y=\""));
  addHtml(toString(textYoffset, 2));
  addHtml("\">");
  addHtml(text);
  addHtml(F("</tspan>\n</text>"));
}

#define SVG_BAR_HEIGHT 16
#define SVG_BAR_WIDTH 400

void write_SVG_image_header(int width, int height) {
  write_SVG_image_header(width, height, false);
}

void write_SVG_image_header(int width, int height, bool useViewbox) {
  String html;

  html.reserve(128);
  html += F("<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"");
  html += width;
  html += F("\" height=\"");
  html += height;
  html += F("\" version=\"1.1\"");

  if (useViewbox) {
    html += F(" viewBox=\"0 0 100 100\"");
  }
  html += '>';
  addHtml(html);
}

/*
   void getESPeasyLogo(int width_pixels) {
   write_SVG_image_header(width_pixels, width_pixels, true);
   addHtml(F("<g transform=\"translate(-33.686 -7.8142)\"><rect x=\"49\" y=\"23.1\" width=\"69.3\" height=\"69.3\" fill=\"#2c72da\"
      stroke=\"#2c72da\"
      stroke-linecap=\"round\"stroke-linejoin=\"round\" stroke-width=\"30.7\"/><g transform=\"matrix(3.3092 0 0 3.3092 -77.788
         -248.96)\"><path d=\"m37.4 89 7.5-7.5M37.4 96.5l15-15M37.4 96.5l15-15M37.4 104l22.5-22.5M44.9 104l15-15\"
      fill=\"none\"stroke=\"#fff\" stroke-linecap=\"round\" stroke-width=\"2.6\"/><circle cx=\"58\" cy=\"102.1\" r=\"3\"
         fill=\"#fff\"/></g></g></svg>");
   }
 */
void getWiFi_RSSI_icon(int rssi, int width_pixels)
{
  const int nbars_filled = (rssi + 100) / 8;
  int nbars              = 5;
  int white_between_bar  = (static_cast<float>(width_pixels) / nbars) * 0.2;

  if (white_between_bar < 1) { white_between_bar = 1; }
  const int barWidth   = (width_pixels - (nbars - 1) * white_between_bar) / nbars;
  int svg_width_pixels = nbars * barWidth + (nbars - 1) * white_between_bar;
  write_SVG_image_header(svg_width_pixels, svg_width_pixels, true);
  float scale               = 100 / svg_width_pixels;
  const int bar_height_step = 100 / nbars;

  for (int i = 0; i < nbars; ++i) {
    unsigned int color = i < nbars_filled ? 0x0 : 0xa1a1a1; // Black/Grey
    int barHeight      = (i + 1) * bar_height_step;
    createSvgRect_noStroke(color, i * (barWidth + white_between_bar) * scale, 100 - barHeight, barWidth, barHeight, 0, 0);
  }
  addHtml(F("</svg>\n"));
}

#ifndef BUILD_MINIMAL_OTA
void getConfig_dat_file_layout() {
  const int shiftY  = 2;
  float     yOffset = shiftY;

  write_SVG_image_header(SVG_BAR_WIDTH + 250, SVG_BAR_HEIGHT + shiftY);

  int max_index, offset, max_size;
  int struct_size = 0;

  // background
  const uint32_t realSize = SettingsType::getFileSize(SettingsType::TaskSettings_Type);
  createSvgHorRectPath(0xcdcdcd, 0, yOffset, realSize, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);

  for (int st = 0; st < SettingsType::SettingsType_MAX; ++st) {
    SettingsType::Enum settingsType = static_cast<SettingsType::Enum>(st);
    if (SettingsType::getSettingsFile(settingsType) == SettingsType::FILE_CONFIG_type) {
      unsigned int color = SettingsType::getSVGcolor(settingsType);
      SettingsType::getSettingsParameters(settingsType, 0, max_index, offset, max_size, struct_size);

      for (int i = 0; i < max_index; ++i) {
        SettingsType::getSettingsParameters(settingsType, i, offset, max_size);

        // Struct position
        createSvgHorRectPath(color, offset, yOffset, max_size, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);
      }
    }
  }

  // Text labels
  float textXoffset = SVG_BAR_WIDTH + 2;
  float textYoffset = yOffset + 0.9 * SVG_BAR_HEIGHT;
  createSvgTextElement(SettingsType::getSettingsFileName(SettingsType::TaskSettings_Type), textXoffset, textYoffset);
  addHtml(F("</svg>\n"));
}

void getStorageTableSVG(SettingsType::Enum settingsType) {
  uint32_t realSize   = SettingsType::getFileSize(settingsType);
  unsigned int color  = SettingsType::getSVGcolor(settingsType);
  const int    shiftY = 2;

  int max_index, offset, max_size;
  int struct_size = 0;

  SettingsType::getSettingsParameters(settingsType, 0, max_index, offset, max_size, struct_size);

  if (max_index == 0) { return; }

  // One more to add bar indicating struct size vs. reserved space.
  write_SVG_image_header(SVG_BAR_WIDTH + 250, (max_index + 1) * SVG_BAR_HEIGHT + shiftY);
  float yOffset = shiftY;

  for (int i = 0; i < max_index; ++i) {
    SettingsType::getSettingsParameters(settingsType, i, offset, max_size);

    // background
    createSvgHorRectPath(0xcdcdcd, 0,      yOffset, realSize, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);

    // Struct position
    createSvgHorRectPath(color,    offset, yOffset, max_size, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);

    // Text labels
    float textXoffset = SVG_BAR_WIDTH + 2;
    float textYoffset = yOffset + 0.9 * SVG_BAR_HEIGHT;
    createSvgTextElement(formatHumanReadable(offset, 1024),   textXoffset, textYoffset);
    textXoffset = SVG_BAR_WIDTH + 60;
    createSvgTextElement(formatHumanReadable(max_size, 1024), textXoffset, textYoffset);
    textXoffset = SVG_BAR_WIDTH + 130;
    createSvgTextElement(String(i),                           textXoffset, textYoffset);
    yOffset += SVG_BAR_HEIGHT;
  }

  // usage
  createSvgHorRectPath(0xcdcdcd, 0, yOffset, max_size, SVG_BAR_HEIGHT - 2, max_size, SVG_BAR_WIDTH);

  // Struct size (used part of the reserved space)
  if (struct_size != 0) {
    createSvgHorRectPath(color, 0, yOffset, struct_size, SVG_BAR_HEIGHT - 2, max_size, SVG_BAR_WIDTH);
  }

  // Text labels
  float textXoffset = SVG_BAR_WIDTH + 2;
  float textYoffset = yOffset + 0.9 * SVG_BAR_HEIGHT;

  if (struct_size != 0) {
    String text = formatHumanReadable(struct_size, 1024);
    text += '/';
    text += formatHumanReadable(max_size, 1024);
    text += F(" per item");
    createSvgTextElement(text, textXoffset, textYoffset);
  } else {
    createSvgTextElement(F("Variable size"), textXoffset, textYoffset);
  }
  addHtml(F("</svg>\n"));
}

#endif // ifndef BUILD_MINIMAL_OTA

#ifdef ESP32

#include <esp_partition.h>

int getPartionCount(byte pType) {
  esp_partition_type_t partitionType       = static_cast<esp_partition_type_t>(pType);
  esp_partition_iterator_t _mypartiterator = esp_partition_find(partitionType, ESP_PARTITION_SUBTYPE_ANY, NULL);
  int nrPartitions                         = 0;

  if (_mypartiterator) {
    do {
      ++nrPartitions;
    } while ((_mypartiterator = esp_partition_next(_mypartiterator)) != NULL);
  }
  esp_partition_iterator_release(_mypartiterator);
  return nrPartitions;
}

void getPartitionTableSVG(byte pType, unsigned int partitionColor) {
  int nrPartitions = getPartionCount(pType);

  if (nrPartitions == 0) { return; }
  const int shiftY = 2;

  uint32_t realSize                      = getFlashRealSizeInBytes();
  esp_partition_type_t     partitionType = static_cast<esp_partition_type_t>(pType);
  const esp_partition_t   *_mypart;
  esp_partition_iterator_t _mypartiterator = esp_partition_find(partitionType, ESP_PARTITION_SUBTYPE_ANY, NULL);
  write_SVG_image_header(SVG_BAR_WIDTH + 250, nrPartitions * SVG_BAR_HEIGHT + shiftY);
  float yOffset = shiftY;

  if (_mypartiterator) {
    do {
      _mypart = esp_partition_get(_mypartiterator);
      createSvgHorRectPath(0xcdcdcd,       0,                yOffset, realSize,      SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);
      createSvgHorRectPath(partitionColor, _mypart->address, yOffset, _mypart->size, SVG_BAR_HEIGHT - 2, realSize, SVG_BAR_WIDTH);
      float textXoffset = SVG_BAR_WIDTH + 2;
      float textYoffset = yOffset + 0.9 * SVG_BAR_HEIGHT;
      createSvgTextElement(formatHumanReadable(_mypart->size, 1024),          textXoffset, textYoffset);
      textXoffset = SVG_BAR_WIDTH + 60;
      createSvgTextElement(_mypart->label,                                    textXoffset, textYoffset);
      textXoffset = SVG_BAR_WIDTH + 130;
      createSvgTextElement(getPartitionType(_mypart->type, _mypart->subtype), textXoffset, textYoffset);
      yOffset += SVG_BAR_HEIGHT;
    } while ((_mypartiterator = esp_partition_next(_mypartiterator)) != NULL);
  }
  addHtml(F("</svg>\n"));
  esp_partition_iterator_release(_mypartiterator);
}

#endif // ifdef ESP32
