// DecaDuinoTWR_client
// A simple implementation of the TWR protocol, client side
// Contributors: Adrien van den Bossche, Réjane Dalcé, Ibrahim Fofana, Robert Try, Thierry Val
// This sketch is part of the DecaDuino Project - refer to the DecaDuino LICENSE file for details
// This implementation includes skew correction as per:
// "Nezo Ibrahim Fofana, Adrien van den Bossche, Réjane Dalcé, Thierry Val, An Original Correction Method for Indoor Ultra Wide Band Ranging-based Localisation System"
// Available at: https://arxiv.org/pdf/1603.06736.pdf


#include <SPI.h>
#include <DecaDuino.h>


// Timeout parameters in milliseconds
#define TIMEOUT_WAIT_START_SENT 5 // Timeout waiting for start message sent
#define TIMEOUT_WAIT_ACK 10       // Timeout waiting for acknowledgment
#define TIMEOUT_WAIT_DATA_REPLY 20 // Timeout waiting for data reply


// Time interval for ranging in milliseconds
#define RANGING_PERIOD 500 // Time period between each ranging operation


// TWR client states: enumeration of state machine states (see protocol documentation)
enum { TWR_ENGINE_STATE_INIT, TWR_ENGINE_STATE_WAIT_START_SENT, TWR_ENGINE_STATE_MEMORISE_T1,
TWR_ENGINE_STATE_WAIT_ACK, TWR_ENGINE_STATE_MEMORISE_T4, TWR_ENGINE_STATE_WAIT_DATA_REPLY,
TWR_ENGINE_STATE_EXTRACT_T2_T3 };


// Message types for TWR protocol
#define TWR_MSG_TYPE_UNKNOWN 0
#define TWR_MSG_TYPE_START 1
#define TWR_MSG_TYPE_ACK 2
#define TWR_MSG_TYPE_DATA_REPLY 3


uint64_t t1, t2, t3, t4; // Variables for timestamps
uint64_t mask = 0xFFFFFFFFFF; // Mask for handling timestamp overflow
int32_t tof; // Variable for time of flight (ToF)


DecaDuino decaduino;
uint8_t txData[128];
uint8_t rxData[128];
uint16_t rxLen;
int state;
uint32_t timeout;


void setup() {
  pinMode(13, OUTPUT); // Set internal LED (pin 13 on DecaWiNo board)
  Serial.begin(115200); // Initialize Serial for debugging


  // Initialize DecaDuino, with error handling via LED blink on failure
  if (!decaduino.init()) {
    Serial.println("DecaDuino init failed");
    while(1) { digitalWrite(13, HIGH); delay(50); digitalWrite(13, LOW); delay(50); }
  }


  // Set RX buffer for receiving data
  decaduino.setRxBuffer(rxData, &rxLen);
  state = TWR_ENGINE_STATE_INIT;


  // Print column headers for the output table
  Serial.println("ToF\td\tToF_sk\td_sk");
}


void loop() {
  float distance;


  switch (state) {
    case TWR_ENGINE_STATE_INIT:
      delay(RANGING_PERIOD); // Wait to avoid congestion if a ranging fails
      decaduino.plmeRxDisableRequest(); // Disable RX to initiate a new ranging
      Serial.println("New TWR");
      txData[0] = TWR_MSG_TYPE_START; // Send start message
      decaduino.pdDataRequest(txData, 1);
      timeout = millis() + TIMEOUT_WAIT_START_SENT; // Set timeout
      state = TWR_ENGINE_STATE_WAIT_START_SENT;
      break;


    case TWR_ENGINE_STATE_WAIT_START_SENT:
      if (millis() > timeout) {
        state = TWR_ENGINE_STATE_INIT; // Restart if timeout
      } else if (decaduino.hasTxSucceeded()) {
        state = TWR_ENGINE_STATE_MEMORISE_T1; // Proceed if transmission succeeded
      }
      break;


    case TWR_ENGINE_STATE_MEMORISE_T1:
      t1 = decaduino.getLastTxTimestamp(); // Store timestamp T1
      timeout = millis() + TIMEOUT_WAIT_ACK; // Set timeout for acknowledgment
      decaduino.plmeRxEnableRequest(); // Enable RX to receive acknowledgment
      state = TWR_ENGINE_STATE_WAIT_ACK;
      break;


    case TWR_ENGINE_STATE_WAIT_ACK:
      if (millis() > timeout) {
        state = TWR_ENGINE_STATE_INIT; // Restart if timeout
      } else if (decaduino.rxFrameAvailable()) {
        if (rxData[0] == TWR_MSG_TYPE_ACK) {
          state = TWR_ENGINE_STATE_MEMORISE_T4; // Proceed if acknowledgment received
        } else {
          decaduino.plmeRxEnableRequest(); // Continue waiting for acknowledgment
          state = TWR_ENGINE_STATE_WAIT_ACK;
        }
      }
      break;


    case TWR_ENGINE_STATE_MEMORISE_T4:
      t4 = decaduino.getLastRxTimestamp(); // Store timestamp T4
      timeout = millis() + TIMEOUT_WAIT_DATA_REPLY; // Set timeout for data reply
      decaduino.plmeRxEnableRequest(); // Enable RX to receive data reply
      state = TWR_ENGINE_STATE_WAIT_DATA_REPLY;
      break;


    case TWR_ENGINE_STATE_WAIT_DATA_REPLY:
      if (millis() > timeout) {
        state = TWR_ENGINE_STATE_INIT; // Restart if timeout
      } else if (decaduino.rxFrameAvailable()) {
        if (rxData[0] == TWR_MSG_TYPE_DATA_REPLY) {
          state = TWR_ENGINE_STATE_EXTRACT_T2_T3; // Proceed if data reply received
        } else {
          decaduino.plmeRxEnableRequest(); // Continue waiting for data reply
          state = TWR_ENGINE_STATE_WAIT_DATA_REPLY;
        }
      }
      break;


    case TWR_ENGINE_STATE_EXTRACT_T2_T3:
      t2 = decaduino.decodeUint40(&rxData[1]); // Extract timestamp T2
      t3 = decaduino.decodeUint40(&rxData[6]); // Extract timestamp T3
      tof = (((t4 - t1) & mask) - ((t3 - t2) & mask)) / 2; // Calculate time of flight (ToF)
      distance = tof * RANGING_UNIT; // Calculate distance
      Serial.print(tof);
      Serial.print("\t");
      Serial.print(distance);
      
      // Apply skew correction for more accurate ToF and distance
      tof = (((t4 - t1) & mask) - (1 + 1.0E-6 * decaduino.getLastRxSkew()) * ((t3 - t2) & mask)) / 2;
      distance = tof * RANGING_UNIT;
      Serial.print("\t");
      Serial.print(tof);
      Serial.print("\t");
      Serial.println(distance);
      
      state = TWR_ENGINE_STATE_INIT; // Restart for the next ranging
      break;


    default:
      state = TWR_ENGINE_STATE_INIT; // Reset in case of an undefined state
      break;
  }
}