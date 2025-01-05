// Dust Collection System for Shop
// Released Version 5.7 01/04/2025

#include <LiquidCrystal_I2C.h>
#include "EmonLib.h"

// Hardware Configuration
LiquidCrystal_I2C lcd(0x27,20,4);
EnergyMonitor tools[4];  // Array of energy monitors

// System Constants
const int NUM_TOOLS = 4;  
const double TOOL_THRESHOLDS[] = {15.0, 12.0, 8.0, 10.0};  
const char* TOOL_NAMES[] = {"Table Saw", "Miter Saw", "CNC Router", "Tool 4"};
const int BLAST_GATE_PINS[] = {27, 35, 39, 43, 47};
const int dustCollectionRelayPin = 51;

// Button Configuration
const int BUTTON_PINS[] = {19, 18, 2, 3};  // Interrupt-capable pins
volatile bool buttonInterrupt[NUM_TOOLS] = {false};
unsigned long lastInterruptTime[NUM_TOOLS] = {0};
const unsigned long DEBOUNCE_TIME = 200;  // 200ms debounce

// Timing Constants
const unsigned long DISPLAY_INTERVAL = 250;      // Display update interval (ms)
const unsigned long TOOL_TRANSITION_TIME = 500;  // Tool activation time (ms)
const unsigned long TOOL_OFF_TRANSITION_TIME = 15000;  // Shutdown delay (ms)

// System State Management
enum SystemState {
    STARTUP,
    MONITORING,
    TOOL_ACTIVATING,
    TOOL_RUNNING,
    TOOL_DEACTIVATING,
    MANUAL_CONTROL
};

SystemState currentState = STARTUP;
unsigned long stateTimer = 0;
unsigned long previousMillis = 0;
unsigned long startupCounter = 0;

// Tool State Tracking
bool manualGateStates[NUM_TOOLS] = {false};
double toolCurrents[NUM_TOOLS];

// Interrupt Service Routines
void buttonISR0() { 
    if (millis() - lastInterruptTime[0] > DEBOUNCE_TIME && !buttonInterrupt[0]) { 
        buttonInterrupt[0] = true; 
        lastInterruptTime[0] = millis();
        // Serial.print("ISR0 triggered - Pin ");
        // Serial.println(BUTTON_PINS[0]);
    } 
}
void buttonISR1() { 
    if (millis() - lastInterruptTime[1] > DEBOUNCE_TIME && !buttonInterrupt[1]) { 
        buttonInterrupt[1] = true; 
        lastInterruptTime[1] = millis();
        // Serial.print("ISR1 triggered - Pin ");
        // Serial.println(BUTTON_PINS[1]);
    } 
}
void buttonISR2() { 
    if (millis() - lastInterruptTime[2] > DEBOUNCE_TIME && !buttonInterrupt[2]) { 
        buttonInterrupt[2] = true; 
        lastInterruptTime[2] = millis();
        // Serial.print("ISR2 triggered - Pin ");
        // Serial.println(BUTTON_PINS[2]);
    } 
}
void buttonISR3() { 
    if (millis() - lastInterruptTime[3] > DEBOUNCE_TIME && !buttonInterrupt[3]) { 
        buttonInterrupt[3] = true; 
        lastInterruptTime[3] = millis();
        // Serial.print("ISR3 triggered - Pin ");
        // Serial.println(BUTTON_PINS[3]);
    } 
}

