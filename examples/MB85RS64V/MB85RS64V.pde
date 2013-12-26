#include <SPI.h>
#include "Adafruit_FRAM_SPI.h"

/* Example code for the Adafruit SPI1 FRAM breakout */
uint8_t CS = 9;
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(CS);
uint16_t          addr = 0;

void setup(void) {
  Serial.begin(9600);
  
  if (fram.begin()) {
    Serial.println("Found SPI FRAM");
  } else {
    Serial.println("No SPI FRAM found ... check your connections\r\n");
    while (1);
  }
  
  // Test write
  // fram.write8(MB85RC_ADDRESS, addr, 0xAB);
}

void loop(void) {
  uint8_t value = fram.read8(addr);
  Serial.print("0x"); Serial.print(addr, HEX); Serial.print(": ");
  Serial.println(value, HEX);
  addr++;
  delay(1000);
}
