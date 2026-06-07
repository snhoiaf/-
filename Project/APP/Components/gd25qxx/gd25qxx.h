#ifndef GD25QXX_H
#define GD25QXX_H

#include "gd32f4xx.h"
#include "gd32f4xx_spi.h"
#include "gd32f4xx_gpio.h"

#define SPI_FLASH_PAGE_SIZE 0x100

#define SPI_FLASH_CS_LOW()  gpio_bit_reset(GPIOA, GPIO_PIN_15)
#define SPI_FLASH_CS_HIGH() gpio_bit_set  (GPIOA, GPIO_PIN_15)

#define SPI_FLASH          SPI0
#define ARRAYSIZE          (12)

void test_spi_flash(void);
void spi_flash_init(void);
void spi_flash_sector_erase(uint32_t sector_addr);
void spi_flash_bulk_erase(void);
void spi_flash_page_write(uint8_t *pbuffer, uint32_t write_addr, uint16_t num_byte_to_write);
void spi_flash_buffer_write(uint8_t *pbuffer, uint32_t write_addr, uint16_t num_byte_to_write);
void spi_flash_buffer_read(uint8_t *pbuffer, uint32_t read_addr, uint16_t num_byte_to_read);
uint32_t spi_flash_read_id(void);
void spi_flash_start_read_sequence(uint32_t read_addr);
void spi_flash_write_enable(void);
void spi_flash_wait_for_write_end(void);
uint8_t spi_flash_send_byte_dma(uint8_t byte);
uint16_t spi_flash_send_halfword_dma(uint16_t half_word);
void spi_flash_transmit_receive_dma(uint8_t *tx_buffer, uint8_t *rx_buffer, uint16_t size);
void spi_flash_wait_for_dma_end(void);

#endif /* GD25QXX_H */



