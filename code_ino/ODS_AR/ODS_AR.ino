#include <SPI.h>
#include <DecaDuino.h>


// SDS-TWR server states state machine enumeration: see state diagram on documentation for more details
enum { ODS_INIT, ODS_MSG_A3, ODS_ATT_M, ODS_MSG, ODS_RECEP_A2, ODS_RECEP_A3, WAIT_AR_SENT, WAIT_A3_MSG, CALC_EST};

// Timeout parameters
#define TIMEOUT_WAIT_ACK_REQ_SENT 5000 //ms
#define TIMEOUT_WAIT_ACK 1000 //ms
#define TIMEOUT_WAIT_DATA_REPLY_SENT 2000 //ms

#define RANGING_PERIOD 500 //ms

// Message types of the SDS-TWR protocol
#define ODS_MSG_TYPE_M 0
#define ODS_MSG_TYPE_AR 1
#define ODS_MSG_TYPE_A2 2
#define ODS_MSG_TYPE_A3 3

uint64_t tR1, tR2, t24, t34, t21, t31, t22, t23, t32, t33;
uint64_t mask = 0xFFFFFFFFFF;
int32_t tof, test;
float distance;

#ifdef ARDUINO_DWM1001_DEV
DecaDuino decaduino(SS1, DW_IRQ);
#elif defined(TEENSYDUINO)
DecaDuino decaduino;
#endif

uint8_t txData[128];
uint8_t rxData[128];
uint16_t rxLen;
int state;
uint32_t timeout;

void setup()
{
    Serial.begin(115200); // Init Serial port
    if (!decaduino.init())
    {
        Serial.println("decaduino init failed");
    }

    // Set RX buffer
    decaduino.setRxBuffer(rxData, &rxLen);
    state = ODS_INIT;
}

void loop()
{
    decaduino.engine();

    switch (state)
    {

    case ODS_INIT:
      delay(RANGING_PERIOD);
      decaduino.plmeRxEnableRequest();
      Serial.println("Initialisation avec succès");
      state = ODS_ATT_M;
      break;

    case ODS_ATT_M:
      if (decaduino.rxFrameAvailable())
      {
          if (rxData[0] == ODS_MSG_TYPE_M)
          {
              Serial.println("Message reçu: M");
              tR1 = decaduino.getLastRxTimestamp();
              state = ODS_MSG;
            }
        }
        break;

    case ODS_MSG:
        decaduino.plmeRxDisableRequest();
        txData[0] = ODS_MSG_TYPE_AR;
        decaduino.pdDataRequest(txData, 1);
        Serial.println("Envoie message AR");
        Serial.println(txData[0]);
        timeout = millis() + TIMEOUT_WAIT_ACK_REQ_SENT;
        state = WAIT_AR_SENT;
        break;


    case WAIT_AR_SENT:
      if ( millis() > timeout ) {
        state = ODS_MSG;
        Serial.println("Timeout envoie");
        } else {
        if ( decaduino.hasTxSucceeded() ) {
          state = ODS_RECEP_A2;
        }
        }

    case ODS_RECEP_A2:
      if ( millis() > timeout ) {
        state = ODS_MSG;
        Serial.println("Timeout A2");
        } else {
      decaduino.plmeRxEnableRequest();
      if (decaduino.rxFrameAvailable()) {
        if (rxData[0] == ODS_MSG_TYPE_A3) {
          // Récupérez les informations du message A2
          t24 = decaduino.getLastRxTimestamp();
          t21 = decaduino.decodeUint40(&rxData[11]);
          t22 = decaduino.decodeUint40(&rxData[26]);
          t32 = decaduino.decodeUint40(&rxData[11]);
          Serial.println("Récupération estempille A2");
          timeout = millis() + TIMEOUT_WAIT_ACK_REQ_SENT;
          test=t22 & mask;
          Serial.print("t22");
          Serial.println(test);
          test=t24 & mask;
          Serial.print("t24");
          Serial.println(test);
          test=t21 & mask;
          Serial.print("t21");
          Serial.println(test);
          test=t32 & mask;
          Serial.print("t32");
          Serial.println(test);


          // Passez à l'état suivant et redémarrez le timeout
          state = ODS_MSG_A3;
      } else {
          // Si ce n'est pas le bon type de message, continuez à écouter
          decaduino.plmeRxEnableRequest();
          ODS_MSG;
      }
      }
      }
    break;

    case ODS_MSG_A3:
        decaduino.plmeRxDisableRequest();
        txData[0] = ODS_MSG_TYPE_AR;
        decaduino.pdDataRequest(txData, 1);
        Serial.println("Envoie message AR");
        Serial.println(txData[0]);
        timeout = millis() + TIMEOUT_WAIT_ACK_REQ_SENT;
        state = WAIT_A3_MSG;
        break;

    case WAIT_A3_MSG:
      decaduino.plmeRxEnableRequest();
      if ( millis() > timeout ) {
        state = ODS_MSG_A3;
        Serial.println("Timeout envoie");
        } else {
        if ( decaduino.hasTxSucceeded() ) {
          state = ODS_RECEP_A3;
        }
        }
      break;

    case ODS_RECEP_A3:
      if ( millis() > timeout ) {
          state = ODS_MSG_A3;
          Serial.println("Timeout A3");
          } else {
      if (decaduino.rxFrameAvailable()) {
        if (rxData[0] == ODS_MSG_TYPE_A3)
          {
              t34 = decaduino.getLastRxTimestamp();
              t31 = decaduino.decodeUint40(&rxData[16]);
              t32 = decaduino.decodeUint40(&rxData[31]);
              t33 = decaduino.decodeUint40(&rxData[41]);
              Serial.println("Récupération estempille A3");
              state = CALC_EST; // Retour à l'état initial après A3
          }
          else{
            state = ODS_MSG_A3;
          }
      }
          }

      break;

    case CALC_EST:
      // Calcul du temps de vol (tof)
      tof = (((t24 - tR2) & mask) - ((t23 - t22) & mask)) / 2;
      distance = (tof * RANGING_UNIT);  // Conversion en entier
      test=t22 & mask;
      Serial.print("t22");
      Serial.println(test);
      Serial.print("Temps de vol ");
      Serial.println(tof);  // Affiche la distance entière
      // Affichage de la distance calculée
      Serial.print("Distance calculée: ");
      Serial.println(distance);  // Affiche la distance entière
      state = ODS_INIT;

    break;


    default:
        state = ODS_INIT;
        break;
    }
    }