/*  * * * * * * * * * * * * * * * * * * * * * * * * * * *
 Original C-code by
 Changed to C++ by Mike Lussier
* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef serLCD_h
#define serLCD_h

#include <inttypes.h>
#include "Print.h"

class serLCD : public Print {
private:
    // Initiates a function command to the display
    void serCommand();
 
public:
    void begin();
   
    // Starts the cursor at the beginning of the first line
    void selectLineOne();

    // Starts the cursor at the beginning of the second line
    void selectLineTwo();

    // Sets the cursor to the given position
    // line 1: 0-15, line 2: 16-31, >31 defaults back to 0
    void goToPosn(int position); 

    // Resets the display, undoing any scroll and removing all text
    void clearLCD();

    // Clears specified line 
    void clrLine(uint8_t lineNo);
    
    // Turns on the backlight
    void backlightOn(); 

    // Turns off the backlight
    void backlightOff(); 

    // Turns on visual display
    void displayOn(); 

    // Turns off visual display
    void displayOff();
    
    virtual size_t write(uint8_t);
    void command(uint8_t);
  
    using Print::write;  
};

#endif
