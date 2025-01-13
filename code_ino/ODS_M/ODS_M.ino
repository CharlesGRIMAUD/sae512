#include <SPI.h>
#include <DecaDuino.h>

// Initialization of the DecaDuino object
#ifdef ARDUINO_DWM1001_DEV
DecaDuino decaduino(SS1, DW_IRQ);
#else
DecaDuino decaduino;
#endif

uint8_t txData[1]; // Data to send (1 byte)
unsigned long lastEmitTime = 0;         // Time of the last emission
const unsigned long emitInterval = 30000; // Emission interval in ms (30 seconds)

void setup() {
  Serial.begin(115200); // Initialize serial port

  // Initialize the DecaDuino module
  if (!decaduino.init()) {
    Serial.println("Error: DecaDuino initialization failed");
    while (1) {
      // Infinite loop in case of failure
    }
  }

  // Initial message to send
  txData[0] = 0; // Fixed data to emit
}

void loop() {
  // Call the DecaDuino engine (handles internal tasks)
  decaduino.engine();

  // Periodic emission every 30 seconds
  if (millis() - lastEmitTime >= emitInterval) {
    lastEmitTime = millis(); // Update the last emission time

    // Emit the message
    Serial.println("Periodic emission of data: 0");
    decaduino.pdDataRequest(txData, 1); // Send one byte (value 0)
  }
}
