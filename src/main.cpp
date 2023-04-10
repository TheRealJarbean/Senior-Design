/*
  Author: Jaron Anderson
  Description: Logic for heated press display
*/

#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <BfButton.h>
#include <max6675.h>

// Custom setup variables
const int CLK = 13; // Arduino static clock pin
const int HEATER_PIN = 11; // Heater control pin

// Display - Set the LCD address to 0x27, 20 and 4 for 20x4 display
LiquidCrystal_I2C lcd(0x27,20,4);

// Button setup variables
const int btnPin = 3; // Encoder - Button pin
const int DT = 4;  // Encoder - DT pin (determines direction of rotation)
BfButton btn(BfButton::STANDALONE_DIGITAL, btnPin, true, LOW); // BfButton instance, from ButtonFever documentation
int aState;
int aLastState;

// Thermocouple setup
float temp0, temp1, avgTemp;
const int thermoDO = 12;
const int thermo0CS = 6;
const int thermo1CS = 5;
const int MELTING_TEMP = 180;

MAX6675 thermocouple0(CLK, thermo0CS, thermoDO);
MAX6675 thermocouple1(CLK, thermo1CS, thermoDO);

// Hot Light setup
const int LEDPin = 11;
bool heaterOn = false;

// Menu vars
String line0, line1, line2, line3;
int selection, selMin, selMax, currentMenu;
bool refresh = false; // Some menus, like temps, need to be refreshed to show current data
const int refreshRate = 1000; // When refresh is set, the menu will reprint every refreshRate ms, lower number equals faster refresh, default 1sec
unsigned long time = 0; // Keeps track of time passed for millis() comparison at refreshRate intervals
const int minutesForCycle = 15; // How long a cycle takes to run
int timeRemaining = minutesForCycle * 60; // Keeps track of how many seconds are left in a cycle

// Menu enum
enum menu {
  HOME,
  TEMPS,
  MANUAL,
  CYCLE
};

// Function prototypes
void setSelection(int sel);
void selectOption();

// A menu must have a value for each of the four lines
// on the display
void MenuHome () {
    line0 = "--------------------";
    line1 = "  Start Cycle";
    line2 = "  Manual Control";
    line3 = "--------------------";
    selMin = 1;
    selMax = 2;
    selection = 1;
}

void MenuCycle () {
    temp0 = thermocouple0.readCelsius();
    temp1 = thermocouple1.readCelsius();
    avgTemp = (temp0 + temp1) / 2;
    line0 = "--------------------";
    line1 = "  Avg Temp: " + String(avgTemp) + char(0xDF) + "C";
    if (avgTemp >= MELTING_TEMP) {
      int minutes = timeRemaining / 60;
      int seconds = timeRemaining % 60;
      String minutesZero = (minutes < 10) ? "0" : ""; // Add a leading zero if less than 10
      String secondsZero = (seconds < 10) ? "0" : ""; // ""
      line2 = "  Time: " + minutesZero + String(minutes) + ":" + secondsZero + String(seconds);
    }
    else {
      line2 = "  Heating up...";
    }
    line3 = "  Cancel";
    selMin = 3;
    selMax = 3;
    selection = 3;
}

void MenuManual () {
    line0 = "--------------------";
    line1 = "  Temps";
    if (heaterOn) {
      line2 = "  Turn heater off";
    }
    else {
      line2 = "  Turn heater on";
    }
    line3 = "  Back";
    selMin = 1;
    selMax = 3;
    selection = 1;
}

void MenuTemps () {
    temp0 = thermocouple0.readCelsius();
    temp1 = thermocouple1.readCelsius();
    avgTemp = (temp0 + temp1) / 2;
    line0 = "   Temp0:   " + String(temp0) + char(0xDF) + "C";
    line1 = "   Temp1:   " + String(temp1) + char(0xDF) + "C";
    line2 = "     Avg:   " + String(avgTemp) + char(0xDF) + "C";
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
  else if (menuID == MANUAL) {
    MenuManual();
  }
  else if (menuID == TEMPS) {
    MenuTemps();
  }
  else if (menuID == CYCLE) {
    MenuCycle();
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

// Moves selection and selection indicator up and down
// on the menu
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

  // Wrap selection back around on hitting an edge
  if (selection < selMin) {
    selection = selMax;
  }
  if (selection > selMax) {
    selection = selMin;
  }

  lcd.setCursor(0, selection);
  lcd.print("*");
}

// Run commands associated with the selection position
// on the current menu
void selectOption() {
  switch(currentMenu) {
    case HOME:
      if (selection == 1) {
        timeRemaining = minutesForCycle * 60;
        printMenu(CYCLE);
        digitalWrite(HEATER_PIN, HIGH); // Turn heater & hot light on
        refresh = true;
      }
      else if (selection == 2) {
        printMenu(MANUAL);
      }
      break;

    case CYCLE:
      digitalWrite(HEATER_PIN, LOW); // Turn heater & hot light off
      printMenu(HOME);
      refresh = false;
      break;
    
    case MANUAL:
      if (selection == 1) {
        printMenu(TEMPS);
        refresh = true;
      }
      else if (selection == 2) {
        heaterOn = !heaterOn;
        if (heaterOn) {
          digitalWrite(HEATER_PIN, HIGH); // Turn heater & hot light on
        }
        else {
          digitalWrite(HEATER_PIN, LOW); // Turn heater & hot light off
        }
        printMenu(MANUAL);
      }
      else if (selection == 3) {
        printMenu(HOME);
      }
      break;

    case TEMPS:
      printMenu(MANUAL);
      refresh = false;
      break;

  }
}

void setup() {
  Serial.begin(9600);
  lcd.init();
  lcd.backlight();

  // Button setup from ButtonFever documentation
  btn.onPress(pressHandler)
  .onDoublePress(pressHandler)     // default timeout
  .onPressFor(pressHandler, 1000); // custom timeout for 1 second

  // Set LED control pin to output
  pinMode(HEATER_PIN, OUTPUT);

  // Startup on home menu
  printMenu(HOME);
}

void loop() {
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

  // Reprint menu if refresh is true
  if (millis() >= time + refreshRate) {
    time += refreshRate;
    if (refresh) {
      Serial.println("Refreshing menu!");
      printMenu(currentMenu);
      if (currentMenu == CYCLE && avgTemp >= MELTING_TEMP) { 
        timeRemaining -= 1;
        if (timeRemaining == 0) {
          selectOption(); // Forces selection of the cancel option upon completed cycle, turning off heater
        }
      }
    }
  }
}