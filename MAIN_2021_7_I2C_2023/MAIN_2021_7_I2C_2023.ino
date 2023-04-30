#include <Time.h>
#include<SPI.h>
#include <OneWire.h>
#include <string.h>
#include <EnableInterrupt.h>
#include <Wire.h>

#define PAYLOAD_SIZE 5
#define I2C_SLAVE_ADDRESS 11
#define analogInput_A0  A0  //Volt/amp éolienne
#define analogInput_A2  A2  //Power_solaire
#define analogInput_A7  A7  //Power_solaire
#define WindSensorPin (5)   // Pin anemometer sensor
#define BROCHE_ONEWIRE  7   //temperature 
#define LATCH 8             //CS 75HC595
#define CS  9               //Chip Select SPI 644
#define RESET 10
#define TIME_HEADER '~' // Header tag for serial time sync message
#define TIME_MSG_LEN 11 // 10 ASCII digits
#define OUTPUT_HEADER 'O' //reception Serial
#define RESET_HEADER 'R' //reception Serial
#define DeltaTempStart 1    // en degrés : Ecart de température nécessaire pour la mise en route de la pompe
#define DeltaTempStop 0     // en degrés : Ecart de température minimum pour le fonctionnement de la pompe
#define on 1 //turn on 75HC595
#define off 0 //turn off 75HC595
#define EtatManuOFF '0'
#define EtatManuON '1'
#define EtatAutoOFF '2'
#define EtatAutoON '3'
#define EtatBoot '4'  //état au démarrage

boolean resync_time = false;

const byte Max_Input_serial_read = 12;//reception serial python

const int Wait_boot = 5000; // attente avant de recuperer certaines données ou activation des sorties

//////////////////////////////      linky////
char Type;
byte pos_c_linky = 0;
char Recept_ser_lky[5] = {0};

byte AMP ;
byte AMP_old_ser;
byte AMP_old_SPI;
unsigned int PAPP_moyen ;
unsigned int PAPP_moyen_old ;
unsigned int PAPP ;
unsigned int PAPP_old_SPI;
unsigned int PAPP_old_ser;
unsigned int Recept_ser_lky_int;
unsigned int PAPPmax;
unsigned int INJECTION ;
unsigned int INJECTION_old_ser;
byte Tarif ;
byte Tarif_old ;

//////////////////////////////      Anémometre/////////////////
volatile unsigned int Rotations; // cup rotation counter used in interrupt routine
volatile unsigned long ContactBounceTime; // Timer to avoid contact bounce in interrupt routine
int WindSpeed; //wind KM/heure
float metre_seconde;
int WindSpeed_old;
int WindSpeed_max;
int WindSpeed_max_old;
float tourminute; //tourminute
unsigned long millis_time_vent = 0;
const int wait_time_vent = 4000;
unsigned long time_WindSpeed_max = 0;

//////////////////////////////      mesure eolienne////
float Vin = 0.0;
const float R1 = 65000.0;
const float R2 = 10000.0;
int Mesure_A0 = 0;
float Amp_wt = 0.0;
byte Power_old = 0;
byte Power = 0;
byte Power_max = 0 ;

//////////////////////////////      Prod solaire/////////////////
unsigned long Power_solaire_time = 2000;
unsigned int Mesure_A2 = 0;
unsigned int Mesure_A7 = 0;
unsigned int MESURE_MIN_solaire_2 = 517;
unsigned int MESURE_MAX_SOLAIRE_2 = 0;
unsigned int MESURE_MIN_solaire_7 = 517;
unsigned int MESURE_MAX_solaire_7 = 0;
unsigned int Power_max_solaire = 0;
unsigned long MESURE_SOLAIRE = 0;
unsigned int Power_max_solaire_sended_SPI = 0;
unsigned int Power_max_solaire_sended_serial = 0;

//////////////////////////////  Température    DS18B20//////////////////////
OneWire ds(BROCHE_ONEWIRE);

/* Résolution disponibles */
const byte RESOLUTION_12_BITS = 0b01111111;
const byte RESOLUTION_11_BITS = 0b01011111;
const byte RESOLUTION_10_BITS = 0b00111111;
const byte RESOLUTION_9_BITS = 0b00011111;
/** Change la résolution du capteur spécifié */

