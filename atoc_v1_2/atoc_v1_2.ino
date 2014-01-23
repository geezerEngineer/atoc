/* 
 * atoc_v1_2.ino
 * Mike Lussier - Jan 5, 2014
 * Compiled with Arduino 1.0.5
 *****************************
 */
#define INO_NAME "ATOC"
#define INO_VERSION "1.2.a"

/* v1.2.a - Integrates one (of two) level switches, ato pump relay, 
 * dosing pump relay, mode and relay switches, alarm and serial LCD (2*16).
 ***************************************** 
 */

#include <Relay.h>
#include <Bounce.h>
#include <Timer.h>

static uint8_t alarm_pin            = 6;     // Piezo alarm
static uint8_t level1Switch_pin     = 7;     // Level 1 switch
static uint8_t modeSwitch_pin       = 8;     // Mode switch
static uint8_t dosingPumpSwitch_pin = 9;     // Dosing pump switch
static uint8_t atoPumpRelay_pin     = 10;    // Relay for ATO pump
static uint8_t dosingPumpRelay_pin  = 11;    // Relay for dosing pump
static uint8_t statLED_pin          = 13;    // Status LED

// From ATO Calculations
#define RESERVOIR_FULL_VOL 3500           // Initial vol of supply water, mL
#define ATO_PUMP_RATE 5.0                 // Calibrated ato pump flow rate, mL/s
#define HYSTERESIS_VOL_CHANGE 618.1       // Expected volume change for nano float switch, mL

#define LEVEL1_SWITCH_READ_INHIBIT 10      // Duration of level 1 switch read inhibit, s
#define MAX_PUMP_RUN_CYCLES 8             // Maximum no of pump run cycles before error

// Instantiate debouncer objects for level 1 switch, mode & pump switches
Bounce level1Switch = Bounce();
Bounce modeSwitch = Bounce();
Bounce dosingPumpSwitch = Bounce();

// Instantiate relay objects for ato and dosing pumps
Relay atoPumpRelay = Relay();
Relay dosingPumpRelay = Relay();

// Instantiate timer object for recurring events
Timer t = Timer();

// Define timed events
uint8_t flashStatLEDEvent;             // Heartbeat LED, also updates display panel, 2 Hz
uint8_t tickEvent;                     // Clock management, 1 Hz
uint8_t readModeSwitchEvent;           // Read mode switch, 5 Hz
uint8_t readLevel1SwitchEvent;         // Read level 1 switch, 10 Hz
uint8_t relayStopEvent;                // 
uint8_t inhibitEvent;                  // Level switch read inhibit reset 

// Other globals
// Global time vars
long t0 = 0;
int tds = 0, ts = 0, tm = 0, th = 0;

// Other globals
// process modes
const static int ATO = 0;
const static int DOSING = 1;
const static int FAULT = -1;
int processMode = ATO;                    // Default fsm mode is ATO

// water levels
const static int BELOW_LEVEL = 0;
const static int AT_LEVEL = 1;
int atoLevel = AT_LEVEL;
int dosingLevel;

// pump switch states
const static int OFF = 0;
const static int ON = 1;
int pumpSwitchState = OFF;
long pumpTimeOn, pumpTimeOff;

bool firstTimeThru = true;
bool level1SwitchReadInhibit = false;    // Inhibits reading switch due to water disturbance
bool manualModeInhibit = false;
int curPumpCycle = 0;                   // Current pump cycle (max 8), n
int freshWaterVol = RESERVOIR_FULL_VOL; // Initial fresh water volume, mL
int dossantVol = 0;

// pumpCycleTime = 1/5 time to refill to nominal level at refill rate, ms
const int pumpCycleTime = (int) (HYSTERESIS_VOL_CHANGE / REFILL_RATE * 1000.0 / 5.0);
// pumpCycleVol = vol dispensed during 1 pump cycle, mL
const int pumpCycleVol = (int) (REFILL_RATE * (float) pumpCycleTime / 1000.0);

bool w = true;


// Functions
void displaySplashScreen(void)
// Display startup message
{
  clearLCD();  
  selectLineOne();
  Serial.print("  ATOC");
  Serial.print("  v");
  Serial.print(INO_VERSION);
  selectLineTwo();
  Serial.print(" Water matters.");
} // displaySplashScreen


