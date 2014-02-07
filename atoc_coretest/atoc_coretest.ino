/* 
 * atoc_coretest.ino
 * Mike Lussier - Jan 25, 2014
 * Compiled with Arduino 1.0.5
 *****************************
 */
#define INO_NAME "ATOC"
#define INO_VERSION "1.2.x"

/* 
 * .c - Adds RTC; nighttime sump lighting function and completes code for 
 * ato function.
 ****** 
 * v1.2 - Integrates one (of two) level switches, ato pump relay, 
 * dosing pump relay, mode and relay switches, alarm and serial LCD (2*16).
 ***************************************** 
 */

#include <Relay.h>
#include <Bounce.h>
#include <Timer.h>
#include <Wire.h>
#include "DS1307.h"

static uint8_t alarm_pin            = 14;    // Piezo alarm (A0)
static uint8_t level1Switch_pin     = 8;     // Level 1 switch (4 is not working)
static uint8_t modeSwitch_pin       = 2;     // Mode switch
static uint8_t pumpSwitch_pin       = 3;     // Pump switch
static uint8_t atoPumpRelay_pin     = 5;     // Relay for ATO pump
static uint8_t dosPumpRelay_pin     = 6;     // Relay for dosing pump
static uint8_t lightRelay_pin       = 7;     // Relay for sump light
static uint8_t statLED_pin          = 13;    // Status LED

// From ATO Calculations
#define RESERVOIR_VOL 3500                // Volume of supply water reservoir, mL
#define ATO_PUMP_RATE 5.0                 // Calibrated ato pump flow rate, mL/s
#define DOS_PUMP_RATE 5.0                 // Calibrated dosing pump flow rate, mL/s
#define HYST_VOL_CHANGE 618.1             // Hysteresis volume change for nano float switch, mL

#define LEVEL1_SWITCH_READ_INHIBIT 10L    // Duration of level 1 switch read inhibit, s
#define MAX_PUMP_RUN_CYCLES 4             // Maximum no of pump run cycles before error

#define SUMPLIGHT_TURNON_HOUR 23          // Time of day that sump light turns on
#define SUMPLIGHT_TURNOFF_HOUR 4          // Time of day that sump light turns off

// Instantiate debouncer objects for level 1 switch, mode & pump switches
Bounce level1Switch = Bounce();
Bounce modeSwitch = Bounce();
Bounce pumpSwitch = Bounce();

// Instantiate relay objects for ato pump, dosing pump and sump light
Relay atoPumpRelay = Relay();
Relay dosPumpRelay = Relay();
Relay sumpLightRelay = Relay();

// Instantiate timer object for recurring events
Timer t = Timer();

// Define timed events
uint8_t mainProcessEvent;              // Call mainProcess, 4 Hz
uint8_t heartbeatEvent;                // Flash status LED, flash lcd heartbeat, 2 Hz
uint8_t pumpStopEvent;                 // Turn off ato pump relay 
uint8_t inhibitEvent;                  // Level 1 switch read inhibit reset
uint8_t enableDisplayEvent;            // Enable LCD display updating, after 3 s
uint8_t enableAtoEvent;
uint8_t enableMainProcessEvent;
uint8_t alarmEvent;                    // Sound alarm to indicate fault
uint8_t simulateFaultEvent;


// Define DS1307 RTC clock object
DS1307 clock;

// Other globals
// - process mode
const static int ATO = 0;
const static int DOSING = 1;
const static int FAULT = -1;
int processMode = ATO;                   // Default main process mode is ATO

// - pump mode
const static int OFF = 0;
const static int ON = 1;
int atoPumpMode = OFF;
int dosPumpMode = OFF;

// - water level
const static int BELOW_LEVEL = 0;
const static int AT_LEVEL = 1;
int waterLevel = AT_LEVEL;
int dosingLevel;

// - sump light
int sumpLightMode = OFF;

// - logical switches 
int ms_state, ps_state, l1s_state;      
bool ms_stateChanged, ps_stateChanged, l1s_stateChanged;

// - first time through
bool ftt_ms = true, ftt_ps = true, ftt_l1s = true, ftt_fault = true;

