#ifdef USES_P142
//#######################################################################################################
//#################################### Plugin 142: RGB-Strip ############################################
//#######################################################################################################
// written by Jochen Krapf (jk@nerd2nerd.org)

// List of commands:
// (1) RGB,<red 0-255>,<green 0-255>,<blue 0-255>
// (2) HSV,<hue 0-360>,<saturation 0-100>,<value/brightness 0-100>
// (3) HSL,<hue 0-360>,<saturation 0-100>,<lightness 0-100>
// (4) HUE,<hue 0-360>
// (5) SAT,<saturation 0-100>
// (6) VAL,<value/brightness 0-100>
// (7) DIMM,<value/brightness 0-100>
// (8) OFF
// (9) CYCLE,<time 1-999>   time for full color hue circle; 0 to return to normal mode

// Usage:
// (1): Set RGB Color to LED (eg. RGB,255,255,255)

//#include <Arduino.h> //  - no external lib required
#include <analogWrite.h>
#include <RGBConverter.h>

static double Plugin_142_hsvPrev[3] = {0,0,0};
static double Plugin_142_hsvDest[3] = {0,0,0};
static double Plugin_142_hsvAct[3] = {0,0,0};
static long millisFadeBegin = 0;
static long millisFadeEnd = 0;
static long millisFadeTime = 1500;
static double Plugin_142_cycle = 0;

static int Plugin_142_pin[4] = {-1,-1,-1,-1};
static int Plugin_142_lowActive = false;

#define PLUGIN_142
#define PLUGIN_ID_142         142
#define PLUGIN_NAME_142       "RGB-Strip"
#define PLUGIN_VALUENAME1_142 "H"
#define PLUGIN_VALUENAME2_142 "S"
#define PLUGIN_VALUENAME3_142 "V"

#define PLUGIN_PWM_OFFSET 0   //ESP-PWM has flickering problems with values <6 and >1017. If problem is fixed in ESP libs the define can be set to 0 (or code removed)
//see https://github.com/esp8266/Arduino/issues/836   https://github.com/SmingHub/Sming/issues/70   https://github.com/espruino/Espruino/issues/914


