#ifndef AIRPLANE_H
#define AIRPLANE_H

#include <tchar.h>
#include <windows.h>
#include <stdlib.h>
#include <string.h>

#include "utils.h"
#include "structs.h"

// Fun��o que cria uma estrutura avi�o
BOOL initAirplane(pAirplane this, pAirport airportsArray, unsigned int airportsSize, unsigned int* airplanesSize, unsigned int capacity, unsigned int velocity, TCHAR* srcAirport);

// Fun��o que inicializa o array de avi�es com valores default
void initAirplanesArray(pAirplane airplanesArray, unsigned int maxAirplanes);

// Copia avi�o src para avi�o dest
void copyAirplane(pAirplane dest, airplane src);

// Mostra info do array de avi�es
void printAirplanesArray(pAirplane airplanesArray, unsigned int airplanesArraySize);

void printAirplane(airplane thisAirplane);

// Remove avi�o do array de avioes
BOOL removeAirplaneFromArray(pAirplane airplaneArray, unsigned int* airplanesArraySize, unsigned int id, HANDLE* hThreadPingArray);

#endif