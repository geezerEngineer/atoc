#include "serLCD.h"

#include <stdio.h>
#include <string.h>
#include <inttypes.h>
#include "Arduino.h"

void serLCD::begin()
{
	Serial.begin(9600);
}

// Starts the cursor at the beginning of the first line (convenient method for goTo(0))
void serLCD::selectLineOne() 
{  //puts the cursor at line 1 char 0.
  Serial.write(0xFE);   //command flag
  Serial.write(128);    //position
  delay(10);
}

// Starts the cursor at the beginning of the second line (convenient method for goTo(16))
void serLCD::selectLineTwo() 
{  //puts the cursor at line 2 char 0.
  Serial.write(0xFE);   //command flag
  Serial.write(192);    //position
  delay(10);
}

// Sets the cursor to the given position
// line 1: 0-15, line 2: 16-31, >31 defaults back to 0
void serLCD::goToPosn(int position) 
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
void serLCD::clearLCD()
{
  Serial.write(0xFE);
  Serial.write(0x01);
  delay(10);
}

// Clears specified line
void clrLine(uint8_t lineNo)
{
  if (lineNo == 1) {
  Serial.write(0xFE);   //command flag
  Serial.write(128);    //position
  delay(10);  
    };
  if (lineNo == 2) {
  Serial.write(0xFE);   //command flag
  Serial.write(192);    //position
  delay(10);
    };
  Serial.print("                ");
  delay(10);
}

// Turns on the backlight
void serLCD::backlightOn() 
{
  Serial.write(0x7C);
  Serial.write(157);   //light level
  delay(10);
}

// Turns off the backlight
void serLCD::backlightOff() 
{
  Serial.write(0x7C);
  Serial.write(128);   //light level
  delay(10);
}

// Turns on visual display
void serLCD::displayOn() 
{
  Serial.write(0xFE);
  Serial.write(0x0C);
  delay(10);
}

// Turns off visual display
void serLCD::displayOff() 
{
  Serial.write(0xFE);
  Serial.write(0x08);
  delay(10);
}

// Initiates a function command to the display
void serLCD::serCommand() 
{
  Serial.write(0xFE);
}