#include <SPI.h>
#include <DecaDuino.h>

// Timeout parameters
#define TIMEOUT_WAIT_ACK_SENT 5 //ms
#define TIMEOUT_WAIT_DATA_REPLY_SENT 5 //ms
#define ACK_DATA_REPLY_INTERFRAME 10 //ms
#define TIMEOUT_WAIT_ACK_REQ_SENT 5000
// TWR server states state machine enumeration: see state diagram on documentation for more details
enum { ODS_INIT, ODS_ATT_MSG_M,ODS_RECUP_T21, ODS_ATT_MSG_AR, ODS_RECUP_T22, ODS_ENV_MSG, WAIT_A2_SENT };

// Message types of the TWR protocol
#define ODS_MSG_TYPE_M 0
#define ODS_MSG_TYPE_AR 1
#define ODS_MSG_TYPE_A2 2
uint64_t timeout;
uint64_t t21, t22, t23;
uint64_t mask =0xFFFFFFFFFF;
#ifdef ARDUINO_DWM1001_DEV
DecaDuino decaduino(SS1, DW_IRQ);
#else
DecaDuino decaduino;
#endif

uint8_t txData[128];
uint8_t rxData[128];
uint16_t rxLen;
int state;
int32_t t21bis,t22bis, t23bis;
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
      Serial.println("Mode réception");
      state = ODS_ATT_MSG_M;
      break;

    case ODS_ATT_MSG_M:
      if ( decaduino.rxFrameAvailable() ) {
        Serial.println("Message reçu");
        if ( rxData[0] == ODS_MSG_TYPE_M) {
          Serial.println("M reçu");
          state = ODS_RECUP_T21;
        } else {
					state = ODS_INIT;
				}
      }
      break;

    case ODS_RECUP_T21:
      t21 = decaduino.getLastRxTimestamp();
      t21bis=t21 & mask;
      Serial.println("t21 :");
      Serial.println(t21bis, HEX);
      Serial.println("Récupère T21");
      state = ODS_ATT_MSG_AR;
      break;

    case ODS_ATT_MSG_AR:
      decaduino.plmeRxEnableRequest();
      if ( decaduino.rxFrameAvailable() ) {
        Serial.println("Message recu 2");
        if ( rxData[0] == 1) {
          Serial.println("AR reçu");
          state = ODS_RECUP_T22;
        } else {
					state = ODS_ATT_MSG_AR;
				}
      }
      break;

    case ODS_RECUP_T22:
      t22 = decaduino.getLastRxTimestamp();
      t22bis=t22&mask;
      Serial.println("Récupère T22");
      Serial.println("t22 :");
      Serial.println(t22bis);
      state = ODS_ENV_MSG;
      break;

    case ODS_ENV_MSG:
      t23 = decaduino.getLastTxTimestamp();
      t23bis=t23&mask;
      Serial.println("t23 :");
      Serial.println(t23bis);
      decaduino.plmeRxDisableRequest();
      txData[0] = 2;
      decaduino.encodeUint40(t21, &txData[11]);
      decaduino.encodeUint40(t22, &txData[26]);
      decaduino.encodeUint40(t23, &txData[36]);
      Serial.println("Envoie message à AR");
      decaduino.pdDataRequest(txData, 46);
      timeout = millis() + TIMEOUT_WAIT_ACK_REQ_SENT;
      state = WAIT_A2_SENT;
      break;

    case WAIT_A2_SENT:
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
