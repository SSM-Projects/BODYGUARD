#include <WinCrypt.h>

void GenerateKey(char *serialnumber);
void RemoveKey();
BYTE *Encrypt(BYTE* data, DWORD dwDataLen);
BYTE *Decrpty(BYTE *data, DWORD dwDataLen);