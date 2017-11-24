#include <SPI.h>
#include "Adafruit_FRAM_SPI.h"

/* Example code to interrogate Adafruit SPI FRAM breakout for address size and storage capacity */

/* WARNING: Running this sketch will overwrite all existing data on the FRAM breakout */


uint8_t FRAM_CS = 10;
Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_CS);  // use hardware SPI

uint8_t FRAM_SCK = 13;
uint8_t FRAM_MISO = 12;
uint8_t FRAM_MOSI = 11;
//Or use software SPI, any pins!
//Adafruit_FRAM_SPI fram = Adafruit_FRAM_SPI(FRAM_SCK, FRAM_MISO, FRAM_MOSI, FRAM_CS);

uint8_t           addrSizeInBytes = 2; //Default to address size of two bytes
uint32_t          memSize;

#if defined(ARDUINO_ARCH_SAMD)
// for Zero, output on USB Serial console, remove line below if using programming port to program the Zero!
   #define Serial SerialUSB
#endif


int32_t testReadBack(uint32_t addr, int32_t data) {
  int32_t check = !data;
  fram.writeEnable(true);
  fram.write(addr, (uint8_t*)&data, sizeof(data));
  fram.writeEnable(false);
  fram.read(addr, (uint8_t*)&check, sizeof(check));
  return check;
}

bool testAddrSize(uint8_t addrSize) {
  fram.setAddressSize(addrSize);
  if (testReadBack(0, 0xbeefbead) == 0xbeefbead)
    return true;
  return false;
}


void setup(void) {
  #ifndef ESP8266
    while (!Serial);     // will pause Zero, Leonardo, etc until serial console opens
  #endif

  Serial.begin(9600);
  
  if (fram.begin(addrSizeInBytes)) {
    Serial.println("Found SPI FRAM");
  } else {
    Serial.println("No SPI FRAM found ... check your connections\r\n");
    while (1);
  }
  
  if (testAddrSize(2))
    addrSizeInBytes = 2;
  else if (testAddrSize(3))
    addrSizeInBytes = 3;
  else if (testAddrSize(4))
    addrSizeInBytes = 4;
  else {
    Serial.println("SPI FRAM can not be read/written with any address size\r\n");
    while (1);
  }

  Serial.println("SPI FRAM address size is ");
  Serial.println(addrSizeInBytes);
  Serial.println(" bytes.");
  
  memSize = 0;
  while (testReadBack(memSize, memSize) == memSize) {
    memSize += 4;
  }
  
  Serial.println("SPI FRAM capacity appears to be..");
  Serial.print(memSize); Serial.println(" bytes");
  Serial.print(memSize/0x1000); Serial.println(" KBytes");
  Serial.print((memSize*8)/0x1000); Serial.println(" KBits");
  if (memSize >= 0x100000) {
    Serial.print((memSize*8)/0x100000); Serial.println(" MBits");
  }
}

void loop(void) {

}