// - switch read inhibits
bool psBlocked = true;                   // If true, inhibits reading pump switch
bool l1sBlocked = false;                 // If true, inhibits reading level 1 switch
bool atoBlocked = false;                 // If true, inhibits ato process

int curPumpCycle = 0;                    // Current pump cycle (out of MAX_PUMP_RUN_CYCLES), n
int freshWaterVol = RESERVOIR_VOL;       // Initial fresh water volume, mL
int dossantVol = 0;

// pumpCycleTime = 1/5 time to refill aquarium to nominal level at ato pump rate, ms
const int pumpCycleTime = (int) (HYST_VOL_CHANGE / ATO_PUMP_RATE * 200.0);
// pumpCycleVol = vol dispensed during 1 pump cycle, mL
const int pumpCycleVol = (int) (ATO_PUMP_RATE * (float) pumpCycleTime / 1000.0);

// LCD display variables
bool displayEnabled = false;            // Allows process messages to be written to LCD
bool inhibitMainProcess = true;
bool du1, du2, du3, du4, du5;           // Refresh display for specific messages

// From GDM1602K LCD character set
const char ptr = 126;                   // Pointer symbol
const char spc = 32;                    // Space


// Functions

void updateDisplay(int msgNo)
// Updates 2x16 LCD display
// Line 1 - sensor/actuator status (process mode, pump state, water level
// Line 2 - process messages
{
  if (displayEnabled) {
    // Clear line(s) for new message(s), print message
    switch (msgNo) {
    case 0 :
      // Display startup message
      displayEnabled = false;
      clearLCD();  
      selectLineOne();
      Serial.print("  ATOC");
      Serial.print(" v");
      Serial.print(INO_VERSION);
      selectLineTwo();
      Serial.print(" Water matters.");
      break;

    case 1 :
      clrLine(1); 
      goToPosn(1);
      if (processMode == ATO) Serial.print("Mode: ATO");
      if (processMode == DOSING) Serial.print("Mode: DOSING");
      if (processMode == FAULT) Serial.print(" ** FAULT **");      
      break;

    case 2 :
      clrLine(2);    
      goToPosn(17);
      if (atoPumpMode == OFF) Serial.print("ATO Pump: OFF");
      if (atoPumpMode == ON) Serial.print("ATO Pump: ON");
      break;

    case 3 :
      clrLine(2);    
      goToPosn(17);
      if (waterLevel == AT_LEVEL) Serial.print("Water: OK");
      if (waterLevel == BELOW_LEVEL) Serial.print("Water: LOW");
      break;

    case 4 :
      clrLine(2);    
      goToPosn(17);
      if (dosPumpMode == OFF) Serial.print("DOS Pump: OFF");
      if (dosPumpMode == ON) Serial.print("DOS Pump: ON");
      break;

    case 5 :
      clrLine(2);    
      goToPosn(17);
      if (sumpLightMode == OFF) Serial.print("Sump Light: OFF");
      if (sumpLightMode == ON) Serial.print("Sump Light: ON");
      break;

    case 6 :
      clrLine(2);    
      goToPosn(16);
      Serial.print("Replenish water");
      break; 

    case 7 :
      clrLine(2);    
      goToPosn(16);
      Serial.print("Bad level switch");
      break;

    case 8 :
      clrLine(1);
      clrLine(2);    
      goToPosn(1);
      Serial.print("Fault cleared");
      break;      
    }
  }
} // updateDisplay


void enableDisplay(void* context)
{
  displayEnabled = true;
  clearLCD();
  updateDisplay(1);
} // enableDisplay


void clrLine(uint8_t lineNo)
// Clears specified line of LCD display
{
  if (lineNo == 1) selectLineOne();
  if (lineNo == 2) selectLineTwo();
  Serial.print("                ");
} // clrLine


void flashStatLED(void* context)
// Flash status LED & show heartbeat on LCD, 2 Hz
{
  bool LEDstate = digitalRead(statLED_pin);
  digitalWrite(statLED_pin, !LEDstate);

  // Show synchronized heartbeat on LCD
  goToPosn(0);
  if (!LEDstate) {
    Serial.print(ptr);
  } 
  else {
    Serial.print(spc);
  }
} // flashStatLED


