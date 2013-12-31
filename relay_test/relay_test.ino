/* 
 * relay_test.ino
 * Mike Lussier - Dec 30, 2013
 * Compiled with Arduino 1.0.5
 * 
 * This is version 1.0
 * Status: TBD.
 *****************************************
 * v1.0 - Used to test the relay class.
 *
 *****************************************
 * Hardware Configuration:
 * Relay switch connected between pin D9 and Gnd. Uses 20kΩ internal pullup.
 * Grove relay SIG connected to pin D10 and relay GND connected to Gnd. 
 * Operation: 
 * When the relay switch is idle, the switch reads HIGH. When pressed, the 
 * switch reads LOW. The relay turns OFF when the switch goes HIGH and turns 
 * ON when it goes LOW. 
 */
 
 #include "Relay.h"

static uint8_t relaySwitch_pin = 9;     // Relay switch; LOW = Relay on

 
 void setup()
 {
 } // setup
 
 void loop()
 {
 } // loop
