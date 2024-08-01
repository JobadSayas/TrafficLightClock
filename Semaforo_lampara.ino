// Version 11.24

#include <Wire.h>
#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd_1(0x27, 16, 2);

int moveButtonPin = 8; // Button to move between modes
int selectButtonPin = 7; // Button to select the mode
int redLedPin = 11; // Red LED pin
int greenLedPin = 9; // Green LED pin
int yellowLedPin = 10; // Yellow LED pin
int mode = 1; // Variable to track the current mode
int subMode = 0; // Variable to track the current sub-mode in settings
bool inSubMenu = false; // Flag to check if in submenu
bool inSubModeScreen = false; // Flag to check if in a sub-mode screen
unsigned long lastDebounceTimeMove = 0; // Last time the move button was toggled
unsigned long lastDebounceTimeSelect = 0; // Last time the select button was toggled
unsigned long debounceDelay = 50; // Debounce time in milliseconds
unsigned long lastButtonPressTime = 0; // Last time the button was pressed
unsigned long backlightTimeout = 10000; // Timeout for the backlight in milliseconds (10 seconds)
bool isLcdOn = false; // Variable to track if the LCD is on
int napTime = 12; // Default nap time
int wakeupTime = 7; // Default wakeup time
int sleepTime = 19; // Default sleep time

// Variable to track the hour for cycling
int currentHour = 12; // Default starting hour for cycling

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

// Variables to track button states and state changes
bool lastMoveButtonState = HIGH;
bool lastSelectButtonState = HIGH;
bool moveButtonPressed = false;
bool selectButtonPressed = false;

// Function prototypes
void updateMode();
void updateSubMenu();
void executeAction();
void executeSubAction();
void setLedBasedOnTime(int hour);
void displayFeedback(const char* line1, const char* line2);
void clearLCD(int line = 0);
void returnToLedBasedOnTime();
void red(int brightness = 100);
void yellow(int brightness = 255);
void green(int brightness = 100);

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

  unsigned long currentMillis = millis(); // Current time in milliseconds

  // Debouncing for move button
  if (moveButtonState != lastMoveButtonState) {
    lastDebounceTimeMove = currentMillis;
  }
  if ((currentMillis - lastDebounceTimeMove) > debounceDelay) {
    if (moveButtonState == LOW && !moveButtonPressed) {
      moveButtonPressed = true;
      handleMoveButtonPress();
    } else if (moveButtonState == HIGH && moveButtonPressed) {
      moveButtonPressed = false;
    }
  }
  lastMoveButtonState = moveButtonState;

  // Debouncing for select button
  if (selectButtonState != lastSelectButtonState) {
    lastDebounceTimeSelect = currentMillis;
  }
  if ((currentMillis - lastDebounceTimeSelect) > debounceDelay) {
    if (selectButtonState == LOW && !selectButtonPressed) {
      selectButtonPressed = true;
      handleSelectButtonPress();
    } else if (selectButtonState == HIGH && selectButtonPressed) {
      selectButtonPressed = false;
    }
  }
  lastSelectButtonState = selectButtonState;

  // Check if the backlight should be turned off
  if (isLcdOn && (currentMillis - lastButtonPressTime) > backlightTimeout) {
    lcd_1.setBacklight(0);
    isLcdOn = false; // Set the flag to indicate the LCD is off
  }
}

void handleMoveButtonPress() {
  unsigned long currentMillis = millis(); // Current time in milliseconds

  if (!isLcdOn) {
    // Turn on the LCD backlight and display the initial menu
    lcd_1.setBacklight(1);
    clearLCD();
    lcd_1.setCursor(0, 0);
    lcd_1.print("Main menu");
    updateMode();
    isLcdOn = true; // Set the flag to indicate the LCD is on
  } else if (inSubMenu) {
    if (inSubModeScreen) {
      // If in a sub-mode screen, handle hour cycling if in subMode 1, 2, or 3
      if (subMode == 1 || subMode == 2 || subMode == 3) {
        // Cycle through the hours for wakeupTime, napTime, or sleepTime
        currentHour++;
        if (currentHour > 24) {
          currentHour = 1; // Reset to 1 after reaching 24
        }
        clearLCD(2); // Clear only the second line
        lcd_1.setCursor(0, 1);
        lcd_1.print(currentHour);
        lcd_1.print(":00");
      }
    } else {
      // Move to the next submenu option
      subMode++;
      if (subMode > 4) {
        subMode = 1; // Reset to subMode 1 after subMode 4
      }
      updateSubMenu();
    }
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
  }
  lastButtonPressTime = currentMillis; // Update the last button press time
}

void handleSelectButtonPress() {
  unsigned long currentMillis = millis(); // Current time in milliseconds

  if (inSubMenu) {
    // Execute action based on the current sub-mode
    if (inSubModeScreen) {
      // If already in a sub-mode screen, return to the submenu
      if (subMode == 1) {
        // Save the selected wakeup time
        wakeupTime = currentHour; // Save the current hour as wakeupTime
        displayFeedback("Wake up time", "Saved");
        delay(2000); // Wait for 2 seconds
        inSubMenu = true;
        subMode = 1; // Position at 3.1 in the submenu
        updateSubMenu(); // Update the submenu display
        inSubModeScreen = false; // Reset the flag for sub-mode screen
      } else if (subMode == 2) {
        // Save the selected nap time
        napTime = currentHour; // Save the current hour as napTime
        displayFeedback("Nap time", "Saved");
        delay(2000); // Wait for 2 seconds
        inSubMenu = true;
        subMode = 2; // Position at 3.2 in the submenu
        updateSubMenu(); // Update the submenu display
        inSubModeScreen = false; // Reset the flag for sub-mode screen
      } else if (subMode == 3) {
        // Save the selected sleep time
        sleepTime = currentHour; // Save the current hour as sleepTime
        displayFeedback("Sleep time", "Saved");
        delay(2000); // Wait for 2 seconds
        inSubMenu = true;
        subMode = 3; // Position at 3.3 in the submenu
        updateSubMenu(); // Update the submenu display
        inSubModeScreen = false; // Reset the flag for sub-mode screen
      } else {
        inSubModeScreen = false; // Set the flag to indicate we are exiting the sub-mode screen
        updateSubMenu();
      }
    } else {
      executeSubAction();
      inSubModeScreen = true; // Set the flag to indicate we are in a sub-mode screen
    }
  } else {
    // Execute action based on the current main mode
    executeAction();
  }
  lastButtonPressTime = currentMillis; // Update the last button press time
}

