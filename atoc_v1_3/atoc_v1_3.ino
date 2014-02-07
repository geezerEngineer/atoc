/* 
 * atoc_v1_3.ino
 * Mike Lussier - Jan 22, 2014
 * Compiled with Arduino 1.0.5
 *****************************
 */
#define INO_NAME "ATOC"
#define INO_VERSION "1.3.b"

/* 
 * .b - Delays starting of pump by 2 seconds when water level goes low.
 * .a - Revamps main and ato functions as per activity diagrams. Adds debug 
 * switch to write process messages to console.
 ****** 
 * v1.3 - Integrates level switch, ato pump relay, dosing pump relay, mode and 
 * relay switches, alarm and serial LCD (2*16).
 ***************************************** 
 */
 
#include <Relay.h>
#include <Bounce.h>
#include <Timer.h>

#define DEBUG 0                     // 0 = debug off, 1 = debug to console

static uint8_t alarm_pin            = 14;    // Piezo alarm (A0)
static uint8_t levelSwitch_pin      = 8;     // Level switch (was D4, but 4 is not working)
static uint8_t modeSwitch_pin       = 2;     // Mode switch
static uint8_t pumpSwitch_pin       = 3;     // Pump switch
static uint8_t atoPumpRelay_pin     = 5;     // Relay for ATO pump
static uint8_t dosPumpRelay_pin     = 6;     // Relay for dosing pump
static uint8_t statLED_pin          = 13;    // Status LED

// From ATO Calculations
#define RESERVOIR_VOL 3500                // Volume, when full, of supply water reservoir, mL
#define PUMP_RATE 5.0                     // Calibrated ato pump flow rate, mL/s
#define VOL_CHANGE 618.1                  // Hysteresis volume change for nano float switch, mL
// The level switch (float type) is affected by ripples caused by inflow of water from the ato 
// pump. The switch is inhibited from being read until the ripples have dissipated. 
#define INFLOW_DISSIPATION_TIME 10L       // Time required for inflow disturbance to subside, s
#define MAX_PUMP_RUN_CYCLES 8             // Maximum no of pump run cycles before error, n

// Instantiate debouncer objects for level 1 switch, mode & pump switches
Bounce levelSwitch = Bounce();
Bounce modeSwitch = Bounce();
Bounce pumpSwitch = Bounce();

// Instantiate relay objects for ato and dosing pumps
Relay atoPumpRelay = Relay();
Relay dosPumpRelay = Relay();

// Instantiate timer object for one-shot and recurring events
Timer t = Timer();

// Define timed events
// - recurring
uint8_t mainProcessEvent;              // Call mainProcess, 4 Hz
uint8_t heartbeatEvent;                // Flash status LED, flash lcd heartbeat, 2 Hz
// - one shot
uint8_t stopPumpEvent;                 // Turn off pump relay 
uint8_t unblockSwitchEvent;            // Turn off level switch read inhibit
uint8_t unblockModeEvent;              // Turn off mainProcess block
uint8_t unblockAtoEvent;               // Turn off atoProcess block
uint8_t clrMessageEvent;               // Clear LCD message (Line 2)
uint8_t alarmEvent;                    // Sound alarm to indicate fault

// Other globals
// - process mode
const static int ATO = 0;
const static int DOSING = 1;
const static int FAULT = -1;
const static int BLOCKED = -2;
int mode = BLOCKED;               // Initial main process mode is BLOCKED

// - pump mode
const static int OFF = 0;
const static int ON = 1;
int pumpMode = OFF;

// - water level
const static int BELOW_LEVEL = 0;
const static int AT_LEVEL = 1;
int waterLevel = AT_LEVEL;

int curPumpCycle = 0;                    // Current pump cycle (out of MAX_PUMP_RUN_CYCLES), n
int freshWaterVol = RESERVOIR_VOL;       // Initial fresh water volume, mL

// pumpCycleTime = 1/5 time to refill aquarium to nominal level at pump rate, ms
const int pumpCycleTime = (int) (VOL_CHANGE / PUMP_RATE * 200.0);
// pumpCycleVol = vol dispensed during 1 pump cycle, mL
const int pumpCycleVol = (int) (PUMP_RATE * (float) pumpCycleTime / 1000.0);

// Process blocks
bool atoBlocked = false;                // Blocks execution of part of atoProcess

