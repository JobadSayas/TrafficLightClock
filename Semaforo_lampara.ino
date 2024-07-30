//Version 11.0

#include <Wire.h>
#include <DS3231.h>

DS3231 clock;
RTCDateTime dt;


// SETUP -----------
void setup(){

  // Initialize DS3231
  clock.begin();

  // Manual (YYYY, MM, DD, HH, II, SS
  // clock.setDateTime(2016, 12, 9, 11, 46, 00);
  
  // Send sketch compiling time to Arduino
  // clock.setDateTime(__DATE__, __TIME__);    
  
  /*Tips:
  This command will be executed every time when Arduino restarts. 
  Comment this line out to store the memory of DS3231 module*/

}


// LOOP ----------

void loop()
{

  //RELOJ
  //Iniciar e imprimir reloj
  dt = clock.getDateTime();

}


// NEW CODE ------------------------------------------------


#include <Adafruit_LiquidCrystal.h>

Adafruit_LiquidCrystal lcd_1(0);
int moveButtonPin = 8; // Button to move between modes
int selectButtonPin = 7; // Button to select the mode
int redLedPin = 11; // Red LED pin
int greenLedPin = 9; // Green LED pin
int yellowLedPin = 10; // Yellow LED pin
int mode = 1; // Variable to track the current mode
int subMode = 0; // Variable to track the current sub-mode in settings
bool inSubMenu = false; // Flag to check if in submenu
bool inSubModeScreen = false; // Flag to check if in a sub-mode screen
unsigned long lastDebounceTime = 0; // Last time the button was toggled
unsigned long debounceDelay = 50; // Debounce time in milliseconds
unsigned long lastButtonPressTime = 0; // Last time the button was pressed
unsigned long backlightTimeout = 10000; // Timeout for the backlight in milliseconds (10 seconds)
bool isLcdOn = false; // Variable to track if the LCD is on

// Define the DateTime struct
struct DateTime {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
};

// Create an instance of DateTime and hardcode values
DateTime dt = {2024, 7, 30, 14, 45, 0}; // Example date and time

void setup() {
  lcd_1.begin(16, 2); // Initialize the LCD with 16 columns and 2 rows
  pinMode(moveButtonPin, INPUT_PULLUP); // Use internal pull-up resistor for the move button
  pinMode(selectButtonPin, INPUT_PULLUP); // Use internal pull-up resistor for the select button
  pinMode(redLedPin, OUTPUT); // Set red LED pin as output
  pinMode(greenLedPin, OUTPUT); // Set green LED pin as output
  pinMode(yellowLedPin, OUTPUT); // Set yellow LED pin as output
  lcd_1.setBacklight(0); // Ensure the backlight is off at the start

  // Call function to set the LED based on the current hour
  setLedBasedOnTime(dt.hour);
}

void loop() {
  int moveButtonState = digitalRead(moveButtonPin); // Read the state of the move button
  int selectButtonState = digitalRead(selectButtonPin); // Read the state of the select button

  // Check if the move button state has changed
  if (moveButtonState == LOW && (millis() - lastDebounceTime) > debounceDelay) {
    if (!isLcdOn) {
      // Turn on the LCD backlight and display the initial menu
      lcd_1.setBacklight(1);
      clearLCD();
      lcd_1.setCursor(0, 0);
      lcd_1.print("Main menu");
      updateMode();
      isLcdOn = true; // Set the flag to indicate the LCD is on
      lastButtonPressTime = millis(); // Update the last button press time
    } else if (inSubMenu) {
      // Cycle through the submenu options
      if (inSubModeScreen) {
        // If in a sub-mode screen, return to the submenu
        inSubModeScreen = false;
        updateSubMenu();
      } else {
        // Move to the next submenu option
        subMode++;
        if (subMode > 4) {
          subMode = 1; // Reset to subMode 1 after subMode 4
        }
        updateSubMenu();
      }
      lastButtonPressTime = millis(); // Update the last button press time
    } else {
      // Cycle through the main modes
      mode++;
      if (mode > 3) {
        mode = 1; // Reset to Mode 1 after Mode 3
      }
      clearLCD();
      lcd_1.setCursor(0, 0); // Set cursor to the first line
      lcd_1.print("Main menu");
      updateMode(); // Update the display with the new mode
      lastButtonPressTime = millis(); // Update the last button press time
    }
    lastDebounceTime = millis(); // Reset the debounce timer
  }

  // Check if the select button state has changed
  if (selectButtonState == LOW && (millis() - lastDebounceTime) > debounceDelay) {
    if (inSubMenu) {
      // Execute action based on the current sub-mode
      if (inSubModeScreen) {
        // If already in a sub-mode screen, return to the submenu
        inSubModeScreen = false;
        updateSubMenu();
      } else {
        executeSubAction();
        inSubModeScreen = true; // Set the flag to indicate we are in a sub-mode screen
      }
    } else {
      if (mode == 3) {
        // Enter the submenu for settings
        inSubMenu = true;
        subMode = 1; // Start with the first submenu option
        updateSubMenu();
      } else {
        // Execute action based on the current main mode
        executeAction();
      }
    }
    lastButtonPressTime = millis(); // Update the last button press time
    lastDebounceTime = millis(); // Reset the debounce timer
  }

  // Check if the backlight should be turned off
  if (isLcdOn && (millis() - lastButtonPressTime) > backlightTimeout) {
    lcd_1.setBacklight(0);
    isLcdOn = false; // Set the flag to indicate the LCD is off
    // Do not turn off LEDs here
  }
}