void changeResolution(const byte addr[], byte resolution) { // Reset le bus 1-Wire, sélectionne le capteur et envoi une demande d'écriture
  ds.reset();
  ds.select(addr);
  ds.write(0x4E);
  ds.write(0x00);
  ds.write(0x00);
  ds.write(resolution);
  ds.reset();
}

const byte  Probe01[] = { 0x28, 0x85, 0x8E, 0xE3, 0x08, 0x00, 0x00, 0x92 }; //garage
const byte  Probe02[] = { 0x28, 0xFF, 0x2C, 0x4A, 0x85, 0x16, 0x03, 0x99 }; //ext1
const byte  Probe03[] = { 0x28, 0xFF, 0xAC, 0xC0, 0x85, 0x16, 0x04, 0x13 }; //ext2
const byte  Probe04[] = { 0x28, 0xFF, 0xEE, 0xAC, 0x92, 0x16, 0x04, 0xCB }; //piscine
const byte  Probe05[] = { 0x28, 0xFF, 0x14, 0xC3, 0x85, 0x16, 0x04, 0xEA };
//0x28, 0x89, 0x04, 0xE3, 0x08, 0x00, 0x00, 0x99 salon (sur atmega644)
//0x28, 0x1A, 0x2B, 0xE2, 0x08, 0x00, 0x00, 0x69 garage2 (sur atmega644)

unsigned long  Time_Temp_Mesure = 0;
bool Temperature_Mesure;
float Temp[6] = {1, 2, 3, 4, 5};
float Temp_old[6] = {1, 2, 3, 4, 5};
float Temp_salon;
float Temp_garage2;
float Temp_salon_old;
float Temp_garage2_old;
bool Temp_New_Mesure[6] = {0, 0, 0, 0, 0};
bool Temp_New_Mesure_Serial[6] = {0, 0, 0, 0, 0};


//////////////////////////////    OUTPUT  75HC595////////////////////////////////////////////////
const byte numberOfChips = 4;
byte Out_data [numberOfChips] = { 0 };
///////////////////////////////////////////////////////////////////////////////



//const int wait_time = 1000;

bool Timestamping = false;




//char SPI_c;
//byte output  = 0;
//////////////////////////////      SPI      ////////////////////////////////////////
/////////////////012345678901234567890123456789012
char Etat[34] = "044444444444444444444444444444444";
char LastEtat[34] = "044444444444444444444444444444444";
unsigned long Time_SPI_send_T = 0;
unsigned long Time_SPI_send_S = 0;
bool RECEPT_SPI = false;
const byte Delay_send_644 = 60;

