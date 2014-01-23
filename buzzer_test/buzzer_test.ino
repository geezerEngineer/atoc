/* 
 * buzzer_test.ino
 * Mike Lussier - Jan 10, 2014
 * Compiled with Arduino 1.0.5
 * 
 * Active Tech 171315 3-28VDC piezo buzzer
 * Uses ~1.6 mA at 5V, so it can be driven directly by Arduino pin.
 * Connect pos to D6 and neg to Gnd. 
 */

#define DOT 100              // Morse code dot for alarm

const static uint8_t alarm_pin = 6;

int dot = DOT, dash = 3 * dot;
int sil = dot, slet = 3 * dot, sword = 7 * dot;

// Functions

void morseS(void)
// - - -
{ 
  for (int i=0; i<3; i++) {
    digitalWrite(alarm_pin, HIGH);
    delay(dash);
    digitalWrite(alarm_pin, LOW);
    delay(dot);
  }
  delay(slet - dot);
} // morseS


void morseO(void)
// . . . 
{
  for (int i=0; i<3; i++) {
    digitalWrite(alarm_pin, HIGH);
    delay(dot);
    digitalWrite(alarm_pin, LOW);
    delay(dot);
  }
  delay(slet - dot);
} // morseS

void setup()
{
  pinMode(alarm_pin, OUTPUT);      // set alarm pin to output
  digitalWrite(alarm_pin, LOW);    // make sure its off
} // setup


void loop()
{
  // Play annoying SOS
  for (int j = 0; j < 2; j++) {
    morseS();
    morseO();
    morseS();
    delay(3000);
  }

  // Enough already!
  for (;;);
} // loop


