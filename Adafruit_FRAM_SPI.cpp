/**************************************************************************/
/*!
    @file     Adafruit_FRAM_SPI.cpp
    @author   KTOWN (Adafruit Industries)
    @license  BSD (see license.txt)

    Driver for the Adafruit SPI FRAM breakout.

    Adafruit invests time and resources providing this open source code,
    please support Adafruit and open-source hardware by purchasing
    products from Adafruit!

    @section  HISTORY

    v1.0 - First release
*/
/**************************************************************************/
//#include <avr/pgmspace.h>
//#include <util/delay.h>
#include <stdlib.h>
#include <math.h>

#include "Adafruit_FRAM_SPI.h"

/*========================================================================*/
/*                            CONSTRUCTORS                                */
/*========================================================================*/

/**************************************************************************/
/*!
    Constructor
*/
/**************************************************************************/
Adafruit_FRAM_SPI::Adafruit_FRAM_SPI()
{
  _cs = _clk = _mosi = _miso = -1;
  _framInitialised = false;
}
Adafruit_FRAM_SPI::Adafruit_FRAM_SPI(int8_t cs)
{
  _cs = cs;
  _clk = _mosi = _miso = -1;
  _framInitialised = false;
}

Adafruit_FRAM_SPI::Adafruit_FRAM_SPI(int8_t clk, int8_t miso, int8_t mosi, int8_t cs)
{
  _cs = cs; _clk = clk; _mosi = mosi; _miso = miso;
  _framInitialised = false;
}

/*========================================================================*/
/*                           SPI CLAIM UTILITY                            */
/*========================================================================*/

// Set SPI_FREQ based on empirical board maxima. The FRAM modules supports 20MHz.
#if defined(SPI_FREQ)
  // Specified by library user in compiler flags or pre-#define
#elif defined(__SAM3X8E__)
   #define SPI_FREQ 9300000 // 9.3 MHz
#elif defined(STM32F2XX)
	// Is seems the photon SPI0 clock runs at 60MHz, but SPI1 runs at
	// 30MHz, so the DIV will need to change if this is ever extended
	// to cover SPI1
   #define SPI_FREQ 15000000 // Particle Photon SPI @ 15MHz
#else
   #define SPI_FREQ 8000000 // 8 MHz
#endif

// Use Resource acquisition is initialization (RAII)
//   ClaimSPI working(cs, clk) // One statement before SPI use
// - ctor will activate cs and begin SPI transaction
// - dtor will close transaction and deactive cs at end of scope
// Optimizing compiler inlines so there is no overhead for this simple
// guarantee of consistency. See assembly here https://godbolt.org/g/ZvaRSG
class ClaimSPI {
 public:
  ClaimSPI(int8_t cs, int8_t clk)
  : _cs(cs), _hardwareSPI(clk==-1) {
      if (_hardwareSPI) {
        SPI.beginTransaction(SPISettings(SPI_FREQ, MSBFIRST, SPI_MODE0));
      }
      digitalWrite(_cs, LOW);
  }
  ~ClaimSPI() {
      digitalWrite(_cs, HIGH);
      if (_hardwareSPI) {
        SPI.endTransaction();
      }
  }
 private:
  const int8_t _cs;
  const bool _hardwareSPI;
};

/*========================================================================*/
/*                           PUBLIC FUNCTIONS                             */
/*========================================================================*/
/**************************************************************************/
/*!
    Initializes SPI and configures the chip (call this function before
    doing anything else)
*/
/**************************************************************************/
boolean Adafruit_FRAM_SPI::begin()
{
	return begin(_cs, 2);
}

boolean Adafruit_FRAM_SPI::begin(uint8_t nAddressSizeBytes)
{
	return begin(_cs, nAddressSizeBytes);
}

boolean Adafruit_FRAM_SPI::begin(int8_t cs, uint8_t nAddressSizeBytes)
{
  if (cs == -1)
  {
    //Serial.println("No cs pin specified!");
    return false;
  }

  _cs = cs;
  setAddressSize(nAddressSizeBytes);

  /* Configure SPI */
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);

  if (_clk == -1) { // hardware SPI!
    SPI.begin();

  } else {
    pinMode(_clk, OUTPUT);
    pinMode(_mosi, OUTPUT);
    pinMode(_miso, INPUT);
  }

  /* Make sure we're actually connected */
  uint8_t manufID;
  uint16_t prodID;
  getDeviceID(&manufID, &prodID);

  if (manufID != 0x04 && manufID != 0x7f)
  {
    //Serial.print("Unexpected Manufacturer ID: 0x"); Serial.println(manufID, HEX);
    return false;
  }
  if (prodID != 0x0302 && prodID != 0x7f7f)
  {
    //Serial.print("Unexpected Product ID: 0x"); Serial.println(prodID, HEX);
    return false;
  }

  /* Everything seems to be properly initialised and connected */
  _framInitialised = true;

  return true;
}

/**************************************************************************/
/*!
    @brief  Enables or disables writing to the SPI flash

    @params[in] enable
                True enables writes, false disables writes
*/
/**************************************************************************/
void Adafruit_FRAM_SPI::writeEnable (bool enable)
{
  ClaimSPI working(_cs, _clk);
  if (enable)
  {
    SPItransfer(OPCODE_WREN);
  }
  else
  {
    SPItransfer(OPCODE_WRDI);
  }
}