void updateDisplay(int msgNo)
// Updates LCD
// Line 1 - sensor/actuator status (process mode, pump state, water level)
// Line 2 - process messages
{
  if (!lcdInhibited) {
    // Clear line(s) for new message(s), print message
    switch (msgNo) {
    case 1 :
      clrLine(1); 
      goTo(1);
      if (processMode == ATO) Serial.print("Mode: ATO");
      if (processMode == DOSING) Serial.print("Mode: DOSING");
      break;

    case 2 :
      clrLine(2)    
        goTo(17);
      if (pumpMode == OFF) Serial.print("Pump: OFF");
      if (pumpMode == ON) Serial.print("Pump: ON");
      break;

    case 3 :
      clrLCD();    
      goTo(1);
      Serial.print(" Test: ");
      Serial.print(testNo);
      Serial.print("/");
      Serial.print(maxTests);
      selectLineTwo();
      Serial.print("Run time: ");
      Serial.print(pumpRunTime);
      Serial.print(" s");
      break;

    case 4 :
      selectLineOne();
      Serial.print("                ");
      selectLineOne();
      Serial.print(" Test: ");
      Serial.print(testNo);
      Serial.print(" Run: ");
      Serial.print(runNo);
      break;

    case 5 :
      selectLineOne();
      Serial.print("                ");
      selectLineTwo();
      Serial.print("                ");
      selectLineOne();
      Serial.print(" Process done");
      selectLineTwo();
      Serial.print("Switch to AUTO");
      break;

    case 6 :
      selectLineOne();
      Serial.print("                ");
      selectLineTwo();
      Serial.print("                ");
      selectLineOne();
      Serial.print(" Cancelled");
      break;

    case 7 :
      selectLineOne();
      Serial.print("                ");
      selectLineTwo();
      Serial.print("                ");
      selectLineOne();
      Serial.print(" Test done");
      selectLineTwo();
      Serial.print("Pump ON for next");
      break;  
    }
  }
} // updateDisplay


void clrLine(int lineNo)
// Clears specified line of LCD display
{
  if (lineNo == 1) selectLineOne();
  if (lineNo == 2) selectLineTwo();
  Serial.print("                ");
} // clrLine


void mainProcess(void* context)
// Called every 250 ms by recurring timed event 
{
  if (processMode != FAULT) {
    // Poll sensors (mode & pump switches, level 1 switch) and update virtual device states
    readAllSwitches();
    // Always perform ATO function
    atoProcess();
    // Only perform dosing function when processMode == DOSING
    if (processMode == DOSING) {
      dosingProcess();
    }
  } 
  else {
    faultProcess();
  }
} // mainProcess


void atoProcess()
// Called conditionally every 250 ms by mainProcess.  
{
  // Is the pump running?
  if (atoPumpRelay.getState() == HIGH) { // yes
    // Do nothing - pump will be turned off by relayStopEvent
  } 
  else { // no
    // Is level 1 switch inhibited?
    if (!level1SwitchInhibited) { // no
      if (waterLevel == BELOW_LEVEL) { // level is LOW
        ++curPumpCycle;
        freshWaterVol -= pumpCycleVol;
        if ((curPumpCycle <= MAX_PUMP_RUN_CYCLES) && (freshWaterVol >= pumpCycleVol)) {
          // Turn on the pump
          atoPumpRelay.turnOn();
          // Set up timed event to turn off pump
          relayStopEvent = t.setTimeout(pumpCycleTime, stopPump);
          updateLCD(); // "ATO Pump : ON"
          // Prevent level 1 switch from being read
          level1SwitchInhibited = true;
        } 
        else {
          faultProcess();
        } 
      }
    }
  }
} // atoProcess


void dosingProcess(void)
// Called conditionally every 250 ms by mainProcess.
// Details of this process will change. For now, the dosing pump will be operated manually
// using the pump switch, when the mode switch is in DOSING mode (inhibited in ATO mode).
{
  if (processMode == DOSING) {
    if (pumpSwitch == ON) {
      // Is dosing pump off?
      if () {
        // Turn on pump
        dosingPumpRelay.turnOn();
        // Update the lcd
        updateLCD(); // "DOS : Pump ON"
      }
    }
    if (pumpSwitch == OFF) {
      // Is dosing pump on?
      if () {
        // Turn off pump
        dosingPumpRelay.turnOff();
        // Update the lcd
        updateLCD(); // "DOS : Pump OFF"
      }
    } 
  } 
  else { // processMode is ATO - pump operation is inhibited
    // Has pumpSwitch changed?
    if () {
      if (pumpSwitch == ON) {
        updateLCD(); // "DOS : Inhibited"
      }
      if (pumpSwitch == OFF) {
        clrLine(2);
      }
    } 
  }
} // dosingProcess