boolean Plugin_142(byte function, struct EventStruct *event, String& string)
{
  boolean success = false;

  switch (function)
  {
    case PLUGIN_DEVICE_ADD:
      {
        Device[++deviceCount].Number = PLUGIN_ID_142;
        Device[deviceCount].Type = DEVICE_TYPE_DUMMY;           // Nothing else really fit the bill ...
        Device[deviceCount].Ports = 0;
        Device[deviceCount].VType = SENSOR_TYPE_TRIPLE;
        Device[deviceCount].PullUpOption = false;
        Device[deviceCount].InverseLogicOption = true;
        Device[deviceCount].FormulaOption = true;
        Device[deviceCount].ValueCount = 3;
        Device[deviceCount].SendDataOption = true;
        Device[deviceCount].TimerOption = true;
        Device[deviceCount].TimerOptional = true;
        Device[deviceCount].GlobalSyncOption = true;
        break;
      }

    case PLUGIN_GET_DEVICENAME:
      {
        string = F(PLUGIN_NAME_142);
        break;
      }

    case PLUGIN_GET_DEVICEVALUENAMES:
      {
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[0], PSTR(PLUGIN_VALUENAME1_142));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[1], PSTR(PLUGIN_VALUENAME2_142));
        strcpy_P(ExtraTaskSettings.TaskDeviceValueNames[2], PSTR(PLUGIN_VALUENAME3_142));
        break;
      }

    case PLUGIN_WEBFORM_LOAD:
      {
        addHtml(F("<TR><TD>GPIO:<TD>"));
        addFormPinSelect(F("1st GPIO (R):"), F("taskdevicepin1"), Settings.TaskDevicePluginConfig[event->TaskIndex][0]);
        addFormPinSelect(F("2nd GPIO (G):"), F("taskdevicepin2"), Settings.TaskDevicePluginConfig[event->TaskIndex][1]);
        addFormPinSelect(F("3rd GPIO (B):"), F("taskdevicepin3"), Settings.TaskDevicePluginConfig[event->TaskIndex][2]);
        addFormPinSelect(F("4th GPIO (W) optional:"), F("taskdeviceport"), Settings.TaskDevicePluginConfig[event->TaskIndex][3]);     
        success = true;
        break;
      }

    case PLUGIN_WEBFORM_SAVE:
      {
        Settings.TaskDevicePluginConfig[event->TaskIndex][0] = getFormItemInt(F("taskdevicepin1"));
        Settings.TaskDevicePluginConfig[event->TaskIndex][1] = getFormItemInt(F("taskdevicepin2"));        
        Settings.TaskDevicePluginConfig[event->TaskIndex][2] = getFormItemInt(F("taskdevicepin3"));        
        Settings.TaskDevicePluginConfig[event->TaskIndex][3] = getFormItemInt(F("taskdeviceport"));        
        
        for (byte i=0; i<4; i++)
          if (Settings.TaskDevicePluginConfig[event->TaskIndex][i] >= 40)
            Settings.TaskDevicePluginConfig[event->TaskIndex][i] = -1;

        success = true;
        break;
      }

    case PLUGIN_INIT:
      {
        analogWriteFrequency(400);
        String log = F("RGB-S: Pin ");
        for (byte i=0; i<4; i++)
        {
          int pin = Settings.TaskDevicePluginConfig[event->TaskIndex][i];
          Plugin_142_pin[i] = pin;
          if (pin >= 0)
            pinMode(pin, OUTPUT);
          log += pin;
          log += F(" ");
        }
        Plugin_142_lowActive = Settings.TaskDevicePin1Inversed[event->TaskIndex];
        addLog(LOG_LEVEL_INFO, log);

        Plugin_142_Output(Plugin_142_hsvDest);

        success = true;
        break;
      }

    case PLUGIN_WRITE:
      {
        bool bNewValue = false;

        String command = parseString(string, 1);

        if (command == F("rgb"))
        {
          double rgb[3];
          rgb[0] = event->Par1 / 255.0;   //R
          rgb[1] = event->Par2 / 255.0;   //G
          rgb[2] = event->Par3 / 255.0;   //B
          Plugin_142_hsvClamp(rgb);
          Plugin_142_rgb2hsv(rgb, Plugin_142_hsvDest);
          bNewValue = true;
        }

        if (command == F("hsv"))
        {
          Plugin_142_hsvDest[0] = event->Par1 / 360.0;   //Hue
          Plugin_142_hsvDest[1] = event->Par2 / 100.0;   //Saturation
          Plugin_142_hsvDest[2] = event->Par3 / 100.0;   //Value/Brightness
          bNewValue = true;
        }

        if (command == F("hsl"))
        {
          double hsl[3];
          hsl[0] = event->Par1 / 360.0;   //Hue
          hsl[1] = event->Par2 / 100.0;   //Saturation
          hsl[2] = event->Par3 / 100.0;   //Lightness
          Plugin_142_hsl2hsv(hsl, Plugin_142_hsvDest);
          bNewValue = true;
        }

        if (command == F("hue"))
        {
          Plugin_142_hsvDest[0] = event->Par1 / 360.0;   //Hue
          bNewValue = true;
        }

        if (command == F("sat"))
        {
          Plugin_142_hsvDest[1] = event->Par1 / 100.0;   //Saturation
          bNewValue = true;
        }

        if (command == F("val") || command == F("dimm"))
        {
          Plugin_142_hsvDest[2] = event->Par1 / 100.0;   //Value/Brightness
          bNewValue = true;
        }

        if (command == F("off"))
        {
          Plugin_142_hsvDest[2] = 0.0;   //Value/Brightness
          bNewValue = true;
        }

        if (command == F("cycle"))
        {
          Plugin_142_cycle = event->Par1;   //seconds for a full color hue circle
          if (Plugin_142_cycle > 0)
            Plugin_142_cycle = (1.0 / 50.0) / Plugin_142_cycle;   //50Hz based increment value
          success = true;
        }

        if (bNewValue)
        {
          Plugin_142_hsvClamp(Plugin_142_hsvDest);
          Plugin_142_hsvClamp(Plugin_142_hsvPrev);

          if (millisFadeBegin!=0)   //still fading?
            for (byte i=0; i<3; i++)
              Plugin_142_hsvPrev[i] = Plugin_142_hsvAct[i];

          //get the shortest way around the color circle
          if ((Plugin_142_hsvDest[0]-Plugin_142_hsvPrev[0]) > 0.5)
            Plugin_142_hsvPrev[0] += 1.0;
          if ((Plugin_142_hsvDest[0]-Plugin_142_hsvPrev[0]) < -0.5)
            Plugin_142_hsvPrev[0] -= 1.0;

          millisFadeBegin = millis();
          millisFadeEnd = millisFadeBegin + millisFadeTime;

          String log = F("RGB-S: hsv ");
          for (byte i=0; i<3; i++)
          {
            log += toString(Plugin_142_hsvDest[i], 3);
            log += F(" ");
          }
          addLog(LOG_LEVEL_INFO, log);

          Plugin_142_cycle = 0;   //ends cycle loop
          success = true;
        }
        break;
      }

    case PLUGIN_READ:
      {
        UserVar[event->BaseVarIndex + 0] = Plugin_142_hsvDest[0] * 360.0;
        UserVar[event->BaseVarIndex + 1] = Plugin_142_hsvDest[1] * 100.0;
        UserVar[event->BaseVarIndex + 2] = Plugin_142_hsvDest[2] * 100.0;
        success = true;
        break;
      }

    case PLUGIN_FIFTY_PER_SECOND:
    //case PLUGIN_TEN_PER_SECOND:
      {
        if (Plugin_142_cycle > 0)   // cyclic colors
        {
          Plugin_142_hsvDest[0] += Plugin_142_cycle;
          Plugin_142_hsvCopy(Plugin_142_hsvDest, Plugin_142_hsvPrev);
          Plugin_142_hsvCopy(Plugin_142_hsvDest, Plugin_142_hsvAct);
          Plugin_142_Output(Plugin_142_hsvDest);
        }
        else if (millisFadeEnd != 0)   //fading required?
        {
          long millisAct = millis();

          if (millisAct >= millisFadeEnd)   //destination reached?
          {
            millisFadeBegin = 0;
            millisFadeEnd = 0;
            Plugin_142_hsvCopy(Plugin_142_hsvDest, Plugin_142_hsvPrev);
            Plugin_142_hsvCopy(Plugin_142_hsvDest, Plugin_142_hsvAct);
          }
          else   //just fading
          {
            double fade = double(millisAct-millisFadeBegin) / double(millisFadeEnd-millisFadeBegin);
            fade = Plugin_142_valueClamp(fade);
            fade = Plugin_142_valueSmoothFadingOut(fade);

            for (byte i=0; i<3; i++)
              Plugin_142_hsvAct[i] = mix(Plugin_142_hsvPrev[i], Plugin_142_hsvDest[i], fade);
          }

          Plugin_142_Output(Plugin_142_hsvAct);
        }
        success = true;
        break;
      }

  }
  return success;
}

