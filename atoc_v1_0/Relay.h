#ifndef Relay_h
#define Relay_h

#include <inttypes.h>

class Relay
{

public:
  // Create an instance of relay
  Relay(); 
  // Attach to a pin (and also sets initial state)
  void attach(int pin);  
  // Turns on the relay
  bool turnOn();
  // Turns off the relay
  bool turnOff();
  
protected:
  // Returns the updated pin state
  uint8_t read();
  uint8_t state;
  uint8_t pin;
};

#endif