///////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////      SETUP      ////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void setup()  {
  Serial.begin(115200);

  pinMode(analogInput_A0, INPUT); //Eolienne
  pinMode(analogInput_A2, INPUT); //Production solaire
  pinMode(analogInput_A7, INPUT); //Production solaire
  pinMode(LATCH, OUTPUT);         //CS 75HC595
  pinMode(CS, OUTPUT);            //CS ATMEGA644
  pinMode(RESET, OUTPUT);         //RESET ATMEGA644
  pinMode(WindSensorPin, INPUT);  //Anémometre

  ///////////////////////          Temp        ///////////////
  changeResolution(Probe01, RESOLUTION_12_BITS);
  changeResolution(Probe02, RESOLUTION_12_BITS);
  changeResolution(Probe03, RESOLUTION_12_BITS);
  changeResolution(Probe04, RESOLUTION_12_BITS);
  changeResolution(Probe05, RESOLUTION_12_BITS);

  /////////////////////////        Setup   644               /////////
  digitalWrite(RESET, LOW);
  digitalWrite(RESET, HIGH);
  digitalWrite(CS, HIGH);  // deasable SlaveSelect SPI
  SPI.begin ();


  ////////////////////            75HC595             ///////////////////////
  for (int i = 0; i < numberOfChips; i++) {
    Out_data [i] = 0x00;
    Refresh_outs ();
  }



  while (timeStatus() == timeNotSet) {
    Serial.println("!");
    while (Serial.available () > 0) {
      Decode_Incomming_Serial (Serial.read ());
    }
    delay(500);
  }
  delay(10);

  Wire.begin(I2C_SLAVE_ADDRESS);
  Wire.onReceive(receiveEvents);

  enableInterrupt( WindSensorPin, isr_rotation, CHANGE );
}
///////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////      END SETUP      ////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
////////////////////////////////      LOOP      ///////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
void loop() {

  Mesure_A2 = analogRead(analogInput_A2) ;//Production Solaire
  Mesure_A7 = analogRead(analogInput_A7) ;//Production Solaire
  Mesure_A0 = analogRead(analogInput_A0);//Production Solaire

  if (Serial.available () > 0) {
    Decode_Incomming_Serial (Serial.read ());
  }
  if ((minute() == 01) && (resync_time  == false )) {
    resync_time  = true ;
    Serial.println("!");
  }
  if (minute() == 02 ) {
    resync_time  = false ;
  }
  if (timeStatus() == timeNotSet) {
    Serial.println("!");   ///! -> demande la date à python
  }

  ////////////////////////////////////////ANENOMETRE////////////////////////////////////////
  if (millis() > millis_time_vent  +  wait_time_vent) {
    millis_time_vent = millis();
    //tourminute = (Rotations * 60) / (wait_time_vent / 1000);
    //  WindSpeed = ((2 * 3.1416 * 0.1 * tourminute * 60 ) / 1000) + (0.1 * tourminute);
    WindSpeed = ((2 * 3.1416 * 0.1 * ((Rotations * 60) / (wait_time_vent / 1000)) * 60 ) / 1000) + (0.1 * ((Rotations * 60) / (wait_time_vent / 1000)));
    if (WindSpeed > WindSpeed_max) {
      WindSpeed_max = WindSpeed ;
      time_WindSpeed_max = millis();
    }
    if (millis() > time_WindSpeed_max + 8640000) {
      WindSpeed_max = WindSpeed ;
      time_WindSpeed_max = millis();
    }
    if (WindSpeed != WindSpeed_old) {
      metre_seconde = (2 * 3.1416 * 0.1 * (Rotations / (wait_time_vent / 1000) )) + (0.1 * (Rotations) / (wait_time_vent / 1000));
      //((2 * pi * radius * RPM)/60) / 1000;
      //(WindSpeed * 1000 / 3600);
      Send_Timestamp_Serial();
      Serial.print(F(";M_S=")); Serial.print(metre_seconde, 2);
      //Serial.print(";");
      Serial.print(F(";KM=")); Serial.print(WindSpeed);
      //Serial.print(";");
      Serial.print(F(";KM_max=")); Serial.print(WindSpeed_max);
      //Serial.print(";");
    }
    Rotations = 0; // Set Rotations count to 0 ready for calculations
  }


  ////////////      Temperature     ///////////////
  recupTemp();
  for (byte i = 1; i < 6; i = i + 1) {
    if (Temp_New_Mesure_Serial[i] == true) {
      Temp_New_Mesure_Serial[i] = false;
      if (Temp[i] == -127.00)  {
        Temp[i] = 99 ;
      }
      Send_Timestamp_Serial();
      Serial.print(";T"); Serial.print(i); Serial.print("="); Serial.print(Temp[i]);
    }
  }

  ////////////////TEMP SALON + GARRAGE /////reçu du 644//////////
  if (millis() > Time_SPI_send_S + 10) {
    if (Temp_salon != Temp_salon_old) {
      Temp_salon_old = Temp_salon;
      Send_Timestamp_Serial();
      Serial.print (F(";T_S=")); Serial.print (Temp_salon);
    }

    if (Temp_garage2 != Temp_garage2_old) {
      Temp_garage2_old = Temp_garage2;
      Send_Timestamp_Serial();
      Serial.print (F(";T_G2=")); Serial.print (Temp_garage2);
    }
  }

  /*/////////////MESURE EOLIENNE//////////////////:
    Vin = (((Mesure_A0 ) * 5.0) / 1023.0) * ((R1 + R2) / R2);
    Amp_wt = Vin / 40;
    Power = (Vin * Vin) / 40 * 10;
    if (Power > Power_max) {
    Power_max = Power;
    }
    if (Power != Power_old) {
    Power_old = Power;
    if (Power == 0) {
      Vin = 0;
      Amp_wt = 0;
    }
    Send_Timestamp_Serial();
    Serial.print(F(";Volt=")); Serial.print(Vin, 2);
    Serial.print(F(";Amp_wt=")); Serial.print(Amp_wt, 2);
    Serial.print(F(";Power=")); Serial.print(Power);
    Serial.print(F(";Power_max=")); Serial.print(Power_max);

    }  //////////////////////////////////FIN-mesure eolienn//////////////////////////////////
  */
  //////////////////////////////           LINKY        /////////////////////////

  if (AMP != AMP_old_ser) {
    AMP_old_ser = AMP;
    Send_Timestamp_Serial();
    Serial.print(F(";AMP=")); Serial.print(AMP);
  }

  if (PAPP != PAPP_old_ser) {
    //  if (PAPP < (AMP * 230 + 500)) {
    /* PAPP_moyen = ((PAPP + PAPP_old_ser) / 2); /// evite les rebonds
            if (PAPP_moyen != PAPP_moyen_old) {
             PAPP_moyen_old = PAPP_moyen;

             Send_Timestamp_Serial();
             Serial.print(F(";PAPP=")); Serial.print(PAPP);
            }*/
    Send_Timestamp_Serial();
    Serial.print(F(";PAPP=")); Serial.print(PAPP);

    if (PAPP > PAPPmax) {
      PAPPmax = PAPP ;
      Serial.print(F(";CONSO_max=")); Serial.print(PAPPmax);
    }
    PAPP_old_ser = PAPP;
  }

  if (PAPP == 0) {
    INJECTION = AMP * 240 ;
    if (INJECTION != INJECTION_old_ser) {
      INJECTION_old_ser = INJECTION;
      Send_Timestamp_Serial();
      Serial.print(F(";injection:")); Serial.print(INJECTION);
    }
  }

  //////////////////////////           Power_solaire         /////////////////////////////////////////////
  if (Mesure_A2 > MESURE_MAX_SOLAIRE_2)
    MESURE_MAX_SOLAIRE_2 = Mesure_A2;
  if (Mesure_A2 < MESURE_MIN_solaire_2)
    MESURE_MIN_solaire_2 = Mesure_A2;

  if (Mesure_A7 > MESURE_MAX_solaire_7)
    MESURE_MAX_solaire_7 = Mesure_A7;

  if (Mesure_A7 < MESURE_MIN_solaire_7)
    MESURE_MIN_solaire_7 = Mesure_A7;

  if  ((millis() - Power_solaire_time  >  2000) && (millis() > Wait_boot))  {
    Power_solaire_time = millis();

    MESURE_SOLAIRE = ((MESURE_MAX_SOLAIRE_2 - MESURE_MIN_solaire_2) + (MESURE_MAX_solaire_7 - MESURE_MIN_solaire_7));
    Power_max_solaire = abs((((MESURE_SOLAIRE / 10)  * (MESURE_SOLAIRE / 10) * 0.42) - 66) / 10);
    Power_max_solaire = Power_max_solaire * 10;///arrondir 10enes


    MESURE_MIN_solaire_2 = 517; MESURE_MAX_SOLAIRE_2 = 0; MESURE_MIN_solaire_7 = 517; MESURE_MAX_solaire_7 = 0;
    if (Power_max_solaire != Power_max_solaire_sended_serial) {
      Power_max_solaire_sended_serial = Power_max_solaire;
      Send_Timestamp_Serial();
      Serial.print(F(";Prod=")); Serial.print(Power_max_solaire);
    }
  }

  //////////////////////////    SWITCH OUTPUT 74HC595    //////////////////////////
  for (byte output = 1; output < 33; output = output + 1) {
    switch (Etat[output]) {
      case EtatBoot:        // ==> démarrage (4)
        TURN (off, output);
        Etat[output] = EtatManuOFF;
        break;

      case EtatManuOFF:        // ==> arrêt manuellement
        break;

      case EtatManuON:       // ==> marche manuelle
        break;
    } /// puis last etat ............

    if (LastEtat[output] != Etat[output]) {
      LastEtat[output] = Etat[output];
      byte etatout = (Etat[output] - '0');
      Send_To_644('O', output * 10 + etatout);
      Send_Timestamp_Serial();
      Serial.print(F(";OUT")); Serial.print(output); Serial.print("="); Serial.print(Etat[output]);
    }
  }
  ///////////////                        Fin switch                    ////////////////


  //////////////////////////////////     ENVOIS -> SLAVE—LCD           ///////////////////////////////////
  if (millis() > Time_SPI_send_T  +  15000) {/// update time sur 644 toutes les 15s
    Time_SPI_send_T = millis();
    Send_To_644('T', long(now()));
  }

  if (WindSpeed != WindSpeed_old) {
    WindSpeed_old = WindSpeed;
    Send_To_644('V', WindSpeed);
  }

  if (WindSpeed_max != WindSpeed_max_old) {
    WindSpeed_max_old = WindSpeed_max;
    Send_To_644('W', WindSpeed_max);
  }

  if (Power_max_solaire !=  Power_max_solaire_sended_SPI) {
    Power_max_solaire_sended_SPI =  Power_max_solaire ;
    Send_To_644('C', Power_max_solaire);
  }

  if (PAPP != PAPP_old_SPI) {
    PAPP_old_SPI = PAPP;
    Send_To_644('P', PAPP);
  }

  if (AMP != AMP_old_SPI) {
    AMP_old_SPI = AMP;
    Send_To_644('I', AMP);
  }

  if (Tarif != Tarif_old) {
    Tarif_old = Tarif;
    Send_To_644('H', Tarif);
    Send_Timestamp_Serial();
    Serial.print(F(";tarif=")); Serial.print(Tarif);
    Send_To_644('H', Tarif);
  }


  if (millis() > Time_SPI_send_S + 50) { ///ENVOI vers SPI des secondes toutes les 50 ms environ, pour l'actualisation des données à recevoir via SPI
    Send_To_644('S', (millis() / 1000));
  }

  //////////////Temp--> 644////////////////////////////////////
  if (Temp_New_Mesure[1] == true) {
    Temp_New_Mesure[1] = false;////////////////////////////temperature garage////////
    Send_To_644('G', Temp[1] * 100);
  }

  if (Temp_New_Mesure[2] == true) {
    Temp_New_Mesure[2] = false;////////////////////////////temperature EXT soleil//////
    Send_To_644('E', Temp[2] * 100);
  }

  if (Temp_New_Mesure[3] == true) {
    Temp_New_Mesure[3] = false;////////////////////////////temperature EXT abris//////
    Send_To_644('A', Temp[3] * 100);
  }
  if (Temp_New_Mesure[4] == true) {
    Temp_New_Mesure[4] = false;////////////////////////////temperature piscine//////
    Send_To_644('N', Temp[4] * 100);
  }


  if (Timestamping) {
    Timestamping = false;
    Serial.println(";");
  }
}


