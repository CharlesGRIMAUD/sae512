#include <SPI.h>
#include <DecaDuino.h>

// Timeout parameters in milliseconds
#define TIMEOUT_WAIT_REQUEST 20 // Timeout waiting for request
#define RESPONSE_DELAY 10       // Delay before sending response

// ODS state machine enumeration
enum { ODS_STATE_INIT, ODS_STATE_WAIT_REQUEST, ODS_STATE_SEND_ACK, ODS_STATE_SEND_DATA_REPLY };

// Message types for ODS protocol
#define ODS_MSG_TYPE_REQUEST 1
#define ODS_MSG_TYPE_ACK 2
#define ODS_MSG_TYPE_DATA_REPLY 3

#ifdef ARDUINO_DWM1001_DEV
DecaDuino decaduino(SS1, DW_IRQ);
#else
DecaDuino decaduino;
#endif

uint8_t txData[128];
uint8_t rxData[128];
uint16_t rxLen;
int state;

void setup() {
  Serial.begin(115200); // Initialize Serial communication

  // Initialize DecaDuino
  if (!decaduino.init()) {
    Serial.println("Initialization failed");
  }
  Serial.println("Initialization successful");

  // Set RX buffer
  decaduino.setRxBuffer(rxData, &rxLen);
  state = ODS_STATE_INIT;
}

void loop() {
  switch (state) {
    decaduino.engine();
    case ODS_STATE_INIT:
      Serial.println("Initializing ODS server...");
      // Activate receiving mode
      decaduino.plmeRxEnableRequest();
      state = ODS_STATE_WAIT_REQUEST;
      break;

    case ODS_STATE_WAIT_REQUEST:
      // Wait for incoming request
      if (decaduino.rxFrameAvailable()) {
        if (rxData[0] == ODS_MSG_TYPE_REQUEST) {
          Serial.println("Received ODS request");
          state = ODS_STATE_SEND_ACK;
        } else {
          Serial.println("Received unexpected message type");
        }
      }
      break;

    case ODS_STATE_SEND_ACK:
      // Send acknowledgment (ACK)
      Serial.println("Sending ACK...");
      txData[0] = ODS_MSG_TYPE_ACK;
      decaduino.pdDataRequest(txData, 1);
      delay(RESPONSE_DELAY); // Small delay before sending data reply
      state = ODS_STATE_SEND_DATA_REPLY;
      break;

    case ODS_STATE_SEND_DATA_REPLY:
      // Send data reply
      Serial.println("Sending data reply...");
      txData[0] = ODS_MSG_TYPE_DATA_REPLY;
      txData[1] = 42;  // Example data value
      txData[2] = 99;  // Another example data value
      decaduino.pdDataRequest(txData, 3); // Send 3 bytes: message type + 2 data values
      state = ODS_STATE_WAIT_REQUEST; // Return to wait state
      break;

    default:
      state = ODS_STATE_INIT; // Reset to init state on any other state
      break;
  }
}