void mainProcess(void* context)
// Called every 250 ms by recurring timed event, mainProcessEvent 
{
  // Poll sensors and update logical device states
  readAllSwitches();

  if ((processMode == FAULT) && (atoPumpMode == ON)) {
    // Recover from FAULT
    processMode = ATO;
    displayEnabled = true;
    updateDisplay(8); // "Ready"
    enableMainProcessEvent= t.after(3500L, enableMainProcess, 0);
  }

  if (!inhibitMainProcess) {
    if (processMode != FAULT) {
      // Always perform ATO function
      atoProcess();

      // Always perform sump light function
      sumpLightProcess();

      // Only perform dosing function when processMode == DOSING
      if (processMode == DOSING) {
        dosingProcess();
      }
    } 
    else {
      faultProcess();
    }
  }
} // mainProcess


void readAllSwitches(void)
// Read mode, pump and level switches, then update logical device states
{
  // Read mode switch
  ms_stateChanged = modeSwitch.update();
  if ((ftt_ms) || (ms_stateChanged)) {
    ms_state = modeSwitch.read();
    if (ms_state == HIGH) {
      processMode = ATO;
    }
    if (ms_state == LOW) {
      processMode = DOSING;
    }
    ftt_ms = false;
    du1 = true;
  }

  // Read pump switch
  ps_stateChanged = pumpSwitch.update();
  if ((ftt_ps) || (ps_stateChanged)) {
    ps_state = pumpSwitch.read();
    if (ps_state == HIGH) {
      if (processMode == DOSING) dosPumpMode = OFF;
      if (processMode != DOSING) atoPumpMode = OFF;
    }
    if (ps_state == LOW) {
      if (processMode == DOSING) dosPumpMode = ON;
      if (processMode != DOSING) atoPumpMode = ON;
    }
    ftt_ps = false;
  }

  // Read level 1 switch
  if (!l1sBlocked) {
    l1s_stateChanged = level1Switch.update();
    if ((ftt_l1s) || (l1s_stateChanged)) {
      l1s_state = level1Switch.read();
      if (l1s_state == HIGH) {
        waterLevel = AT_LEVEL;
      }
      if (l1s_state == LOW) {
        waterLevel = BELOW_LEVEL;
      }
      ftt_l1s = false;
      du3 = true;
    } 
  }        
} // readAllSwitches


void atoProcess()
// Called conditionally every 250 ms by mainProcess.  
{
  // Is ato blocked?
  if (!atoBlocked) { // no  
    if (du1) {
      updateDisplay(1);
      du1 = false;
    }
    // Is the ato pump running?
    if (atoPumpRelay.getState() == HIGH) { // yes
      // Do nothing - pump will be turned off by pumpStopEvent
    } 
    else { // no
      // Is level 1 switch blocked?
      if (!l1sBlocked) { // no

        if (waterLevel == BELOW_LEVEL) { // level is LOW
          ++curPumpCycle;
          freshWaterVol -= pumpCycleVol;
          if ((curPumpCycle <= MAX_PUMP_RUN_CYCLES) && (freshWaterVol >= pumpCycleVol)) {
            // Turn on the pump
            atoPumpRelay.turnOn();
            atoPumpMode = ON;
            // Prevent level 1 switch from being read until 10 s after pump has stopped
            l1sBlocked = true;
            // Set up timed event to turn off pump
            pumpStopEvent = t.after(pumpCycleTime, stopPump, 0);
            updateDisplay(2); // "ATO Pump : ON"
            atoBlocked = true;
          } 
          else {
            processMode = FAULT;
          } 
        }
        else {
          if (du3) {
            du3 = false;
            updateDisplay(3); // "Water: OK"
            enableDisplayEvent = t.after(3000L, enableDisplay, 0);
          } // du3
        }
      } // l1sBlocked
    }
  } // !atoBlocked
} // atoProcess


