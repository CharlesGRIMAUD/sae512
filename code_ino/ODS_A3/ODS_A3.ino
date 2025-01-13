#include <SPI.h>
#include <DecaDuino.h>

// Timeout parameters
#define TIMEOUT_WAIT_ACK_SENT 5 //ms
#define TIMEOUT_WAIT_DATA_REPLY_SENT 5 //ms
#define ACK_DATA_REPLY_INTERFRAME 10 //ms
#define TIMEOUT_WAIT_ACK_REQ_SENT 5000

// TWR server states state machine enumeration: see state diagram on documentation for more details
enum { ODS_INIT, ODS_ATT_MSG_M,ODS_RECUP_T31, ODS_ATT_MSG_AR, ODS_RECUP_T32, ODS_ENV_MSG, WAIT_A3_SENT };

// Message types of the TWR protocol
#define ODS_MSG_TYPE_M 0
#define ODS_MSG_TYPE_AR 1
#define ODS_MSG_TYPE_A3 3
uint64_t timeout;
uint64_t t31, t32, t33;

#ifdef ARDUINO_DWM1001_DEV
DecaDuino decaduino(SS1, DW_IRQ);
#else
DecaDuino decaduino;
#endif

uint8_t txData[128];
uint8_t rxData[128];
uint16_t rxLen;
int state;


void setup()
{
  Serial.begin(115200); // Init Serial port
  if ( !decaduino.init() ) {
    Serial.println("decaduino init failed");
  }

  // Set RX buffer
  decaduino.setRxBuffer(rxData, &rxLen);
  state = ODS_INIT;
}

void loop()
{
  decaduino.engine();

  switch (state) {
   
    case ODS_INIT:
      decaduino.plmeRxEnableRequest();
      Serial.println("Mode réception A3");
      state = ODS_ATT_MSG_M;
      break;

    case ODS_ATT_MSG_M:
      if ( decaduino.rxFrameAvailable() ) {
        Serial.println("Message reçu");
        if ( rxData[0] == ODS_MSG_TYPE_M) {
          Serial.println("M reçu");
          state = ODS_RECUP_T31;
        } else {
					state = ODS_INIT;
				}
      }
      break;

    case ODS_RECUP_T31:
      t31 = decaduino.getLastRxTimestamp();
      Serial.println("Récupère T31");
      delay(500);
      state = ODS_ATT_MSG_AR;
      break;

    case ODS_ATT_MSG_AR:
      decaduino.plmeRxEnableRequest();
      if ( decaduino.rxFrameAvailable() ) {
        Serial.println("Message recu 3");
        if ( rxData[0] == ODS_MSG_TYPE_AR) {
          Serial.println("AR reçu");
          state = ODS_RECUP_T32;
        } else {
					state = ODS_ATT_MSG_AR;
				}
      }
      break;

    case ODS_RECUP_T32:
      t32 = decaduino.getLastRxTimestamp();
      Serial.println("Récupère T32");
      state = ODS_ENV_MSG;
      break;

    case ODS_ENV_MSG:
      t33 = decaduino.getLastTxTimestamp();
      decaduino.plmeRxDisableRequest();
      txData[0] = ODS_MSG_TYPE_A3;
      decaduino.encodeUint40(t31, &txData[16]);
      decaduino.encodeUint40(t32, &txData[31]);
      decaduino.encodeUint40(t33, &txData[41]);
      Serial.println("Envoie message à AR");
      decaduino.pdDataRequest(txData, 46);
      timeout = millis() + TIMEOUT_WAIT_ACK_REQ_SENT;
      state = WAIT_A3_SENT;
      break;

    case WAIT_A3_SENT:
    if (millis() > timeout){
      state = ODS_ENV_MSG;
      Serial.println("Timeout Send");
    }else{
      if(decaduino.hasTxSucceeded()){
        state = ODS_INIT;
      }
     }

    default:
      state = ODS_INIT;
      break;
  }
}