// From GDM1602K LCD character set
const char ptr = 126;                   // Pointer symbol
const char spc = 32;                    // Space

// Other
bool ftt_fault = true;
String s1, s2;                          // Utility strings


// Functions triggered by recurring events
void mainProcess(void* context)
{
  // Is mode blocked?
  if (mode == BLOCKED) return; // yes
  // no
  // Has a fault occurred?
  if (mode == FAULT) { // yes
    faultProcess();
    return;
  } 
  else // no, mode is not fault
  {
    atoProcess();
    if (mode == FAULT) { // yes
      faultProcess();
      return;
    } 
    else { // no, mode is not fault
      // read modeSwitch
      bool changed = modeSwitch.update();
      int value = modeSwitch.read();
      if ((changed) && (value == LOW)) {
        mode = DOSING;
        s1 = "Mode: DOSING";
        wd(&s1, 1);
      }
      if ((changed) && (value == HIGH)) {
        mode = ATO;
        s1 = "Mode: ATO";
        wd(&s1, 1);
        clrLine(2);
      }
      if (mode == DOSING) dosingProcess();
      sumpLightProcess(); 
      return;
    }
  } 
} // mainProcess


void flashStatLED(void* context)
// Flash status LED & show heartbeat on LCD, 2 Hz
{
  bool LEDstate = digitalRead(statLED_pin);
  digitalWrite(statLED_pin, !LEDstate);

#if DEBUG == 0
  // Show synchronized heartbeat on LCD  
  goToPosn(0);
  if (!LEDstate) {
    Serial.print(ptr);
  } 
  else {
    Serial.print(spc);
  }
#endif  
} // flashStatLED


// Functions
void atoProcess(void)
{
  if (levelSwitch.isBlocked) {
    return;
  } 
  else {
    bool changed = levelSwitch.update();
    int value = levelSwitch.read();
    if (value == LOW) waterLevel = BELOW_LEVEL;
    if (value == HIGH) waterLevel = AT_LEVEL;

    if (waterLevel == BELOW_LEVEL) { // yes
      // If first time since waterLevel change then
      // delay 2 seconds before proceeding
      if (changed) {
        atoBlocked = true;
        unblockAtoEvent = t.after(2000L, unblockAto, 0);
#if DEBUG == 0
        s2 = " Water: LOW";
        wd(&s2, 2);
#else
        Serial.println("Water level is LOW.");
#endif  
      }
      // Is pump running?
      if (atoPumpRelay.getState() == HIGH) { // yes
#if DEBUG == 1      
        Serial.println("Pump is running.");
#endif        
        return;
      } 
      else { // no, pump is not running
#if DEBUG == 1
        Serial.println("Pump is stopped.");
#endif
        if (!atoBlocked) { // this part of ato is blocked for a few seconds
          ++curPumpCycle;
          // Is there a fault?
          if ((curPumpCycle > MAX_PUMP_RUN_CYCLES) || (freshWaterVol < pumpCycleVol)) { // yes
#if DEBUG == 1
            Serial.println("Fault, so will not start pump.");
#endif          
            mode = FAULT;
            return;
          } 
          else { // no, there is no fault
#if DEBUG == 1        
            Serial.println("No fault, so starting pump.");
#endif
            atoPumpRelay.turnOn();
#if DEBUG == 0
            s2 = "ATO Pump: ON";
            wd(&s2, 2);
#else
            Serial.print("Pump is on. Cycle = ");
            Serial.print(curPumpCycle);
            Serial.print("  Remaining vol = ");
            Serial.print(freshWaterVol);
            Serial.println(" mL");
#endif          
            levelSwitch.isBlocked = true;
#if DEBUG == 1
            Serial.println("Blocking level switch.");
#endif
            stopPumpEvent = t.after(pumpCycleTime, stopPump, 0);
            freshWaterVol -= pumpCycleVol;
          }
          return;
        }
      }
    } 
    else { // no, waterLevel is not LOW
#if DEBUG == 0
      if (changed) {
        s2 = " Water: OK";
        wd(&s2, 2);
        clrMessageEvent=t.after(4000L, clrMessage, (void*)2);    
#else
        Serial.println("Water level is OK.");
#endif
      }
      curPumpCycle = 0;
      // Is pump stopped?
      if (atoPumpRelay.getState() == LOW) { // yes
#if DEBUG == 1
        Serial.println("Pump is stopped.");
#endif
        return;
      } 
      else { // no, pump is not stopped
#if DEBUG == 1
        Serial.print("Pump is running...");
#endif
        atoPumpRelay.turnOff();
#if DEBUG == 1
        Serial.println("Immediately stopping pump.");
#endif
        t.stop(stopPumpEvent);
        unblockSwitchEvent = t.after(INFLOW_DISSIPATION_TIME * 1000L, unblockLevelSwitch, 0);
        return;
      }
    }
  }
} // atoProcess