void manualProcess()
// called conditionally every 200 ms by readModeSwitch
{
  curPumpCycle = 0;
  // Read pump switch
  readPumpSwitch();  
  if (pumpSwitchState == ON) {
    if (pumpRelay.getState() == LOW) {
      if (waterLevel == BELOW_LEVEL) {
        pumpRelay.turnOn();
        pumpTimeOn = millis();
        printTimestamp();
        Serial.println("Water is BELOW_LEVEL.");      
        Serial.println("      - Pump is now ON.");
      } 
      else {
        if(w) {
          printTimestamp();
          Serial.println("Water is AT_LEVEL.");      
          Serial.println("      - Pump is prohibited from starting.");
          w = false;
        }
      }
    }  
  }
  if (pumpSwitchState == OFF) {
    if (pumpRelay.getState() == HIGH) {
      pumpRelay.turnOff();
      pumpTimeOff = millis();
      printTimestamp();
      Serial.print("Pump is now OFF. ");
      dossantVol += (int) ((float) (pumpTimeOff - pumpTimeOn) * REFILL_RATE / 1000.0);
      Serial.print(dossantVol);
      Serial.println(" mL dossant has been dispensed.");
      w = true;
    }  
  }
} // manualProcess


void faultProcess()
// The fault process will identify two possible faults, issue a corresponding warning message 
// and take corrective action, if possible.
// Faults include 1) depleted fresh water supply, 2) malfunctioning level switch.
{
  if (freshWaterVol < pumpCycleVol) {
    Serial.println("The ATO fresh water supply has been depleted and needs to be replenished.");
  } 
  else {
    Serial.println("Level switch 1 (ATO float switch) may be malfunctioning. Please inspect ");
    Serial.println("the switch and rectify the problem, if possible.");  
  };
  Serial.println();
  Serial.println("The ATOC will now shut itself off to avoid damaging the aquarium.");
  Serial.println("Hopefully, no marine life will die today. Please send help soon!");
  Serial.println(); 
  Serial.println("** Press MANUAL mode to silence the alarm and restart the controller. **");
  contMode = FAULT;
}  


void readModeSwitch()
// Read mode switch, called every 200 ms by timer
{
  bool stateChanged;
  if (!firstTimeThru) {
    stateChanged = modeSwitch.update();
  } 
  else {
    stateChanged = true;
  }
  int value = modeSwitch.read();
  if ((stateChanged) && (value == HIGH)) {
    atoMode = AUTO;
    printTimestamp();      
    Serial.println("Mode is now AUTO.");
  }
  if ((stateChanged) && (value == LOW)) {
    atoMode = MANUAL;
    printTimestamp();      
    Serial.println("Mode is now MANUAL.");
    dossantVol = 0;
  }
  if ((firstTimeThru) && (atoMode != AUTO)) {
    // WARNING - system should start in AUTO mode. Issue warning to LCD panel. 
  }
  if (atoMode == AUTO) autoProcess();
  if ((atoMode == MANUAL) && (!manualModeInhibit)) manualProcess();
  firstTimeThru = false;  
} // readModeSwitch


void readPumpSwitch()
// Read pump switch
{ 
  bool stateChanged = pumpSwitch.update();
  int value = pumpSwitch.read();
  if (stateChanged) {
    if (value == LOW) pumpSwitchState = ON;
    if (value == HIGH) pumpSwitchState = OFF;      
  }  
} // readPumpSwitch


void readLevel1Switch()
// Read level 1 switch
{
  bool stateChanged;
  if (!firstTimeThru) {
    stateChanged = level1Switch.update();    
  } 
  else {
    stateChanged = true;
  }
  int value = level1Switch.read();
  //if ((value == HIGH) && (stateChanged)) {
  if (value == HIGH) {  
    waterLevel = AT_LEVEL;
    curPumpCycle = 0;
  }
  if (value == LOW) {  
    waterLevel = BELOW_LEVEL;    
  } 
} // readLevel1Switch


void flashStatLED()
// Flash status LED
{
  bool LEDstate = digitalRead(statLED_pin);
  digitalWrite(statLED_pin, !LEDstate);
} // flashStatLED


