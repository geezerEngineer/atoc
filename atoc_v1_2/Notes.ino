/*
atoc_v1_2 Hardware Configuration:
 Alarm piezo buzzer
  - connected between pin D6 and Gnd.
 Level 1 switch
  - connected between pin D7 and Gnd. Uses 20kΩ internal pullup.
 Mode switch
  - connected between pin D8 and Gnd. Uses 20kΩ internal pullup. 
 Dosing pump switch 
  - connected between pin D9 and Gnd. Uses 20kΩ internal pullup.
 ATO pump relay 
  - SIG connected to pin D10, 
  - VCC connected to 5V and 
  - GND connected to Gnd.
 Dosing pump relay
  - SIG connected to pin D11, 
  - VCC connected to 5V and 
  - GND connected to Gnd.
 Serial LCD Panel
  - RX connected to Arduino TX,
  - VCC connected to 5V, and
  - GND connected to Gnd.
Note, the above configuration will change to accommodate the I2C LCD panel. 
 *
Theory of Operation: 
In DOSING mode - When the dosing pump switch is idle, the switch reads HIGH. When  
pressed the switch reads LOW. The dosing pump relay turns ON when the switch becomes  
stable LOW and turns OFF when the switch goes stable HIGH. The pump will not turn ON 
if the mode switch is HIGH (ATO mode).
 *
In ATO mode - When the level float drops to within .12” (±.04”) of the bottom clip the  
internal reed contacts draw together, completing the circuit and causing 
the level switch pin to go LOW. A stable LOW value triggers the ato pump relay to turn ON 
and starts a pumpStart callback timer. When the float rises .20” (±.04”) from the 
bottom clip, the contacts separate, opening the circuit and causing the pin to go HIGH. 
A stable HIGH value turns the pump relay OFF. The pump relay will also turn OFF when the 
pumpStop callback occurs.
*/