///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////     (END main LOOP)     ////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////


///////////////////////////////     Reset function     ////////////////////////////////
void(* Reset_Func) (void) = 0;

///////////////////////////////     Ajoute l'horodatade à chaque ligne -> Serial     ////////////////////////////////
void Send_Timestamp_Serial() {
  if (!Timestamping) {
    Timestamping = true;
    Serial.print (" "); Serial.print (day()); Serial.print("/"); Serial.print(month()); Serial.print("/"); Serial.print(year() - 2000); Serial.print(" ");
    Serial.print(hour()); printDigits(minute());  printDigits(second());
  }
}

void printDigits(byte digits) {
  Serial.print(":");
  if (digits < 10)
    Serial.print('0');
  Serial.print(digits);
}


////////////////////////////////       LINKY   I2C    /////////////////////////////////////////////////////////
void receiveEvents() {
  while (Wire.available()) {
    char  linky_c =  Wire.read();
    if (isUpperCase(linky_c)) {
      Type = linky_c;
      pos_c_linky = 0;
      LINKY_DECODE();
      return;
    }
    Recept_ser_lky[pos_c_linky] = linky_c;
    pos_c_linky++;
  }
}

void LINKY_DECODE () {
  const byte linky_fin = strlen(Recept_ser_lky)  ;
  Recept_ser_lky_int = 0;
  for (byte pos = 0; pos < linky_fin  ; pos++) {
    Recept_ser_lky_int = (10 * Recept_ser_lky_int) + (Recept_ser_lky[pos] - '0') ;
  }
  memset(Recept_ser_lky, 0, sizeof(Recept_ser_lky));
  if ((Type == 'P') || (Type == 'C')) {
    PAPP = Recept_ser_lky_int * 10;
    if (Type == 'C')
      Tarif = 0;
    else
      Tarif = 1;
  }
  else if ((Type == 'A')) {
    AMP = Recept_ser_lky_int ;
  }
}


