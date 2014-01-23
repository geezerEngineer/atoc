/*
 * working_with_serLCD.ino
 * Mike Lussier - Jan 11, 2014
 * Compiled with Arduino 1.0.5 
 ****************************** 
 */
 #define INO_VERSION "1.1.d"
 /*
 * c - Adds powered pump to the mix. And it works!
 * d - Calculates pump run time.
 *
 */

#include <Relay.h>
#include <Bounce.h>
#include <Timer.h>

static uint8_t level1Switch_pin = 7;   // Level 1 switch
static uint8_t modeSwitch_pin   = 8;   // Mode switch
static uint8_t pumpSwitch_pin   = 9;   // Pump switch
static uint8_t pumpRelay_pin    = 10;  // Relay for ATO pump
static uint8_t statLED_pin      = 13;  // Status LED

// Instantiate debouncer objects for level, mode & pump switches
Bounce level1Switch = Bounce();
Bounce modeSwitch = Bounce();
Bounce pumpSwitch = Bounce();

// Instantiate relay object for ATO pump
Relay pumpRelay = Relay();

// Instantiate timer object for recurring events
Timer t = Timer();

// Define timed events
int tickEvent;                         // Clock management, 5 Hz
int readAllSwitchesEvent;              // Read all switches, 4 Hz
int heartbeatEvent;                    // Heartbeat LED and LCD, 2 Hz
int lcdAllowEvent;                     // Enable LCD display updating, after 5 s

// Global time vars
long t0 = 0;
int tds = 0, ts = 0, tm = 0, th = 0;

// Other globals
// atoMode values
const static int AUTO = 0;
const static int MANUAL = 1;
const static int FAULT = -1;
int atoMode;

// pumpMode values
const static int OFF = 0;
const static int ON = 1;
int pumpMode = OFF;

// waterLevel values
const static int BELOW_LEVEL = 0;
const static int AT_LEVEL = 1;
int waterLevel = AT_LEVEL;

int ms_state = HIGH, ps_state = HIGH, l1s_state = HIGH;
bool ms_stateChanged, ps_stateChanged, l1s_stateChanged;
bool ftt_ms = true, ftt_ps = true, ftt_l1s;
bool psBlocked = true;
bool lcdAllowed = false;

int msgNo = 1;                  // Message sent to LCD
double pumpRunTime;             // Length of time pump has been on, s

// From GDM1602K LCD character set
const char ptr = 126;           // pointer symbol
const char spc = 32;            // space

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
// Line 1 - system status (mode, pump state, water level)
// Line 2 - messages
{
  if (lcdAllowed) {
    // Clear screen for new message
    selectLineOne();
    Serial.print("                ");
    selectLineTwo();
    Serial.print("                ");

    switch (msgNo) {
    case 1 : 
      selectLineOne();
      if (atoMode == AUTO) Serial.print(" Mode: AUTO");
      if (atoMode == MANUAL) Serial.print(" Mode: MANUAL");
      break;
    case 2 :
      selectLineOne();
      if (pumpMode == OFF) {
        Serial.print(" Pump: OFF");
        selectLineTwo();
        Serial.print("On time: ");
        Serial.print(pumpRunTime, 1);
        Serial.print(" s");
      }
      if (pumpMode == ON) {
        Serial.print(" Pump: ON");
      }
      break;
    case 3 :
      selectLineOne();
      Serial.print(" Water: ");
      if (waterLevel == AT_LEVEL) Serial.print("AT LEVEL");
      if (waterLevel == LOW) Serial.print("LOW");
      break;
    case 4 :
      selectLineOne();
      Serial.print(" Inhibited");
      selectLineTwo();
      Serial.print(" in AUTO mode");
      break;  
    }
  }
} // updateDisplay


void readAllSwitches(void* context) 
{
  readModeSwitch();
  if ((ftt_ms) || (ms_stateChanged)) {
    if (ms_state == HIGH) {
      atoMode = AUTO;
      psBlocked = true;
    }
    if (ms_state == LOW) {
      atoMode = MANUAL;
      psBlocked = false;
    }
    ftt_ms = false;
    updateDisplay(1);
  }

  readPumpSwitch();
  if ((ftt_ps) || (ps_stateChanged)) {
    if (ps_state == LOW) {
      pumpMode = ON;
      if (!psBlocked) {
        pumpRelay.turnOn();
        t0 = millis();
      }
    }
    if (ps_state == HIGH) {
      pumpMode = OFF;
      if (!psBlocked) {
        pumpRelay.turnOff();
        pumpRunTime = (millis() - t0) / 1000.0; 
      }
    }
    ftt_ps = false; 
    if (!psBlocked) {
      updateDisplay(2);
    } 
    else {
      if (pumpMode == ON) updateDisplay(4);
      if (pumpMode == OFF) updateDisplay(1);
    }
  }

  readLevel1Switch();
  if ((ftt_l1s) || (l1s_stateChanged)) {
    if (l1s_state == LOW) {
      waterLevel = LOW;
    }
    if (l1s_state == HIGH) {
      waterLevel = AT_LEVEL;
    }
    ftt_l1s = false; 
    updateDisplay(3);
  }
} // readAllSwitches


void readModeSwitch()
{
  ms_stateChanged = modeSwitch.update();
  if ((ms_stateChanged) || (ftt_ms)) {
    ms_state = modeSwitch.read();
  }
} // readModeSwitch


void readPumpSwitch()
{
  ps_stateChanged = pumpSwitch.update();
  if ((ps_stateChanged) || (ftt_ps)) {
    ps_state = pumpSwitch.read();
  }
} // readPumpSwitch