void updateMode() {
  clearLCD(2); // Clear only the second line
  lcd_1.setCursor(0, 1); // Set cursor to the second line
  switch (mode) {
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
  red(0);
  yellow(0);
  green(0);

  // Execute actions based on the current main mode
  switch (mode) {
    case 1:
      // Sleep mode - Set the red LED on
      red(3);
      lcd_1.clear();
      lcd_1.setCursor(0, 0);
      lcd_1.print("Sleep mode");
      delay(5400000); // Wait for 1.5 hours
      returnToLedBasedOnTime();
      break;
    case 2:
      // Force wake up - Set the green LED on
      green();
      lcd_1.clear();
      lcd_1.setCursor(0, 0);
      lcd_1.print("Force wake up");
      break;
    case 3:
      // Settings mode - Enter the submenu
      inSubMenu = true;
      subMode = 1;
      updateSubMenu();
      break;
  }
}

void executeSubAction() {
  // Handle submenu actions
  switch(subMode) {
    case 1:
      // For subMode 1 (Wake up time)
      clearLCD();
      lcd_1.setCursor(0, 0); // Set cursor to the first line
      lcd_1.print("Setting wake up");
      lcd_1.setCursor(0, 1); // Set cursor to the second line
      lcd_1.print(wakeupTime);
      lcd_1.print(":00");
      currentHour = wakeupTime; // Initialize the current hour with wakeupTime
      break;
    case 2:
      // For subMode 2 (Nap time)
      clearLCD();
      lcd_1.setCursor(0, 0); // Set cursor to the first line
      lcd_1.print("Setting nap");
      lcd_1.setCursor(0, 1); // Set cursor to the second line
      lcd_1.print(napTime);
      lcd_1.print(":00");
      currentHour = napTime; // Initialize the current hour with napTime
      break;
    case 3:
      // For subMode 3 (Sleep time)
      clearLCD();
      lcd_1.setCursor(0, 0); // Set cursor to the first line
      lcd_1.print("Setting sleep");
      lcd_1.setCursor(0, 1); // Set cursor to the second line
      lcd_1.print(sleepTime);
      lcd_1.print(":00");
      currentHour = sleepTime; // Initialize the current hour with sleepTime
      break;
    case 4:
      // For subMode 4 (Exit)
      lcd_1.clear();
      lcd_1.setCursor(0, 0);
      lcd_1.print("Exiting settings");
      delay(2000); // Display for 2 seconds
      inSubMenu = false;
      clearLCD();
      lcd_1.setCursor(0, 0);
      lcd_1.print("Main menu");
      updateMode();
      inSubModeScreen = false; // Ensure the flag is reset
      return; // Exit the function to avoid displaying the second line
  }
}

void setLedBasedOnTime(int hour) {
  // Turn off all LEDs before setting the appropriate one
  red(0);
  yellow(0);
  green(0);

  // Set LED based on the current hour
  if (hour >= 0 && hour < (wakeupTime - 1)) {
    red(3);
  } else if (hour >= (wakeupTime - 1) && hour < wakeupTime) {
    yellow(10);
  } else if (hour >= wakeupTime && hour < (wakeupTime + 1)) {
    green(3);
  } else if (hour >= (wakeupTime + 1) && hour < (napTime - 1)) {
    green();
  } else if (hour >= (napTime - 1) && hour < napTime) {
    yellow();
  } else if (hour >= napTime && hour < (napTime + 1)) {
    red();
  } else if (hour >= (napTime + 1) && hour < (sleepTime - 1)) {
    green();
  } else if (hour >= (sleepTime - 1) && hour < sleepTime) {
    yellow();
  } else if (hour >= sleepTime && hour < 24) {
    red();
  }
}

void displayFeedback(const char* line1, const char* line2) {
  clearLCD();
  lcd_1.setCursor(0, 0); // Set cursor to the first line
  lcd_1.print(line1); // Print the feedback message on the first line
  lcd_1.setCursor(0, 1); // Set cursor to the second line
  lcd_1.print(line2); // Print the feedback message on the second line
}

void clearLCD(int line) {
  if (line == 0 || line == 1) {
    lcd_1.setCursor(0, 0);
    lcd_1.print("                "); // Clear the first line
  }
  if (line == 0 || line == 2) {
    lcd_1.setCursor(0, 1);
    lcd_1.print("                "); // Clear the second line
  }
}

void returnToLedBasedOnTime() {
  // Call setLedBasedOnTime with the current hour
  setLedBasedOnTime(dt.hour);
}

// Functions to set LED brightness
void red(int brightness = 100) {
  analogWrite(redLedPin, brightness);
}

void yellow(int brightness = 255) {
  analogWrite(yellowLedPin, brightness);
}

void green(int brightness = 100) {
  analogWrite(greenLedPin, brightness);
}
