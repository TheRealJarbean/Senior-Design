#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BfButton.h>
#include <max6675.h>

// Multi-use setup variables
const int CLK = 13; // Arduino static clock pin

// Display - Set the LCD address to 0x27, 20 and 4 for 20x4 display
LiquidCrystal_I2C lcd(0x27,20,4);

// SSR Setup
const int SSR_PIN = 7;

// Button setup variables
const int BTN_PIN = 3; // Encoder - Button pin
const int DT = 4;  // Encoder - DT pin (determines direction of rotation)
BfButton btn(BfButton::STANDALONE_DIGITAL, BTN_PIN, true, LOW); // BfButton instance, from ButtonFever documentation
int aState;
int aLastState;

// Thermocouple setup
const int THERMO_DO = 12;
const int THERMO_CS_0 = 6;
const int THERMO_CS_1 = 5;

MAX6675 thermocouple0(CLK, THERMO_CS_0, THERMO_DO);
MAX6675 thermocouple1(CLK, THERMO_CS_1, THERMO_DO);

// Hot Light setup
const int LED_PIN = 11;
bool heaterOn = false;

// Menu vars
String line0, line1, line2, line3;
int selection, selMin, selMax, currentMenu;
bool refresh = false; // Some menus, like temps, need to be refreshed to show current data
const int REFRESH_RATE = 3000; // When refresh is set, the menu will reprint every REFRESH_RATE ms,
                               // lower number equals faster refresh
unsigned long time = 0; // Keeps track of time passed for millis() comparison at REFRESH_RATE intervals

// Menu enum
enum menu {
  HOME,
  TEMPS
};

// Function prototypes
void setSelection(int sel);
void selectOption();

// Menu that is used to navigate to other menus, start
// heating operations, returned to by default in most cases
void MenuHome () {
    line0 = "--------------------";
    line1 = "  Temps";
    if (heaterOn) {
      line2 = "  Turn heater off";
    }
    else {
      line2 = "  Turn heater on";
    }
    line3 = "--------------------";
    selMin = 1;
    selMax = 2;
    selection = 1;
}

// Menu that shows current temperatures of both sensors,
// avg temp, and option to go back to home menu
void MenuTemps () {
    float temp0 = thermocouple0.readCelsius();
    float temp1 = thermocouple1.readCelsius();
    line0 = "   Temp0: " + String(temp0) + char(0xDF) + "C";
    line1 = "   Temp1: " + String(temp1) + char(0xDF) + "C";
    line2 = "     Avg: " + String((temp0 + temp1) / 2) + char(0xDF) + "C";
    line3 = "  Back";
    selMin = 3;
    selMax = 3;
    selection = 3;
}

// Switch to selected menu and print its lines
void printMenu(int menuID) {
  if (menuID == HOME) {
    MenuHome();
  }
  else if (menuID == TEMPS) {
    MenuTemps();
  }

  lcd.clear();
  lcd.setCursor(0,0);
  lcd.print(line0);
  lcd.setCursor(0,1);
  lcd.print(line1);
  lcd.setCursor(0,2);
  lcd.print(line2);
  lcd.setCursor(0,3);
  lcd.print(line3);

  setSelection(selection);
  currentMenu = menuID;
}

// From ButtonFever documentation, receive presses and return actions
void pressHandler (BfButton *btn, BfButton::press_pattern_t pattern) {
  switch (pattern)  {
    case BfButton::SINGLE_PRESS:
      Serial.println("Button pressed!");
      selectOption();
      break;
    case BfButton::DOUBLE_PRESS: // Do nothing
      Serial.println("Button double pressed!");
      break;
    case BfButton::LONG_PRESS: // Do nothing
      Serial.println("Button long pressed!");
      break;
  }
}

// Forces selection of a certain option, such as when changing menus
void setSelection(int sel) {
  lcd.setCursor(0, sel);
  lcd.print("*");
  selection = sel;
}

// Shifts selection on current screen up or down
// shift == -1 : Move cursor up 1 or wrap around
// shift == 1  : Move cursor down 1 or wrap around
void cycleSelection(int shift) {
  lcd.setCursor(0, selection);
  lcd.print(" ");

  switch (shift) {
    case -1:
      Serial.println("Shifting selection up!");
      selection -= 1;
      break;
    case 1:
      Serial.println("Shifting selection down!");
      selection += 1;
      break;
  }

  if (selection < selMin) {
    selection = selMax;
  }
  if (selection > selMax) {
    selection = selMin;
  }

  lcd.setCursor(0, selection);
  lcd.print("*");
}

// Contains options for all menus
// Selection refers to row that is currently selected
// Ensure that selection number matches corresponding menu line
void selectOption() {
  switch(currentMenu) {
    case HOME:
      if (selection == 1) {
        printMenu(TEMPS);
        refresh = true;
      }
      if (selection == 2) {
        heaterOn = !heaterOn;
        if (heaterOn) {
          digitalWrite(11, HIGH); // Turn hot light on
          digitalWrite(7, HIGH); // Turn heater on
        }
        else {
          digitalWrite(11, LOW); // Turn hot light off
          digitalWrite(7, LOW); // Turn heater off
        }
        printMenu(HOME);
      }
      break;

    case TEMPS:
      if (selection == 3) {
        printMenu(HOME);
        refresh = false;
      }
      break;
  }
}

void setup() {
  // put your setup code here, to run once:

  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // Button setup from ButtonFever documentation
  btn.onPress(pressHandler)
  .onDoublePress(pressHandler)     // default timeout
  .onPressFor(pressHandler, 1000); // custom timeout for 1 second

  // Set SSR control pin to output
  pinMode(7, OUTPUT);

  // Set LED control pin to output
  pinMode(11, OUTPUT);

  // Startup on home menu
  printMenu(HOME);
}

void loop() {
  // put your main code here, to run repeatedly:
  
  btn.read();
  aState = digitalRead(CLK);

  // Encoder rotation tracking
  if (aState != aLastState) {
    if (digitalRead(DT) != aState) {
      cycleSelection(1);
      delay(500);
    }
    else {
      cycleSelection(-1);
      delay(500);
    }
  }
  aState = digitalRead(CLK);
  aLastState = aState;

  if (millis() >= time + REFRESH_RATE) {
    time += REFRESH_RATE;
    // Reprint menu if refresh is true
    if (refresh) {
      Serial.println("Refreshing menu!");
      printMenu(currentMenu);
    }
  }
}