/*-----------------------------------------------------------------------*/
/* Low level disk I/O module SKELETON for FatFs     (C)ChaN, 2019        */
/*-----------------------------------------------------------------------*/
/* If a working storage control module is available, it should be        */
/* attached to the FatFs via a glue function rather than modifying it.   */
/* This is an example of glue functions to attach various exsisting      */
/* storage control modules to the FatFs module with a defined API.       */
/*-----------------------------------------------------------------------*/

// sd card specification:
// https://www.sdcard.org/downloads/pls/pdf/?p=Part1_Physical_Layer_Simplified_Specification_Ver9.00.jpg&f=Part1_Physical_Layer_Simplified_Specification_Ver9.00.pdf&e=EN_SS1_9

#include "ff.h"			/* Obtains integer types */
#include "diskio.h"		/* Declarations of disk functions */
#include "stm32f4xx.h"
#include "main.h"
#include <string.h>
#include <stdio.h>

#define CS_LOW()			HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_RESET)
#define CS_HIGH()			HAL_GPIO_WritePin(SPI1_CS_GPIO_Port, SPI1_CS_Pin, GPIO_PIN_SET)

extern SPI_HandleTypeDef hspi1;

static volatile uint8_t sd_initialized = 0;
static volatile uint8_t sd_ccs;

#define SPI_HANDLE hspi1

#if FF_MIN_SS != FF_MAX_SS
#error "Variable sector size not supported"
#else
#define SECTOR_SIZE FF_MAX_SS
#endif

/* Response lengths */
#define R1_LEN 1
#define R3_LEN 5
#define R7_LEN 5

#define ASSERT_CS_LOW()		{ spi_send_single_byte(0xff); CS_LOW(); spi_send_single_byte(0xff); }
#define ASSERT_CS_HIGH()	{ spi_send_single_byte(0xff); CS_HIGH(); spi_send_single_byte(0xff); }

// Based on Figure 4-20
static uint8_t crc7(uint64_t in) {
	uint8_t crc = 0;

	for(int i = 0; i < 40; i++) {
		uint8_t bit = (crc & 0x40) != 0;
		crc <<= 1;

		uint8_t xor_flag = bit ^ ((in & 0x8000000000) != 0);
		if(xor_flag) {
			crc ^= 0b1001;
		}
		in <<= 1;
	}
	return (crc << 1) | 1;
}

// Based on Figure 4-21
static uint16_t crc16(uint8_t *data) {
	uint16_t crc = 0;

	for(int i = 0; i < 512; i++) {
		uint8_t in = data[i];
		for(int j = 0; j < 8; j++) {
			uint8_t bit = (crc & 0x8000) != 0;
			crc <<= 1;

			uint8_t xor_flag = bit ^ ((in & 0x80) != 0);
			if(xor_flag) {
				crc ^= 0b1000000100001;
			}
			in <<= 1;
		}
	}

	return crc;
}

static void spi_send_single_byte(uint8_t byte) {
	uint8_t data = byte;
	HAL_SPI_Transmit(&hspi1, &data, 1, 0xffff);
}

static void spi_send_sd_command(uint8_t command, uint32_t arg, uint8_t *response, uint32_t response_length) {
	uint8_t data[6];
	data[0] = (command & 0x7f) | 0x40;
	data[1] = (arg & 0xff000000) >> 24;
	data[2] = (arg & 0x00ff0000) >> 16;
	data[3] = (arg & 0x0000ff00) >> 8;
	data[4] = (arg & 0x000000ff);
	data[5] = crc7(((uint64_t)data[0] << 32) | arg);

	ASSERT_CS_LOW();
	HAL_SPI_Transmit(&hspi1, data, sizeof(data), 0xffff);

	memset(response, 0xff, response_length);
	do {
		HAL_SPI_Receive(&hspi1, response, 1, 0xffff);
	} while(response[0] == 0xff);

	if(response_length > 1) {
		HAL_SPI_Receive(&hspi1, response + 1, response_length - 1, 0xffff);
	}
	ASSERT_CS_HIGH();
}

static uint8_t spi_send_sd_command_r1(uint8_t command, uint32_t arg) {
	uint8_t response;
	spi_send_sd_command(command, arg, &response, 1);
	return response;
}

#define SD_READ_ERROR					0x01
#define SD_READ_CARD_CONTROLLER_ERROR	0x02
#define SD_READ_ECC_FAILED				0x04
#define SD_READ_OUT_OF_RANGE			0x08
#define SD_READ_BAD_R1					0x10
#define SD_READ_BAD_CRC					0x20
#define SD_READ_UNKNOWN					0x40
static uint8_t spi_sd_read_block(uint32_t block_index, uint8_t *data) {
	uint8_t ret = 0;

	uint32_t address = (sd_ccs ? block_index : block_index * SECTOR_SIZE);
	if(spi_send_sd_command_r1(17, address) != 0x00)
		return SD_READ_BAD_R1;

	ASSERT_CS_LOW();
	uint8_t received = 0xff;
	do {
		HAL_SPI_Receive(&hspi1, &received, 1, 0xffff);
	} while(received == 0xff);

	if((received & 0xf0) == 0) {
		ret = received;
	} else if(received != 0xfe) {
		ret = SD_READ_UNKNOWN;
	} else {
		memset(data, 0xff, SECTOR_SIZE);
		uint16_t crc = 0xffff;
		HAL_SPI_Receive(&hspi1, data, SECTOR_SIZE, 0xffff);
		HAL_SPI_Receive(&hspi1, (uint8_t*)&crc, 2, 0xffff);
		uint16_t crc_swapped = crc << 8 | crc >> 8;

		if(crc_swapped != crc16(data)) {
			ret = SD_READ_BAD_CRC;
		}
	}

	ASSERT_CS_HIGH();
	return ret;
}

