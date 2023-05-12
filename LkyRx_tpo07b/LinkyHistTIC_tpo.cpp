/***********************************************************************
               Objet decodeur de teleinformation client (TIC)
               format Linky "historique" ou anciens compteurs
               electroniques.

                           Version tarif "tempo"

Lit les trames et decode les groupes :
  BBRHCJB = index(hcjb=0) : heures creuses jours bleu,
  BBRHPJB = index(hpjb=1) : heures pleines jours bleu,
  BBRHCJW = index(hcjw=2) : heures creuses jours blancs,
  BBRHPJW = index(hpjw=3) : heures pleines jours blancs,
  BBRHCJR = index(hcjr=4) : heures creuses jours rouges,
  BBRHPJR = index(hpjr=5) : heures pleines jours rouges,
  DEMAIN : periode tarifaire du lendemain (0 = j_bleu, 1 = j_blanc,
           2 = j_rouge)
  IINST : (iinst) intensit� instantan�e en A,
  PAPP  : (papp) puissance apparente en VA.

Reference : ERDF-NOI-CPT_54E V1

V06 : MicroQuettas mars 2018
_tpo07b : "tempo" tarif, tested OK on 22/03/18.

***********************************************************************/
// TODO : dont
/***************************** Includes *******************************/
#include <string.h>
#include <Streaming.h>
#include "LinkyHistTIC_tpo.h"

//#define LINKYDEBUG true

/***********************************************************************
                  Objet r�cepteur TIC Linky historique

 Trame historique :
  - d�limiteurs de trame :     <STX> trame <ETX>
    <STX> = 0x02
    <ETX> = 0x03

  - groupes dans une trame : <LF>lmnopqrs<SP>123456789012<SP>C<CR>
    <LR> = 0x0A
    lmnopqrs = label 8 char max
    <SP> = 0x20 ou 0x09 (<HT>)
    123456789012 = data 12 char max
    C = checksum 1 char
    <CR> = 0x0d
       Longueur max : label + data = 7 + 9 = 16

  _FR : flag register

    |  7  |  6  |   5  |   4  |   3  |  2  |   1  |   0  |
    |     |     | _Dec | _GId | _Cks |     | _RxB | _Rec |

     _Rec : receiving
     _RxB : receive in buffer B, decode in buffer A
     _GId : Group identification
     _Dec : decode data

  _DNFR : data available flags

    |   15  |    14  |  13  |  12  |  11  |  10  |   9  |  8   |
    |       |        |      |      |      |      |      | papp |

    |   7   |    6   |   5  |   4  |   3  |  2   |   1  |  0   |
    | iinst | demain | petc | hpjr | hcjr | hcjw | hpjb | hcjb |

  Exemple of group :
       <LF>lmnopqr<SP>123456789<SP>C<CR>
           0123456  7 890123456  7 8  9
     Cks:  xxxxxxx  x xxxxxxxxx    ^
                                   |  iCks
    Minimum length group  :
           <LF>lmno<SP>12<SP>C<CR>
               0123  4 56  7 8  9
     Cks:      xxxx  x xx    ^
                             |  iCks

    The storing stops at CRC (included), ie a max of 19 chars

***********************************************************************/

/****************************** Macros ********************************/
#ifndef SetBits
#define SetBits(Data, Mask) \
Data |= Mask
#endif

#ifndef ResetBits
#define ResetBits(Data, Mask) \
Data &= ~Mask
#endif

#ifndef InsertBits
#define InsertBits(Data, Mask, Value) \
Data &= ~Mask;\
Data |= (Value & Mask)
#endif

#ifndef P1
#define P1(name) const char name[] PROGMEM
#endif

/***************************** Defines ********************************/
#define bLy_Rec   0x01     /* Receiving */
#define bLy_RxB   0x02     /* Receive in buffer B */
#define bLy_Cks   0x08     /* Check Cks */
#define bLy_GId   0x10     /* Group identification */
#define bLy_Dec   0x20     /* Decode */

#define Car_SP    0x20     /* Char space */
#define Car_HT    0x09     /* Horizontal tabulation */

#define CLy_Void  0xff

#define CLy_MinLg  8       /* Minimum useful message length */