void updateMode() {
  lcd_1.setCursor(0, 1); // Set cursor to the second line
  switch(mode) {
    case 1:
      lcd_1.print("1. Sleep mode"); // Ensure sufficient space to overwrite previous text
      break;
    case 2:
      lcd_1.print("2. Force wake up"); // Ensure sufficient space to overwrite previous text
      break;
    case 3:
      lcd_1.print("3. Settings"); // Ensure sufficient space to overwrite previous text
      break;
  }
}

void updateSubMenu() {
  clearLCD();
  lcd_1.setCursor(0, 0); // Set cursor to the first line
  lcd_1.print("Settings");
  lcd_1.setCursor(0, 1); // Set cursor to the second line
  switch(subMode) {
    case 1:
      lcd_1.print("3.1 Wake up time");
      break;
    case 2:
      lcd_1.print("3.2 Nap time");
      break;
    case 3:
      lcd_1.print("3.3 Sleep time");
      break;
    case 4:
      lcd_1.print("3.4 Exit");
      break;
  }
}

void executeAction() {
  // Turn off all LEDs before setting the appropriate one
  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(yellowLedPin, LOW);

  // Perform action based on the current mode
  switch(mode) {
    case 1:
      digitalWrite(redLedPin, HIGH); // Turn on red LED for Mode 1
      displayFeedback("Sleep mode", "Applied");
      break;
    case 2:
      digitalWrite(greenLedPin, HIGH); // Turn on green LED for Mode 2
      displayFeedback("Force wake up", "Applied");
      break;
    case 3:
      // For Mode 3 (Settings), no LED is turned on
      inSubMenu = true;
      updateSubMenu();
      break;
  }
}

void executeSubAction() {
  clearLCD();
  lcd_1.setCursor(0, 0); // Set cursor to the first line
  switch(subMode) {
    case 1:
      lcd_1.print("Setting wake up");
      break;
    case 2:
      lcd_1.print("Setting nap time");
      break;
    case 3:
      lcd_1.print("Setting sleep time");
      break;
    case 4:
      // Exit the submenu and return to the main menu
      inSubMenu = false;
      clearLCD();
      lcd_1.setCursor(0, 0);
      lcd_1.print("Main menu");
      updateMode();
      inSubModeScreen = false; // Ensure the flag is reset
      return; // Exit the function to avoid displaying the second line
  }
  // Removed delay and automatic return to submenu
}

void setLedBasedOnTime(int hour) {
  // Turn off all LEDs before setting the appropriate one
  digitalWrite(redLedPin, LOW);
  digitalWrite(greenLedPin, LOW);
  digitalWrite(yellowLedPin, LOW);

  // Set LED based on the current hour
  if (hour >= 0 && hour < 6) {
    digitalWrite(redLedPin, HIGH); // Red LED
  } else if (hour >= 6 && hour < 7) {
    digitalWrite(yellowLedPin, HIGH); // Yellow LED
  } else if (hour >= 7 && hour < 11) {
    digitalWrite(greenLedPin, HIGH); // Green LED
  } else if (hour >= 11 && hour < 12) {
    digitalWrite(yellowLedPin, HIGH); // Yellow LED
  } else if (hour >= 12 && hour < 13) {
    digitalWrite(redLedPin, HIGH); // Red LED
  } else if (hour >= 13 && hour < 18) {
    digitalWrite(greenLedPin, HIGH); // Green LED
  } else if (hour >= 18 && hour < 19) {
    digitalWrite(yellowLedPin, HIGH); // Yellow LED
  } else if (hour >= 19 && hour < 24) {
    digitalWrite(redLedPin, HIGH); // Red LED
  }
}

void displayFeedback(const char* line1, const char* line2) {
  clearLCD();
  lcd_1.setCursor(0, 0); // Set cursor to the first line
  lcd_1.print(line1); // Print the feedback message on the first line
  lcd_1.setCursor(0, 1); // Set cursor to the second line
  lcd_1.print(line2); // Print the feedback message on the second line
}

void clearLCD() {
  lcd_1.setCursor(0, 0);
  lcd_1.print("                "); 
  lcd_1.setCursor(0, 1);
  lcd_1.print("                "); 
}