///////////////////////////         RECEPTION SERIAL     ////////////////////////////////
void Decode_Incomming_Serial (const char inByte) {
  static char input_line [Max_Input_serial_read];
  static byte input_pos = 0;
  switch (inByte) {
    case '\n':   // end of text
      input_line [input_pos] = 0;  // terminating null byte
      Process_Data (input_line);// terminator reached! process input_line here ...
      input_pos = 0;  // reset Buffer for next time
      break;

    case '\r':   // discard carriage return
      break;

    default:
      if (input_pos < (Max_Input_serial_read - 1))
        input_line [input_pos++] = inByte;
      break;
  }  // end of switch
}

///////////////////////////   Analyse RECEPTION SERIAL   pour pilotage des sorties, heure, et reset     ////////////////////////////////
void Process_Data (const char * data) {
  const char Head = (data [0]);

  if ( Head == OUTPUT_HEADER ) {
    byte NUM_output = (data [1] - '0') * 10 + (data [2] - '0');
    byte ON_OFF = (data [3] - '0');
    if (ON_OFF == 1) {
      TURN (on, NUM_output);
      Etat[NUM_output] = EtatManuON;
    }
    if (ON_OFF == 0) {
      TURN (off, NUM_output);
      Etat[NUM_output] = EtatManuOFF;
    }
    if (ON_OFF == 2) { ///AUTO
      TURN (off, NUM_output);
      Etat[NUM_output] = EtatAutoOFF;
    }
    return;
  }

  if ( Head == TIME_HEADER ) {
    unsigned long pctime = 0;
    for (byte pos = 1; pos < TIME_MSG_LEN  ; pos++) { // TIME_MSG_LEN(10) pos[0]= ~
      pctime = (10 * pctime) + (data[pos] - '0') ;
    }
    setTime(pctime);
    return;
  }

  if ( Head == RESET_HEADER )
    Reset_Func(); /// Reset
}