void setup() {
    Serial.begin(9600);
    startupCounter = 0;
    
    // Initialize LCD
    lcd.init();
    lcd.backlight();
    displayStartupScreen();

    // Initialize tools
    for(int i = 0; i < NUM_TOOLS; i++) {
        tools[i].current(i, 111.1);
    }

    // Initialize outputs
    pinMode(dustCollectionRelayPin, OUTPUT);
    digitalWrite(dustCollectionRelayPin, HIGH);  // Start with dust collection off

    // Initialize blast gates
    for(int i = 0; i < sizeof(BLAST_GATE_PINS)/sizeof(BLAST_GATE_PINS[0]); i++) {
        pinMode(BLAST_GATE_PINS[i], OUTPUT);
        digitalWrite(BLAST_GATE_PINS[i], HIGH);  // Start with gates closed
    }

    // Initialize button interrupts with pullup and noise filtering
    for(int i = 0; i < NUM_TOOLS; i++) {
        pinMode(BUTTON_PINS[i], INPUT_PULLUP);
        
        // Add some delay between pin configurations
        delay(50);
        
        // Serial.print("Button "); // Uncomment to see button pin states
        // Serial.print(i + 1);
        // Serial.print(" (Pin ");
        // Serial.print(BUTTON_PINS[i]);
        // Serial.print(") state: ");
        // Serial.println(digitalRead(BUTTON_PINS[i]));
    }
    
    // Clear any pending interrupts
    for(int i = 0; i < NUM_TOOLS; i++) {
        buttonInterrupt[i] = false;
        lastInterruptTime[i] = millis();
    }
    
    delay(100);  // Longer delay to ensure stability
    
    // Attach interrupts last, after everything is stable
    attachInterrupt(digitalPinToInterrupt(BUTTON_PINS[0]), buttonISR0, FALLING);
    delay(50);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PINS[1]), buttonISR1, FALLING);
    delay(50);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PINS[2]), buttonISR2, FALLING);
    delay(50);
    attachInterrupt(digitalPinToInterrupt(BUTTON_PINS[3]), buttonISR3, FALLING);

    // Serial.println("Setup complete - monitoring buttons"); 
}

void loop() {
    unsigned long currentMillis = millis();
    
    // Handle button interrupts
    checkButtonInterrupts();
    
    // Read current values
    updateToolCurrents();

    // Update display periodically
    if (currentMillis - previousMillis >= DISPLAY_INTERVAL) {
        updateDisplay();
        previousMillis = currentMillis;
    }

    // State machine
    switch(currentState) {
        case STARTUP:
            handleStartupState();
            break;

        case MONITORING:
            handleMonitoringState();
            break;

        case TOOL_ACTIVATING:
            handleToolActivatingState(currentMillis);
            break;

        case TOOL_RUNNING:
            handleToolRunningState();
            break;

        case TOOL_DEACTIVATING:
            handleToolDeactivatingState(currentMillis);
            break;

        case MANUAL_CONTROL:
            handleManualControlState(currentMillis);
            break;
    }
}

// Helper Functions
void displayStartupScreen() {
    lcd.clear();
    lcd.setCursor(0,0);
    lcd.print("ShopTool Monitor");
    lcd.setCursor(0,1);
    lcd.print("DC v5.7");
    delay(2000);
    lcd.clear();
}

void updateToolCurrents() {
    for(int i = 0; i < NUM_TOOLS; i++) {
        toolCurrents[i] = tools[i].calcIrms(1480);
    }
}

void updateDisplay() {
    for(int i = 0; i < NUM_TOOLS; i++) {
        lcd.setCursor((i % 2) * 10, i / 2);
        lcd.print("T");
        lcd.print(i + 1);
        lcd.print(" ");
        lcd.print(toolCurrents[i], 1);
    }
}

void updateGateStatusDisplay() {
    lcd.setCursor(0, 2);
    lcd.print("BG ");
    for(int i = 0; i < NUM_TOOLS; i++) {
        lcd.print(i + 1);
        lcd.print(":");
        lcd.print(manualGateStates[i] ? "O" : "C");
        lcd.print(" ");
    }
}

void checkButtonInterrupts() {
    for(int i = 0; i < NUM_TOOLS; i++) {
        if (buttonInterrupt[i]) {
            // Double-check the pin state
            if (digitalRead(BUTTON_PINS[i]) == LOW) {  // Verify button is actually pressed
                handleButtonPress(i);
            } else {
                // Add LCD notification for false trigger
                lcd.setCursor(0, 3);
                lcd.print("False trigger: T");
                lcd.print(i + 1);
                lcd.print("    ");
                // Serial.print("False trigger on pin ");
                // Serial.println(BUTTON_PINS[i]);
            }
            buttonInterrupt[i] = false;
        }
    }
}

