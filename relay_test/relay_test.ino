/* 
 * relay_test.ino
 * Mike Lussier - Dec 30, 2013
 * Compiled with Arduino 1.0.5
 * 
 * This is version 1.0
 * Status: TBD.
 *****************************************
 * v1.0 - Used to test the Relay class.
 *
 *****************************************
 * Hardware Configuration:
 * Pump switch 
 *  - connected between pin D9 and Gnd. Uses 20kΩ internal pullup.
 * Pump relay 
 *  - SIG connected to pin D10, 
 *  - VCC connected to 5V and 
 *  - GND connected to Gnd. 
 *
 * Operation: 
 * When the pump switch is idle, the switch reads HIGH. When pressed, the 
 * switch reads LOW. The relay turns ON when the switch goes LOW and turns 
 * OFF when either the switch goes HIGH or when the pumpStop callback occurs.
 */

#include <Relay.h>
#include <Bounce.h>
#include <SimpleTimer.h>

static uint8_t pumpSwitch_pin = 9;     // Relay switch, LOW = Relay on
static uint8_t pumpRelay_pin = 10;     // Relay for ATO pump
static uint8_t statLED_pin = 13;       // Status LED

#define PUMP_MAX_RUN_TIME 5L

// Instantiate debouncer object for pump switch
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
unsigned long t0 = 0, sec = 0;
int nticks = 0, d1ticks = 0, d2ticks = 0, hourNo = 0;

// Other globals
uint8_t atoMode = 1;                  // 0 = AUTO, 1 = MANUAL

// Functions
void readSwitches()
// Read mode and pump switches
{
  // Not reading the mode switch yet
  // Assume mode switch is in MANUAL

  // Read pump switch
  boolean stateChanged = pumpSwitch.update();
  int value = pumpSwitch.read();
  if ((stateChanged) && (atoMode == 1)) {
    if (value == LOW) {
      pumpRelay.turnOn();
      relayStopEvent = t.setTimeout(1000L * PUMP_MAX_RUN_TIME, stopPump);      
    }
    if (value == HIGH) pumpRelay.turnOff(); 
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
  // Increment seconds counter, manage hours counter
  ++sec;
  if ((sec % 3600) == 0) ++hourNo;
  if (hourNo == 24) {
    // Reset time
    hourNo = 0;
    sec = 0;
  }
} // tick


void stopPump()
// Turn off pump relay
{
  pumpRelay.turnOff();
  // Stop the event, so it can be reused
  t.deleteTimer(relayStopEvent);
}


void setup()
{
  Serial.begin(9600);
  // Configure digital inputs
  pinMode(pumpSwitch_pin, INPUT);      // set pin to input
  digitalWrite(pumpSwitch_pin, HIGH);  // turn on pullup resistor
  pumpSwitch.attach(pumpSwitch_pin);   // attach pumpSwitch to pin
  pumpSwitch.interval(5);

  // Configure digital outputs
  pinMode(pumpRelay_pin, OUTPUT);      // set pin to output
  pumpRelay.attach(pumpRelay_pin);     // attach pumpRelay to pin
  pinMode(statLED_pin, OUTPUT);        // setup statLED
  digitalWrite(statLED_pin, HIGH);     // turn on LED

  Serial.println("H/W configuration complete."); 

  // Setup timer events
  readSwitchesEvent = t.setInterval(200L, readSwitches);
  flashStatLEDEvent = t.setInterval(500L, flashStatLED);
  tickEvent = t.setInterval(1000L, tick);

  Serial.println("Timer configuration complete.");
  Serial.print(t.getNumTimers() + 1);
  Serial.println(" timed events have been setup.");

} // setup


void loop()
{
  // Update timer
  t.run();
} // loop


















