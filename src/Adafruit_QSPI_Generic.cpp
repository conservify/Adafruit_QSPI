/*
 * Flash_Generic.cpp
 *
 *  Created on: Jan 2, 2018
 *      Author: deanm
 */

#include "Adafruit_QSPI_Generic.h"

#define ADAFRUIT_QSPI_GENERIC_STATUS_BUSY 0x01

// Include all possible supported flash devices used by Adafruit boards
const external_flash_device possible_devices[] =
{
  GD25Q16C, GD25Q64C,
  S25FL116K, S25FL216K,
  W25Q16FW, W25Q32BV, W25Q64JV_IQ,
  MX25R6435F,
};

enum
{
  EXTERNAL_FLASH_DEVICE_COUNT = sizeof(possible_devices)/sizeof(possible_devices[0])
};

Adafruit_QSPI_Generic::Adafruit_QSPI_Generic(void) : Adafruit_SPIFlash(0)
{
  _flash_dev = NULL;
}

/**************************************************************************/
/*! 
    @brief begin the default QSPI peripheral
    @returns true
*/
/**************************************************************************/
bool Adafruit_QSPI_Generic::begin(void){

	QSPI0.begin();

	uint8_t jedec_ids[3];
	QSPI0.readCommand(QSPI_CMD_READ_JEDEC_ID, jedec_ids, 3);

	for (uint8_t i = 0; i < EXTERNAL_FLASH_DEVICE_COUNT; i++) {
	  const external_flash_device* possible_device = &possible_devices[i];
	  if (jedec_ids[0] == possible_device->manufacturer_id &&
	      jedec_ids[1] == possible_device->memory_type &&
	      jedec_ids[2] == possible_device->capacity) {
	    _flash_dev = possible_device;
	    break;
	  }
	}

	if (_flash_dev == NULL) return false;

  // We don't know what state the flash is in so wait for any remaining writes and then reset.

  // The write in progress bit should be low.
  while ( readStatus() & 0x01 ) {}

  // The suspended write/erase bit should be low.
  while ( readStatus2() & 0x80 ) {}

  QSPI0.runCommand(QSPI_CMD_ENABLE_RESET);
  QSPI0.runCommand(QSPI_CMD_RESET);

  // Wait 30us for the reset
  delayMicroseconds(30);

  // Enable Quad Mode if available
  if (_flash_dev->quad_enable_bit_mask)
  {
    // Verify that QSPI mode is enabled.
    uint8_t status = _flash_dev->single_status_byte ? readStatus() : readStatus2();

    // Check the quad enable bit.
    if ((status & _flash_dev->quad_enable_bit_mask) == 0) {
        writeEnable();

        uint8_t full_status[2] = {0x00, _flash_dev->quad_enable_bit_mask};

        if (_flash_dev->write_status_register_split) {
            QSPI0.writeCommand(QSPI_CMD_WRITE_STATUS2, full_status + 1, 1);
        } else if (_flash_dev->single_status_byte) {
            QSPI0.writeCommand(QSPI_CMD_WRITE_STATUS, full_status + 1, 1);
        } else {
            QSPI0.writeCommand(QSPI_CMD_WRITE_STATUS, full_status, 2);
        }
    }
  }

  if (_flash_dev->has_sector_protection)  {
    writeEnable();

    // Turn off sector protection
    uint8_t data[1] = {0x00};
    QSPI0.writeCommand(QSPI_CMD_WRITE_STATUS, data, 1);
  }

  // Turn off writes in case this is a microcontroller only reset.
  QSPI0.runCommand(QSPI_CMD_WRITE_DISABLE);

  // wait for flash ready
  while ( readStatus() & 0x01 ) {}

  // Adafruit_SPIFlash variables
  currentAddr = 0;
  totalsize = _flash_dev->total_size;

//  type, addrsize'
  pagesize = 256;
  pages = totalsize/256;

	return true;
}

#if 0
/**************************************************************************/
/*! 
    @brief set the type of flash. Setting the type is necessary for use with a FAT filesystem.
    @param t the type of flash to set.
	@returns true if a valid flash type was passed in, false otherwise.
*/
/**************************************************************************/
bool Adafruit_QSPI_Generic::setFlashType(spiflash_type_t t){
  type = t;

  if (type == SPIFLASHTYPE_W25Q16BV) {
    pagesize = 256;
    addrsize = 24;
    pages = 8192;
    totalsize = pages * pagesize; // 2 MBytes
  } 
  else if (type == SPIFLASHTYPE_25C02) {
    pagesize = 32;
    addrsize = 16;
    pages = 8;
    totalsize = pages * pagesize; // 256 bytes
  } 
  else if (type == SPIFLASHTYPE_W25X40CL) {
    pagesize = 256;
    addrsize = 24;
    pages = 2048;
    totalsize =  pages * pagesize; // 512 Kbytes
  } else if (type == SPIFLASHTYPE_AT25SF041) {
    pagesize = 256;
    addrsize = 24;
    pages = 4096;
    totalsize = pages * pagesize;  // 1 MBytes
  } else if (type == SPIFLASHTYPE_W25Q64) {
	  pagesize = 256;
	  addrsize = 24;
	  pages = 32768;
	  totalsize = pages * pagesize; // 8 MBytes
  }
  else {
    pagesize = 0;
    return false;
  }

  return true;
}
#endif

