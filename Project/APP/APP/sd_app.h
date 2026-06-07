#ifndef __SD_APP_H
#define __SD_APP_H

#ifndef SD_FATFS_DEMO_ENABLE
#define SD_FATFS_DEMO_ENABLE  1
#endif

void sd_fatfs_init(void);
void sd_fatfs_test(void);

#endif
