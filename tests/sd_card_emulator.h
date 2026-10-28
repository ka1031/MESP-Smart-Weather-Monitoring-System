#ifndef SD_CARD_EMULATOR_H
#define SD_CARD_EMULATOR_H
#include "sd_spi.h"
#include <stdio.h>
extern SPI_HandleTypeDef test_spi;
extern int card_absent, card_legacy, card_byte_addressed, card_bad_echo;
extern int card_read_error, card_write_reject, card_busy, card_status_error, card_hal_error;
extern unsigned int card_write_commands, card_read_commands, card_init_commands;
extern uint32_t card_last_address;
extern char sd_output[8192];
void CardEmulator_Open(const char *path);
void CardEmulator_Close(void);
void CardEmulator_ResetFaults(void);
#endif
