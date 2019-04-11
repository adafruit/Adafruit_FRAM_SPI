/*!
 * @file Adafruit_FRAM_SPI.cpp
 *
 *  @mainpage Adafruit SPI FRAM breakout.
 *
 *  @section intro_sec Introduction
 *
 *  This is a library for the Adafruit SPI FRAM breakout.
 *
 *  Designed specifically to work with the Adafruit SPI FRAM breakout.
 *
 *  Pick one up today in the adafruit shop!
 *  ------> https://www.adafruit.com/product/1897
 *
 *  These module use SPI to communicate, 4 pins are required to interface.
 *
 *  Adafruit invests time and resources providing this open source code,
 *  please support Adafruit andopen-source hardware by purchasing products
 *  from Adafruit!
 *
 *  @section author Author
 *
 *  K.Townsend (Adafruit Industries)
 *
 *  @section license License
 *
 *  BSD license, all text above must be included in any redistribution
 */

#include <math.h>
#include <stdlib.h>

#include "Adafruit_FRAM_SPI.h"

/*!
 *  @brief  If class instance uses hardware SPI, beginTransaction
 */
void Adafruit_FRAM_SPI::SPI_TRANSACTION_START() {
  if (_clk == -1) {
    _spi->beginTransaction(spiSettings);
  } ///< Pre-SPI
}

/*!
 *  @brief  If class instance uses hardware SPI, endTransaction
 */
void Adafruit_FRAM_SPI::SPI_TRANSACTION_END() {
  if (_clk == -1) {
    _spi->endTransaction();
  } ///< Post-SPI
}

/*!
 *  @brief  Instantiates a new SPI FRAM class using hardware SPI
 *  @param  cs
 *          optional cs pin number
 *  @param  *theSPI
 *          SPI optional object
 */
Adafruit_FRAM_SPI::Adafruit_FRAM_SPI(int8_t cs, SPIClass *theSPI) {
  _cs = cs;
  _clk = _mosi = _miso = -1;
  _framInitialised = false;
  *_spi = *theSPI;
}

/*!
 *  @brief  Instantiates a new SPI FRAM class using bitbang SPI
 *  @param  clk
 *          CLK pin
 *  @param  miso
 *          MISO pin
 *  @param  mosi
 *          MOSI pin
 *  @param  cs
 *          CS pin
 */
Adafruit_FRAM_SPI::Adafruit_FRAM_SPI(int8_t clk, int8_t miso, int8_t mosi,
                                     int8_t cs) {
  _cs = cs;
  _clk = clk;
  _mosi = mosi;
  _miso = miso;
  _framInitialised = false;
}

/*!
 *  @brief  Initializes SPI and configures the chip (call this function before
 *          doing anything else)
 *  @param  nAddressSizeBytes
 *          sddress size in bytes (default 2)
 *  @return true if succesful
 */
boolean Adafruit_FRAM_SPI::begin(uint8_t nAddressSizeBytes) {
  if (_cs == -1) {
    // Serial.println("No cs pin specified!");
    return false;
  }

  setAddressSize(nAddressSizeBytes);

  /* Configure SPI */
  pinMode(_cs, OUTPUT);
  digitalWrite(_cs, HIGH);

  if (_clk == -1) { // hardware SPI!
    _spi->begin();
    spiSettings =
        SPISettings(20000000, MSBFIRST,
                    SPI_MODE0); // Max SPI frequency for MB85RS64V is 20 MHz
  } else {
    pinMode(_clk, OUTPUT);
    pinMode(_mosi, OUTPUT);
    pinMode(_miso, INPUT);
  }

  /* Make sure we're actually connected */
  uint8_t manufID;
  uint16_t prodID;
  getDeviceID(&manufID, &prodID);

  if (manufID != 0x04 && manufID != 0x7f) {
    // Serial.print("Unexpected Manufacturer ID: 0x"); Serial.println(manufID,
    // HEX);
    return false;
  }
  if (prodID != 0x0302 && prodID != 0x7f7f) {
    // Serial.print("Unexpected Product ID: 0x"); Serial.println(prodID, HEX);
    return false;
  }

  /* Everything seems to be properly initialised and connected */
  _framInitialised = true;

  return true;
}

/*!
    @brief  Enables or disables writing to the SPI flash
    @param enable
            True enables writes, false disables writes
*/
void Adafruit_FRAM_SPI::writeEnable(bool enable) {
  SPI_TRANSACTION_START();
  digitalWrite(_cs, LOW);
  if (enable) {
    SPItransfer(OPCODE_WREN);
  } else {
    SPItransfer(OPCODE_WRDI);
  }
  digitalWrite(_cs, HIGH);
  SPI_TRANSACTION_END();
}

/*!
 *  @brief  Writes a byte at the specific FRAM address
 *  @param addr
 *         The 32-bit address to write to in FRAM memory
 *  @param value
 *         The 8-bit value to write at framAddr
 */
void Adafruit_FRAM_SPI::write8(uint32_t addr, uint8_t value) {
  SPI_TRANSACTION_START();
  digitalWrite(_cs, LOW);
  SPItransfer(OPCODE_WRITE);
  writeAddress(addr);
  SPItransfer(value);
  /* CS on the rising edge commits the WRITE */
  digitalWrite(_cs, HIGH);
  SPI_TRANSACTION_END();
}

