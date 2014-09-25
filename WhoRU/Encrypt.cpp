#include "stdafx.h"
#include "WhoRU.h"
#include <WinCrypt.h>

#pragma warning(disable:4996) 

#define    KeyLen        0x0080 * 0x10000    // 128-bit

HCRYPTPROV  hProv;
HCRYPTHASH  hHash;
HCRYPTKEY   hKey;

void GenerateKey(char *SeqKey)
{
	// CSP(Crystographic Service Provider) 핸들 얻기
	if (!CryptAcquireContext(&hProv, NULL, MS_ENHANCED_PROV,
		PROV_RSA_FULL, 0))
	{
		// 유저용 키 컨테이너 만들기
		if (!CryptAcquireContext(&hProv, NULL, MS_ENHANCED_PROV,
			PROV_RSA_FULL, CRYPT_NEWKEYSET))
		{
			printf("유저용 키 켄테이너 만들기 에러\n");
			return;
		}
	}

	// 해쉬 만들기
	if (!CryptCreateHash(hProv, CALG_SHA, 0, 0, &hHash))
	{
		printf("해쉬 만들기 에러\n");
		return;
	}

	// 해쉬 값 계산
	if (!CryptHashData(hHash, (BYTE*)SeqKey, (DWORD)strlen(SeqKey), 0))
	{
		printf("해쉬 값 계산 에러\n");
		return;
	}

	// 키 만들기
	if (!CryptDeriveKey(hProv, CALG_RC4, hHash, KeyLen, &hKey))
	{
		printf("키 만들기 에러\n");
		return;
	}
}

void RemoveKey()
{
	// 해쉬 없애기
	CryptDestroyHash(hHash);

	// CSP 핸들 풀어주기
	CryptReleaseContext(hProv, 0);
}

BYTE *Encrypt(BYTE* data, DWORD dwDataLen)
{
	// 암호화
	if (!CryptEncrypt(hKey, 0, TRUE, 0, data, &dwDataLen, MAX_USB_SERIAL))
	{
		printf("암호화 에러\n");
		return NULL;
	}

	return data;
}


BYTE *Decrpty(BYTE *data, DWORD dwDataLen)
{
	// 복호화
	if (!CryptDecrypt(hKey, 0, TRUE, 0, data, &dwDataLen))
	{
		printf("복호화 에러\n");
		return NULL;
	}

	return data;
}