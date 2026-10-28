# FatFs R0.16

Vendored unmodified `ff.c`, `ff.h`, and `diskio.h` from ChaN's official archive:
https://elm-chan.org/fsw/ff/arc/ff16.zip

Archive SHA-256: `99f7dc1f7e095356e4a9e3dbe29959090d8b948afe2bbc5441e52fdf4b85449e`

The license is retained at the top of ff.c and ff.h. `ffconf.h` is project
configuration: FAT12/16/32, 512-byte sectors, one volume, short filenames,
shared sector buffer, no exFAT, no formatting/deletion APIs, no heap or RTOS.
Driver integration is in Application/Src/sd_diskio.c. STM32 HAL SPI files
come unchanged from STM32Cube_FW_F1_V1.8.7, matching the existing HAL package.
