/* 
 * ATO_Controller.ino
 * Mike Lussier - Jan 5, 2014
 * Compiled with Arduino 1.0.5
 * 
 * This is version 1.1
 * Status: TBD.
 *****************************************
 * v1.0 - Integrates one (of two) level switches, pump relay, 
 * and mode and relay switches. (LCD panel TBD.)
 *
 *****************************************
 * Hardware Configuration:
 *  Alarm speaker
 *   - connected between pin D2 and Gnd with a 100 resistor in series on the data line.
 *  Level 1 switch
 *   - connected between pin D7 and Gnd. Uses 20kΩ internal pullup.
 *  Mode switch
 *   - connected between pin D8 and Gnd. Uses 20kΩ internal pullup. 
 *  Pump switch 
 *   - connected between pin D9 and Gnd. Uses 20kΩ internal pullup.
 *  Pump relay 
 *   - SIG connected to pin D10, 
 *   - VCC connected to 5V and 
 *   - GND connected to Gnd.
 * Note, the above configuration will change to accommodate the LCD panel. 
 *
 * Operation: 
 * In MANUAL mode - When the pump switch is idle, the switch reads HIGH. When  
 * pressed the switch reads LOW. The relay turns ON when the switch becomes stable 
 * LOW and turns OFF when either the pump switch or the Level 1 switch goes stable HIGH.
 * The pump will not turn ON if the level 1 switch is HIGH.
 *
 * In AUTOMATIC mode - When the level float drops to within .12” (±.04”) of the  
 * bottom clip the internal reed contacts draw together, completing the circuit and causing 
 * the level switch pin to go LOW. A stable LOW value triggers the pump relay to turn ON 
 * and starts a pumpStart callback timer. When the float rises .20” (±.04”) from the 
 * bottom clip, the contacts separate, opening the circuit and causing the pin to go HIGH. 
 * A stable HIGH value turns the pump relay OFF. The pump relay will also turn OFF when the 
 * pumpStop callback occurs. 
 */

#include <Relay.h>
#include <Bounce.h>
#include <SimpleTimer.h>
#include <LiquidCrystal.h>

#define INO_NAME "ATOC"
#define INO_VERSION "1.1.d"

static uint8_t alarmSpeaker_pin = 2;   // Alarm speaker
static uint8_t level1Switch_pin = 7;   // Level 1 switch
static uint8_t modeSwitch_pin   = 8;   // Mode switch
static uint8_t pumpSwitch_pin   = 9;   // Pump switch
static uint8_t pumpRelay_pin    = 10;  // Relay for ATO pump
static uint8_t statLED_pin      = 13;  // Status LED

// From ATO Calculations
#define RESERVOIR_FULL_VOL 3500        // Initial vol of supply water, mL
#define REFILL_RATE 36.8               // Nominal pump flow rate, mL/s
#define HYSTERESIS_VOL_CHANGE 618.1    // Expected volume change, mL

#define LEVEL_SWITCH_READ_INHIBIT 10   // Duration of level switch read inhibit, s
#define MAX_PUMP_RUN_CYCLES 8          // Maximum no of pump run cycles before error

// Instantiate debouncer objects for level 1 switch, mode & pump switches
Bounce level1Switch = Bounce();
Bounce modeSwitch = Bounce();
Bounce pumpSwitch = Bounce();

// Instantiate relay object for ATO pump
Relay pumpRelay = Relay();

// Instantiate timer object for recurring events
SimpleTimer t = SimpleTimer();

// Define timed events
uint8_t flashStatLEDEvent;             // Heartbeat LED, also updates display panel, 2 Hz
uint8_t tickEvent;                     // Clock management, 1 Hz
uint8_t readModeSwitchEvent;           // Read mode switch, 5 Hz
uint8_t readLevel1SwitchEvent;         // Read level 1 switch, 10 Hz
uint8_t relayStopEvent;                // 
uint8_t inhibitEvent;                  // Level switch read inhibit reset 

// Global time vars
int secs = 0, mins = 0, hours = 0;

// Other globals
const static uint8_t AUTO = 0;
const static uint8_t MANUAL = 1;
const static uint8_t FAULT = -1;
uint8_t atoMode = AUTO;                // Default mode is AUTO

const static uint8_t BELOW_LEVEL = 0;
const static uint8_t AT_LEVEL = 1;
uint8_t waterLevel = AT_LEVEL;

const static uint8_t OFF = 0;
const static uint8_t ON = 1;
uint8_t pumpSwitchState = OFF;
long pumpTimeOn, pumpTimeOff;

char timestamp[12];                     // Logging timestamp
bool firstTimeThru = true;
bool levelSwitchReadInhibit = false;    // Inhibits reading switch due to water disturbance
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
void autoProcess()
// called conditionally every 200 ms by readModeSwitch  
{
  // Is the pump running?
  if (pumpRelay.getState() == LOW) { // no
    // Is the level switch read inhibit in effect?
    if (!levelSwitchReadInhibit) { // no
      if (waterLevel == BELOW_LEVEL) { // level is LOW
        ++curPumpCycle;
        freshWaterVol -= pumpCycleVol;
        if ((curPumpCycle <= MAX_PUMP_RUN_CYCLES) && (freshWaterVol >= pumpCycleVol)) {
          // turn on the pump, then turn it off after pumpCycleTime
          pumpRelay.turnOn();
          printTimestamp();      
          Serial.print("Pump is now ON. (");
          Serial.print(curPumpCycle);
          Serial.println(")");        
          relayStopEvent = t.setTimeout(pumpCycleTime, stopPump);
          // make levelSwitchReadInhibit true to prevent level switch from being read
          levelSwitchReadInhibit = true;
          manualModeInhibit = true;
        } 
        else {
          faultProcess();
        } 
      }
    }
  }
} // autoProcess


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
{
  printTimestamp();
  Serial.println("*");  
  Serial.println("WARNING");
  Serial.println();
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
  atoMode = FAULT;
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


void tick()
// Perform simple time management every second
{
  // Increment seconds counter, manage minutes & hours counters
  ++secs;
  if (secs == 60) {
    ++mins;
    secs = 0;
  }
  if (mins == 60) {
    ++hours;
    mins = 0;
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
