void dosingProcess(void)
{
#if DEBUG == 0
  // Read pump switch
  bool changed = pumpSwitch.update();
  int value = pumpSwitch.read();
  // Is pump switch on?
  if ((changed) && (value == LOW)) { // yes
    if (dosPumpRelay.getState() != HIGH) { 
      dosPumpRelay.turnOn();
      s2 = "DOS Pump: ON";
    }
  }
  if ((changed) && (value == HIGH)) { // no
    if (dosPumpRelay.getState() != LOW) { 
      dosPumpRelay.turnOff();
      s2 = "DOS Pump: OFF";
    }
  }
  if (changed) {
    s1 = "Mode: DOSING";
    wd(&s1, 1);
    wd(&s2, 2);
  }
#else
  Serial.println("In dosingProcess.");
#endif
  return;
} // dosingProcess


void sumpLightProcess(void)
{
  return;
} // sumpLightProcess


void faultProcess(void)
{
  if (ftt_fault) { // yes, first time through
    // Unconditionally turn off pump
    atoPumpRelay.turnOff();

#if DEBUG == 0 
    // Send message to display
    s1 = " ** FAULT **";
    wd(&s1, 1);
    if (freshWaterVol < pumpCycleVol) {
      s2 = "Replenish water";
      wd(&s2, 2); 
      // Reset water vol
      freshWaterVol = RESERVOIR_VOL;
    } 
    else {
      s2 = "Bad level switch";
      wd(&s2, 2);
      // Reset pumpCycle 
      curPumpCycle = 0;  
    }
#else 
    Serial.println("In faultProcess.");
#endif   
    ftt_fault = false;
    // Sound alarm repeatedly every 7 seconds, until stopped
    alarm(0);
    alarmEvent = t.every(7000L, alarm, 0);
  }

  // Fault recovery - recover when pump switch is pressed while in ATO mode
  // Read mode switch
  modeSwitch.update();
  int value = modeSwitch.read();
  // Is ATO mode selected?
  if (value == HIGH) { // yes
    // Read pump switch
    bool changed = pumpSwitch.update();
    value = pumpSwitch.read();
    // Is pump switch on?
    if ((changed) && (value == LOW)) { // yes
      // Recovering...
      mode = ATO;
      ftt_fault = true;
      // Turn off alarm
      t.stop(alarmEvent);
      // Assume fault has been corrected
      // Was fault caused by low fresh water supply?
      if (freshWaterVol < pumpCycleVol) { // yes
# if DEBUG == 1
        Serial.println(" - low fresh water volume.");
#endif       
        // Reset water vol
        freshWaterVol = RESERVOIR_VOL;
      } 
      else { // no, fault was caused by bad level switch
# if DEBUG == 1
        Serial.println(" - bad level switch.");
#endif      
        // Reset curPumpCycle
        curPumpCycle = 0;  
      }
# if DEBUG == 0
      clearLCD();
      s1 = " Fault cleared";
      wd(&s1, 1);
      mode = BLOCKED;
      unblockModeEvent = t.after(2000L, unblockMode, 0);
#else
      Serial.println("Recovered from fault.");
#endif      
    }
  }

  return;
} // faultProcess


// Functions triggered by one shot events
void unblockMode(void* context)
{
  mode = ATO;
#if DEBUG == 0  
  s1 = "Mode: ATO";
  clrLine(2);
  wd(&s1, 1);
#endif
} // unblockMode

void unblockAto(void* context)
{
  atoBlocked = false;
} // unblockAto


void stopPump(void* context)
{
  atoPumpRelay.turnOff();
  unblockSwitchEvent = t.after(INFLOW_DISSIPATION_TIME * 1000L, unblockLevelSwitch, 0);
#if DEBUG == 0
  s2 = "ATO Pump: OFF";
  wd(&s2, 2);
#else
  Serial.println("Turning off pump.");
#endif  
} // stopPump