/////////////////////////////////////////    VENT:function:interrupt calls to increment rotation count     ////////////////////////////////
void isr_rotation () {
  if ((millis() - ContactBounceTime) > 10 ) { // debounce the switch contact.
    ContactBounceTime = millis();
    Rotations++;
  }
}

///////////////////TEMPERATURE  /////////////////////
void Start_Temperature_Mesure(const byte addr[]) {
  ds.reset();
  ds.select(addr);
  ds.write(0x44, 1);
}

float Read_Temperature_Mesure(const byte addr[]) {
  byte data[9];
  ds.reset();
  ds.select(addr);
  ds.write(0xBE);
  for (byte i = 0; i < 9; i++) {
    data[i] = ds.read();
  }
  return (int16_t) ((data[1] << 8) | data[0]) * 0.0625;
}

void recupTemp() {
  if ( millis() - Time_Temp_Mesure >  5000) {
    Start_Temperature_Mesure(Probe01);
    Start_Temperature_Mesure(Probe02);
    Start_Temperature_Mesure(Probe03);
    Start_Temperature_Mesure(Probe04);
    Start_Temperature_Mesure(Probe05);
    if (Temperature_Mesure == false) {
      Time_Temp_Mesure = millis();
      Temperature_Mesure = true;
    }
  }
  if (Temperature_Mesure) {
    if ( millis() - Time_Temp_Mesure >  1200) {
      Temp[1] = Read_Temperature_Mesure(Probe01);
      Temp[2] = Read_Temperature_Mesure(Probe02);
      Temp[3] = Read_Temperature_Mesure(Probe03);
      Temp[4] = Read_Temperature_Mesure(Probe04);
      Temp[5] = Read_Temperature_Mesure(Probe05);
      Temperature_Mesure = false;
    }
  }
  for (byte i = 1; i < 6; i = i + 1) {
    if (Temp[i] != Temp_old[i]) {
      Temp_old[i] = Temp[i];
      Temp_New_Mesure_Serial[i] = true;
      Temp_New_Mesure[i] = true;

    }
  }
}
////////////////////////////   74hc595    ///////////////////////////////////////////////////////////////
void TURN (byte State, byte OutpuT) {
  byte realout = OutpuT - 1; // make zero relative
  byte Chip = realout / 8;  // divide by 8 to work out which Chip
  byte bit = 1 << (realout % 8);// remainder is bit number
  if (State == 1) {
    Out_data [Chip] |= bit; //ON ALLUME
  }
  else
    Out_data [Chip] &= ~ bit;//On éteint
  Refresh_outs ();
}

