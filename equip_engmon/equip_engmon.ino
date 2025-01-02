// Dust Collection System for Shop
// Released Version 5.0 01/02/2025
//
// Code developed with help from: Cursor AI

#include <LiquidCrystal_I2C.h>
LiquidCrystal_I2C lcd(0x27,20,4);  // set the LCD address to 0x27 for a 16 chars and 4 line display
/*********************************************************/

/* Program for Dust Collection System */

#include "EmonLib.h"          // Include Emon Library
EnergyMonitor tool1, tool2, tool3, tool4;   // Create instances for shop equipment - Table Saw, Miter Saw, CNC Router

const int dustCollectionRelayPin = 51; //Relay to Dust collector
const int manualButtons[] = { 5, 6, 7, 8 };  // Pins for buttons

unsigned long startupCounter = 0;

// Constants for tool configuration
const int NUM_TOOLS = 4;  
const double TOOL_THRESHOLDS[] = {15.0, 12.0, 8.0, 10.0};  
const char* TOOL_NAMES[] = {"Table Saw", "Miter Saw", "CNC Router", "Tool 4"};
const int BLAST_GATE_PINS[] = {27, 35, 39, 43, 47};

// Add new state variables and timing controls
unsigned long previousMillis = 0;
const unsigned long DISPLAY_INTERVAL = 250;  // Update display every 0.5 seconds
const unsigned long TOOL_TRANSITION_TIME = 500;  // 0.5 seconds for tool transitions
const unsigned long TOOL_OFF_TRANSITION_TIME = 15000;  // 15 seconds for shutdown
const unsigned long DEBOUNCE_DELAY = 25;    // Reduced to 25ms for faster response while maintaining stability
const unsigned long BUTTON_HOLD_THRESHOLD = 200; // 200ms threshold
unsigned long lastDebounceTime[NUM_TOOLS] = {0}; // Last time buttons were toggled
bool debouncedButtonStates[NUM_TOOLS] = {false}; // Stable button states

enum SystemState {
  STARTUP,
  MONITORING,
  TOOL_ACTIVATING,
  TOOL_RUNNING,
  TOOL_DEACTIVATING,
  MANUAL_CONTROL  // New state
};

SystemState currentState = STARTUP;
unsigned long stateTimer = 0;

const unsigned long DUST_OFF_DELAY = 12000;  // 12 seconds in milliseconds

// Combine tool monitoring into arrays for easier management
EnergyMonitor tools[NUM_TOOLS];
double toolCurrents[NUM_TOOLS];

// Add button state tracking
bool buttonStates[NUM_TOOLS] = {false};
bool lastButtonStates[NUM_TOOLS] = {false};
bool manualGateStates[NUM_TOOLS] = {false};

// Add status display helper function
void updateGateStatusDisplay() {
  // clearRow(1);
  // clearRow(2);
  // lcd.setCursor(0, 1);
  // lcd.print("Gates:");  // hide this line for IRMS on lines 1&2
  
  // Show gate states on line 3
  lcd.setCursor(0, 2);
  lcd.print("BG ");
  for(int i = 0; i < NUM_TOOLS; i++) {
    lcd.print(i + 1);
    lcd.print(":");
    lcd.print(manualGateStates[i] ? "O" : "C");
    lcd.print(" ");
  }
}

void setup()
{
  Serial.begin(9600);  // Initialize serial communication
  startupCounter = 0;
  
  // Replace individual Irms variables with an array
  double Irms[NUM_TOOLS] = {0};
   
  lcd.init();       //initialize the lcd
  lcd.backlight();  //open the backlight 

  lcd.clear();
  lcd.setCursor(0,0);           // set cursor to column 0, row 0 (the first row)
  lcd.print("ShopTool Monitor");
  lcd.setCursor(0,1);
  lcd.print("DC v5.0");
  delay(4000);  //Consider removing this if splash screen isn't needed
  lcd.clear();

  // Initialize tools in a loop
  for(int i = 0; i < NUM_TOOLS; i++) {
    tools[i].current(i, 111.1);
  }

  // Set pin modes and initial states using arrays
  pinMode(dustCollectionRelayPin, OUTPUT);
  digitalWrite(dustCollectionRelayPin, HIGH);  // enable off

  for(int i = 0; i < sizeof(BLAST_GATE_PINS)/sizeof(BLAST_GATE_PINS[0]); i++) {
    pinMode(BLAST_GATE_PINS[i], OUTPUT);
    digitalWrite(BLAST_GATE_PINS[i], HIGH);    // enable off
  }

  // Initialize button pins
  for(int i = 0; i < NUM_TOOLS; i++) {
    pinMode(manualButtons[i], INPUT_PULLUP);
  }
}

