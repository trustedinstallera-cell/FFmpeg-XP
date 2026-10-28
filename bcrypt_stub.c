#include <windows.h>

/* 不链 advapi32，直接手写 Crypt* 的 GetProcAddress 转发 */
typedef HCRYPTPROV (WINAPI *pCryptAcquireContextW)(HCRYPTPROV*, LPCWSTR, LPCWSTR, DWORD, DWORD);
typedef BOOL (WINAPI *pCryptGenRandom)(HCRYPTPROV, DWORD, BYTE*);
typedef BOOL (WINAPI *pCryptReleaseContext)(HCRYPTPROV, DWORD);

static HMODULE hAdvapi = NULL;
static pCryptAcquireContextW _CAC = NULL;
static pCryptGenRandom _CGR = NULL;
static pCryptReleaseContext _CRC = NULL;

static void init_advapi(void)
{
    if (hAdvapi) return;
    hAdvapi = LoadLibraryW(L"advapi32.dll");
    if (!hAdvapi) return;
    _CAC = (pCryptAcquireContextW)GetProcAddress(hAdvapi, "CryptAcquireContextW");
    _CGR = (pCryptGenRandom)GetProcAddress(hAdvapi, "CryptGenRandom");
    _CRC = (pCryptReleaseContext)GetProcAddress(hAdvapi, "CryptReleaseContext");
}

#ifndef STATUS_SUCCESS
#define STATUS_SUCCESS          ((NTSTATUS)0x00000000L)
#endif
#ifndef STATUS_UNSUCCESSFUL
#define STATUS_UNSUCCESSFUL     ((NTSTATUS)0xC0000001L)
#endif
typedef PVOID BCRYPT_ALG_HANDLE;

NTSTATUS WINAPI BCryptGenRandom(BCRYPT_ALG_HANDLE hAlgorithm, PUCHAR pbBuffer,
                                ULONG cbBuffer, ULONG dwFlags)
{
    HCRYPTPROV hProv = 0;
    (void)hAlgorithm;
    (void)dwFlags;
    init_advapi();
    if (!_CAC || !_CGR || !_CRC) return STATUS_UNSUCCESSFUL;
    if (!_CAC(&hProv, NULL, NULL, PROV_RSA_FULL, CRYPT_VERIFYCONTEXT))
        return STATUS_UNSUCCESSFUL;
    _CGR(hProv, cbBuffer, pbBuffer);
    _CRC(hProv, 0);
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptOpenAlgorithmProvider(BCRYPT_ALG_HANDLE *phAlgorithm,
                                            LPCWSTR pszAlgId,
                                            LPCWSTR pszImplementation,
                                            DWORD dwFlags)
{
    (void)pszAlgId;
    (void)pszImplementation;
    (void)dwFlags;
    *phAlgorithm = (BCRYPT_ALG_HANDLE)1;
    return STATUS_SUCCESS;
}

NTSTATUS WINAPI BCryptCloseAlgorithmProvider(BCRYPT_ALG_HANDLE hAlgorithm,
                                             DWORD dwFlags)
{
    (void)hAlgorithm;
    (void)dwFlags;
    return STATUS_SUCCESS;
}
