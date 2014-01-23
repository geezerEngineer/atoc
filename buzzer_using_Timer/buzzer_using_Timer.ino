/* 
 * buzzer_using_Timer.ino
 * Mike Lussier - Jan 12, 2014
 * Compiled with Arduino 1.0.5
 * 
 * Active Tech 171315 3-28VDC piezo buzzer
 * Uses ~1.6 mA at 5V, so it can be driven directly by Arduino pin.
 * Connect pos to D6 and neg to Gnd. 
 */

#include <Timer.h>
#define DOT 100                        // Morse code dot duration for alarm, ms

const static uint8_t alarm_pin = 6;    // HIGH on pin 6 sounds the buzzer

// Morse code timings
long dot = DOT, dash = 3 * dot;

// Instantiate a timer
Timer t = Timer();

// Functions
void alarm1()
{
  digitalWrite(alarm_pin, HIGH);
  delay(dot);
  digitalWrite(alarm_pin, LOW);
} // alarm1


void alarm2()
{
  for (int j = 0; j < 3; j++) {
    digitalWrite(alarm_pin, HIGH);
    delay(dash);
    digitalWrite(alarm_pin, LOW);
    delay(2*dot);
  }

} // alarm2


void setup()
{
  pinMode(alarm_pin, OUTPUT);      // set alarm pin to output
  digitalWrite(alarm_pin, LOW);    // start with the alarm off
} // setup


void loop()
{
  // Play annoying alarms
  alarm1();
  delay(3000);
  alarm2();

  // Enough already!
  for (;;);
} // loop