void tick(void* context)
// Perform simple time management at 5 Hz
{
  ++tds;
  if (tds == 5) {
    tds = 0;
    // Increment seconds counter, manage minutes & hours counters
    ++ts;
    if (ts == 60) {
      ++tm;
      ts = 0;
    }
    if (tm == 60) {
      ++th;
      tm = 0;
    }
  }
} // tick


void stopPump()
// Turn off pump relay
{
  if (pumpRelay.getState() == HIGH) {
    pumpRelay.turnOff();
    printTimestamp();      
    Serial.print("Pump is now OFF. ");
    Serial.print(freshWaterVol);
    Serial.println(" mL supply water remains.");
  } 
  // Create timer event to reset level switch read inhibit
  inhibitEvent = t.setTimeout(1000L * LEVEL_SWITCH_READ_INHIBIT, resetReadInhibit);
  // Stop the relayStopEvent, so it can be reused
  t.deleteTimer(relayStopEvent);
  manualModeInhibit = false;
}

void resetReadInhibit()
// Reset level switch read inhibit to allow level switch to be read
{
  levelSwitchReadInhibit = false;
  // Stop the event, so it can be reused
  t.deleteTimer(inhibitEvent);
} // resetReadInhibit


void printTimestamp()
//
{
  sprintf(timestamp, "%02d:%02d - ", mins, secs);
  Serial.print(timestamp);
} // printTimestamp


void setup()
{
  Serial.begin(9600);
  // Configure digital inputs
  pinMode(level1Switch_pin, INPUT);        // set pin to input
  digitalWrite(level1Switch_pin, HIGH);    // turn on pullup resistor
  level1Switch.attach(level1Switch_pin);   // attach level1Switch to pin
  // ******** Proper interval will need to be determined for level switch  
  level1Switch.interval(5);
  // ***************************

  pinMode(modeSwitch_pin, INPUT);          // set pin to input
  digitalWrite(modeSwitch_pin, HIGH);      // turn on pullup resistor
  modeSwitch.attach(modeSwitch_pin);       // attach modeSwitch to pin
  modeSwitch.interval(5);

  pinMode(pumpSwitch_pin, INPUT);      // set pin to input
  digitalWrite(pumpSwitch_pin, HIGH);  // turn on pullup resistor
  pumpSwitch.attach(pumpSwitch_pin);   // attach pumpSwitch to pin
  pumpSwitch.interval(5);

  // Configure digital outputs
  pinMode(pumpRelay_pin, OUTPUT);      // set pin to output
  pumpRelay.attach(pumpRelay_pin);     // attach pumpRelay to pin
  pinMode(statLED_pin, OUTPUT);        // setup statLED
  digitalWrite(statLED_pin, HIGH);     // turn on LED

  Serial.println("*");
  Serial.println("*");
  Serial.println("*");
  Serial.print("* * * ATO Controller v");
  Serial.print(INO_VERSION);
  Serial.println(" * * *");
  Serial.println("*"); 
  Serial.println("H/W configuration complete.");
  Serial.print(" - Pump cycle time, ms   : ");
  Serial.println(pumpCycleTime);
  Serial.print(" - Pump cycle vol, mL    : ");
  Serial.println(pumpCycleVol); 
  Serial.println();

  // Setup timer events
  readLevel1SwitchEvent = t.setInterval(200L, readLevel1Switch);
  readModeSwitchEvent = t.setInterval(400L, readModeSwitch);
  flashStatLEDEvent = t.setInterval(500L, flashStatLED);
  tickEvent = t.setInterval(1000L, tick);

  Serial.println("Timer configuration complete.");
  Serial.print(" - ");
  Serial.print(t.getNumTimers() + 2);
  Serial.println(" timed events have been set up.");
  Serial.println(" ---");

} // setup


void loop()
{
  while (atoMode > -1) {
    // Update timer
    t.run();
  }
  while (atoMode < 0) {
    // Sound alarm (annoying, isn't it!)
    tone(alarmSpeaker_pin, 440, 300);
    tone(alarmSpeaker_pin, 523, 300);
    // Recover from fault
    int value = modeSwitch.read();
    if (value == LOW) {
      atoMode = MANUAL;
      if (freshWaterVol < pumpCycleVol) freshWaterVol = RESERVOIR_FULL_VOL;
      break;
    }
  }
} // loop






