/**************************************************************************/
/*!
    @brief  Writes a byte at the specific FRAM address

    @params[in] addr
                The 32-bit address to write to in FRAM memory
    @params[in] i2cAddr
                The 8-bit value to write at framAddr
*/
/**************************************************************************/
void Adafruit_FRAM_SPI::write8 (uint32_t addr, uint8_t value)
{
  ClaimSPI working(_cs, _clk);
  SPItransfer(OPCODE_WRITE);
  writeAddress(addr);
  SPItransfer(value);
  /* CS on the rising edge commits the WRITE */
}

/**************************************************************************/
/*!
    @brief  Writes count bytes starting at the specific FRAM address

    @params[in] addr
                The 32-bit address to write to in FRAM memory
    @params[in] values
                The pointer to an array of 8-bit values to write starting at addr
    @params[in] count
                The number of bytes to write
*/
/**************************************************************************/
void Adafruit_FRAM_SPI::write (uint32_t addr, const uint8_t *values, size_t count)
{
  ClaimSPI working(_cs, _clk);
  SPItransfer(OPCODE_WRITE);
  writeAddress(addr);
  for (size_t i = 0; i < count; i++)
  {
    SPItransfer(values[i]);
  }
  /* CS on the rising edge commits the WRITE */
}

/**************************************************************************/
/*!
    @brief  Reads an 8 bit value from the specified FRAM address

    @params[in] addr
                The 32-bit address to read from in FRAM memory

    @returns    The 8-bit value retrieved at framAddr
*/
/**************************************************************************/
uint8_t Adafruit_FRAM_SPI::read8 (uint32_t addr)
{
  ClaimSPI working(_cs, _clk);
  SPItransfer(OPCODE_READ);
  writeAddress(addr);
  uint8_t x = SPItransfer(0);
  return x;
}

/**************************************************************************/
/*!
    @brief  Read count bytes starting at the specific FRAM address

    @params[in] addr
                The 32-bit address to write to in FRAM memory
    @params[out] values
                The pointer to an array of 8-bit values to read starting at addr
    @params[in] count
                The number of bytes to read
*/
/**************************************************************************/
void Adafruit_FRAM_SPI::read (uint32_t addr, uint8_t *values, size_t count)
{
  ClaimSPI working(_cs, _clk);
  SPItransfer(OPCODE_READ);
  writeAddress(addr);
  for (size_t i = 0; i < count; i++)
  {
    uint8_t x = SPItransfer(0);
    values[i] = x;
  }
}

/**************************************************************************/
/*!
    @brief  Reads the Manufacturer ID and the Product ID from the IC

    @params[out]  manufacturerID
                  The 8-bit manufacturer ID (Fujitsu = 0x04)
    @params[out]  productID
                  The memory density (bytes 15..8) and proprietary
                  Product ID fields (bytes 7..0). Should be 0x0302 for
                  the MB85RS64VPNF-G-JNERE1.
*/
/**************************************************************************/
void Adafruit_FRAM_SPI::getDeviceID(uint8_t *manufacturerID, uint16_t *productID)
{
  uint8_t a[4] = { 0, 0, 0, 0 };
  //uint8_t results;

  { // Create block in which SPI is claimed. Released at end of block.
    ClaimSPI working(_cs, _clk);
    SPItransfer(OPCODE_RDID);
    a[0] = SPItransfer(0);
    a[1] = SPItransfer(0);
    a[2] = SPItransfer(0);
    a[3] = SPItransfer(0);
  }

  /* Shift values to separate manuf and prod IDs */
  /* See p.10 of http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RS64V-DS501-00015-4v0-E.pdf */
  *manufacturerID = (a[0]);
  *productID = (a[2] << 8) + a[3];
}

/**************************************************************************/
/*!
    @brief  Reads the status register
*/
/**************************************************************************/
uint8_t Adafruit_FRAM_SPI::getStatusRegister(void)
{
  uint8_t reg = 0;
  ClaimSPI working(_cs, _clk);
  SPItransfer(OPCODE_RDSR);
  reg = SPItransfer(0);
  return reg;
}

/**************************************************************************/
/*!
    @brief  Sets the status register
*/
/**************************************************************************/
void Adafruit_FRAM_SPI::setStatusRegister(uint8_t value)
{
  ClaimSPI working(_cs, _clk);
  SPItransfer(OPCODE_WRSR);
  SPItransfer(value);
}

void Adafruit_FRAM_SPI::setAddressSize(uint8_t nAddressSize)
{
  _nAddressSizeBytes = nAddressSize;
}

uint8_t Adafruit_FRAM_SPI::SPItransfer(uint8_t x) {
  if (_clk == -1) {
    return SPI.transfer(x);
  } else {
    // Serial.println("Software SPI");
    uint8_t reply = 0;
    for (int i=7; i>=0; i--) {
      reply <<= 1;
      digitalWrite(_clk, LOW);
      digitalWrite(_mosi, x & (1<<i));
      digitalWrite(_clk, HIGH);
      if (digitalRead(_miso))
	reply |= 1;
    }
    return reply;
  }
}

void Adafruit_FRAM_SPI::writeAddress(uint32_t addr)
{
  if (_nAddressSizeBytes>3)
  	SPItransfer((uint8_t)(addr >> 24));
  if (_nAddressSizeBytes>2)
  	SPItransfer((uint8_t)(addr >> 16));
  SPItransfer((uint8_t)(addr >> 8));
  SPItransfer((uint8_t)(addr & 0xFF));
}
