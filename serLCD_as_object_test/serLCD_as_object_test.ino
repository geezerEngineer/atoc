#include <SoftwareSerial.h>

#define TX_PIN 2
#define LCD_DELAY 10 // Was 10

SoftwareSerial lcd = SoftwareSerial(0, TX_PIN);

// Goto row & column of lcd
void gotoPosn(int row, int col) 
{
  lcd.write(0xFE);   //command flag
  lcd.write((col + row*64 + 128));    //position 
  delay(LCD_DELAY);
}

void clearLCD()
{
  lcd.write(0xFE);   //command flag
  lcd.write(0x01);   //clear command.
  delay(LCD_DELAY);
}

void backlightOn() 
{  // turns on the backlight
  lcd.write(0x7C);   //command flag for backlight stuff
  lcd.write(157);    //light level.
  delay(LCD_DELAY);
}

void backlightOff()
{  // turns off the backlight
  lcd.write(0x7C);   //command flag for backlight stuff
  lcd.write(128);     //light level for off.
   delay(LCD_DELAY);
}

void serCommand()
{ // general function to call the command flag for issuing all other commands   
  lcd.write(0xFE);
}

void setup()
{
  pinMode(TX_PIN, OUTPUT);
  lcd.begin(9600);
  clearLCD();
  gotoPosn(0,0);
  lcd.print("Hello world!");
}

void loop()
{
}
