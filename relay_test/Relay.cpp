/*
 * Relay.cpp
 */

#if defined(ARDUINO) && ARDUINO >= 100
#include "Arduino.h"
#else
#include "WProgram.h"
#endif
#include "Relay.h"

Relay::Relay() {
}

void Relay::attach(int pin) {
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
  getState() == HIGH ? return 0 : return 1 ;
}

bool Relay::turnOff()
{
  digitalWrite(pin, LOW);
  delay(4);
  getState() == LOW ? return 0 : return 1 ;
}


