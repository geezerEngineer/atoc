#ifndef Relay_h
#define Relay_h

#include <inttypes.h>

class Relay
{

public:
  // Create an instance of relay
  Relay(); 
  // Attach to a pin (and also turns off relay)
  void attach(int pin);  
  // Turn on relay
  bool turnOn();
  // Turn off relay
  bool turnOff();
  
protected:
  // Return updated pin state
  uint8_t getState();
  uint8_t state;
  uint8_t pin;
};

#endif
