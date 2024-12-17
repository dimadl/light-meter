#include <BH1750.h>
#include <Wire.h>
#include <EEPROM.h>
#include <U8g2lib.h>

BH1750 ligthSensor;

//Display
U8G2_SSD1306_128X64_NONAME_2_HW_I2C u8g2(U8G2_R0, SCL, SDA, U8X8_PIN_NONE);

class Shutter {
  public:
    double speed;
    char name[6];
};

typedef struct {
  double fStop;
  Shutter* shutter;
} mapFShutter;

typedef struct {
  double ev;
  double fStop;
  Shutter* shutter;
} resultMeterring;

Shutter shutter38 = {0.0111111, "1/90"};
Shutter shutter46 = {0.0103092, "1/97"};
Shutter shutter56 = {0.0076923, "1/130"};
Shutter shutter63 = {0.0048543, "1/206"};
Shutter shutter71 = {0.0030769, "1/325"};
Shutter shutter8 = {0.001953125, "1/512"};
Shutter shutter11 = {0.001953125, "1/512"};
Shutter shutter16 = {0.001953125, "1/512"};

mapFShutter fs1 = {3.8, &shutter38};
mapFShutter fs2 = {4.6, &shutter46};
mapFShutter fs3 = {5.6, &shutter56};
mapFShutter fs4 = {6.3, &shutter63};
mapFShutter fs5 = {7.1, &shutter71};
mapFShutter fs6 = {8, &shutter8};
mapFShutter fs7 = {11, &shutter11};
mapFShutter fs8 = {16, &shutter16};

mapFShutter* definedFShutterMapping[8] = {&fs1, &fs2, &fs3, &fs4, &fs5, &fs6, &fs7, &fs8} ;

//ISO Settings
int isoSettingAddress = 0;

uint16_t isoSettings[4] = {400, 200, 100, 50}; 
uint8_t isoSettingsPosition = 4;

void log(uint16_t lux) {
  Serial.print("Light measured: ");
  Serial.print(lux);
  Serial.println( "lx");
}

resultMeterring caclulate(uint16_t& lux) {
    //1. Create a map between f-stops and shutter speed (they are hardcoded by the photoapparat and can't be changed)

    //2. Add setting for ISO
    //Serial.print("debug: ");
    //Serial.println(lux);

    //3. Calculate EV100 based on Lux
    double ev100 = log10(lux/2.5)/log10(2);
    // Serial.print("EV for 100: ");
    // Serial.println(ev100);   

    //4. Adjust EV according to the chosen film speed
    double evISO = ev100 + log10(((double)isoSettings[isoSettingsPosition - 1]/ (double) 100))/log10(2);
    // Serial.print("EV for ISO: ");
    // Serial.println(evISO);

    //5. Caclulate Shutter Speed as there is a mode for fixed Aperture for all f-stops
    int minIndex;
    double latestMinDiff = -1;

    for (int i=0; i<8; i++) {
      double fStop = (*definedFShutterMapping[i]).fStop;
      double calculatedShutterSpeed = fStop/pow(2, evISO);

      //6. Find the minimum deviation
      double diff = abs(calculatedShutterSpeed - (*definedFShutterMapping[i]).shutter->speed);

      if (diff < latestMinDiff || latestMinDiff == -1) {
        latestMinDiff = diff;
        minIndex = i;
      }
    }

    //7. return f-stop asa result
    resultMeterring result = {evISO, (*definedFShutterMapping[minIndex]).fStop, (*definedFShutterMapping[minIndex]).shutter};
    return result;
}

const char *string_list = 
  "Measure\n"
  "ISO";

const char *iso_settings_list = 
  "400\n"
  "200\n"
  "100\n"
  "50";

uint8_t current_selection = 0;
uint8_t initial_current_selection = 1;
int8_t previous_menu_selection = -1;

bool isMenuActivated = true;

void resolveISOSetting() {
  uint8_t memorizedISOValuePosition = EEPROM.read(isoSettingAddress);
  Serial.println(memorizedISOValuePosition);
  if (memorizedISOValuePosition <= 3) {
    isoSettingsPosition = memorizedISOValuePosition;
  }
}

void drawMeasurePage() {

  uint16_t lux = ligthSensor.readLightLevel();
  log(lux);
  resultMeterring result  = caclulate(lux);
  
  char resultStr[30];
  int fStopIntPart = result.fStop;
  int fStopDecimalPart = ceil((result.fStop - fStopIntPart) * 10);

  Serial.println(fStopDecimalPart); 
  sprintf(resultStr, "Lux: %d lx\nF: %d.%d \nShtr: %s",  lux, fStopIntPart, fStopDecimalPart, result.shutter->name);
  
  uint8_t select = u8g2.userInterfaceMessage(
	      resultStr, 
	      "",
	        "",
	        " Mes. \n Back ");

  Serial.println(select);

  if (select == 1) {
    drawMeasurePage();
  } else if (select == 2) {
    current_selection = 0;
  }
}

void drawSettingsPage() {
  u8g2.clearDisplay();
  Serial.println(iso_settings_list);
  isoSettingsPosition = u8g2.userInterfaceSelectionList(
      "ISO",
      isoSettingsPosition, 
    iso_settings_list);

  EEPROM.update(isoSettingAddress, isoSettingsPosition);

  //Reset to menu
  current_selection = 0;
}

void setup() {

  //Wire.begin();
  Serial.begin(9600);
  resolveISOSetting();

  u8g2.begin(2, 4, U8X8_PIN_NONE, U8X8_PIN_NONE, U8X8_PIN_NONE, 7);
  u8g2.setFont(u8g2_font_6x12_tr);
  Serial.println("loaded display");

  ligthSensor.begin();
  Serial.println("loaded sensor");
}

void loop() {

  if (previous_menu_selection != current_selection) {
    previous_menu_selection = current_selection;
      //Settings page activated
    if (current_selection == 2) {
      Serial.println("Draw settings");
      drawSettingsPage();
    }
    //Measure page
    else if (current_selection == 1) {

      Serial.println("Draw measure page");
      drawMeasurePage();

    } 
    //Menu
    else {
      Serial.println("Draw menu");
      current_selection = u8g2.userInterfaceSelectionList(
        "Menu",
        initial_current_selection, 
      string_list);
    }
  };
    Serial.print(current_selection);

    delay(200);
  }