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
  PTEC : periode tarifaire en cours,
  DEMAIN : periode tarifaire du lendemain (0 = j_bleu, 1 = j_blanc,
           2 = j_rouge)
  IINST : (iinst) intensit� instantan�e en A,
  PAPP  : (papp) puissance apparente en VA.

Reference : ERDF-NOI-CPT_54E V1

V06 : MicroQuettas mars 2018
_tpo07b : "tempo" tarif, tested OK on 22/03/18.

***********************************************************************/
#ifndef _LinkyHistTIC
#define _LinkyHistTIC true

//#include <SoftwareSerial.h>
#include <ReceiveOnlySoftwareSerial.h>
//#define LINKYSERIAL true

/******************************** Class *******************************
      LinkyHistTIC : Linky historique ITIC (t�l�information client)

Reference : ERDF-NOI-CPT_54E V1
***********************************************************************/
#define CLy_BfSz 22       /* Maximum size of the Rx buffers */
#define CLy_Bds 1200      /* Default transmission speed in bds */

class LinkyHistTIC_tpo
  {
  public:
    enum GroupId:uint8_t
      {
      hcjb, hpjb, hcjw, hpjw,
      hcjr, hpjr, petc, demain,
      iinst, papp
      };

    enum TempoColors:uint8_t {j_bleu, j_blanc, j_rouge };

    LinkyHistTIC_tpo(uint8_t pin_Rx);
              /* Constructor */

    void Init(uint16_t Bd = CLy_Bds);   /* Initialisation,
                                            call from setup() */
    void Update();      /* Update, call from loop() */

    bool IndexIsNew(uint8_t id); /* Returns true if index has changed*/
    bool PetcIsNew();   /* Returns true if PETC has changed */
    bool DemainIsNew(); /* Returns true if DEMAIN has changed */
    bool IinstIsNew();  /* Returns true if iinst has changed */
    bool PappIsNew();   /* Returns true if papp has changed */

    uint32_t Index(uint8_t id); /* Returns index in Wh */
    uint8_t Petc();     /* Returns periode tarifaire en cours */
    uint8_t Demain();   /* Returns periode tarifaire lendemain */
    uint8_t Iinst();    /* Returns iinst in A */
    uint16_t Papp();    /* Returns papp in VA */

  private:

    char _BfA[CLy_BfSz];        /* Buffer A */
    char _BfB[CLy_BfSz];        /* Buffer B */

    uint8_t _FR;                /* Flag register */
    uint16_t _DNFR;              /* Data new flag register */

    uint32_t _index[6];
    uint8_t _iinst, _petc, _demain;
    uint16_t _papp;

    #ifndef LINKYSERIAL
    ReceiveOnlySoftwareSerial _LRx;   /* Needs to be constructed at the same time
                            * as LinkyHistTIC. Cf. Special syntax
                            * (initialisation list) in LinkyHistTIC
                            * constructor */
    #endif

    char *_pRec;    /* Reception pointer in the buffer */
    char *_pDec;    /* Decode pointer in the buffer */
    uint8_t _iRec;   /*  Received char index */
    uint8_t _iCks;   /* Index of Cks in the received message */
    uint8_t _GId;    /* Group identification */

    uint8_t _pin_Rx;
    //uint8_t _pin_Tx;

    bool Lookup_P(char **Tab, char *Src, uint8_t Nb, \
                   uint8_t Lg, uint8_t *Rk);

  };

#endif
/*************************** End of code ******************************/