/**************************************************************************/
/*! 
    @brief read the manufacturer ID and device ID
    @param manufID pointer to where to put the manufacturer ID
	@param deviceID pointer to where to put the device ID
*/
/**************************************************************************/
void Adafruit_QSPI_Generic::GetManufacturerInfo (uint8_t *manufID, uint8_t *deviceID)
{
  uint32_t jedec_id = GetJEDECID();

	*deviceID = (uint8_t) (jedec_id & 0xffUL);
	*manufID  = (uint8_t) (jedec_id >> 16);
}

/**************************************************************************/
/*! 
    @brief read JEDEC ID information from the device
	@returns the read id as a uint32
*/
/**************************************************************************/
uint32_t Adafruit_QSPI_Generic::GetJEDECID (void)
{
	uint8_t ids[3];
	QSPI0.readCommand(QSPI_CMD_READ_JEDEC_ID, ids, 3);

	return (ids[0] << 16) | (ids[1] << 8) | ids[2];
}

/**************************************************************************/
/*! 
    @brief read the generic status register.
    @returns the status register reading
*/
/**************************************************************************/
uint8_t Adafruit_QSPI_Generic::readStatus(void)
{
	uint8_t r;
	QSPI0.readCommand(QSPI_CMD_READ_STATUS, &r, 1);
	return r;
}

uint8_t Adafruit_QSPI_Generic::readStatus2(void)
{
	uint8_t r;
	QSPI0.readCommand(QSPI_CMD_READ_STATUS2, &r, 1);
	return r;
}

bool Adafruit_QSPI_Generic::writeEnable(void)
{
  return QSPI0.runCommand(QSPI_CMD_WRITE_ENABLE);
}

// Read flash contents into buffer
uint32_t Adafruit_QSPI_Generic::readBuffer (uint32_t address, uint8_t *buffer, uint32_t len)
{
  return QSPI0.readMemory(address, buffer, len) ? len : 0;
}

// Write buffer into flash
uint32_t Adafruit_QSPI_Generic::writeBuffer (uint32_t address, uint8_t *buffer, uint32_t len)
{
	return QSPI0.writeMemory(address, buffer, len) ? len : 0;
}

uint8_t Adafruit_QSPI_Generic::read8(uint32_t addr)
{
	uint8_t ret;
	return readBuffer(addr, &ret, sizeof(ret)) ? 0xff : ret;
}

uint16_t Adafruit_QSPI_Generic::read16(uint32_t addr)
{
	uint16_t ret;
	return readBuffer(addr, (uint8_t*) &ret, sizeof(ret)) ? 0xffff : ret;
}

uint32_t Adafruit_QSPI_Generic::read32(uint32_t addr)
{
	uint32_t ret;
	return readBuffer(addr, (uint8_t*) &ret, sizeof(ret)) ? 0xffffffff : ret;
}

/**************************************************************************/
/*! 
    @brief perform a chip erase. All data on the device will be erased.
*/
/**************************************************************************/
void Adafruit_QSPI_Generic::chipErase(void)
{
	writeEnable();
	QSPI0.runCommand(QSPI_CMD_ERASE_CHIP);

	//wait for busy
	while(readStatus() & ADAFRUIT_QSPI_GENERIC_STATUS_BUSY);
}

/**************************************************************************/
/*! 
    @brief erase a block of data
    @param blocknum the number of the block to erase.
*/
/**************************************************************************/
void Adafruit_QSPI_Generic::eraseBlock(uint32_t blocknum)
{
	writeEnable();

//	QSPI0.runInstruction(&cmdSetGeneric[ADAFRUIT_QSPI_GENERIC_CMD_BLOCK64K_ERASE], blocknum*W25Q16BV_BLOCKSIZE, NULL, NULL, 0);
//	QSPI0.
	// TODO not implement

	//wait for busy
	while(readStatus() & ADAFRUIT_QSPI_GENERIC_STATUS_BUSY);
}

/**************************************************************************/
/*! 
    @brief erase a sector of flash
    @param sectorNumber the sector number to erase. The address erased will be (sectorNumber * W25Q16BV_SECTORSIZE)
    @returns true
*/
/**************************************************************************/
bool Adafruit_QSPI_Generic::eraseSector (uint32_t sectorNumber)
{
	uint32_t address = sectorNumber * W25Q16BV_SECTORSIZE;
	QSPI0.eraseSector(address);
	return true;
}
