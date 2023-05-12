/***********************************************************************
                        Récepteur TIC Linky
                        Mode historique

V03  : External SoftwareSerial. Tested OK on 07/03/18.
V04  : Replaced available() by new(). Tested Ok on 08/03/18.
V05  : Internal SoftwareSerial. Cf special construction syntax.
V06  : Separate compilation version, single and tri-phase.
V07  : modified for "tempo" tarif.

***********************************************************************/

/***************************** Includes *******************************/
#include <string.h>
#include <Streaming.h>
#include "LinkyHistTIC_tpo.h"
#include <Wire.h>


/****************************** Defines *******************************/

#define NANO_Master 11

/****************************** Constants *****************************/
const uint8_t pin_LkyRx = 10;
//const uint8_t pin_LkyTx = 11;   /* !!! Not used but reserved !!!
//                                  * Do not use for anything else */

/************************* Global variables ***************************/

char T = 'P'; //tarif
String Send_String;

uint32_t diff[6] ;
uint32_t Linky_Index_old[6];
/************************* Object instanciation ***********************/
LinkyHistTIC_tpo Linky(pin_LkyRx);

/****************************  Routines  ******************************/




/******************************  Setup  *******************************/
void setup()
  {

  /* Initialise serial link */
  Serial.begin(9600);
  Wire.begin();
  /* Initialise the Linky receiver */
  Linky.Init();
  Wire.beginTransmission(NANO_Master);
  Wire.write(12);
  Wire.endTransmission();  

 // Serial << F("Bonjour") << endl;
  }


/******************************* Loop *********************************/
void loop()
  {
  uint8_t i;
  Linky.Update();


  for (i = Linky.hcjb; i <= Linky.hpjr; i++)//hcjb, hpjb, hcjw, hpjw,hcjr, hpjr
  
    {
    if (Linky.IndexIsNew(i))
      {/*
            
      Serial << F("Index ") << i << F(" = ") << \
                Linky.Index(i) << F(" Wh") << endl;
      Serial << F("0 : HC j bleu, 1 : HP j bleu, ") \
             << F("2 : HC j blanc,") << endl;
      Serial << F("3 : HP j blanc, 4 : HC j rouge, ") \
             << F("5 : HP j rouge.") << endl;
      */
      //Serial << F("Index ") << i << F(" = ") << Linky.Index(i) << F(" Wh") << endl;
     
      diff[i]= Linky.Index(i)-Linky_Index_old[i];
      //conso=(diff)*3600000/(time_new-time_old);
      //Serial << F("diff index ") << i << F(" = ") << diff[i]  << endl; 
      //Serial << F("time_diff ") << i << F(" = ") << time_new-time_old  << endl;
      
      //time_old=time_new; 
      if (millis()/1000>10){
        Send_String = String(diff[i]) + 'I';//index
        Serial << Send_String << endl;
        Wire.beginTransmission(NANO_Master);
        Wire.print(Send_String);
        Wire.endTransmission();
      }
      Linky_Index_old[i]=Linky.Index(i);
         
      //Serial << millis()/1000 << endl;   
      //Serial << conso << endl;
      
      if (i == 0)
      T='L';  
      else if (i == 1)
      T='M';
      else if (i == 2)
      T='N';
      else if (i == 3)
      T='O';
      else if (i == 4)
      T='P';        
      else if (i == 5)
      T='Q';     
    }

      
      
  }

  if (Linky.PetcIsNew())
    {
    if (Linky.Petc() == 0)
    T='L';  
    else if (Linky.Petc() == 1)
    T='M';
    else if (Linky.Petc() == 2)
    T='N';
    else if (Linky.Petc() == 3)
    T='O';
    else if (Linky.Petc() == 4)
    T='P';        
    else if (Linky.Petc() == 5)
    T='Q';     
    }

  /*if (Linky.DemainIsNew())
    {
    Serial << F("Demain, jour = ") << Linky.Demain() << endl;
    Serial << F("0 : jour bleu, 1 : jour blanc, "\
                "2 : jour rouge.") << endl;
    }
*/
  if (Linky.IinstIsNew())
    {
    Send_String = String(Linky.Iinst()) + 'A' ;
    Serial << Send_String << endl;
    Wire.beginTransmission(NANO_Master);
    Wire.print(Send_String);
    Wire.endTransmission();  
    }

  if (Linky.PappIsNew())
    {
      if (millis()/1000>10){
      Send_String = String(Linky.Papp() / 10) + T;
      Serial << Send_String << endl;
      Wire.beginTransmission(NANO_Master);
      Wire.print(Send_String);
      Wire.endTransmission();
      }
    }
  };

