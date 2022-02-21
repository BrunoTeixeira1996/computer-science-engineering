#include "airport.h"

// Fun��o que cria uma estrutura de um aeroporto
BOOL initAirport(pAirport this, unsigned int* airportsSize, TCHAR* name, int x, int y) {
	_tcscpy_s(this->name, _countof(this->name) - 1, name);

	this->coordinates.x = x;
	this->coordinates.y = y;

	this->currentPassengers = 0;

	(*airportsSize)++;

	return TRUE;
}

// Fun��o que inicia o array de aeroportos com valores default
void initAirportsArray(pAirport airportsArray, unsigned int maxAirports) {
	unsigned int i;

	for (i = 0; i < maxAirports; i++) {
		_tcscpy_s(airportsArray[i].name, _countof(airportsArray[i].name) - 1, TEXT(" "));

		airportsArray[i].coordinates.x = -1;
		airportsArray[i].coordinates.y = -1;

		airportsArray[i].maxAirports = maxAirports;

		airportsArray[i].currentPassengers = 0;
	}
}

// Mostra info do array de aeroportos
void printAirportsArray(pAirport airportsArray, unsigned int airportsArraySize) {
	unsigned int i;

	for (i = 0; i < airportsArraySize; i++) {
		if (i != 0) _tprintf(TEXT("===\n"));

		_tprintf(TEXT("%s (%i, %i)\n"), airportsArray[i].name, airportsArray[i].coordinates.x, airportsArray[i].coordinates.y);
		_tprintf(TEXT("Nr. Passageiros: %u\n"), airportsArray[i].currentPassengers);
	}

	_tprintf(TEXT("\n"));
}

// Verifica se � poss�vel criar um novo aeroporto com as caracter�sticas espec�ficadas (Nome nao repetido e coordenadas �nao em cima de outro aeroporto)
BOOL isAirportValid(pAirport airportsArray, unsigned int airportsArraySize, TCHAR* newAirportName, int newAirportX, int newAirportY) {
	unsigned int i;

	for (i = 0; i < airportsArraySize; i++) {
		if (_tcsicmp(airportsArray[i].name, newAirportName) == 0) {
			_ftprintf(stderr, TEXT("[ERRO] J� existe um aeroporto com esse nome!\n"));
			return FALSE;
		}

		if (airportsArray[i].coordinates.x + 10 >= newAirportX && airportsArray[i].coordinates.y + 10 >= newAirportY && airportsArray[i].coordinates.x - 10 <= newAirportX && airportsArray[i].coordinates.y - 10 <= newAirportY) {
			_ftprintf(stderr, TEXT("[ERRO] Aeroporto muito pr�ximo de um aeroporto j� existente!\n"));
			return FALSE;
		}
	}

	return TRUE;
}