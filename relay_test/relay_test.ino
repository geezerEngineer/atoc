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
 * Pump switch connected between pin D9 and Gnd. Uses 20kΩ internal pullup.
 * Pump relay SIG connected to pin D10 and relay GND connected to Gnd. 
 *
 * Operation: 
 * When the pump switch is idle, the switch reads HIGH. When pressed, the 
 * switch reads LOW. The relay turns OFF when the switch goes HIGH and turns 
 * ON when it goes LOW. 
 */

#include <Relay.h>
#include <Bounce.h> 

static uint8_t pumpSwitch_pin = 9;     // Relay switch, LOW = Relay on
static uint8_t pumpRelay_pin = 10;     // Relay for ATO pump
static uint8_t statLED_pin = 13;       // Status LED

// Instantiate debouncer for pump switch
Bounce pumpSwitch = Bounce();

// Instantiate relay for ATO pump
Relay pumpRelay = Relay();

// Global time vars
unsigned long t0 = 0, sec = 0;
int nticks = 0, d1ticks = 0, d2ticks = 0, hourNo = 0;

// Functions
void flashStatLED()
{
  if (nticks == 0) digitalWrite(statLED_pin, HIGH);
  if (nticks == 5) digitalWrite(statLED_pin, LOW);
} // flashStatLED

void flashRelay()
{
  if ((sec % 2) == 0) {
    if (pumpRelay.getState() == LOW) {
      pumpRelay.turnOn();
    } 
    else {
      pumpRelay.turnOff();
    }
  }
} // flashRelay

void tick()
// Perform scheduled tasks on 100 ms ticks over 1000 ms cycle (10 ticks)
{
  if (millis() - t0 >= 100) { // On 100 ms boundary
    t0 = millis();
    ++nticks;  // Increment tick count
    if (nticks > 9) nticks = 0;

    flashStatLED();

    // Every 1000 ms, update clock
    if (nticks == 0) {
      //  - Increment seconds counter, manage hours counter
      ++sec;
      if ((sec % 3600) == 0) ++hourNo;
      if (hourNo == 24) {
        // Reset time
        hourNo = 0;
        sec = 0;
      }
      flashRelay();
    }
  }
} // tick

void setup()
{
  // Configure digital inputs
  pinMode(pumpSwitch_pin, INPUT);      // set pin to input
  digitalWrite(pumpSwitch_pin, HIGH);  // turn on pullup resistor
  pumpSwitch.attach(pumpSwitch_pin);   // attach pumpSwitch to pin
  pumpSwitch.interval(5);

  // Configure digital outputs
  pinMode(pumpRelay_pin, OUTPUT);      // set pin to output
  pumpRelay.attach(pumpRelay_pin);     // attach pumpRelay to pin
  pinMode(statLED_pin, OUTPUT);        // setup statLED

} // setup

void loop()
{
  tick();
  /*
  // Test 1 - Operate pump relay in accordance with pump switch
   // Update the pumpSwitch
   pumpSwitch.update();
   
   // Get the updated value
   int value = pumpSwitch.read();
   
   // Turn on/off pump accordingly
   if (value == LOW) {
   pumpRelay.turnOn();
   } 
   else {
   pumpRelay.turnOff();
   }
   */

  // Test 2 - Operate pump relay in accordance with changes to pump switch setting
  boolean stateChanged = pumpSwitch.update();
  int value = pumpSwitch.read();

  // Turn on/off pump accordingly only if the pumpSwitch has changed state
  if (stateChanged) {
    if (value == LOW) pumpRelay.turnOn();
    if (value == HIGH) pumpRelay.turnOff();
  }
} // loop