void loop()
{
  unsigned long currentMillis = millis();
  static unsigned long lastDisplayUpdate = 0;
  
  // Check for button presses with enhanced feedback
  bool anyButtonPressed = false;
  for(int i = 0; i < NUM_TOOLS; i++) {
    // Read the current raw button state (active LOW due to INPUT_PULLUP)
    bool rawButtonState = (digitalRead(manualButtons[i]) == LOW);
    
    // Button state changed from not pressed to pressed
    if (rawButtonState && !lastButtonStates[i]) {
      lastDebounceTime[i] = currentMillis;
    }
    
    // If button is pressed and debounce time has passed
    if (rawButtonState && 
        !debouncedButtonStates[i] &&
        (currentMillis - lastDebounceTime[i]) > DEBOUNCE_DELAY) {
      
      debouncedButtonStates[i] = true;
      manualGateStates[i] = !manualGateStates[i];  // Toggle gate state
      digitalWrite(BLAST_GATE_PINS[i], !manualGateStates[i]);  // Update physical gate
      
      if (currentState != MANUAL_CONTROL) {
        currentState = MANUAL_CONTROL;
        dustOn();
        // Force immediate display update when entering manual mode
        clearRow(3);
        lcd.setCursor(0,3);
        lcd.print("Manual - DC ON");
        updateGateStatusDisplay();
      }
      anyButtonPressed = true;
      
      // Enhanced debug output
      Serial.print("\n=== Button Event ===\n");
      Serial.print("Button: ");
      Serial.print(i + 1);
      Serial.print(" | Gate: ");
      Serial.print(manualGateStates[i] ? "OPEN" : "CLOSED");
      Serial.print(" | Time: ");
      Serial.print(currentMillis);
      Serial.print("ms\n");
      
      // Check if all gates are now closed after this button press
      bool allGatesClosed = true;
      for(int j = 0; j < NUM_TOOLS; j++) {
        if (manualGateStates[j]) {
          allGatesClosed = false;
          break;
        }
      }
      
      if (allGatesClosed && currentState == MANUAL_CONTROL) {
        Serial.println("All gates closed - returning to monitoring mode");
        dustOff();
        currentState = MONITORING;
        clearRow(3);
        lcd.setCursor(0,3);
        lcd.print("Monitoring - DC OFF");
      }
      
      // Immediate display update on button press
      updateGateStatusDisplay();
      lastDisplayUpdate = currentMillis;
    } 
    // Reset when button is released
    else if (!rawButtonState) {
      debouncedButtonStates[i] = false;
    }
    
    lastButtonStates[i] = rawButtonState;
  }

  // Periodic check for all gates closed (backup check)
  if (currentState == MANUAL_CONTROL) {
    bool anyGateOpen = false;
    for(int i = 0; i < NUM_TOOLS; i++) {
      if (manualGateStates[i]) {
        anyGateOpen = true;
        break;
      }
    }
    if (!anyGateOpen) {
      Serial.println("All gates closed (periodic check) - returning to monitoring mode");
      dustOff();
      currentState = MONITORING;
      clearRow(3);
      lcd.setCursor(0,3);
      lcd.print("Monitoring - DC OFF");
    }
  }

  // Periodic display updates in manual mode
  if (currentState == MANUAL_CONTROL && 
      (currentMillis - lastDisplayUpdate) >= DISPLAY_INTERVAL) {
    clearRow(3);
    lcd.setCursor(0,3);
    lcd.print("Manual - DC ON");
    updateGateStatusDisplay();
    lastDisplayUpdate = currentMillis;
  }

  // Read current values
  for(int i = 0; i < NUM_TOOLS; i++) {
    toolCurrents[i] = tools[i].calcIrms(1480);
  }

  // Update display periodically
  if (currentMillis - lastDisplayUpdate >= DISPLAY_INTERVAL) {
    updateDisplay();
    lastDisplayUpdate = currentMillis;
  }

  // Handle TOOL_DEACTIVATING state immediately after current readings
  if (currentState == TOOL_DEACTIVATING) {
    unsigned long elapsedTime = currentMillis - stateTimer;
    
    // Check if any tool has started running again
    for(int i = 0; i < NUM_TOOLS; i++) {
      if(toolCurrents[i] > (TOOL_THRESHOLDS[i] * 0.8)) {
        currentState = TOOL_RUNNING;
        return;
      }
    }
    
    // If timer is complete, proceed with shutdown
    if (elapsedTime >= TOOL_OFF_TRANSITION_TIME) {
      dustOff();
      
      for(int i = 0; i < sizeof(BLAST_GATE_PINS)/sizeof(BLAST_GATE_PINS[0]); i++) {
        digitalWrite(BLAST_GATE_PINS[i], HIGH);
      }
      
      clearRow(3);
      lcd.setCursor(0,3);
      lcd.print("Monitoring - DC OFF");
      currentState = MONITORING;
    }
    return;
  }

  // State machine
  switch(currentState) {
    case STARTUP:
      if (startupCounter++ >= 5) {
        currentState = MONITORING;
      }
      break;

    case MONITORING:
      // Check all tools in a loop with individual thresholds
      for(int i = 0; i < NUM_TOOLS; i++) {
        if(toolCurrents[i] > TOOL_THRESHOLDS[i]) {
          digitalWrite(BLAST_GATE_PINS[i], LOW);
          clearRow(3);
          lcd.setCursor(0,3);
          lcd.print(TOOL_NAMES[i]);
          lcd.print(" ~ DC ON");
          currentState = TOOL_ACTIVATING;
          stateTimer = currentMillis;
          break;
        }
      }
      break;

    case TOOL_ACTIVATING:
      if (currentMillis - stateTimer >= TOOL_TRANSITION_TIME) {
        dustOn();
        currentState = TOOL_RUNNING;
      }
      break;

    case TOOL_RUNNING:
      // Remove debug output section
      if (currentMillis - previousMillis >= DISPLAY_INTERVAL) {
        bool allToolsOff = true;
        
        for(int i = 0; i < NUM_TOOLS; i++) {
          double threshold = TOOL_THRESHOLDS[i] * 0.8;  // 80% threshold
          if(toolCurrents[i] > threshold) {
            allToolsOff = false;
          }
        }
      }
      
      // Move the actual state transition logic outside the display interval
      bool allToolsOff = true;
      for(int i = 0; i < NUM_TOOLS; i++) {
        if(toolCurrents[i] > (TOOL_THRESHOLDS[i] * 0.8)) {
          allToolsOff = false;
          break;
        }
      }
      
      if (allToolsOff) {
        Serial.println("\n!!! TRANSITION TRIGGERED !!!");
        Serial.println("State changing: TOOL_RUNNING -> TOOL_DEACTIVATING");
        clearRow(3);
        lcd.setCursor(0,3);
        lcd.print("Shutting down...");
        currentState = TOOL_DEACTIVATING;
        stateTimer = currentMillis;
      }
      break;

    case TOOL_DEACTIVATING:
      // Remove status printing
      bool toolStillRunning = false;
      for(int i = 0; i < NUM_TOOLS; i++) {
        if(toolCurrents[i] > (TOOL_THRESHOLDS[i] * 0.8)) {
          toolStillRunning = true;
          currentState = TOOL_RUNNING;
          break;
        }
      }
      
      if (!toolStillRunning && (currentMillis - stateTimer >= TOOL_OFF_TRANSITION_TIME)) {
        dustOff();
        
        for(int i = 0; i < sizeof(BLAST_GATE_PINS)/sizeof(BLAST_GATE_PINS[0]); i++) {
          digitalWrite(BLAST_GATE_PINS[i], HIGH);
        }
        
        clearRow(3);
        lcd.setCursor(0,3);
        lcd.print("Monitoring - DC OFF");
        currentState = MONITORING;
      }
      break;

    case MANUAL_CONTROL:
      if (currentMillis - previousMillis >= DISPLAY_INTERVAL) {
        previousMillis = currentMillis;
        
        // Print gate states periodically
        Serial.println("\n--- Manual Control Status ---");
        for(int i = 0; i < NUM_TOOLS; i++) {
          Serial.print("Gate ");
          Serial.print(i + 1);
          Serial.print(": ");
          Serial.println(manualGateStates[i] ? "OPEN" : "CLOSED");
        }
        Serial.println("-------------------------");
        
        // Update display status
        clearRow(3);
        lcd.setCursor(0,3);
        lcd.print("Manual - DC ON");
        updateGateStatusDisplay();  // Make sure gate status is updated regularly
      }
      break;
      
    default:
      Serial.println("ERROR: Unknown state detected");
      currentState = MONITORING;
      break;
  }

  // If we're in manual mode, don't process automatic current sensing
  if (currentState == MANUAL_CONTROL) {
    return;
  }
}

void updateDisplay() {
  // Update tool current readings
  for(int i = 0; i < NUM_TOOLS; i++) {
    lcd.setCursor((i % 2) * 10, i / 2);
    lcd.print("T");
    lcd.print(i + 1);
    lcd.print(" ");
    lcd.print(toolCurrents[i], 1); // Display with 1 decimal place
  }
}

void dustOn()
{
  digitalWrite(dustCollectionRelayPin, LOW);  // LOW = ON for relay
  Serial.println("Dust Collector ON");  // Debug output
}

void dustOff()
{
  digitalWrite(dustCollectionRelayPin, HIGH);  // HIGH = OFF for relay
  Serial.println("Dust Collector OFF");  // Debug output
}

void clearRow(byte rowToClear)
{
  lcd.setCursor(0, rowToClear);
  lcd.print("                    ");  
}