const char CLy_Sep[] = {Car_SP, Car_HT, '\0'};  /* Separators */

/************************* Donn�es en progmem *************************/
P1(PLy_H)  = "BBR";        /* Start */
P1(PLy_H1)  = "HCJB";      /* Rank 0 = CLy_HCJB */
P1(PLy_H2)  = "HPJB";      /* Rank 1 = CLy_HPJB */
P1(PLy_H3)  = "HCJW";      /* Rank 2 = CLy_HCJW */
P1(PLy_H4)  = "HPJW";      /* Rank 3 = CLy_HPJW */
P1(PLy_H5)  = "HCJR";      /* Rank 4 = CLy_HCJR */
P1(PLy_H6)  = "HPJR";      /* Rank 5 = CLy_HPJR */
P1(PLy_PTEC)  = "PTE";     /* Rank 6 = CLy_PTEC */
P1(PLy_Dem)  = "DEM";      /* Rank 7 = CLy_Demain */
P1(PLy_iinst) = "IIN";     /* Rank 8 = CLy_iinst */
P1(PLy_papp)  = "PAP";     /* Rank 9 = CLy_papp */

P1(PbbrB) = "BLE";          /* Rank 0 = CLy_bleu */
P1(PbbrW) = "BLA";          /* Rank 1 = CLy_blanc */
P1(PbbrR) = "ROU";          /* Rank 2 = CLy_rouge */

char *GLy_Groupes[] = \
  {
  PLy_H, PLy_PTEC, PLy_Dem, PLy_iinst, PLy_papp
  };
#define CLy_NbGroupes (sizeof(GLy_Groupes) / sizeof(char*))
#define CLy_LgGroupes 3

char *GLy_Colors[] = \
  {
  PbbrB, PbbrW , PbbrR
  };
#define CLy_NbColors (sizeof(GLy_Colors) / sizeof(char*))
#define CLy_LgColors 3

char *GLy_Tarifs[] = \
  {
  PLy_H1, PLy_H2, PLy_H3, PLy_H4, PLy_H5, PLy_H6
  };
#define CLy_NbTarifs (sizeof(GLy_Tarifs) / sizeof(char*))
#define CLy_LgTarifs 4

/*************** Constructor, methods and properties ******************/
#ifdef LINKYSERIAL
#define _LRx Serial
#endif

#ifndef LINKYSERIAL  /* Normal mode : use SoftwareSerial */
LinkyHistTIC_tpo::LinkyHistTIC_tpo(uint8_t pin_Rx) \
      : _LRx (pin_Rx)  /* Software serial constructor
                                * Achtung : special syntax */
#else  /* Test mode : use Serial */
LinkyHistTIC_tpo::LinkyHistTIC_tpo(uint8_t pin_Rx)
#endif
  {
  uint8_t i;

  _FR = 0;
  _DNFR = 0;
  _pRec = _BfA;    /* Receive in A */
  _pDec = _BfB;    /* Decode in B */
  _iRec = 0;
  _iCks = 0;
  _GId = hcjb;
  _pin_Rx = pin_Rx;
 // _pin_Tx = pin_Tx;

  for (i = hcjb; i <= hpjr ; i++)
    {
    _index[i] = 0;
    }
  _petc = CLy_Void;
  _demain = CLy_Void;
  _iinst = 0;
  _papp = 0;

  };

void LinkyHistTIC_tpo::Init(uint16_t Bd = CLy_Bds)
  {
  /* Initialise the SoftwareSerial */
  pinMode (_pin_Rx, INPUT);
//  pinMode (_pin_Tx, OUTPUT);

  #ifndef LINKYSERIAL
  _LRx.begin(Bd);
  #endif
  }

bool LinkyHistTIC_tpo::Lookup_P(char **Tab, char *Src, \
                           uint8_t Nb, uint8_t Lg, uint8_t *Rk)
  {
  bool Res = false;
  bool Run = true;
  uint8_t i = 0;

  while (Run)
    {
    if (strncmp_P(Src, *(Tab + i), Lg) == 0)
      {  /* Text recognised */
      Run = false;   /* Stop */
      Res = true;    /* Text is found */
      *Rk = i;        /* Rank in the table */
      }
      else
      {
      i++;
      if (i > Nb)
        {
        Run = false;
        }
      }
    }  /* End while */
  return Res;
  }

