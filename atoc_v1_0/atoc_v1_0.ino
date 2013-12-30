

/* 
 * ATO_Controller.ino
 * Mike Lussier - Dec 22, 2013
 * Compiled with Arduino 1.0.5
 * 
 * This is version 1.0
 * Status: TBD.
 *****************************************
 * v1.0 - Used to calibrate level switches and pump, 
 * mode switch and LCD Panel.
 *
 *****************************************
 * Hardware Configuration
 * Mode switch connected between pin D9 and Gnd. Uses 20kΩ internal pullup.
 * Level switch 1 connected between pin D6 and Gnd. Uses 20KΩ internal pullup.
 * Level switch 2 connected between pin D7 and Gnd. Uses 20KΩ internal pullup.
 * Operation: When the float drops to within .12” (+/- .04”) of the bottom clip, 
 * the internal reed contacts draw together and complete a circuit. Pin goes low. 
 * When the float rises .20” (+/- .04”) from the bottom clip, the contacts 
 * separate and open the circuit. Pin goes high.
 * Grove relay SIG connected to pin D10.
 * Grove relay GND connected to Gnd.
 */

#include "Relay.h"
#include <Bounce.h>
#include <LiquidCrystal.h>

#define INO_NAME "ATOC"
#define INO_VERSION "1.0.a"

#define MAX_RUN_TIME 2500        // Maximum time pump will run when activated, ms

// Digital pin assignments
// Inputs
static uint8_t level1Switch_pin = 6;      // Level switch 1; low when level is low
static uint8_t level2Switch_pin = 7;      // Level switch 2; low when level is low
static uint8_t atoPumpSwitch_pin = 8;     // Pump switch; only works when modeSwitch is high, low = atoPump on
static uint8_t modeSwitch_pin = 9;        // Mode switch; low = automatic, high = manual 

// Outputs
static uint8_t atoPumpRelay_pin = 10;    // ATO pump relay; high = pump on
static uint8_t statLED_pin = 13;         // System heartbeat LED

// Instantiate debouncer objects for switches
Bounce level1Switch = Bounce();
Bounce level2Switch = Bounce();
Bounce atoPumpSwitch = Bounce();
Bounce modeSwitch = Bounce();

// Instantiate relay object 
Relay atoPumpRelay = Relay();

// Instantiate LCD Panel


// Global time vars
unsigned long t0 = 0, sec = 0;
int nticks = 0, d1ticks = 0, d2ticks = 0, hourNo = 0;

// Constants
static int levelS_hold = 4000.0;
typedef enum {
  AUTO, MANUAL} 
systemModes;
typedef enum {
  AT_LEVEL, BELOW}
levels;

// Variables
bool systemMode = AUTO;

void setup(void)
{
  // Configure digital inputs
  pinMode(modeSwitch_pin, INPUT);      // set pin to input
  digitalWrite(modeSwitch_pin, HIGH);  // turn on pullup resistor
  pinMode(level1Switch_pin, INPUT);
  digitalWrite(level1Switch_pin, HIGH);
  pinMode(level2Switch_pin, INPUT);
  digitalWrite(level2Switch_pin, HIGH);

  // Configure digital outputs
  pinMode(atoPumpRelay_pin, OUTPUT);
  atoPumpRelay.attach(atoPumpRelay_pin);
  atoPumpRelay.turnOn();
  
  pinMode(statLED_pin, OUTPUT);
  
  // Start serial port
  Serial.begin(19200);

  Serial.println(" ");
  Serial.println("*");
  Serial.println("*");
  Serial.print(INO_NAME);
  Serial.print(" ");
  Serial.println(INO_VERSION);
  Serial.println("*");
} // Setup

void readModeSwitch()
// Read mode switch and set status
{
  systemMode = modeSwitch.read() ? MANUAL : AUTO; 
} // readMode

void updateDisplay()
{
  // Print headline
  lcd.setCursor(0,0);
  // lcd.print("12345678901234567890");
  lcd.print("ATO Controller ");
  lcd.print(INO_VERSION);
  // Print mode and pump state
  lcd.setCursor(0,1);
  systemMode == AUTO ? lcd.print("Mode: Auto") : lcd.print("Mode: Manual");
  pumpState == OFF ? lcd.print("  Pump: Off") : lcd.print("  Pump: On");
  // Print water level
  lcd.setCursor(0,2);
  waterLevel == AT_LEVEL ? lcd.print("  Water level: OK") : lcd.print("  Water level: LOW");
  // Print error message
  lcd.setCursor(0,3);
  //lcd.print("12345678901234567890");
  lcd.print("Low ATO water level")
  } // updateDisplay

  void tick()
    // Perform scheduled tasks on 100 ms ticks over 1000 ms cycle (10 ticks)
  {
    if (millis() - t0 >= 100) { // On 100 ms boundary
      t0 = millis();
      ++nticks;  // Increment tick count
      if (nticks > 9) nticks = 0;

      // Read mode switch
      readModeSwitch();

      // Read level switches
      readLevel();

      // Update pump status
      updatePump();

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
      }
    }
  } // tick

void loop()
{
  // Perform scheduled tasks on 100 ms ticks
  tick();
} // loop





