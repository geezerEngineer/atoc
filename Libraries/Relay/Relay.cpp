/*
 * Relay.cpp
 */

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include "Relay.h"

Relay::Relay(void) {
}

void Relay::attach(uint8_t pin) {
  this->pin = pin;
  turnOff();
}

uint8_t Relay::getState()
{
  state = digitalRead(pin);
  return state;
}

bool Relay::turnOn()
{
  digitalWrite(pin, HIGH);
  delay(4);
  getState();
  if (state == HIGH) {
    return 0; 
  } 
  else {
    return 1;
  }
}

bool Relay::turnOff()
{
  digitalWrite(pin, LOW);
  delay(4);
  getState();
  if (state == LOW) {
    return 0; 
  } 
  else {
    return 1;
  }
}