/*!
 *   @brief  Writes count bytes starting at the specific FRAM address
 *   @param addr
 *           The 32-bit address to write to in FRAM memory
 *   @param values
 *           The pointer to an array of 8-bit values to write starting at addr
 *   @param count
 *           The number of bytes to write
 */
void Adafruit_FRAM_SPI::write(uint32_t addr, const uint8_t *values,
                              size_t count) {
  SPI_TRANSACTION_START();
  digitalWrite(_cs, LOW);
  SPItransfer(OPCODE_WRITE);
  writeAddress(addr);
  for (size_t i = 0; i < count; i++) {
    SPItransfer(values[i]);
  }
  /* CS on the rising edge commits the WRITE */
  digitalWrite(_cs, HIGH);
  SPI_TRANSACTION_END();
}

/*!
 *   @brief  Reads an 8 bit value from the specified FRAM address
 *   @param  addr
 *           The 32-bit address to read from in FRAM memory
 *   @return The 8-bit value retrieved at framAddr
 */
uint8_t Adafruit_FRAM_SPI::read8(uint32_t addr) {
  SPI_TRANSACTION_START();
  digitalWrite(_cs, LOW);
  SPItransfer(OPCODE_READ);
  writeAddress(addr);
  uint8_t x = SPItransfer(0);
  digitalWrite(_cs, HIGH);
  SPI_TRANSACTION_END();

  return x;
}

/*!
 *   @brief  Read count bytes starting at the specific FRAM address
 *   @param  addr
 *           The 32-bit address to write to in FRAM memory
 *   @param  values
 *           The pointer to an array of 8-bit values to read starting at addr
 *   @param  count
 *           The number of bytes to read
 */
void Adafruit_FRAM_SPI::read(uint32_t addr, uint8_t *values, size_t count) {
  SPI_TRANSACTION_START();
  digitalWrite(_cs, LOW);
  SPItransfer(OPCODE_READ);
  writeAddress(addr);
  for (size_t i = 0; i < count; i++) {
    uint8_t x = SPItransfer(0);
    values[i] = x;
  }
  digitalWrite(_cs, HIGH);
  SPI_TRANSACTION_END();
}

/*!
 *   @brief  Reads the Manufacturer ID and the Product ID from the IC
 *   @param  manufacturerID
 *          The 8-bit manufacturer ID (Fujitsu = 0x04)
 *   @param productID
 *          The memory density (bytes 15..8) and proprietary
 *          Product ID fields (bytes 7..0). Should be 0x0302 for
 *          the MB85RS64VPNF-G-JNERE1.
 */
void Adafruit_FRAM_SPI::getDeviceID(uint8_t *manufacturerID,
                                    uint16_t *productID) {
  uint8_t a[4] = {0, 0, 0, 0};
  // uint8_t results;

  SPI_TRANSACTION_START();
  digitalWrite(_cs, LOW);
  SPItransfer(OPCODE_RDID);
  a[0] = SPItransfer(0);
  a[1] = SPItransfer(0);
  a[2] = SPItransfer(0);
  a[3] = SPItransfer(0);
  digitalWrite(_cs, HIGH);
  SPI_TRANSACTION_END();

  /* Shift values to separate manuf and prod IDs */
  /* See p.10 of
   * http://www.fujitsu.com/downloads/MICRO/fsa/pdf/products/memory/fram/MB85RS64V-DS501-00015-4v0-E.pdf
   */
  *manufacturerID = (a[0]);
  *productID = (a[2] << 8) + a[3];
}

/*!
    @brief  Reads the status register
    @return register value
*/
uint8_t Adafruit_FRAM_SPI::getStatusRegister() {
  uint8_t reg = 0;

  SPI_TRANSACTION_START();
  digitalWrite(_cs, LOW);
  SPItransfer(OPCODE_RDSR);
  reg = SPItransfer(0);
  digitalWrite(_cs, HIGH);
  SPI_TRANSACTION_END();

  return reg;
}

/*!
 *   @brief  Sets the status register
 *   @param  value
 *           value that will be set
 */
void Adafruit_FRAM_SPI::setStatusRegister(uint8_t value) {
  SPI_TRANSACTION_START();
  digitalWrite(_cs, LOW);
  SPItransfer(OPCODE_WRSR);
  SPItransfer(value);
  digitalWrite(_cs, HIGH);
  SPI_TRANSACTION_END();
}

/*!
 *   @brief  Sets adress size to provided value
 *   @param  nAddressSize
 *           address size in bytes
 */
void Adafruit_FRAM_SPI::setAddressSize(uint8_t nAddressSize) {
  _nAddressSizeBytes = nAddressSize;
}

uint8_t Adafruit_FRAM_SPI::SPItransfer(uint8_t x) {
  if (_clk == -1) {
    return _spi->transfer(x);
  } else {
    // Serial.println("Software SPI");
    uint8_t reply = 0;
    for (int i = 7; i >= 0; i--) {
      reply <<= 1;
      digitalWrite(_clk, LOW);
      digitalWrite(_mosi, x & (1 << i));
      digitalWrite(_clk, HIGH);
      if (digitalRead(_miso))
        reply |= 1;
    }
    return reply;
  }
}

void Adafruit_FRAM_SPI::writeAddress(uint32_t addr) {
  if (_nAddressSizeBytes > 3)
    SPItransfer((uint8_t)(addr >> 24));
  if (_nAddressSizeBytes > 2)
    SPItransfer((uint8_t)(addr >> 16));
  SPItransfer((uint8_t)(addr >> 8));
  SPItransfer((uint8_t)(addr & 0xFF));
}