void sumpLightProcess(void)
// Called every 250 ms by mainProcess.
// Controls the sump light; for now, light goes on at SUMPLIGHT_TURNON_HOUR, 
// then off at SUMPLIGHT_TURNOFF_HOUR 
{

} // sumpLightProcess


void dosingProcess(void)
// Called conditionally every 250 ms by mainProcess.
// Details of this process will change. For now, the dosing pump will be operated manually
// using the pump switch, when the mode switch is in DOSING mode (inhibited in ATO mode).
{

} // dosingProcess


void faultProcess()
// The fault process will identify two possible faults, issue a corresponding warning message 
// and take corrective action, if possible.
// Faults include 1) depleted fresh water supply, 2) malfunctioning level switch.
{
  // faultProcess is not reentrant
  inhibitMainProcess = true;
  atoPumpRelay.turnOff();
  displayEnabled = true;
  updateDisplay(1); // " ** FAULT **"
  if (freshWaterVol < pumpCycleVol) {
    updateDisplay(6); // "Replenish water"
    // Reset water vol
    freshWaterVol = RESERVOIR_VOL;
  } 
  else {
    updateDisplay(7); // "Bad level switch"
    curPumpCycle = 0;  
  }

  clock.getTime();
  // Sound alarm for 3 s if time is between 06h and 23h
  //if (6 < clock.hour < 23) { 
  alarmEvent = t.oscillate(alarm_pin, 300, LOW, 1);
  //}
} // faultProcess


void stopPump(void* context)
// Turn off ato pump
{
  if (atoPumpRelay.getState() == HIGH) {
    atoPumpRelay.turnOff();
    atoPumpMode = OFF;
    updateDisplay(2);
  } 
  l1sBlocked = true;
  // Create timer event to unblock level 1 switch read
  inhibitEvent = t.after(1000L * LEVEL1_SWITCH_READ_INHIBIT, resetl1sBlocked, 0);
}


void resetl1sBlocked(void* context)
// Allow level 1 switch to be read and immediately read it
{
  l1sBlocked = false;
  //l1s_stateChanged = level1Switch.update();
  //if (l1s_stateChanged) {
  l1s_state = level1Switch.read();
  if (l1s_state == HIGH) {
    waterLevel = AT_LEVEL;
  }
  if (l1s_state == LOW) {
    waterLevel = BELOW_LEVEL;
  }
  du3 = true;
  atoBlocked = false;
  //}
} // resetl1sBlocked


void enableMainProcess(void* context)
// Allow ato process
{
  inhibitMainProcess = false;
  enableDisplay(0);
} // enableMainProcess


void setup()
{
  // Start the ADM1602U serial interface  
  Serial.begin(9600);

  // Instantiate RTC clock
  clock.begin();

  // Configure digital inputs
  // Mode switch
  pinMode(modeSwitch_pin, INPUT);          // Set pin to input
  digitalWrite(modeSwitch_pin, HIGH);      // Turn on pullup resistor
  modeSwitch.attach(modeSwitch_pin);       // Attach modeSwitch to pin
  modeSwitch.interval(5);

  // Pump switch
  pinMode(pumpSwitch_pin, INPUT);          // Set pin to input
  digitalWrite(pumpSwitch_pin, HIGH);      // Turn on pullup resistor
  pumpSwitch.attach(pumpSwitch_pin);       // Attach pumpSwitch to pin
  pumpSwitch.interval(5);

  // Level 1 switch
  pinMode(level1Switch_pin, INPUT);        // Set pin to input
  digitalWrite(level1Switch_pin, HIGH);    // Turn on pullup resistor
  level1Switch.attach(level1Switch_pin);   // Attach level1Switch to pin  
  level1Switch.interval(5);

  // Level 2 switch -- NOT CONFIGURED
  //pinMode(level2Switch_pin, INPUT);      // Set pin to input
  //digitalWrite(level2Switch_pin, HIGH);  // Turn on pullup resistor
  //level2Switch.attach(level2Switch_pin); // Attach level2Switch to pin  
  //level2Switch.interval(5);

  // Configure digital outputs
  // ato pump relay
  pinMode(atoPumpRelay_pin, OUTPUT);       // Set pin to output
  atoPumpRelay.attach(atoPumpRelay_pin);   // Attach pumpRelay to pin

    // dos pump relay
  pinMode(dosPumpRelay_pin, OUTPUT);       // Set pin to output
  dosPumpRelay.attach(dosPumpRelay_pin);   // Attach pumpRelay to pin

    // Sump light relay
  pinMode(lightRelay_pin, OUTPUT);         // Set pin to output
  sumpLightRelay.attach(lightRelay_pin);   // Attach lightRelay to pin

    // Status LED
  pinMode(statLED_pin, OUTPUT);            // Setup statLED
  digitalWrite(statLED_pin, HIGH);         // Turn on LED

  // Alarm
  pinMode(alarm_pin, OUTPUT);             // Set alarm pin to output
  digitalWrite(alarm_pin, LOW);           // Start with the alarm off 

  clearLCD();
  backlightOn();
  displayOn();

  // Display splash screen
  displayEnabled = true;
  updateDisplay(0);
  // inhibit mainProcess for 3 seconds while splash screen is displayed
  inhibitMainProcess = true;

  // Setup timer recurring events
  mainProcessEvent = t.every(250L, mainProcess, 0);  
  heartbeatEvent = t.every(500L, flashStatLED, 0);

  // Setup timer one shot events
  enableMainProcessEvent = t.after(3000L, enableMainProcess, 0);
  //simulateFaultEvent = t.after(30000L, simulateFault, 0);
} // setup