void readLevel1Switch()
// Read level 1 switch
{
  l1s_stateChanged = level1Switch.update();
  if ((l1s_stateChanged) || (ftt_l1s)) {
    l1s_state = level1Switch.read();
  } 
} // readLevel1Switch


void flashStatLED(void* context)
// Flash status LED & show heartbeat on LCD, 2 Hz
{
  bool LEDstate = digitalRead(statLED_pin);
  digitalWrite(statLED_pin, !LEDstate);

  // Show synchronized heartbeat on LCD
  goToPosn(0);
  if (!LEDstate) {
    Serial.print(ptr);
  } 
  else {
    Serial.print(spc);
  }
} // flashStatLED


void enableLCDupdate(void* context)
{
  lcdAllowed = true;
  clearLCD();
  updateDisplay(1);
}    


void tick(void* context)
// Perform time management at 5 Hz
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


void setup(void) 
{

  // Start the ADM1602U serial interface
  Serial.begin(9600);

  // Configure digital inputs
  // Mode switch
  pinMode(modeSwitch_pin, INPUT);          // set pin to input
  digitalWrite(modeSwitch_pin, HIGH);      // turn on pullup resistor
  modeSwitch.attach(modeSwitch_pin);       // attach modeSwitch to pin
  modeSwitch.interval(5);

  // Pump switch
  pinMode(pumpSwitch_pin, INPUT);          // set pin to input
  digitalWrite(pumpSwitch_pin, HIGH);      // turn on pullup resistor
  pumpSwitch.attach(pumpSwitch_pin);       // attach pumpSwitch to pin
  pumpSwitch.interval(5);

  // Level 1 switch
  pinMode(level1Switch_pin, INPUT);        // set pin to input
  digitalWrite(level1Switch_pin, HIGH);    // turn on pullup resistor
  level1Switch.attach(level1Switch_pin);   // attach level1Switch to pin
  // ******** Proper interval will need to be determined for level switch  
  level1Switch.interval(5);
  // ***************************

  // Configure digital outputs
  pinMode(pumpRelay_pin, OUTPUT);      // set pin to output
  pumpRelay.attach(pumpRelay_pin);     // attach pumpRelay to pin
  pinMode(statLED_pin, OUTPUT);        // setup statLED
  digitalWrite(statLED_pin, HIGH);     // turn on LED

  clearLCD();
  backlightOn();
  displayOn();

  displaySplashScreen();

  // Setup timer recurring events
  tickEvent = t.every(200L, tick, 0);
  readAllSwitchesEvent = t.every(250L, readAllSwitches, 0);  
  heartbeatEvent = t.every(500L, flashStatLED, 0);
  //flashStatLEDEvent = t.pulseImmediate(statLED_pin, 500L, HIGH);
  // Setup timer one shot events
  lcdAllowEvent = t.after(3000L, enableLCDupdate, 0);
}


void loop(void)
{
  t.update();
} // loop

////////////////////////////////////////////////////////
/////////////////////// GDM1602K ///////////////////////
////////////////////////////////////////////////////////

//////////// Display Functions ////////////////

// Scrolls the display to the left by the number of characters passed in, and waits a given
// number of milliseconds between each step
void scrollLeft(int num, int wait) 
{
  for(int i=0;i<num;i++) {
    Serial.write(0xFE);
    Serial.write(0x18);
    //delay(10);
    //delay(wait);
  }
}

// Scrolls the display to the right by the number of characters passed in, and waits a given
// number of milliseconds between each step
void scrollRight(int num, int wait) 
{
  for(int i=0;i<num;i++) {
    Serial.write(0xFE);
    Serial.write(0x1C);
    //delay(10);
    //delay(wait);
  }
}

// Starts the cursor at the beginning of the first line (convenient method for goTo(0))
void selectLineOne() {  //puts the cursor at line 1 char 0.
  Serial.write(0xFE);   //command flag
  Serial.write(128);    //position
  delay(10);
}

// Starts the cursor at the beginning of the second line (convenient method for goTo(16))
void selectLineTwo() 
{  //puts the cursor at line 2 char 0.
  Serial.write(0xFE);   //command flag
  Serial.write(192);    //position
  delay(10);
}

// Sets the cursor to the given position
// line 1: 0-15, line 2: 16-31, >31 defaults back to 0
void goToPosn(int position) 
{
  if (position < 16) { 
    Serial.write(0xFE);   //command flag
    Serial.write((position+128));
  } 
  else if (position < 32) {
    Serial.write(0xFE);   //command flag
    Serial.write((position+48+128));
  } 
  else { 
    goToPosn(0); 
  }
  delay(10);
}

// Resets the display, undoing any scroll and removing all text
void clearLCD() 
{
  Serial.write(0xFE);
  Serial.write(0x01);
  delay(10);
}

// Turns on the backlight
void backlightOn() 
{
  Serial.write(0x7C);
  Serial.write(157);   //light level
  delay(10);
}

// Turns off the backlight
void backlightOff() 
{
  Serial.write(0x7C);
  Serial.write(128);   //light level
  delay(10);
}

// Turns on visual display
void displayOn() 
{
  Serial.write(0xFE);
  Serial.write(0x0C);
  delay(10);
}

// Turns off visual display
void displayOff() 
{
  Serial.write(0xFE);
  Serial.write(0x08);
  delay(10);
}

// Initiates a function command to the display
void serCommand() 
{
  Serial.write(0xFE);
}


