void Plugin_142_Output(double* hsvIn)
{
  double hsvw[4];
  double rgbw[4];

  String log = F("RGB-S: RGBW ");

  Plugin_142_hsvCopy(hsvIn, hsvw);
  Plugin_142_hsvClamp(hsvw);

  if (Plugin_142_pin[3] >= 0)   //has white channel?
  {
    hsvw[3] = (1.0 - hsvw[1]) * hsvw[2];   // w = (1-s)*v
    hsvw[2] *= hsvw[1];   // v = s*v
  }
  else
    hsvw[3] = 0.0;   // w = 0

  //convert to RGB color space
  Plugin_142_hsv2rgb(hsvw, rgbw);
  rgbw[3] = hsvw[3];

  //reduce power for mix colors
  double cv = sqrt(rgbw[0]*rgbw[0] + rgbw[1]*rgbw[1] + rgbw[2]*rgbw[2]);
  if (cv > 0.0)
  {
    cv = hsvw[2] / cv;
    cv = mix(1.0, cv, 0.42);
    for (byte i=0; i<3; i++)
      rgbw[i] *= cv;
  }

  int actRGBW[4];
  static int lastRGBW[4] = {-1,-1,-1,-1};

  //converting and corrections for each RGBW value
  for (byte i=0; i<4; i++)
  {
    int pin = Plugin_142_pin[i];
    //log += pin;
    //log += F("=");
    if (pin >= 0)   //pin assigned for RGBW value
    {
      rgbw[i] *= rgbw[i];   //simple gamma correction

      actRGBW[i] = rgbw[i] * (PWMRANGE-2*PLUGIN_PWM_OFFSET) + PLUGIN_PWM_OFFSET + 0.5;
      //if (actRGBW[i] > PWMRANGE)
      //  actRGBW[i] = PWMRANGE;

      log += String(actRGBW[i], DEC);
      log += F(" ");
    }
    else
    {
      log += F("- ");
    }
  }

  if (PLUGIN_PWM_OFFSET != 0 && actRGBW[0] == PLUGIN_PWM_OFFSET && actRGBW[1] == PLUGIN_PWM_OFFSET && actRGBW[2] == PLUGIN_PWM_OFFSET)
    actRGBW[0] = actRGBW[1] = actRGBW[2] = 0;

  //output to PWM
  for (byte i=0; i<4; i++)
  {
    int pin = Plugin_142_pin[i];
    if (pin >= 0)   //pin assigned for RGBW value
    {
      if (lastRGBW[i] != actRGBW[i])   //has changed since last output?
      {
        lastRGBW[i] = actRGBW[i];

        if (Plugin_142_lowActive)   //low active or common annode LED?
          actRGBW[i] = PWMRANGE - actRGBW[i];

        analogWrite(pin, actRGBW[i]);
        //setPinState(PLUGIN_ID_142, pin, PIN_MODE_PWM, actRGBW[i]);

        log += F("~");
      }
      else
        log += F(".");
    }
  }

  addLog(LOG_LEVEL_DEBUG, log);
}

