/* 
 * Working_with_switches.ino
 * Mike Lussier - Jan 8, 2014
 * Compiled with Arduino 1.0.5
 * 
 * This is version 1.0 using SimpleTimer
 * Works as desired.
 * SimpleTimer class provides:
 * void run()
 * // call function f every d milliseconds
 * int setInterval(long d, timer_callback f) 
 * // call function f once after d milliseconds
 * int setTimeout(long d, timer_callback f) 
 * 
 * Timer class provides:
 * void update(void) 
 * // call function f every d milliseconds
 * int8_t every(unsigned long d, void (*f)(void*), void* context)
 * call function f once after d milliseconds
 * int8_t after(unsigned long d, void (*f)(void*), void* context)
 */

#include <Relay.h>
#include <Bounce.h>
#include <SimpleTimer.h>

static uint8_t modeSwitch_pin   = 8;   // Mode switch
static uint8_t pumpSwitch_pin   = 9;   // Pump switch
static uint8_t pumpRelay_pin    = 10;  // Relay for ATO pump
static uint8_t statLED_pin      = 13;  // Status LED

// Instantiate debouncer objects for mode & pump switches
Bounce modeSwitch = Bounce();
Bounce pumpSwitch = Bounce();

// Instantiate relay object for ATO pump
Relay pumpRelay = Relay();

// Instantiate timer object for recurring events
SimpleTimer t = SimpleTimer();


// Define timed events
int flashStatLEDEvent;                 // Heartbeat LED, also updates display panel, 2 Hz
int tickEvent;                         // Clock management, 5 Hz
int readAllSwitchesEvent;              // Read all switches, 4 Hz
int psBlockEvent = -1;                 // 
int delayEvent;                        // 

// Global time vars
int tds = 0, ts = 0, tm = 0, th = 0;

// Other globals
const static int AUTO = 0;
const static int MANUAL = 1;
int atoMode = AUTO;                    // Default mode is AUTO

const static int OFF = 0;
const static int ON = 1;
int ms_state, ps_state;
int ms_count = 0, ps_count = 0;
bool ms_stateChanged, ps_stateChanged;
bool ftt_ms = true, ftt_ps = true;
bool psBlocked = false;

char timestamp[14];                    // Logging timestamp

// Functions

void readAllSwitches() 
{
  readModeSwitch();
  if ((ftt_ms) || (ms_stateChanged)) {
    ++ms_count;
    if (ms_state == HIGH) {
      atoMode = AUTO;
      printTimestamp();
      Serial.print("Mode is now AUTO. (");
      Serial.print(ms_count);
      Serial.println(")");
      if (psBlocked) Serial.println(" - Pump switch is blocked.");
      if (!psBlocked) Serial.println("- Pump switch is not blocked.");
    }
    if (ms_state == LOW) {
      atoMode = MANUAL;
      printTimestamp();
      Serial.print("Mode is now MANUAL. (");
      Serial.print(ms_count);
      Serial.println(")");
    }
    ftt_ms = false;
  }
  if (!psBlocked) {
    readPumpSwitch();
    if ((ftt_ps) || (ps_stateChanged)) {
      ++ps_count;
      if (ps_state == OFF) {
        printTimestamp();
        Serial.print("Pump switch is OFF. (");
        Serial.print(ps_count);
        Serial.println(")");
      }
      if (ps_state == ON) {
        printTimestamp();
        Serial.print("Pump switch is ON. (");
        Serial.print(ps_count);
        Serial.println(")");
        psBlocked = true; // Block ps for next 5 seconds
        psBlockEvent = t.setTimeout(5000L, psUnblock);
        Serial.println(" - Pump switch blocked for next 5 seconds.");
      }
      ftt_ps = false;
    }
  }
}


void readModeSwitch()
{
  ms_stateChanged = modeSwitch.update();
  ms_state = HIGH;
  if (ms_stateChanged) {
    int value = modeSwitch.read();
    if (value == LOW) ms_state = LOW;
  }
}


void readPumpSwitch()
{
  ps_stateChanged = pumpSwitch.update();
  ps_state = OFF;
  if (ps_stateChanged) {
    int value = pumpSwitch.read();
    if (value == LOW) ps_state = ON;
  }
}


void psUnblock()
{
  t.deleteTimer(psBlockEvent);
  psBlocked = false;
}


void flashStatLED()
// Flash status LED
{
  bool LEDstate = digitalRead(statLED_pin);
  digitalWrite(statLED_pin, !LEDstate);
} // flashStatLED


void tick()
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


void printTimestamp()
//
{
  sprintf(timestamp, "%02d:%02d.%1d - ", tm, ts, tds*2);
  Serial.print(timestamp);
} // printTimestamp


void setup()
{
  Serial.begin(19200);
  // Configure digital inputs
  // Mode switch
  pinMode(modeSwitch_pin, INPUT);          // set pin to input
  digitalWrite(modeSwitch_pin, HIGH);      // turn on pullup resistor
  modeSwitch.attach(modeSwitch_pin);       // attach modeSwitch to pin
  modeSwitch.interval(5);

  // Pump switch
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
  Serial.print("* * * Working with Switches");
  Serial.println(" * * *");
  Serial.println("*"); 
  Serial.println("H/W configuration complete."); 

  // Setup timer events
  readAllSwitchesEvent = t.setInterval(250L, readAllSwitches);  
  flashStatLEDEvent = t.setInterval(500L, flashStatLED);
  tickEvent = t.setInterval(200L, tick);

  Serial.println("Timer configuration complete.");
  Serial.print(" - ");
  Serial.print(t.getNumTimers() + 1);
  Serial.println(" timed events have been set up.");
  Serial.println("Ready");

} // setup


void loop()
{
  while (atoMode > -1) {
    // Update timer
    t.run();
  }
} // loop




