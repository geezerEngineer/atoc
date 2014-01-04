/* 
 * ATO_Controller.ino
 * Mike Lussier - Jan 3, 2014
 * Compiled with Arduino 1.0.5
 * 
 * This is version 1.0
 * Status: TBD.
 *****************************************
 * v1.0 - Integrates one (of two) level switches, pump relay, 
 * and mode and relay switches. (LCD panel TBD.)
 *
 *****************************************
 * Hardware Configuration:
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
 * In AUTOMATIC mode - When the level float drops to within .12” (+/- .04”) of the  
 * bottom clip the internal reed contacts draw together, completing the circuit and causing 
 * the level switch pin to go LOW. A stable LOW value triggers the pump relay to turn ON 
 * and starts a pumpStart callback timer. When the float rises .20” (+/- .04”) from the 
 * bottom clip, the contacts separate, opening the circuit and causing the pin to go HIGH. 
 * A stable HIGH value turns the pump relay OFF. The pump relay will also turn OFF when the 
 * pumpStop callback occurs. 
 */

#include <Relay.h>
#include <Bounce.h>
#include <SimpleTimer.h>
#include <LiquidCrystal.h>

#define INO_NAME "ATOC"
#define INO_VERSION "1.0.d"

static uint8_t level1Switch_pin = 7;   // Level 1 switch
static uint8_t modeSwitch_pin   = 8;   // Mode switch
static uint8_t pumpSwitch_pin   = 9;   // Pump switch
static uint8_t pumpRelay_pin    = 10;  // Relay for ATO pump
static uint8_t statLED_pin      = 13;  // Status LED

#define PUMP_RUN_TIME_LIMIT 10L        // Pump run time limit, s

// Instantiate debouncer objects for level 1 switch, mode & pump switches
Bounce level1Switch = Bounce();
Bounce modeSwitch = Bounce();
Bounce pumpSwitch = Bounce();

// Instantiate relay object for ATO pump
Relay pumpRelay = Relay();

// Instantiate timer object for recurring events
SimpleTimer t = SimpleTimer();

// Define timed events
uint8_t flashStatLEDEvent;             // Heartbeat LED, 2 Hz
uint8_t tickEvent;                     // Clock management, 1 Hz
uint8_t readSwitchesEvent;             // Read mode & pump switches, 5 Hz
uint8_t relayStopEvent;                // 

// Global time vars
int secs = 0, mins = 0, hours = 0;

// Other globals
const static uint8_t AUTO = 0;
const static uint8_t MANUAL = 1;
uint8_t atoMode = MANUAL;              
char timeS[12];
bool firstTimeThru = true;


// Functions
void readSwitches()
// Read mode and pump switches
{
  bool stateChanged;
  int value;
  // Read mode switch
  if (firstTimeThru) {
    stateChanged = true;
  } 
  else {
    stateChanged = modeSwitch.update();
  }
  value = modeSwitch.read();
  if ((stateChanged) && (value == HIGH)) {
    atoMode = AUTO;
    printTimestamp();      
    Serial.println("Mode is now AUTO.");
  }
  if ((stateChanged) && (value == LOW)) {
    atoMode = MANUAL;
    printTimestamp();      
    Serial.println("Mode is now MANUAL.");
  }
  // Read pump switch
  stateChanged = pumpSwitch.update();
  value = pumpSwitch.read();
  if ((stateChanged) && (atoMode == MANUAL)) {
    if ((value == LOW) && (level1Switch.read() == LOW)) {
      pumpRelay.turnOn();
      printTimestamp();      
      Serial.println("Pump is now ON.");      
    }
    if (value == HIGH) {
      pumpRelay.turnOff();
      printTimestamp();
      Serial.println("Pump is now OFF.");      
    }
  }

  // Read level 1 switch
  if (firstTimeThru) {
    stateChanged = true;
  } 
  else {
    stateChanged = level1Switch.update();
  }
  value = level1Switch.read();
  if (stateChanged) {
    if (value == HIGH) {
      pumpRelay.turnOff();
      printTimestamp();
      Serial.println("Water level is OK. Pump is now OFF.");
    }
    if ((value == LOW) && (atoMode == AUTO)) {
      pumpRelay.turnOn();
      // Create callback timer to stop pump
      relayStopEvent = t.setTimeout(1000L * PUMP_RUN_TIME_LIMIT, stopPump);
      printTimestamp();
      Serial.println("Water level is LOW. Pump is now ON.");      
    }
    firstTimeThru = false;
  } 
} // readSwitches


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
    Serial.println("Pump run time limit reached. Pump is now OFF.");
  } 
  // Stop the event, so it can be reused
  t.deleteTimer(relayStopEvent);
}


void printTimestamp()
//
{
  sprintf(timeS, "%02d:%02d - ", mins, secs);
  Serial.print(timeS);
} // printTimestamp


void setup()
{
  Serial.begin(9600);
  // Configure digital inputs
  pinMode(level1Switch_pin, INPUT);        // set pin to input
  digitalWrite(level1Switch_pin, HIGH);    // turn on pullup resistor
  level1Switch.attach(level1Switch_pin);   // attach level1Switch to pin
  // ******** Proper interval will need to be determined for level switch  
  level1Switch.interval(10);
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
  Serial.println("* * * ATO Controller * * *");
  Serial.println("*");  
  Serial.println("H/W configuration complete."); 

  // Setup timer events
  readSwitchesEvent = t.setInterval(200L, readSwitches);
  flashStatLEDEvent = t.setInterval(500L, flashStatLED);
  tickEvent = t.setInterval(1000L, tick);

  Serial.println("Timer configuration complete.");
  Serial.print(t.getNumTimers() + 1);
  Serial.println(" timed events have been setup.");
  Serial.println(" ---");

} // setup


void loop()
{
  // Update timer
  t.run();
} // loop









