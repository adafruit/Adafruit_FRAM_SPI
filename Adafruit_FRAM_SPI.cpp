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
 *
 *  History
 *  - FUEL4EP: Added sleep mode support
 */

#include <stdlib.h>

#include "Adafruit_FRAM_SPI.h"

/// Enable debug output
#define FRAM_DEBUG 0

/// Supported flash devices
const struct {
  uint8_t manufID;    ///< Manufacture ID
  uint16_t prodID;    ///< Product ID
  uint32_t size;      ///< Size in bytes
  bool support_sleep; ///< Support sleep mode
} _supported_devices[] = {
    // Sorted in numerical order
    // Fujitsu
    {0x04, 0x0101, 2 * 1024UL, false},  // MB85RS16
    {0x04, 0x0302, 8 * 1024UL, false},  // MB85RS64V
    {0x04, 0x2303, 8 * 1024UL, true},   // MB85RS64T
    {0x04, 0x2503, 32 * 1024UL, true},  // MB85RS256TY
    {0x04, 0x2703, 128 * 1024UL, true}, // MB85RS1MT
    {0x04, 0x4803, 256 * 1024UL, true}, // MB85RS2MTA
    {0x04, 0x2803, 256 * 1024UL, true}, // MB85RS2MT
    {0x04, 0x4903, 512 * 1024UL, true}, // MB85RS4MT
    {0x04, 0x490B, 512 * 1024UL, true}, // MB85RS4MTY

    // Cypress
    {0x7F, 0x7F7f, 32 * 1024UL, false}, // FM25V02
    // (manu = 7F7F7F7F7F7FC2, device = 0x2200)

    // Lapis
    {0xAE, 0x8305, 8 * 1024UL, false} // MR45V064B
};

/*!
 *  @brief  Check if the flash device is supported
 *  @param  manufID
 *          ManufactureID to be checked
 *  @param  prodID
 *          ProductID to be checked
 *  @return device index, -1 if not supported
 */
static int get_supported_idx(uint8_t manufID, uint16_t prodID) {
  for (unsigned int i = 0;
       i < sizeof(_supported_devices) / sizeof(_supported_devices[0]); i++) {
    if (manufID == _supported_devices[i].manufID &&
        prodID == _supported_devices[i].prodID)
      return i;
  }

  return -1;
}

/*!
 * @brief  Initialize the SPI FRAM class
 */
void Adafruit_FRAM_SPI::init(void) {
  _nAddressSizeBytes = 0;
  _dev_idx = -1;
}

/*!
 *  @brief  Instantiates a new SPI FRAM class using hardware SPI
 *  @param  cs
 *          Required chip select pin number
 *  @param  *theSPI
 *          SPI interface object, defaults to &SPI
 *  @param  freq
 *          The SPI clock frequency to use, defaults to 1MHz
 */
Adafruit_FRAM_SPI::Adafruit_FRAM_SPI(int8_t cs, SPIClass *theSPI,
                                     uint32_t freq) {
  init();
  spi_dev = new Adafruit_SPIDevice(cs, freq, SPI_BITORDER_MSBFIRST, SPI_MODE0,
                                   theSPI);
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
  init();
  spi_dev = new Adafruit_SPIDevice(cs, clk, miso, mosi, 1000000,
                                   SPI_BITORDER_MSBFIRST, SPI_MODE0);
}

Adafruit_FRAM_SPI::~Adafruit_FRAM_SPI(void) {
  if (spi_dev) {
    delete spi_dev;
  }
}

/*!
 *  @brief  Initializes SPI and configures the chip (call this function before
 *          doing anything else)
 *  @param  nAddressSizeBytes
 *          sddress size in bytes (default 2)
 *  @return true if successful
 */
