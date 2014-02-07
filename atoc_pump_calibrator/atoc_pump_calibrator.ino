/* 
 * atoc_pump_calibrator.ino
 * Mike Lussier - Jan 11, 2014
 * Compiled with Arduino 1.0.5 
 *****************************
 */
#define INO_VERSION "1.1.b"
#define INO_NAME "ACAL"
/*
 * v1.1 - Changes the core code for reading switches to that used in 
 * working_with_serLCD.ino v1.1.d to incorporate the GDM1602K serLCD.
 * v1.0 - Used to calibrate the nominal flow rate of the Aqua Lifter pump. 
 * Operates the pump through the pump relay for a series of timed on-off 
 * run cycles ranging from MIN_RUN_TIME to MAX_RUN_TIME with an increment of 
 * RUN_TIME_INC. Several runs (RUNS_PER_TIME) are performed automatically 
 * at each run time duration. The mode and pump switches are used to trigger 
 * the calibration procedure.
 *****************************************
 * 
 * Hardware Configuration:
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
 *  ACAL starts up in AUTO mode. The calibration process starts with switching 
 *  mode to MANUAL. An info message is displayed for 2 seconds, then test # 
 *  and calculated pump run time are displayed on line 1 & 2, resp (based on 
 *  values of macros set in program.)
 *  Pressing & releasing the PUMP ON switch starts the tests. Test # 
 *  and run # are displayed on line 1 and pump status on line 2. The pump  
 *  starts and stops automatically until all the runs in the test are  
 *  completed. The display is updated automatically for each run. At the 
 *  conclusion of the last run, an alarm is sounded. Pressing PUMP ON 
 *  again, starts the next test. 
 *  On conclusion of the last test, the calibration process terminates. 
 *  Switching to AUTO mode at anytime cancels the process.
 */

#include <Relay.h>
#include <Bounce.h>
#include <Timer.h>

static uint8_t alarm_pin = 6;          // Piezo alarm
static uint8_t level1Switch_pin = 7;   // Level 1 switch
static uint8_t modeSwitch_pin   = 8;   // Mode switch
static uint8_t pumpSwitch_pin   = 9;   // Pump switch
static uint8_t pumpRelay_pin    = 10;  // Relay for ATO pump
static uint8_t statLED_pin      = 13;  // Status LED

// Next 4 need to be datatype long
#define MIN_RUN_TIME 10L               // Starting pump run time, s
#define MAX_RUN_TIME 40L               // Ending pump run time, s
#define RUN_TIME_INC 10L               // Pump run time increment, s
#define TIME_BETWEEN_RUNS 3L           // Time between runs
#define RUNS_PER_TIME 5                // No of runs performed at each pump run time
#define DOT 100                        // Morse code dot for alarm

// Instantiate debouncer objects for level, mode & pump switches
Bounce level1Switch = Bounce();
Bounce modeSwitch = Bounce();
Bounce pumpSwitch = Bounce();

// Instantiate relay object for ATO pump
Relay pumpRelay = Relay();

// Instantiate timer object for recurring events
//SimpleTimer t = SimpleTimer();
Timer t = Timer();

// Define timed events
int tickEvent;                         // Clock management, 5 Hz
int readAllSwitchesEvent;              // Read all switches, 4 Hz
int heartbeatEvent;                    // Heartbeat LED and LCD, 2 Hz
int lcdAllowEvent;                     // Enable LCD display updating, after 5 s
int relayOffEvent;                     // 
int intervalEvent;                     // 
int psBlockEvent;                      //
int delay3sEvent;                      //

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

int ms_state, ps_state;      
bool ms_stateChanged, ps_stateChanged;
bool ftt_ms = true, ftt_ps = true;
bool psBlocked = true;

int testNo = 0, runNo = 0;
bool stillTesting = false;
bool waitingForNextTest = false;

// pumpRunTime ranges from MIN_RUN_TIME to MAX_RUN_TIME
long pumpRunTime = MIN_RUN_TIME;
int maxTests = 1 + (MAX_RUN_TIME - MIN_RUN_TIME) / RUN_TIME_INC;

bool lcdAllowed = false;        // Allows process messages to be written to LCD
int msgNo = 1;                  // Message sent to LCD
// From GDM1602K LCD character set
const char ptr = 126;           // pointer symbol
const char spc = 32;            // space

int dot = DOT, dash = 3 * dot;
int sil = dot, slet = 3 * dot, sword = 7 * dot;

// Functions
void displaySplashScreen(void)
// Display startup message
{
  clearLCD();  
  selectLineOne();
  Serial.print("  ACAL");
  Serial.print("  v");
  Serial.print(INO_VERSION);
  selectLineTwo();
  Serial.print(" Water matters.");
} // displaySplashScreen