void unblockLevelSwitch(void* context)
{
  // Read level switch
  bool changed = levelSwitch.update();
  int value = levelSwitch.read();
  if (value == LOW) waterLevel = BELOW_LEVEL;
  if (value == HIGH) waterLevel = AT_LEVEL;
  // Now, unblock switch
  levelSwitch.isBlocked = false;
#if DEBUG == 1
  Serial.println("Unblocking level switch.");
#endif  
} // unblockLevelSwitch


void alarm(void* context)
{
  //clock.getTime();
  // Sound alarm if time is between 06h and 23h
  //if (6 < clock.hour < 23) {
  t.oscillate(alarm_pin, 300, LOW, 2);
  //}
} // alarm


void wd(String* msg, int line)
{
  if (line == 1) {
    clrLine(1);
    goToPosn(1);
  }
  if (line == 2) {
    clrLine(2);
    goToPosn(16);
  }
  Serial.print(*msg);
} // wd


// **********
void setup(void)
{
  Serial.begin(9600);

#if DEBUG == 0
  mode = BLOCKED;
  clearLCD();
  backlightOn();
  displayOn();

  // Display splash screen for 4 seconds
  s1 = " ATOC v"; 
  s1.concat(INO_VERSION);
  s2 = " Water matters";
  wd(&s1,1);
  wd(&s2,2);
  unblockModeEvent = t.after(4000L, unblockMode, 0);
#else  
  Serial.println();
  Serial.println();
  Serial.print("ATOC v");
  Serial.println(INO_VERSION);
  mode = ATO;
#endif  

  // Configure digital inputs
  // Mode switch
  pinMode(modeSwitch_pin, INPUT);          // Set pin to input
  digitalWrite(modeSwitch_pin, HIGH);      // Turn on pullup resistor
  modeSwitch.attach(modeSwitch_pin);       // Attach modeSwitch to pin
  modeSwitch.interval(5);

  // Pump switch
  pinMode(pumpSwitch_pin, INPUT);          // Set pin to input
  digitalWrite(pumpSwitch_pin, HIGH);      // Turn on pullup resistor
  pumpSwitch.attach(pumpSwitch_pin);       // Attach pumpSwitch to pin
  pumpSwitch.interval(15);                 // Longer interval needed for this one

  // Level switch
  pinMode(levelSwitch_pin, INPUT);        // Set pin to input
  digitalWrite(levelSwitch_pin, HIGH);    // Turn on pullup resistor
  levelSwitch.attach(levelSwitch_pin);    // Attach level1Switch to pin  
  levelSwitch.interval(5);
  levelSwitch.isBlocked = false;          //  

  // Configure digital outputs
  // Pump relay
  pinMode(atoPumpRelay_pin, OUTPUT);         // Set pin to output
  atoPumpRelay.attach(atoPumpRelay_pin);        // Attach atoPumpRelay to pin

    // Light relay
  pinMode(dosPumpRelay_pin, OUTPUT);        // Set pin to output
  dosPumpRelay.attach(dosPumpRelay_pin);  // Attach dosPumpRelay to pin

    // Status LED
  pinMode(statLED_pin, OUTPUT);           // Setup statLED
  digitalWrite(statLED_pin, HIGH);        // Turn on LED

  // Alarm
  pinMode(alarm_pin, OUTPUT);             // Set alarm pin to output
  digitalWrite(alarm_pin, LOW);           // Start with the alarm off

  // Setup timer recurring events
  mainProcessEvent = t.every(250L, mainProcess, 0);  
  heartbeatEvent = t.every(500L, flashStatLED, 0); 

} // setup


void loop(void)
{
  // Update timer
  t.update();
} // loop





////////////////////////////////////////////////////////
/////////////////////// GDM1602K ///////////////////////
////////////////////////////////////////////////////////

//////////// Display Functions ////////////////

void clrLine(uint8_t lineNo)
// Clears specified line of LCD display
{
  if (lineNo == 1) selectLineOne();
  if (lineNo == 2) selectLineTwo();
  Serial.print("                ");
} // clrLine

void clrMessage(void* context)
// Called by timer event
{
  int lineNo = (int) context;
  if (lineNo == 1) clrLine(1);
  if (lineNo == 2) clrLine(2);
} // clrMessage

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




