bool Adafruit_FRAM_SPI::begin(uint8_t nAddressSizeBytes) {
  // not used anymore, since we will use auto-detect size
  (void)nAddressSizeBytes;

  /* Configure SPI */
  if (!spi_dev->begin()) {
    return false;
  }

  /* Make sure we're actually connected */
  uint8_t manufID;
  uint16_t prodID;
  getDeviceID(&manufID, &prodID);

  /* Everything seems to be properly initialised and connected */
  _dev_idx = get_supported_idx(manufID, prodID);

  if (_dev_idx == -1) {
#if FRAM_DEBUG
    Serial.print(F("Unexpected Device: Manufacturer ID = 0x"));
    Serial.print(manufID, HEX);
    Serial.print(F(", Product ID = 0x"));
    Serial.println(prodID, HEX);
#endif

    return false;
  } else {
    uint32_t const fram_size = _supported_devices[_dev_idx].size;

#if FRAM_DEBUG
    Serial.print(F("FRAM Size = 0x"));
    Serial.println(fram_size, HEX);
#endif

    // Detect address size in bytes either 2 or 3 bytes (4 bytes is not
    // supported)
    if (fram_size > 64UL * 1024) {
      setAddressSize(3);
    } else {
      setAddressSize(2);
    }

    return true;
  }
}

/*!
 *  @brief  Enables or disables writing to the SPI flash
 *  @param enable
 *          True enables writes, false disables writes
 *  @return true if successful
 */
bool Adafruit_FRAM_SPI::writeEnable(bool enable) {
  uint8_t cmd;

  if (enable) {
    cmd = OPCODE_WREN;
  } else {
    cmd = OPCODE_WRDI;
  }
  return spi_dev->write(&cmd, 1);
}

/*!
 *  @brief  Writes a byte at the specific FRAM address
 *  @param addr
 *         The 32-bit address to write to in FRAM memory
 *  @param value
 *         The 8-bit value to write at framAddr
 *  @return true if successful
 */
bool Adafruit_FRAM_SPI::write8(uint32_t addr, uint8_t value) {
  uint8_t buffer[10];
  uint8_t i = 0;

  buffer[i++] = OPCODE_WRITE;
  if (_nAddressSizeBytes > 3) {
    buffer[i++] = (uint8_t)(addr >> 24);
  }
  if (_nAddressSizeBytes > 2) {
    buffer[i++] = (uint8_t)(addr >> 16);
  }
  buffer[i++] = (uint8_t)(addr >> 8);
  buffer[i++] = (uint8_t)(addr & 0xFF);
  buffer[i++] = value;

  return spi_dev->write(buffer, i);
}

/*!
 *   @brief  Writes count bytes starting at the specific FRAM address
 *   @param addr
 *           The 32-bit address to write to in FRAM memory
 *   @param values
 *           The pointer to an array of 8-bit values to write starting at addr
 *   @param count
 *           The number of bytes to write
 *   @return true if successful
 */
bool Adafruit_FRAM_SPI::write(uint32_t addr, const uint8_t *values,
                              size_t count) {
  uint8_t prebuf[10];
  uint8_t i = 0;

  prebuf[i++] = OPCODE_WRITE;
  if (_nAddressSizeBytes > 3) {
    prebuf[i++] = (uint8_t)(addr >> 24);
  }
  if (_nAddressSizeBytes > 2) {
    prebuf[i++] = (uint8_t)(addr >> 16);
  }
  prebuf[i++] = (uint8_t)(addr >> 8);
  prebuf[i++] = (uint8_t)(addr & 0xFF);

  return spi_dev->write(values, count, prebuf, i);
}

/*!
 *   @brief  Reads an 8 bit value from the specified FRAM address
 *   @param  addr
 *           The 32-bit address to read from in FRAM memory
 *   @return The 8-bit value retrieved at framAddr
 */
uint8_t Adafruit_FRAM_SPI::read8(uint32_t addr) {
  uint8_t buffer[10], val;
  uint8_t i = 0;

  buffer[i++] = OPCODE_READ;
  if (_nAddressSizeBytes > 3) {
    buffer[i++] = (uint8_t)(addr >> 24);
  }
  if (_nAddressSizeBytes > 2) {
    buffer[i++] = (uint8_t)(addr >> 16);
  }
  buffer[i++] = (uint8_t)(addr >> 8);
  buffer[i++] = (uint8_t)(addr & 0xFF);

  spi_dev->write_then_read(buffer, i, &val, 1);

  return val;
}

/*!
 *   @brief  Read count bytes starting at the specific FRAM address
 *   @param  addr
 *           The 32-bit address to write to in FRAM memory
 *   @param  values
 *           The pointer to an array of 8-bit values to read starting at addr
 *   @param  count
 *           The number of bytes to read
 *   @return true if successful
 */