void updateDisplay(int msgNo)
// Updates LCD
// Line 1 - system status (mode, pump state, water level)
// Line 2 - process messages
{
  if (lcdAllowed) {
    // Clear lines for new message
    //selectLineOne();
    //Serial.print("                ");
    //selectLineTwo();
    //Serial.print("                ");

    switch (msgNo) {
    case 1 :
      selectLineOne();
      Serial.print("                "); 
      selectLineOne();
      if (atoMode == AUTO) Serial.print(" Mode: AUTO");
      if (atoMode == MANUAL) Serial.print(" Mode: MANUAL");
      break;

    case 2 :
      selectLineTwo();
      Serial.print("                ");    
      selectLineTwo();
      if (pumpMode == OFF) {
        Serial.print(" Pump: OFF");
      }
      if (pumpMode == ON) {
        Serial.print(" Pump: ON");
      }
      break;

    case 3 :
      selectLineOne();
      Serial.print("                ");
      selectLineTwo();
      Serial.print("                ");    
      selectLineOne();
      Serial.print(" Test: ");
      Serial.print(testNo);
      Serial.print("/");
      Serial.print(maxTests);
      selectLineTwo();
      Serial.print("Run time: ");
      Serial.print(pumpRunTime);
      Serial.print(" s");
      break;

    case 4 :
      selectLineOne();
      Serial.print("                ");
      selectLineOne();
      Serial.print(" Test: ");
      Serial.print(testNo);
      Serial.print(" Run: ");
      Serial.print(runNo);
      break;

    case 5 :
      selectLineOne();
      Serial.print("                ");
      selectLineTwo();
      Serial.print("                ");
      selectLineOne();
      Serial.print(" Process done");
      selectLineTwo();
      Serial.print("Switch to AUTO");
      break;

    case 6 :
      selectLineOne();
      Serial.print("                ");
      selectLineTwo();
      Serial.print("                ");
      selectLineOne();
      Serial.print(" Cancelled");
      break;

    case 7 :
      selectLineOne();
      Serial.print("                ");
      selectLineTwo();
      Serial.print("                ");
      selectLineOne();
      Serial.print(" Test done");
      selectLineTwo();
      Serial.print("Pump ON for next");
      break;  
    }
  }
} // updateDisplay


void testProcess()
{
  if (stillTesting) {
    turnOnPump();
    updateDisplay(4); 
  }
} // testProcess  


void turnOnPump()
// 
{
  if (pumpRelay.getState() == LOW) {
    pumpRelay.turnOn();
    pumpMode = ON;
    // Create timer event to stop pump
    relayOffEvent = t.after(pumpRunTime * 1000, turnOffPump, 0);
    updateDisplay(2);
    // return to testProcess
  }   
} // turnOnPump


void turnOffPump(void* context)
// Turn off pump relay
{
  // Turn off the relay
  pumpRelay.turnOff();
  pumpMode = OFF;
  updateDisplay(2);
  delay(10);
  if (runNo < RUNS_PER_TIME) alarm1();

  long interval;  // interval between runs
  if (runNo == RUNS_PER_TIME) {
    // This is the last run in the test
    alarm2();  
    nextRun(0); 
  }
  else {
    interval = TIME_BETWEEN_RUNS * 1000L;
    // Create one shot event to provide interval between tests
    intervalEvent = t.after(interval, nextRun, 0);
  }
} // turnOffPump


void nextRun(void* context)
{  
  ++runNo;
  if (runNo > RUNS_PER_TIME) {
    if (testNo < maxTests) {
      waitingForNextTest = true;
      psBlocked = false;
      updateDisplay(7);
      delay(10);
      return;
    } 
    else {
      stillTesting = false;
    }
  }

  if (stillTesting) {  
    testProcess();
  } 
  else {
    endProcess();
  }
} // nextRun


void nextTest(void)
{
  ++testNo;
  if (testNo <= maxTests) {
    runNo = 1;
    if (testNo == 1) pumpRunTime = MIN_RUN_TIME;
    if (testNo > 1) pumpRunTime += RUN_TIME_INC;
    stillTesting = true;
  } 
  else {
    stillTesting = false;
  }
} // nextTest


void endProcess()
{
  // Make sure pump is off
  pumpRelay.turnOff();
  pumpMode = OFF;
  updateDisplay(5);
  delay(10);
} // endProcess


