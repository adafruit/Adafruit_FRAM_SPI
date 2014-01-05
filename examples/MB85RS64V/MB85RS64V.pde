#include <SPI.h>
#include "Adafruit_FRAM_SPI.h"

/* Example code for the Adafruit SPI FRAM breakout */
uint8_t CS = 10;
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
  fram.writeEnable(true);
  fram.write8(addr, 0xAB);
  fram.writeEnable(false);
}

void loop(void) {
  uint8_t value = fram.read8(addr);
  Serial.print("0x"); Serial.print(addr, HEX); Serial.print(": ");
  Serial.println(value, HEX);
  addr++;
  delay(1000);
}
