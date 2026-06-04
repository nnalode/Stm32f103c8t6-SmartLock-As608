#ifndef __MAIN_INTERFACE_H
#define __MAIN_INTERFACE_H

#include <stdint.h>

void MainInterface_Display(void);
uint8_t Admin_VerifyPassword(void);
uint8_t User_VerifyPassword(void);
uint8_t InputPassword(char *out_buf, uint8_t len);
void Func_Unlock(void);
void Func_Lock(void);
void Verify_Fingerprint(void);
void CheckRfidCard(void);

#endif