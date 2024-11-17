#include <SPI.h>
#include <DecaDuino.h>


// Timeout parameters
#define TIMEOUT_WAIT_ACK_SENT 5 //ms
#define TIMEOUT_WAIT_DATA_REPLY_SENT 5 //ms
#define ACK_DATA_REPLY_INTERFRAME 10 //ms


// TWR server state machine enumeration
enum { TWR_ENGINE_STATE_INIT, TWR_ENGINE_STATE_WAIT_START, TWR_ENGINE_STATE_MEMORISE_T2,
TWR_ENGINE_STATE_SEND_ACK, TWR_ENGINE_STATE_WAIT_ACK_SENT, TWR_ENGINE_STATE_MEMORISE_T3,
TWR_ENGINE_STATE_SEND_DATA_REPLY, TWR_ENGINE_STATE_WAIT_DATA_REPLY_SENT };


// Message types of the TWR protocol
#define TWR_MSG_TYPE_UNKNOWN 0
#define TWR_MSG_TYPE_START 1
#define TWR_MSG_TYPE_ACK 2
#define TWR_MSG_TYPE_DATA_REPLY 3


uint64_t t2, t3;


DecaDuino decaduino;
uint8_t txData[128];
uint8_t rxData[128];
uint16_t rxLen;
int state;
uint32_t timeout;


void setup()
{
  pinMode(13, OUTPUT); // Internal LED setup on pin 13
  Serial.begin(115200); // Initialize Serial port
  SPI.setSCK(14); // Set SPI clock pin


  // Initialize DecaDuino and blink if initialization fails
  if ( !decaduino.init() ) {
    Serial.println("decaduino init failed");
    while(1) { digitalWrite(13, HIGH); delay(50); digitalWrite(13, LOW); delay(50); }
  }


  // Set RX buffer
  decaduino.setRxBuffer(rxData, &rxLen);
  state = TWR_ENGINE_STATE_INIT;
}


void loop()
{
  switch (state) {
   
    case TWR_ENGINE_STATE_INIT:
      decaduino.plmeRxEnableRequest(); // Enable reception
      state = TWR_ENGINE_STATE_WAIT_START; // Wait for start signal
      break;


    case TWR_ENGINE_STATE_WAIT_START:
      if ( decaduino.rxFrameAvailable() ) { // Check if frame is available
        if ( rxData[0] == TWR_MSG_TYPE_START ) { // If start signal received
          state = TWR_ENGINE_STATE_MEMORISE_T2;
        } else {
          state = TWR_ENGINE_STATE_INIT;
        }
      }
      break;


    case TWR_ENGINE_STATE_MEMORISE_T2:
      t2 = decaduino.getLastRxTimestamp(); // Record timestamp
      state = TWR_ENGINE_STATE_SEND_ACK; // Move to sending ACK
      break;


    case TWR_ENGINE_STATE_SEND_ACK:
      txData[0] = TWR_MSG_TYPE_ACK; // Prepare ACK message
      decaduino.pdDataRequest(txData, 1); // Send ACK message
      timeout = millis() + TIMEOUT_WAIT_ACK_SENT; // Set timeout
      state = TWR_ENGINE_STATE_WAIT_ACK_SENT;
      break;


    case TWR_ENGINE_STATE_WAIT_ACK_SENT:
      if ( millis() > timeout ) { // Check if timeout occurred
        state = TWR_ENGINE_STATE_INIT; // Reset to initial state
      } else {
        if ( decaduino.hasTxSucceeded() ) { // Check if transmission succeeded
          state = TWR_ENGINE_STATE_MEMORISE_T3;  
        }
      }
      break;


    case TWR_ENGINE_STATE_MEMORISE_T3:
      t3 = decaduino.getLastTxTimestamp(); // Record timestamp after ACK
      state = TWR_ENGINE_STATE_SEND_DATA_REPLY;
      break;


    case TWR_ENGINE_STATE_SEND_DATA_REPLY:
      delay(ACK_DATA_REPLY_INTERFRAME); // Interframe delay
      txData[0] = TWR_MSG_TYPE_DATA_REPLY; // Prepare DATA_REPLY message
      decaduino.encodeUint40(t2, &txData[1]); // Encode T2
      decaduino.encodeUint40(t3, &txData[6]); // Encode T3
      decaduino.pdDataRequest(txData, 11); // Send DATA_REPLY
      timeout = millis() + TIMEOUT_WAIT_DATA_REPLY_SENT; // Set timeout
      state = TWR_ENGINE_STATE_WAIT_DATA_REPLY_SENT;
      break;


    case TWR_ENGINE_STATE_WAIT_DATA_REPLY_SENT:
      if ( (millis()>timeout) || (decaduino.hasTxSucceeded()) ) { // Check if timeout or successful transmission
        state = TWR_ENGINE_STATE_INIT; // Return to initial state
      }
      break;
   
    default:
      state = TWR_ENGINE_STATE_INIT;
      break;
  }
}