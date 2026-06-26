#ifndef SD_SPI_H
#define SD_SPI_H

#include <stdint.h>

/* Returns 0 on success, negative on error */
int      sd_spi_init(void);
int      sd_spi_card_present(void);   /* 1 = card detected, 0 = no card */
uint32_t sd_spi_get_sector_count(void);
int      sd_spi_read(uint32_t sector, uint8_t *buf, uint32_t count);
int      sd_spi_write(uint32_t sector, const uint8_t *buf, uint32_t count);

#endif /* SD_SPI_H */