// initialization based on Figure 7-2: SPI Mode Initialization Flow
static uint8_t spi_init_sd(void) {
	uint8_t buffer[16];
	memset(buffer, 0xff, sizeof(buffer));

	// need at least 74 cycles before init (ceil[74 / 8] = 10 bytes)
	HAL_SPI_Transmit(&hspi1, buffer, 10, 0xffff);

	spi_send_sd_command_r1(0, 0); // reset
	spi_send_sd_command(8, 0x1aa, buffer, R7_LEN); // check voltage
	spi_send_sd_command_r1(59, 1); // enable CRC
	if(buffer[0] == 0x01) { // CMD8 is legal: newer card
		spi_send_sd_command(58, 0, buffer, R3_LEN); // read OCR
		if(buffer[0] != 0x01) return 1; // according to spec, this should not happen

		do {
			spi_send_sd_command_r1(55, 0); // next command is ACMD
			buffer[0] = spi_send_sd_command_r1(41, 0x40000000); // initialize
		} while(buffer[0] == 0x01);

		if(buffer[0] != 0x00) return 1; // init failed
		
		spi_send_sd_command(58, 0, buffer, R3_LEN); // read OCR again
		if(buffer[0] != 0x00) return 1; // according to spec, this should not happen
		
		sd_ccs = (buffer[1] & 0x40) != 0;
	} else { // illegal command: older card
		spi_send_sd_command(58, 0, buffer, R3_LEN); // read OCR
		if(buffer[0] != 0x01) return 1; // according to spec, this should not happen

		do {
			spi_send_sd_command_r1(55, 0); // next command is ACMD
			buffer[0] = spi_send_sd_command_r1(41, 0); // initialize
		} while(buffer[0] == 0x01);
		if(buffer[0] != 0x00) return 1; // init failed

		sd_ccs = 0;
	}

	if(spi_send_sd_command_r1(16, SECTOR_SIZE) != 0x00) { // set block size
		return 1;
	}
	return 0;
}

/*-----------------------------------------------------------------------*/
/* Get Drive Status                                                      */
/*-----------------------------------------------------------------------*/

DSTATUS disk_status (
	BYTE pdrv		/* Physical drive nmuber to identify the drive */
)
{
	if(sd_initialized) return 0;
	else return STA_NOINIT;
}



/*-----------------------------------------------------------------------*/
/* Inidialize a Drive                                                    */
/*-----------------------------------------------------------------------*/

//TODO: Handle timeouts
DSTATUS disk_initialize (
	BYTE pdrv				/* Physical drive nmuber to identify the drive */
)
{
	if(spi_init_sd() == 0) {
		sd_initialized = 1;
		return 0;
	} else {
		sd_initialized = 0;
		return STA_NOINIT;
	}
}



/*-----------------------------------------------------------------------*/
/* Read Sector(s)                                                        */
/*-----------------------------------------------------------------------*/

DRESULT disk_read (
	BYTE pdrv,		/* Physical drive nmuber to identify the drive */
	BYTE *buff,		/* Data buffer to store read data */
	LBA_t sector,	/* Start sector in LBA */
	UINT count		/* Number of sectors to read */
)
{
	if(sd_initialized == 0) return RES_NOTRDY;
	for(int i = 0; i < count; i++) {
		if(spi_sd_read_block(sector, buff + i) != 0) { return RES_ERROR; }
	}
	return RES_OK;
}



/*-----------------------------------------------------------------------*/
/* Write Sector(s)                                                       */
/*-----------------------------------------------------------------------*/

#if FF_FS_READONLY == 0

DRESULT disk_write (
	BYTE pdrv,			/* Physical drive nmuber to identify the drive */
	const BYTE *buff,	/* Data to be written */
	LBA_t sector,		/* Start sector in LBA */
	UINT count			/* Number of sectors to write */
)
{
	return RES_PARERR;
}

#endif


/*-----------------------------------------------------------------------*/
/* Miscellaneous Functions                                               */
/*-----------------------------------------------------------------------*/

DRESULT disk_ioctl (
	BYTE pdrv,		/* Physical drive nmuber (0..) */
	BYTE cmd,		/* Control code */
	void *buff		/* Buffer to send/receive control data */
)
{
	return RES_PARERR;
}

