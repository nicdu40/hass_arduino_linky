/***************************** Includes *******************************/
#include <string.h>
#include <Streaming.h>
#include "LinkyHistTIC.h"
#include <Wire.h>
# define NANO_Master 11



/****************************** Defines *******************************/
String Send_String;
/****************************** Constants *****************************/
const uint8_t pin_LkyRx = 10;
LinkyHistTIC Linky(pin_LkyRx);
char H = 'P';
/******************************  Setup  *******************************/
void setup()
{
/* Initialise serial link */
  Serial.begin(9600);
  /* Initialise the Linky receiver */
  Linky.Init();

  Wire.begin();
  Serial.begin(9600);
  Wire.beginTransmission(NANO_Master);
  Wire.write(12);
  Wire.endTransmission();

}

/******************************* Loop *********************************/
void loop()
{
 // uint8_t i;

  Linky.Update();

  if (Linky.pappIsNew()) {
    Send_String = String(Linky.papp() / 10) + H;
    Serial << Send_String << endl;
    Wire.beginTransmission(NANO_Master);
    Wire.print(Send_String);
    Wire.endTransmission();
  }

#ifdef LKY_HPHC
  if (Linky.ptecIsNew()) {
    if (Linky.ptec() == Linky.C_HPleines)
      H = 'P';
    else
      H = 'C';
  }
#endif

#ifdef LKY_IMono
  if (Linky.iinstIsNew())  {
Send_String = String(Linky.iinst()) + 'A' ;
    Serial << Send_String << endl;
    Wire.beginTransmission(NANO_Master);
    Wire.print(Send_String);
    Wire.endTransmission();
 }
#endif

}