void readAllSwitches(void* context)
// Only the mode and pump switches are used
{
  readModeSwitch();
  if ((ftt_ms) || (ms_stateChanged)) {
    if (ms_state == HIGH) {
      atoMode = AUTO;
      psBlocked = true;
      if (!stillTesting) {
        updateDisplay(1);
        updateDisplay(2);
      }
      if (stillTesting) { // Cancel the calibration process
        // Turn off pump, in case it's still running
        pumpRelay.turnOff();
        stillTesting = false;
        // Kill the intervalEvent
        t.stop(intervalEvent);
        // After 3 seconds showing cancel message, update display
        delay3sEvent = t.after(3000L, resumeRegDisplay, 0);
        updateDisplay(6);
      }
    }
    if (ms_state == LOW) {
      atoMode = MANUAL;
      psBlocked = false;
      testNo = 1;
      runNo = 1;
      pumpRunTime = MIN_RUN_TIME;
      updateDisplay(3);
    }
    ftt_ms = false;
  }

  if (!psBlocked) {
    readPumpSwitch();
    if ((ftt_ps) || (ps_stateChanged)) {
      if (ps_state == HIGH) {
        pumpMode = OFF;
      }
      if (ps_state == LOW) {
        pumpMode = ON;
        stillTesting = true; // Start the calibration process
        psBlocked = true;
        if (waitingForNextTest) {
          waitingForNextTest = false;
          psBlocked = false;
          nextTest();
          updateDisplay(3);
        } 
        else {
          testProcess();
        }
      }
    }
    ftt_ps = false;
  }
} // readAllSwitches


void readModeSwitch()
{
  ms_stateChanged = modeSwitch.update();
  if ((ms_stateChanged) || (ftt_ms)) {
    ms_state = modeSwitch.read();
  }
} // readModeSwitch


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


void resumeRegDisplay(void* context)
{
  updateDisplay(1);
  updateDisplay(2);
}


void alarm1(void)
{
  t.pulseImmediate(alarm_pin, 100, HIGH);
}


void alarm2(void)
{
  t.oscillate(alarm_pin, 300, LOW, 3);
} // alarm2


void tick(void* context)
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


void setup()
{
  // Start the ADM1602U serial interface
  Serial.begin(9600);

  // Configure digital inputs
  // Mode switch
  pinMode(modeSwitch_pin, INPUT);      // set pin to input
  digitalWrite(modeSwitch_pin, HIGH);  // turn on pullup resistor
  modeSwitch.attach(modeSwitch_pin);   // attach modeSwitch to pin
  modeSwitch.interval(5);

  // Dosing pump switch
  pinMode(pumpSwitch_pin, INPUT);      // set pin to input
  digitalWrite(pumpSwitch_pin, HIGH);  // turn on pullup resistor
  pumpSwitch.attach(pumpSwitch_pin);   // attach pumpSwitch to pin
  pumpSwitch.interval(5);

  // Level 1 switch
  pinMode(level1Switch_pin, INPUT);        // set pin to input
  digitalWrite(level1Switch_pin, HIGH);    // turn on pullup resistor
  level1Switch.attach(level1Switch_pin);   // attach level1Switch to pin
  // ******** Proper interval will need to be determined for level switch  
  level1Switch.interval(5);
  // ***************************
  
  // Level 2 switch
  pinMode(level2Switch_pin, INPUT);        // set pin to input
  digitalWrite(level2Switch_pin, HIGH);    // turn on pullup resistor
  level2Switch.attach(level2Switch_pin);   // attach level2Switch to pin
  // ******** Proper interval will need to be determined for level switch  
  level2Switch.interval(5);
  // ***************************

  // Configure digital outputs
  pinMode(ATOpumpRelay_pin, OUTPUT);            // set pin to output
  ATOpumpRelay.attach(ATOpumpRelay_pin);        // attach pumpRelay to pin
  pinMode(DOSINGpumpRelay_pin, OUTPUT);         // set pin to output
  DOSINGpumpRelay.attach(DOSINGpumpRelay_pin);  // attach pumpRelay to pin
  pinMode(statLED_pin, OUTPUT);           // setup statLED
  digitalWrite(statLED_pin, HIGH);        // turn on LED
  pinMode(alarm_pin, OUTPUT);             // set alarm pin to output
  digitalWrite(alarm_pin, LOW);           // start with the alarm off  

  clearLCD();
  backlightOn();
  displayOn();

  displaySplashScreen();

  // Setup timer recurring events
  tickEvent = t.every(200L, tick, 0);
  readAllSwitchesEvent = t.every(250L, readAllSwitches, 0);  
  heartbeatEvent = t.every(500L, flashStatLED, 0);

  // Setup timer one shot events
  lcdAllowEvent = t.after(3000L, enableLCDupdate, 0);
} // setup


void loop()
{
  while (atoMode > -1) {
    // Update timer
    t.update();
  }
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


















