void LinkyHistTIC_tpo::Update()
  {   /* Called from the main loop */
  char c;
  uint8_t cks, i;
  uint32_t ba;
  uint16_t pa;
  bool Run = false;

  /* Achtung : actions are in the reverse order to prevent
   *           execution of all actions in the same turn of
   *           the loop */

  /* 1st part, last action : decode information */
  if (_FR & bLy_Dec)
    {
    ResetBits(_FR, bLy_Dec);     /* Clear requesting flag */
    _pDec = strtok(NULL, CLy_Sep);

//    #ifdef LINKYDEBUG
//    Serial << F("Id=") << _GId << endl;
//    #endif
    
    switch (_GId)
      {
      case hcjb:
      case hpjb:
      case hcjw:
      case hpjw:
      case hcjr:
      case hpjr:
        ba = atol(_pDec);
        if (_index[_GId] != ba)
          {  /* New value for _index */
          _index[_GId] = ba;
          SetBits(_DNFR, (1<<_GId));
          }
        break;

      case petc:
        if (Lookup_P(GLy_Tarifs, _pDec, CLy_NbTarifs, \
                      CLy_LgTarifs, &i))
          {  /* Tarif is found */
          if (i != _petc)
            { /* New value for PETC */
            _petc = i;
            SetBits(_DNFR, (1<<petc));
            }
          }
        break;

      case demain:
        if (Lookup_P(GLy_Colors, _pDec, CLy_NbColors, \
                      CLy_LgColors, &i))
          {  /* Color is valid */
          if (i != _demain)
            { /* New value for demain */
            _demain = i;
            SetBits(_DNFR, (1<<demain));
            }
          }
        break;

      case iinst:
        i = (uint8_t) atoi(_pDec);
        if (_iinst != i)
          {  /* New value for _iinst */
          _iinst = i;
          SetBits(_DNFR, (1<<iinst));
          }
        break;

      case papp:
        pa = atoi(_pDec);
        if (_papp != pa)
          {  /* New value for papp */
          _papp = pa;
          SetBits(_DNFR, (1<<papp));
          }
        break;

      default:
        break;
      }
    }

  /* 2nd part, second action : group identification  */
  if (_FR & bLy_GId)
    {
    ResetBits(_FR, bLy_GId);   /* Clear requesting flag */
    _pDec = strtok(_pDec, CLy_Sep);

    if (Lookup_P(GLy_Groupes, _pDec, CLy_NbGroupes,
                  CLy_LgGroupes, &i))
      {  /* Group is valid */
//      #ifdef LINKYDEBUG
//      Serial << F("i1=") << i <<endl;
//      #endif
      
      if (i)
        { /* 1 = petc etc. */
        _GId = i + petc - 1;
        Run = true;
        }
        else
        { /* i = 0, i.e. one of the 6 indexes
           *   Format : BBRHCJB
           *            0123456
           *               |       */
        if (Lookup_P(GLy_Tarifs, _pDec + 3, CLy_NbTarifs,
                     CLy_LgTarifs, &i))
          {  /* Tarif is found */
//          #ifdef LINKYDEBUG
//          Serial << F("i2=") << i <<endl;
//          #endif
            
          _GId = i;
          Run = true;
          }
        }
      if (Run)
        {
        SetBits(_FR, bLy_Dec);   /* Next = decode */
        }
      }
    }

  /* 3rd part, first action : check cks */
  if (_FR & bLy_Cks)
    {
    ResetBits(_FR, bLy_Cks);   /* Clear requesting flag */
    cks = 0;
    if (_iCks >= CLy_MinLg)
      {   /* Message is long enough */
      for (i = 0; i < _iCks - 1; i++)
        {
        cks += *(_pDec + i);
        }
      cks = (cks & 0x3f) + Car_SP;

      #ifdef LINKYDEBUG
      Serial << _pDec << endl;
      #endif

      if (cks == *(_pDec + _iCks))
        {  /* Cks is correct */
        *(_pDec + _iCks-1) = '\0';
                       /* Terminate the string just before the Cks */
        SetBits(_FR, bLy_GId);  /* Next step, group identification */

        #ifdef LINKYDEBUG
        }
        else
        {
        i = *(_pDec + _iCks);
        Serial << F("Error Cks ");
        Serial.write(cks);
        Serial << F(" - ");
        Serial.write(i);
        Serial << endl;
        #endif
        }   /* Else, Cks error, do nothing */

      }     /* Message too short, do nothing */
    }

  /* 4th part, receiver processing */
  while (_LRx.available())
    {  /* At least 1 char has been received */
    c = _LRx.read() & 0x7f;   /* Read char, exclude parity */

    if (_FR & bLy_Rec)
      {  /* On going reception */
      if (c == '\r')
        {   /* Received end of group char */
        ResetBits(_FR, bLy_Rec);   /* Receiving complete */
        SetBits(_FR, bLy_Cks);     /* Next check Cks */
        _iCks = _iRec-1;           /* Index of Cks in the message */
        *(_pRec + _iRec) = '\0';   /* Terminate the string */

        /* Swap reception and decode buffers */
        if (_FR & bLy_RxB)
          {  /* Receiving in B, Decode in A, swap */
          ResetBits(_FR, bLy_RxB);
          _pRec = _BfA;       /* --> Receive in A */
          _pDec = _BfB;       /* --> Decode in B */
          }
          else
          {  /* Receiving in A, Decode in B, swap */
          SetBits(_FR, bLy_RxB);
          _pRec = _BfB;     /* --> Receive in B */
          _pDec = _BfA;     /* --> Decode in A */
          }

        }  /* End reception complete */
        else
        {  /* Other character */
        *(_pRec+_iRec) = c;   /* Store received character */
        _iRec += 1;
        if (_iRec >= CLy_BfSz-1)
          {  /* Buffer overrun */
          ResetBits(_FR, bLy_Rec); /* Stop reception and do nothing */
          }
        }  /* End other character than '\r' */
      }    /* End on-going reception */
      else
      {    /* Reception not yet started */
      if (c == '\n')
        {   /* Received start of group char */
        _iRec = 0;
        SetBits(_FR, bLy_Rec);   /* Start reception */
        }
      }
    }  /* End while */
  }


