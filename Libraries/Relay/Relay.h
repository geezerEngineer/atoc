/*  * * * * * * * * * * * * * * * * * * * * * * * * * * *
 Code by Mike Lussier
* * * * * * * * * * * * * * * * * * * * * * * * * * * * */

#ifndef Relay_h
#define Relay_h

#include <inttypes.h>

class Relay
{

public:
  // Constructor
  Relay(void);
   
  // Attach relay to a pin (and also turns off relay)
  void attach(uint8_t pin);
    
  // Turn on relay
  bool turnOn();
  
  // Turn off relay
  bool turnOff();
  
  // Return updated relay state
  uint8_t getState();
  
protected:
  uint8_t state;
  uint8_t pin;
};

#endif