void handleButtonPress(int buttonIndex) {
    manualGateStates[buttonIndex] = !manualGateStates[buttonIndex];
    digitalWrite(BLAST_GATE_PINS[buttonIndex], !manualGateStates[buttonIndex]);
    
    if (currentState != MANUAL_CONTROL) {
        currentState = MANUAL_CONTROL;
        dustOn();
    }
    
    updateGateStatusDisplay();
    
    // Check if all gates are closed
    if (areAllGatesClosed() && currentState == MANUAL_CONTROL) {
        dustOff();
        currentState = MONITORING;
        clearRow(3);
        lcd.setCursor(0,3);
        lcd.print("Monitoring - DC OFF");
    }
}

bool areAllGatesClosed() {
    for(int i = 0; i < NUM_TOOLS; i++) {
        if (manualGateStates[i]) return false;
    }
    return true;
}

// State Handler Functions
void handleStartupState() {
    if (startupCounter++ >= 5) {
        currentState = MONITORING;
    }
}

void handleMonitoringState() {
    for(int i = 0; i < NUM_TOOLS; i++) {
        if(toolCurrents[i] > TOOL_THRESHOLDS[i]) {
            digitalWrite(BLAST_GATE_PINS[i], LOW);
            clearRow(3);
            lcd.setCursor(0,3);
            lcd.print(TOOL_NAMES[i]);
            lcd.print(" ~ DC ON");
            currentState = TOOL_ACTIVATING;
            stateTimer = millis();
            break;
        }
    }
}

void handleToolActivatingState(unsigned long currentMillis) {
    if (currentMillis - stateTimer >= TOOL_TRANSITION_TIME) {
        dustOn();
        currentState = TOOL_RUNNING;
    }
}

void handleToolRunningState() {
    bool allToolsOff = true;
    for(int i = 0; i < NUM_TOOLS; i++) {
        if(toolCurrents[i] > (TOOL_THRESHOLDS[i] * 0.8)) {
            allToolsOff = false;
            break;
        }
    }
    
    if (allToolsOff) {
        clearRow(3);
        lcd.setCursor(0,3);
        lcd.print("Shutting down...");
        currentState = TOOL_DEACTIVATING;
        stateTimer = millis();
    }
}

void handleToolDeactivatingState(unsigned long currentMillis) {
    for(int i = 0; i < NUM_TOOLS; i++) {
        if(toolCurrents[i] > (TOOL_THRESHOLDS[i] * 0.8)) {
            currentState = TOOL_RUNNING;
            return;
        }
    }
    
    if (currentMillis - stateTimer >= TOOL_OFF_TRANSITION_TIME) {
        dustOff();
        for(int i = 0; i < sizeof(BLAST_GATE_PINS)/sizeof(BLAST_GATE_PINS[0]); i++) {
            digitalWrite(BLAST_GATE_PINS[i], HIGH);
        }
        clearRow(3);
        lcd.setCursor(0,3);
        lcd.print("Monitoring - DC OFF");
        currentState = MONITORING;
    }
}

void handleManualControlState(unsigned long currentMillis) {
    clearRow(3);
    lcd.setCursor(0,3);
    lcd.print("Manual - DC ON");
    updateGateStatusDisplay();
}

void dustOn() {
    digitalWrite(dustCollectionRelayPin, LOW);
    Serial.println("Dust Collector ON");
}

void dustOff() {
    digitalWrite(dustCollectionRelayPin, HIGH);
    Serial.println("Dust Collector OFF");
}

void clearRow(byte rowToClear) {
    lcd.setCursor(0, rowToClear);
    lcd.print("                    ");
}