bool Adafruit_FRAM_SPI::read(uint32_t addr, uint8_t *values, size_t count) {
  uint8_t buffer[10];
  uint8_t i = 0;

  buffer[i++] = OPCODE_READ;
  if (_nAddressSizeBytes > 3) {
    buffer[i++] = (uint8_t)(addr >> 24);
  }
  if (_nAddressSizeBytes > 2) {
    buffer[i++] = (uint8_t)(addr >> 16);
  }
  buffer[i++] = (uint8_t)(addr >> 8);
  buffer[i++] = (uint8_t)(addr & 0xFF);

  return spi_dev->write_then_read(buffer, i, values, count);
}

/*!
 *   @brief  Reads the Manufacturer ID and the Product ID from the IC
 *   @param  manufacturerID
 *          The 8-bit manufacturer ID (Fujitsu = 0x04)
 *   @param productID
 *          The memory density (bytes 15..8) and proprietary
 *          Product ID fields (bytes 7..0). Should be 0x0302 for
 *          the MB85RS64VPNF-G-JNERE1.
 *   @return true if successful
 */
bool Adafruit_FRAM_SPI::getDeviceID(uint8_t *manufacturerID,
                                    uint16_t *productID) {
  uint8_t cmd = OPCODE_RDID;
  uint8_t a[4] = {0, 0, 0, 0};

  if (!spi_dev->write_then_read(&cmd, 1, a, 4)) {
    return false;
  }

  if (a[1] == 0x7f) {
    // Device with continuation code (0x7F) in their second byte
    // Manu ( 1 byte)  - 0x7F - Product (2 bytes)
    *manufacturerID = (a[0]);
    *productID = (a[2] << 8) + a[3];
  } else {
    // Device without continuation code
    // Manu ( 1 byte)  - Product (2 bytes)
    *manufacturerID = (a[0]);
    *productID = (a[1] << 8) + a[2];
  }

  return true;
}

/*!
    @brief  Reads the status register
    @return register value
*/
uint8_t Adafruit_FRAM_SPI::getStatusRegister() {
  uint8_t cmd, val;

  cmd = OPCODE_RDSR;

  spi_dev->write_then_read(&cmd, 1, &val, 1);

  return val;
}

/*!
 *   @brief  Sets the status register
 *   @param  value
 *           value that will be set
 *   @return true if successful
 */
bool Adafruit_FRAM_SPI::setStatusRegister(uint8_t value) {
  uint8_t cmd[2];

  cmd[0] = OPCODE_WRSR;
  cmd[1] = value;

  return spi_dev->write(cmd, 2);
}

/*!
 *   @brief  Sets adress size to provided value
 *   @param  nAddressSize
 *           address size in bytes
 */
void Adafruit_FRAM_SPI::setAddressSize(uint8_t nAddressSize) {
  _nAddressSizeBytes = nAddressSize;
}

/*!
 *  @brief  Enters the FRAM's low power sleep mode
 *  @return true if successful
 */
// WARNING: this method has not yet been validated
bool Adafruit_FRAM_SPI::enterSleep(void) {
  if (_dev_idx == -1 || !_supported_devices[_dev_idx].support_sleep) {
    return false;
  }
  uint8_t cmd = OPCODE_SLEEP;
  return spi_dev->write(&cmd, 1);
}

/*!
 *  @brief  exits the FRAM's low power sleep mode
 *  @return true if successful
 */
// WARNING: this method has not yet been validated
bool Adafruit_FRAM_SPI::exitSleep(void) {
  if (_dev_idx == -1 || !_supported_devices[_dev_idx].support_sleep) {
    return false;
  }

  // Returning to a normal operation from the SLEEP mode is carried out after
  // tREC (Max 400 μs) time from the falling edge of CS
  spi_dev->beginTransactionWithAssertingCS();
  delayMicroseconds(300);
  // It is possible to return CS to H level before tREC time. However, it
  // is prohibited to bring down CS to L level again during tREC period.
  spi_dev->endTransactionWithDeassertingCS();
  delayMicroseconds(100);

  // MB85RS4MTY requires 450us (extra 50us) to wake from "Hibernate"
  if (_supported_devices[_dev_idx].manufID == 0x04 &&
      _supported_devices[_dev_idx].prodID == 0x0B) {
    delayMicroseconds(50);
  }

  return true;
}