bool LinkyHistTIC_tpo::IndexIsNew(uint8_t id)
  {
  bool Res = false;

  if(id <= hpjr)
    {  /* Index identification is valid */
    if(_DNFR & (1<<id))
      {
      Res = true;
      ResetBits(_DNFR, (1<<id));
      }
    }

  return Res;
  }

bool LinkyHistTIC_tpo::PetcIsNew()
  {
  bool Res = false;

  if(_DNFR & (1<<petc))
    {
    Res = true;
    ResetBits(_DNFR, (1<<petc));
    }
  return Res;
  }

bool LinkyHistTIC_tpo::DemainIsNew()
  {
  bool Res = false;

  if(_DNFR & (1<<demain))
    {
    Res = true;
    ResetBits(_DNFR, (1<<demain));
    }
  return Res;
  }

bool LinkyHistTIC_tpo::IinstIsNew()
  {
  bool Res = false;

  if(_DNFR & (1<<iinst))
    {
    Res = true;
    ResetBits(_DNFR, (1<<iinst));
    }
  return Res;
  }

bool LinkyHistTIC_tpo::PappIsNew()
  {
  bool Res = false;

  if(_DNFR & (1<<papp))
    {
    Res = true;
    ResetBits(_DNFR, (1<<papp));
    }
  return Res;
  }

uint32_t LinkyHistTIC_tpo::Index(uint8_t id)
  {
  uint32_t Res = 0;

  if(id <= hpjr)
    {  /* Index identification is valid */
    Res = _index[id];
    }

  return Res;
  }

uint8_t LinkyHistTIC_tpo::Petc()
  {
  return _petc;
  }

uint8_t LinkyHistTIC_tpo::Demain()
  {
  return _demain;
  }

uint8_t LinkyHistTIC_tpo::Iinst()
  {
  return _iinst;
  }

uint16_t LinkyHistTIC_tpo::Papp()
  {
  return _papp;
  }


#ifdef LINKYSERIAL
#undef _LRx
#endif

/***********************************************************************
               Fin d'objet r�cepteur TIC Linky historique
***********************************************************************/
