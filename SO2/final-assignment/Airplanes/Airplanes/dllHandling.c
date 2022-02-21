#include "dllHandling.h"

BOOL setupDll() {
	static HMODULE hLib;
	static BOOL firstTime = TRUE;


	if (firstTime) {
		hLib = LoadLibrary(DLL_NAME);

		if (hLib == NULL) {
			_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel carregar a DLL!\n"));
			return FALSE;
		}

		getNextPosition = (int (*)(int, int, int, int, int*, int*))GetProcAddress(hLib, "move");

		if (getNextPosition == NULL) {
			_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel obter endere�o da fun��o da DLL!\n"));
			return FALSE;
		}

		firstTime = FALSE;

		return TRUE;
	}

	FreeLibrary(hLib);
	return TRUE;
}