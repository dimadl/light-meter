#include <BH1750.h>
#include <Wire.h>
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <EEPROM.h>


#define SCREEN_WIDTH 128 // OLED display width, in pixels
#define SCREEN_HEIGHT 64 // OLED display height, in pixels

// Declaration for SSD1306 display connected using software SPI (default case):
// #define OLED_MOSI  2
// #define OLED_CLK   8
//#define OLED_DC    12
//#define OLED_CS    13
// #define OLED_RESET 4


#define OLED_MOSI  11
#define OLED_CLK   13
#define OLED_DC 6
#define OLED_CS    10
#define OLED_RESET 5

// Button to cath value
#define capture_button_in_pin 7
#define scroll_button_pin 9

Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT,OLED_MOSI, OLED_CLK, OLED_DC, OLED_RESET, OLED_CS);
BH1750 ligthSensor;

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

//Global values
uint16_t latestMetering = 0;

int isoSettingAddress = 0;

uint16_t isoSettings[] = {50, 100, 200, 400}; 
uint8_t isoSettingsPosition = 0;

uint8_t currrentMenuPosition = 0;
bool isMenuActive = false;
bool isSettingsActve = false;
bool isMeasureActive = false;

void log(uint16_t lux) {
  Serial.print("Light measured: ");
  Serial.print(lux);
  Serial.println( "lx");
}

void displayMenu(int position) {

  isMenuActive = true;
  isSettingsActve = false;
  isMeasureActive = false;

  display.clearDisplay();
  display.setTextColor(BLACK, WHITE);
  display.setCursor(10, 5);
  display.print("Menu");
  display.setCursor(10, 30);
  display.setTextColor(WHITE);
  display.print("Measure");
  display.setCursor(10, 40);
  display.print("ISO");
  display.drawRect(8, 28 + 11 * position, 60, 11, WHITE);
  display.display();
}

void displaySettings1() {

  isMenuActive = false;
  isMeasureActive = false;
  isSettingsActve = true;

  display.clearDisplay();
  display.setTextColor(BLACK, WHITE);
  display.setCursor(10, 5);
  display.print("Settings");

  int8_t currentDistribIndex = isoSettingsPosition - 1;

  display.setTextColor(WHITE);
  display.setCursor(10, 20);

  if (currentDistribIndex >= 0) {
    display.print(isoSettings[currentDistribIndex]);
  }

  display.setTextSize(2);
  display.setCursor(10, 31);

  display.print(isoSettings[currentDistribIndex+1]);
  
  display.setCursor(10, 50);
  display.setTextSize(1);

  if(currentDistribIndex+2 <= 3) {
    display.print(isoSettings[currentDistribIndex+2]);
  }
  
  display.display();
}

void displayTitle() {
    display.setTextSize(1);
    display.setTextColor(BLACK, WHITE);
    display.setCursor(10, 5);
    display.print("ISO");
    display.setTextColor(WHITE);
    display.print(" ");
    display.print(isoSettings[isoSettingsPosition]);
    display.display();
}

void updateOnTheScreen(uint16_t lux, resultMeterring& result) {
    
    isMenuActive = false;
    isMeasureActive = true;
    isSettingsActve = false;

    display.clearDisplay();

    displayTitle();

    display.setTextSize(1);
    display.setTextColor(WHITE);
    display.setCursor(10, 20);
    display.print("Lux: ");
    display.print(lux);
    display.print(" lx");
    
    
    display.setCursor(10, 30);
    display.print("EV: ");
    display.print(result.ev);
    
    display.setCursor(10, 40);
    display.print("F: ");
    display.print(result.fStop);

    display.setCursor(10, 50);
    display.print("Shutter: ");
    display.print(result.shutter->name);

    display.display();


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
    double evISO = ev100 + log10(((double)isoSettings[isoSettingsPosition]/ (double) 100))/log10(2);
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

void resolveISOSetting() {
  uint8_t memorizedISOValuePosition = EEPROM.read(isoSettingAddress);
  Serial.println(memorizedISOValuePosition);
  if (memorizedISOValuePosition <= 3) {
    isoSettingsPosition = memorizedISOValuePosition;
  }
}

void setup() {

  Serial.begin(9600);

  pinMode(capture_button_in_pin, INPUT);
  pinMode(scroll_button_pin, INPUT);

  resolveISOSetting();

  Wire.begin();

  ligthSensor.begin();


  if(!display.begin(SSD1306_SWITCHCAPVCC)) {
    Serial.println(F("SSD1306 allocation failed"));
    for(;;); 
  }
  display.display();
  delay(2000); 
  display.clearDisplay();
  displayMenu(currrentMenuPosition);
}

void loop() {

  // uint16_t lux = ligthSensor.readLightLevel();
  // log(lux);

  int captureButtonState = digitalRead(capture_button_in_pin);
  int captureScrollButtonState = digitalRead(scroll_button_pin);

  if (captureScrollButtonState == HIGH) {
    if (isMenuActive) {
      currrentMenuPosition++;
      currrentMenuPosition = currrentMenuPosition <=1 ? currrentMenuPosition : 0;
      displayMenu(currrentMenuPosition);
    } else if (isSettingsActve) {
      
      isoSettingsPosition++;
      
      if (isoSettingsPosition > 3) {
        isoSettingsPosition = 0;
      }
      
      displaySettings1();

    } else {
      //Serial.print("Back to the menu. Active Position: ");
      //Serial.println(currrentMenuPosition);
      displayMenu(currrentMenuPosition);
    }
  }

  // capture value only if the capture button is pressed
  if (captureButtonState == HIGH) {

    if (isMenuActive && currrentMenuPosition == 1) {
    
      displaySettings1();

    } else if ((isMenuActive && currrentMenuPosition == 0) || isMeasureActive) {
      
      isMeasureActive = true;

      uint16_t lux = ligthSensor.readLightLevel();
      log(lux);

      if (latestMetering != lux) {
        latestMetering = lux;
        
        resultMeterring result  = caclulate(latestMetering);
        updateOnTheScreen(latestMetering, result);
      }
    } else if (isSettingsActve) {
      
      EEPROM.update(isoSettingAddress, isoSettingsPosition);

      displayMenu(currrentMenuPosition);
    }
  }

  delay(200);
}