// HSV->RGB conversion based on GLSL version
// expects hsv channels defined in 0.0 .. 1.0 interval
double fract(double x) { return x - int(x); }

double mix(double a, double b, double t) { return a + (b - a) * t; }

double step(double e, double x) { return x < e ? 0.0 : 1.0; }

double* Plugin_142_hsv2rgb(const double* hsv, double* rgb)
{
  RGBConverter conv;
  byte rgbtmp[3];
  conv.hsvToRgb(hsv[0], hsv[1], hsv[2], rgbtmp);
  rgb[0] = (double)(rgbtmp[0]*255); //do we need the *255?
  rgb[1] = (double)(rgbtmp[1]*255); //do we need the *255?
  rgb[2] = (double)(rgbtmp[2]*255); //do we need the *255?
  return rgb;
}

double* Plugin_142_rgb2hsv(const double* rgb, double* hsv)
{
  RGBConverter conv;
  byte r = (byte)(rgb[0]*255);
  byte g = (byte)(rgb[1]*255);
  byte b = (byte)(rgb[2]*255);
  conv.rgbToHsv(r,g,b,hsv);
  return hsv;
}

//see http://codeitdown.com/hsl-hsb-hsv-color/
double* Plugin_142_hsl2hsv(const double* hsl, double* hsv)
{
  RGBConverter conv;
  byte rgb[3];
  conv.hslToRgb(hsl[0], hsl[1], hsl[2], rgb);
  conv.rgbToHsv(rgb[0],rgb[1],rgb[2],hsv);
  return hsv;
}

double* Plugin_142_hsvCopy(const double* hsvSrc, double* hsvDst)
{
  for (byte i=0; i<3; i++)
    hsvDst[i] = hsvSrc[i];
  return hsvDst;
}

double* Plugin_142_hsvClamp(double* hsv)
{
  while (hsv[0] > 1.0)
    hsv[0] -= 1.0;
  while (hsv[0] < 0.0)
    hsv[0] += 1.0;

  for (byte i=1; i<3; i++)
  {
    if (hsv[i] < 0.0)
      hsv[i] = 0.0;
    if (hsv[i] > 1.0)
      hsv[i] = 1.0;
  }
  return hsv;
}

double Plugin_142_valueClamp(double v)
{
  if (v < 0.0)
    v = 0.0;
  if (v > 1.0)
    v = 1.0;
  return v;
}

double Plugin_142_valueSmoothFadingOut(double v)
{
  v = Plugin_142_valueClamp(v);
  v = 1.0-v;   //smooth fading out
  v *= v;
  v = 1.0-v;
  return v;
}
#endif
