README.txt

Ideas for operation of ATO controller FSM
The Controller is essentially a Sensor->Controller->Actuator feedback loop.

When the level switch or pump switch changes state, the relay turns on
t_on = millis();
t_off = t_on + pumpRunTime
end


test if its time to turn off relay
if (millis() - t_off > 0) pumpRelay.turnOff()