void Refresh_outs () {
  digitalWrite (LATCH, LOW);
  for (int i = numberOfChips - 1; i >= 0; i--)
    SPI.transfer (Out_data [i]);
  //delay(1);
  digitalWrite (LATCH, HIGH);
  delay(1);
}


/////////////////////////           SPI///Send_To_644e--> ATMEGA644 =LCD     +RX////////


void Send_To_644(char TYPE, long Mes) {
  char SPI_c;
  byte SEND = 0;
  byte pos = 255;//(=-1)
  char Buf_recept [6] = { '\0' };
  const String string1 = String (TYPE);
  const String string2 = String (Mes);
  const String String_char = String (string1 + string2);

  size_t BuffSize;
  BuffSize = String_char.length() + 1 ;
  char Data_char[BuffSize] = {'\0'};
  String_char.toCharArray(Data_char, BuffSize) ;
  SPI.beginTransaction (SPISettings (1000000, MSBFIRST, SPI_MODE0));
  digitalWrite(CS, LOW); // enable Slave Select
  delayMicroseconds (20);
  for (const char * p = (Data_char) ; (SPI_c = *p); p++) {
    if (SEND < 6) {
      Buf_recept [pos++] = SPI.transfer (SPI_c);
      SEND++;
      delayMicroseconds (25);
    }
    else
      SPI.transfer (SPI_c);
    delayMicroseconds (25);
  }
  if (SEND < 6) {
    Buf_recept [pos++] = SPI.transfer ('"'); //fin du transfert de SPI_c
    SEND++;
    delayMicroseconds (25);
  }
  else {
    SPI.transfer ('"');//fin du transfert de SPI_c
    delayMicroseconds (25);
  }
  if (SEND < 6) {
    for (byte pos = SEND - 1; pos < 6 ; pos++) {
      Buf_recept [pos] = SPI.transfer ('!');
      delayMicroseconds (25);
    }
  }

  if ( isUpperCase (Buf_recept[0])) { //Si on a bien reçu quelque chose , on envoit # comme accuser de reception au 644
    SPI.transfer ('#');//ACCUSER_RECEPTION
    delayMicroseconds (25);
    RECEPT_SPI = true;
  }
  digitalWrite(CS, HIGH);// disable Slave Select
  SPI.endTransaction ();
  delay(20);// agit sur affichage etat out

  unsigned long Diff_now_last_send  = millis() - Time_SPI_send_S; //ajout d'un delay entre 2 transferts SPI
  if (Diff_now_last_send < Delay_send_644) {
    delay(Delay_send_644 - Diff_now_last_send);
  }
  Time_SPI_send_S = millis();

  if (RECEPT_SPI) {
    RECEPT_SPI = false;
    if (Buf_recept[0] == 'O') {
      byte NUM_output = (Buf_recept[1] - '0') * 10 + (Buf_recept[2] - '0');
      byte ON_OFF = (Buf_recept[3] - '0');
      if (ON_OFF == 1) {
        TURN (on, NUM_output);
        Etat[NUM_output] = EtatManuON;
      }
      else if (ON_OFF == 0) {
        TURN (off, NUM_output);
        Etat[NUM_output] = EtatManuOFF;
      }
      else if (ON_OFF == 2) { ///AUTO
        TURN (off, NUM_output);
        Etat[NUM_output] = EtatAutoOFF;
      }
    }///////////////////////////////////////
    else if (Buf_recept[0] == 'L') {
      Temp_salon = ((Buf_recept[1] - '0') * 1000 + (Buf_recept[2] - '0') * 100 + (Buf_recept[3] - '0') * 10 + (Buf_recept[4] - '0')) / 100.0 ;
    }

    else if (Buf_recept[0] == 'G') {
      Temp_garage2 = ((Buf_recept[1] - '0') * 1000 + (Buf_recept[2] - '0') * 100 + (Buf_recept[3] - '0') * 10 + (Buf_recept[4] - '0')) / 100.0 ;
    }
  }
}

//*********( THE END )***********