void loop()
{
  // Update timer
  t.update();
} // loop










////////////////////////////////////////////////////////
/////////////////////// GDM1602K ///////////////////////
////////////////////////////////////////////////////////

//////////// Display Functions ////////////////

// Scrolls the display to the left by the number of characters passed in, and waits a given
// number of milliseconds between each step
void scrollLeft(int num, int wait) 
{
  for(int i=0;i<num;i++) {
    Serial.write(0xFE);
    Serial.write(0x18);
    //delay(10);
    //delay(wait);
  }
}

// Scrolls the display to the right by the number of characters passed in, and waits a given
// number of milliseconds between each step
void scrollRight(int num, int wait) 
{
  for(int i=0;i<num;i++) {
    Serial.write(0xFE);
    Serial.write(0x1C);
    //delay(10);
    //delay(wait);
  }
}

// Starts the cursor at the beginning of the first line (convenient method for goTo(0))
void selectLineOne() {  //puts the cursor at line 1 char 0.
  Serial.write(0xFE);   //command flag
  Serial.write(128);    //position
  delay(10);
}

// Starts the cursor at the beginning of the second line (convenient method for goTo(16))
void selectLineTwo() 
{  //puts the cursor at line 2 char 0.
  Serial.write(0xFE);   //command flag
  Serial.write(192);    //position
  delay(10);
}

// Sets the cursor to the given position
// line 1: 0-15, line 2: 16-31, >31 defaults back to 0
void goToPosn(int position) 
{
  if (position < 16) { 
    Serial.write(0xFE);   //command flag
    Serial.write((position+128));
  } 
  else if (position < 32) {
    Serial.write(0xFE);   //command flag
    Serial.write((position+48+128));
  } 
  else { 
    goToPosn(0); 
  }
  delay(10);
}

// Resets the display, undoing any scroll and removing all text
void clearLCD() 
{
  Serial.write(0xFE);
  Serial.write(0x01);
  delay(10);
}

// Turns on the backlight
void backlightOn() 
{
  Serial.write(0x7C);
  Serial.write(157);   //light level
  delay(10);
}

// Turns off the backlight
void backlightOff() 
{
  Serial.write(0x7C);
  Serial.write(128);   //light level
  delay(10);
}

// Turns on visual display
void displayOn() 
{
  Serial.write(0xFE);
  Serial.write(0x0C);
  delay(10);
}

// Turns off visual display
void displayOff() 
{
  Serial.write(0xFE);
  Serial.write(0x08);
  delay(10);
}

// Initiates a function command to the display
void serCommand() 
{
  Serial.write(0xFE);
}





















































