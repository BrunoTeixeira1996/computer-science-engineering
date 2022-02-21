#include "controlThreads.h"

#include "utils.h"
#include "structs.h"
#include "airplane.h"
#include "airport.h"
#include "passenger.h"
#include "keyHandling.h"

// Thread do Consumidor
DWORD WINAPI consumerThread(LPVOID param) {
	pDataThreadStruct data = (pDataThreadStruct)param;
	unsigned int i;
	DWORD error;
	airplane aux;
	BOOL airplaneExists = FALSE;

	HANDLE hGenericEvent;
	TCHAR genericEventName[STR_SIZE] = TEXT("");
	TCHAR tempStr1[STR_SIZE], tempStr2[STR_SIZE];

	unsigned int tempUInt;

	// Entramos na sec��o critica do bool
	EnterCriticalSection(data->criticalSectionBool);
	while (!*data->stop) {
		LeaveCriticalSection(data->criticalSectionBool);

		airplaneExists = FALSE;

		// Esperamos por uma posi��o para ler
		error = WaitForSingleObject(data->hSemaphoreRead, 1000);

		if (error == WAIT_TIMEOUT) {
			EnterCriticalSection(data->criticalSectionBool);
			continue;
		}

		// Copiamos a proxima posicao de leitura do buffer circular para a nossa struct auxiliar
		CopyMemory(
			&aux,
			&data->sharedMemory->buffer[data->sharedMemory->readIndex],
			sizeof(airplane)
		);

		// Incrementa a posicao de leitura
		data->sharedMemory->readIndex++;

		// Entramos na sec��o critica dos avioes
		EnterCriticalSection(data->criticalSectionAirplanes);
		// Verifica se o aviao ja existe, failsafe
		for (i = 0; i < *data->nrAirplanes; i++) {
			if (aux.id == data->airplanesArray[i].id) {
				airplaneExists = TRUE;
				break;
			}
		}
		// Caso exista, atualizamos a struct local
		if (airplaneExists) {
			while (1) {
				// Enquanto estiver � espera de passageiros, assinala evento
				if (aux.getOnBoard && aux.stopped) {
					_tcscpy_s(tempStr1, _countof(tempStr1), aux.srcAirport);
					_tcscpy_s(tempStr2, _countof(tempStr2), aux.destAirport);

					toUpperString(tempStr1);
					toUpperString(tempStr2);

					_stprintf_s(genericEventName, _countof(genericEventName) - 1, TEXT("ONBOARD_%s_%s"), tempStr1, tempStr2);
					hGenericEvent = CreateEvent(
						NULL,
						TRUE,
						FALSE,
						genericEventName
					);

					if (hGenericEvent == NULL) {
						_ftprintf(stderr, TEXT("[ERRO] Um avi�o pediu para passageiros embarcarem, mas n�o foi poss�vel faz�-lo!"));
						break;
					}

					EnterCriticalSection(data->criticalSectionPassengers);
					for (unsigned int i = 0; i < *data->nrPassengers; i++) {
						SetEvent(hGenericEvent);
						ResetEvent(hGenericEvent);
					}
					LeaveCriticalSection(data->criticalSectionPassengers);

					break;
				}

				// Se ele estiver em andamento ou for a primeira vez que para depois de ter viajado, assinala evento
				if (!aux.stopped || (aux.stopped != data->airplanesArray[i].stopped && !data->airplanesArray[i].stopped)) {
					_stprintf_s(genericEventName, _countof(genericEventName) - 1, TEXT("MOVED_%ld"), aux.id);
					hGenericEvent = CreateEvent(
						NULL,
						TRUE,
						FALSE,
						genericEventName
					);

					if (hGenericEvent == NULL) {
						_ftprintf(stderr, TEXT("[ERRO] Um avi�o mexeu-se mas n�o foi poss�vel avisar os passageiros!"));
						break;
					}

					for (unsigned int i = 0; i < *data->nrAirplanes; i++) {
						if (data->airplanesArray[i].id == aux.id) {
							for (unsigned int j = 0; j < data->airplanesArray[i].currentPassengers; j++) {
								SetEvent(hGenericEvent);
								ResetEvent(hGenericEvent);
							}

							if (aux.stopped != data->airplanesArray[i].stopped && !data->airplanesArray[i].stopped) {
								data->airplanesArray[i].currentPassengers = 0;
							}

							break;
						}
					}

					break;
				}

				break;
			}

			tempUInt = data->airplanesArray[i].currentPassengers;

			CopyMemory(
				&data->airplanesArray[i],
				&aux,
				sizeof(airplane)
			);

			data->airplanesArray[i].currentPassengers = tempUInt;

			LeaveCriticalSection(data->criticalSectionAirplanes);

			// Caso cheguemos ao fim do buffer circular, voltamos � primeira posi��o do mesmo
			if (data->sharedMemory->readIndex == CIRCULAR_BUFFER_SIZE) data->sharedMemory->readIndex = 0;

			// Libertamos o sem�foro de escrita
			ReleaseSemaphore(data->hSemaphoreWrite, 1, NULL);

			// Entramos na sec��o critica do bool porque saimos no inicio do while
			EnterCriticalSection(data->criticalSectionBool);
			continue;
		}

		// Se n�o houver espa�o para avioes ...
		if (*data->nrAirplanes >= *data->maxAirplanes) {
			_tprintf(TEXT("[AVISO] Um avi�o tentou conectar-se, mas j� foi atingido o n�mero m�ximo de avi�es!\n"));
			LeaveCriticalSection(data->criticalSectionAirplanes);

			EnterCriticalSection(data->criticalSectionBool);
			continue;
		}

		pPingThreadStruct temp = malloc(sizeof(pingThreadStruct));

		// Adicionamos o aviao � thread dos pings para ele come�ar a enviar pings ao controlador aereo
		temp->thisAirplaneId = aux.id;
		temp->airplanesArray = data->airplanesArray;
		temp->hThreadPingArray = data->hThreadPingArray;
		temp->nrAirplanes = data->nrAirplanes;
		temp->criticalSectionAirplanes = data->criticalSectionAirplanes;
		temp->criticalSectionBool = data->criticalSectionBool;
		temp->criticalSectionPings = data->criticalSectionPings;
		temp->stop = data->stop;
		
		EnterCriticalSection(data->criticalSectionPings);
		// Lan�amos a Thread dos pings
		data->hThreadPingArray[*data->nrAirplanes] = CreateThread(
			NULL,
			0,
			pingThread,
			temp,
			0,
			NULL
		);

		if (data->hThreadPingArray[*data->nrAirplanes] == NULL) {
			_ftprintf(stderr, TEXT("[ERRO] Imposs�vel criar thread dos pings para avi�o %i!\n"), aux.id);

			LeaveCriticalSection(data->criticalSectionAirplanes);
			LeaveCriticalSection(data->criticalSectionPings);

			EnterCriticalSection(data->criticalSectionBool);
			continue;
		}

		LeaveCriticalSection(data->criticalSectionPings);

		// Inserimos o aviao no array de avi�es
		copyAirplane(&data->airplanesArray[(*data->nrAirplanes)++], aux);
		LeaveCriticalSection(data->criticalSectionAirplanes);

		// Caso cheguemos ao fim do buffer circular, voltamos � primeira posi��o do mesmo
		if (data->sharedMemory->readIndex == CIRCULAR_BUFFER_SIZE) data->sharedMemory->readIndex = 0;

		// Libertamos o sem�foro de escrita
		ReleaseSemaphore(data->hSemaphoreWrite, 1, NULL);

		EnterCriticalSection(data->criticalSectionBool);
	}
	LeaveCriticalSection(data->criticalSectionBool);

	return 0;
}

// Thread da Interface
DWORD WINAPI threadInterface(LPVOID param) {
	pInterfaceStruct mainInterface = (pInterfaceStruct)param;
	TCHAR command[STR_SIZE], ** commandArray = NULL;
	const TCHAR delim[2] = TEXT(" ");
	unsigned int nrArguments = 0;
	int cordX, cordY;

	EnterCriticalSection(mainInterface->criticalSectionBool);
	while (!*mainInterface->stop) {
		LeaveCriticalSection(mainInterface->criticalSectionBool);

		// Recebemos o comando
		_tprintf(TEXT("\nCOMANDO: "));
		_getts_s(command, _countof(command) - 1);

		free(commandArray); // a fun��o free com um ponteiro de valor NULL n�o faz nada

		// Dividimos o comando
		commandArray = splitString(command, delim, &nrArguments);

		// Se a funcao splitString retornar NULL � porque o comando estava vazio
		if (commandArray == NULL) {
			EnterCriticalSection(mainInterface->criticalSectionBool);
			continue;
		}

		// caeroporto <nome> <coordenada x> <coordenada y>
		if (_tcscmp(commandArray[0], TEXT("caeroporto")) == 0) {
			// Verifica se o programa tem o n�mero de argumentos correto
			if (nrArguments != 4) {
				_ftprintf(stderr, TEXT("[ERRO-1] Argumentos inv�lidos! Uso: caeroporto <nome> <coordenada x> <coordenada y>\n"));
				
				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			EnterCriticalSection(mainInterface->criticalSectionAirports);

			// Verifica se chegamos ao limite m�ximo de aeroportos
			if (*mainInterface->maxAirports == *mainInterface->nrAirports) {
				_ftprintf(stderr, TEXT("[ERRO] N�mero m�ximo de aeroportos atingido!\n"));

				LeaveCriticalSection(mainInterface->criticalSectionAirports);

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			// Verifica se o argumento 2 e 3 s�o numeros
			if (!isStringANumber(commandArray[2]) || !isStringANumber(commandArray[3])) {
				_ftprintf(stderr, TEXT("[ERRO-2] Argumentos inv�lidos! Uso: caeroporto <nome> <coordenada x> <coordenada y>\n"));

				LeaveCriticalSection(mainInterface->criticalSectionAirports);

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			// converte de string para int
			cordX = _tstoi(commandArray[2]);
			cordY = _tstoi(commandArray[3]);

			// Verifica se as coordenadas s�o validas
			if (cordX > CORD_MAX_X && cordY > CORD_MAX_Y) {
				_ftprintf(stderr, TEXT("[ERRO] Coordenadas inv�lidas!\n"));

				LeaveCriticalSection(mainInterface->criticalSectionAirports);

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			// Verifica nome e coordenadas do aeroporto (se o aeroporto ja existe e se nao est� perto de outro aeroporto)
			if (!isAirportValid(mainInterface->airportsArray, *mainInterface->nrAirports, commandArray[1], cordX, cordY)) {
				_ftprintf(stderr, TEXT("[ERRO] Aeroporto inv�lido!\n"));

				LeaveCriticalSection(mainInterface->criticalSectionAirports);

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			// Inicia um aeroporto no array de aeroportos
			initAirport(
				&mainInterface->airportsArray[*mainInterface->nrAirports],
				mainInterface->nrAirports,
				commandArray[1],
				cordX,
				cordY
			);

			WaitForSingleObject(mainInterface->hSharedAirportsMutex, INFINITE);
			mainInterface->airportsSharedArray[*mainInterface->nrAirports - 1] = mainInterface->airportsArray[*mainInterface->nrAirports - 1];
			ReleaseMutex(mainInterface->hSharedAirportsMutex);

			LeaveCriticalSection(mainInterface->criticalSectionAirports);

			_tprintf(TEXT("Aeroporto criado com sucesso!\n"));

			EnterCriticalSection(mainInterface->criticalSectionBool);
			continue;
		}

		// laeroportos -> lista os aeroportos
		if (_tcscmp(commandArray[0], TEXT("laeroportos")) == 0) {
			// Verifica se o programa tem o n�mero de argumentos correto
			if (nrArguments != 1) {
				_ftprintf(stderr, TEXT("[ERRO] Argumentos inv�lidos! Uso: laeroporto\n"));

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			EnterCriticalSection(mainInterface->criticalSectionAirports);

			// Verifica se existem aeroportos
			if (*mainInterface->nrAirports == 0) {
				_tprintf(TEXT("N�o existem aeroportos!\n"));

				LeaveCriticalSection(mainInterface->criticalSectionAirports);

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			// Se existirem aeroportos, mostra os que existem
			printAirportsArray(mainInterface->airportsArray, *mainInterface->nrAirports);

			LeaveCriticalSection(mainInterface->criticalSectionAirports);

			EnterCriticalSection(mainInterface->criticalSectionBool);
			continue;
		}

		// snavioes
		if (_tcscmp(commandArray[0], TEXT("snavioes")) == 0) {
			// Verifica se o programa tem o n�mero de argumentos correto
			if (nrArguments != 1) {
				_ftprintf(stderr, TEXT("[ERRO] Argumentos inv�lidos! Uso: snavioes\n"));

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}
			// retira o evento do estado assinalado nao permitindo entrarem avioes
			ResetEvent(mainInterface->hEventActivateSuspend);

			EnterCriticalSection(mainInterface->criticalSectionBool);
			continue;

			// L�gica que vai suspender a aceita��o de novos avi�es
			// � inteligente verificar se j� t� suspendido ou n�o
		}

		// anavioes
		if (_tcscmp(commandArray[0], TEXT("anavioes")) == 0) {
			// Verifica se o programa tem o n�mero de argumentos correto
			if (nrArguments != 1) {
				_ftprintf(stderr, TEXT("[ERRO] Argumentos inv�lidos! Uso: anavioes\n"));

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			// assinala o evento para permitir a entrada de avioes
			SetEvent(mainInterface->hEventActivateSuspend);


			EnterCriticalSection(mainInterface->criticalSectionBool);
			continue;


			// L�gica que vai ativar a aceita��o de novos avi�es
			// � inteligente verificar se j� t� ativado ou n�o
		}

		// podemos receber um comando para ver informa��o acerca de aeroportos
		// podemos receber um comando para ver informa��o acerca dos avi�es

		// lavioes -> lista avioes
		if (_tcscmp(commandArray[0], TEXT("lavioes")) == 0) {
			// Verifica se o programa tem o n�mero de argumentos correto
			if (nrArguments != 1) {
				_ftprintf(stderr, TEXT("[ERRO] Argumentos inv�lidos! Uso: lavioes\n"));

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			EnterCriticalSection(mainInterface->criticalSectionAirplanes);

			// Verifica se existem avioes
			if (*mainInterface->nrAirplanes == 0) {
				_tprintf(TEXT("N�o existem avi�es!\n"));

				LeaveCriticalSection(mainInterface->criticalSectionAirplanes);

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}

			// Caso existam avioes, mostramos
			printAirplanesArray(mainInterface->airplanesArray, *mainInterface->nrAirplanes);

			LeaveCriticalSection(mainInterface->criticalSectionAirplanes);

			EnterCriticalSection(mainInterface->criticalSectionBool);
			continue;
		}

		// terminar
		if (_tcscmp(commandArray[0], TEXT("terminar")) == 0) {
			// Verifica se o programa tem o n�mero de argumentos correto
			if (nrArguments != 1) {
				_ftprintf(stderr, TEXT("[ERRO] Argumentos inv�lidos! Uso: terminar\n"));

				EnterCriticalSection(mainInterface->criticalSectionBool);
				continue;
			}


			EnterCriticalSection(mainInterface->criticalSectionBool);
			break;
		}

		_ftprintf(stderr, TEXT("[ERRO] Comando desconhecido!\n"));

		EnterCriticalSection(mainInterface->criticalSectionBool);
	}
	LeaveCriticalSection(mainInterface->criticalSectionBool);

	free(commandArray);

	return 0;
}

// Thread das coordenadas
DWORD WINAPI coordinatesThread(LPVOID param){
	pCoordThreadStruct coordinates = (pCoordThreadStruct)param;

	// Espera que o mutex esteja livre e entra na critical section dos avioes
	WaitForSingleObject(coordinates->hMutex, INFINITE);
	EnterCriticalSection(coordinates->criticalSectionAirplanes);
	// Preenche inicialmente a memoria partilhada com informa��o local
	for (unsigned int i = 0; i < coordinates->size; i++) {
		coordinates->sharedMemory[i].x = coordinates->airplanesArray[i].currentCoordinates.x;
		coordinates->sharedMemory[i].y = coordinates->airplanesArray[i].currentCoordinates.y;
		coordinates->sharedMemory[i].maxAirplanes = coordinates->size;
	}
	LeaveCriticalSection(coordinates->criticalSectionAirplanes);
	ReleaseMutex(coordinates->hMutex);

	EnterCriticalSection(coordinates->criticalSectionBool);
	while (!*coordinates->stop) {
		LeaveCriticalSection(coordinates->criticalSectionBool);

		// Espera que o aviao tenha lido a memoria partilhada
		WaitForSingleObject(coordinates->hMutex, INFINITE);
		EnterCriticalSection(coordinates->criticalSectionAirplanes);

		// Atualizar a memoria partilhada com a nova informa��o
		for (unsigned int i = 0; i < coordinates->size; i++) {
			coordinates->sharedMemory[i].x = coordinates->airplanesArray[i].currentCoordinates.x;
			coordinates->sharedMemory[i].y = coordinates->airplanesArray[i].currentCoordinates.y;
		}

		LeaveCriticalSection(coordinates->criticalSectionAirplanes);
		// Libertamos o mutex para o aviao conseguir aceder
		ReleaseMutex(coordinates->hMutex);
		Sleep(10);

		EnterCriticalSection(coordinates->criticalSectionBool);
	}
	LeaveCriticalSection(coordinates->criticalSectionBool);	
	
	return 0;
}

// Thread dos pings
DWORD WINAPI pingThread(LPVOID param){
	pPingThreadStruct pingStruct = (pPingThreadStruct)param;
	TCHAR eventName[STR_SIZE];
	DWORD error;
	HANDLE hEvent;

	HANDLE hGenericEvent;
	TCHAR genericEventName[STR_SIZE];

	// Cria um evento com o id do aviao
	_stprintf_s(eventName, _countof(eventName) - 1, TEXT("EVENT_PING_%i"), pingStruct->thisAirplaneId);

	hEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		eventName
	);

	// Caso o evento d� erro
	if (hEvent == NULL) {
		_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel iniciar o evento de ping para o avi�o %i!\n"), pingStruct->thisAirplaneId);

		EnterCriticalSection(pingStruct->criticalSectionBool);
		*pingStruct->stop = TRUE;
		LeaveCriticalSection(pingStruct->criticalSectionBool);

		return -1;
	}

	_stprintf_s(genericEventName, _countof(genericEventName) - 1, TEXT("MOVED_%ld"), pingStruct->thisAirplaneId);

	hGenericEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		genericEventName
	);

	if (hGenericEvent == NULL) {
		_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel iniciar o evento gen�rico na thread ping para o avi�o %i!\n"), pingStruct->thisAirplaneId);

		EnterCriticalSection(pingStruct->criticalSectionBool);
		*pingStruct->stop = TRUE;
		LeaveCriticalSection(pingStruct->criticalSectionBool);
		
		return -1;
	}

	// Verifica os eventos que estao assinalados

	EnterCriticalSection(pingStruct->criticalSectionBool);
	while (!*pingStruct->stop) {
		LeaveCriticalSection(pingStruct->criticalSectionBool);

		// Espera 3 segundos pelo evento para que um aviao assinele que esta vivo
		error = WaitForSingleObject(hEvent, 3000);

		// Se passarem os 3 segundos assumimos que o aviao deixou de responder
		if (error == WAIT_TIMEOUT) {
			EnterCriticalSection(pingStruct->criticalSectionAirplanes);
			EnterCriticalSection(pingStruct->criticalSectionPings);

			for (unsigned int i = 0; i < *pingStruct->nrAirplanes; i++) {
				if (pingStruct->airplanesArray[i].id == pingStruct->thisAirplaneId) {
					for (unsigned int j = 0; j < pingStruct->airplanesArray[i].currentPassengers; j++) {
						SetEvent(hGenericEvent);
						ResetEvent(hGenericEvent);
					}

					break;
				}
			}

			// Retira o aviao do array de avioes
			if (!removeAirplaneFromArray(pingStruct->airplanesArray, pingStruct->nrAirplanes, pingStruct->thisAirplaneId, pingStruct->hThreadPingArray)){
				_ftprintf(stderr, TEXT("[ERRO] Um avi�o que n�o existia foi tentado ser removido!\n"));
			}
			LeaveCriticalSection(pingStruct->criticalSectionAirplanes);
			LeaveCriticalSection(pingStruct->criticalSectionPings);

			EnterCriticalSection(pingStruct->criticalSectionBool);
			break;
		}

		EnterCriticalSection(pingStruct->criticalSectionBool);
	}
	LeaveCriticalSection(pingStruct->criticalSectionBool);

	free(pingStruct);

	return 0;
}

DWORD WINAPI pipeThread(LPVOID param) {
	pPipeThreadStruct data = (pPipeThreadStruct)param;

	BOOL continueFlag;

	namedPipeStruct hPipes[MAX_PASSENGERS];
	HANDLE hEvents[MAX_PASSENGERS];
	HANDLE hThreadPassengersArray[MAX_PASSENGERS];

	HANDLE hPipeTemp, hPipeEventTemp;

	pPassengerThreadStruct passengerThreadStructTemp;
	DWORD offset, nBytes;
	int i;

	for (unsigned int i = 0; i < MAX_PASSENGERS; i++) {
		// aqui passamos a constante FILE_FLAG_OVERLAPPED para o named pipe aceitar comunica��es assincronas
		hPipeTemp = CreateNamedPipe(
			PASSENGERS_PIPE_NAME,
			PIPE_ACCESS_DUPLEX | FILE_FLAG_OVERLAPPED,
			PIPE_WAIT | PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE,
			MAX_PASSENGERS,
			sizeof(passenger),
			sizeof(passenger),
			1000,
			NULL
		);

		if (hPipeTemp == INVALID_HANDLE_VALUE) {
			_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel criar o pipe (%i)."), i);

			EnterCriticalSection(data->criticalSectionBool);
			*data->stop = FALSE;
			LeaveCriticalSection(data->criticalSectionBool);

			return -1;
		}

		// criar evento que vai ser associado � esturtura overlaped
		// os eventos aqui tem de ter sempre reset manual e nao autom�tico porque temos de delegar essas responsabilidades ao sistema operativo
		hPipeEventTemp = CreateEvent(
			NULL,
			TRUE,
			FALSE,
			NULL
		);

		if (hPipeEventTemp == NULL) {
			_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel criar evento para pipe (%i).\n"), i);

			EnterCriticalSection(data->criticalSectionBool);
			*data->stop = FALSE;
			LeaveCriticalSection(data->criticalSectionBool);;

			return -1;
		}

		hPipes[i].hPipe = hPipeTemp;
		hPipes[i].active = FALSE;
		//temos de garantir que a estrutura overlap est� limpa
		ZeroMemory(
			&hPipes[i].overlap,
			sizeof(hPipes[i].overlap)
		);

		//preenchemos agora o evento
		hPipes[i].overlap.hEvent = hPipeEventTemp;
		hEvents[i] = hPipeEventTemp;

		// _tprintf(TEXT("[DEBUG] Esperar liga��o de um passageiro... (ConnectNamedPipe)\n"));

		// aqui passamos um ponteiro para a estrutura overlap
		ConnectNamedPipe(hPipeTemp, &hPipes[i].overlap);
	}

	EnterCriticalSection(data->criticalSectionBool);
	while (!*data->stop && *data->nrPassengers < MAX_PASSENGERS) {
		LeaveCriticalSection(data->criticalSectionBool);

		continueFlag = FALSE;

		// Permite estar bloqueado , � espera que 1 evento do array de eventos seja assinalado
		for (DWORD nrWaits = 0; nrWaits + MAXIMUM_WAIT_OBJECTS < MAX_PASSENGERS; ) {
			offset = WaitForMultipleObjects(MAXIMUM_WAIT_OBJECTS, &hEvents[nrWaits], FALSE, 10);

			// Um dos eventos estava assinalado (n�o deu timeout)
			if (offset != WAIT_TIMEOUT) {
				i = (nrWaits + offset) - WAIT_OBJECT_0;
				break;
			}

			// J� experimentei fazer wait a todos
			if (nrWaits + MAXIMUM_WAIT_OBJECTS == MAX_PASSENGERS - 1) {
				continueFlag = TRUE;
				break;
			}

			nrWaits += MAXIMUM_WAIT_OBJECTS;

			// No caso de o Wait ficar � espera de eventos que s�o lixo
			// Isto garante que todos s�o vistos
			// (alguns poder�o vistos 2x)
			if (nrWaits + MAXIMUM_WAIT_OBJECTS >= MAX_PASSENGERS) {
				nrWaits = MAX_PASSENGERS - MAXIMUM_WAIT_OBJECTS - 1;
			}
		}

		if (continueFlag) {
			EnterCriticalSection(data->criticalSectionBool);
			continue;
		}

		// se � um indice v�lido ...
		if (i >= 0 && i < MAX_PASSENGERS) {
			EnterCriticalSection(data->criticalSectionPipe);
			if (GetOverlappedResult(hPipes[i].hPipe, &hPipes[i].overlap, &nBytes, FALSE)) {
				// se entrarmos aqui significa que a funcao correu tudo bem
				// fazemos reset do evento porque queremos que o WaitForMultipleObject desbloqueio com base noutro evento e nao neste
				ResetEvent(hEvents[i]);

				//vamos esperar que o mutex esteja livre
				hPipes[i].active = TRUE; // dizemos que esta instancia do named pipe est� ativa

				passengerThreadStructTemp = malloc(sizeof(passengerThreadStruct));

				passengerThreadStructTemp->thisPassengerId = 0;
				passengerThreadStructTemp->namedPipe = &hPipes[i];
				passengerThreadStructTemp->airportsArray = data->airportsArray;
				passengerThreadStructTemp->nrAirports = data->nrAirports;
				passengerThreadStructTemp->airplanesArray = data->airplanesArray;
				passengerThreadStructTemp->nrAirplanes = data->nrAirplanes;
				passengerThreadStructTemp->passengersArray = data->passengersArray;
				passengerThreadStructTemp->nrPassengers = data->nrPassengers;
				passengerThreadStructTemp->hThreadPassengersArray = hThreadPassengersArray;
				passengerThreadStructTemp->criticalSectionAirports = data->criticalSectionAirports;
				passengerThreadStructTemp->criticalSectionPassengers = data->criticalSectionPassengers;
				passengerThreadStructTemp->criticalSectionBool = data->criticalSectionBool;
				passengerThreadStructTemp->criticalSectionAirplanes = data->criticalSectionAirplanes;
				passengerThreadStructTemp->stop = data->stop;

				hThreadPassengersArray[*data->nrPassengers] = CreateThread(
					NULL,
					0,
					passengerThread,
					passengerThreadStructTemp,
					0,
					NULL
				);

				if (hThreadPassengersArray[*data->nrPassengers] == NULL) {
					_ftprintf(stderr, TEXT("[ERRO-%i] N�o foi poss�vel criar a thread %i para passageiro!\n"), GetLastError(), *data->nrPassengers);
					
					LeaveCriticalSection(data->criticalSectionPipe);

					EnterCriticalSection(data->criticalSectionBool);
					continue;
				}
			}
			LeaveCriticalSection(data->criticalSectionPipe);
		}

		EnterCriticalSection(data->criticalSectionBool);
	}
	LeaveCriticalSection(data->criticalSectionBool);

	EnterCriticalSection(data->criticalSectionPassengers);
	if (*data->nrPassengers < MAXIMUM_WAIT_OBJECTS) {
		WaitForMultipleObjects(*data->nrPassengers, hThreadPassengersArray, TRUE, INFINITE);
		LeaveCriticalSection(data->criticalSectionPassengers);
		return 0;
	}

	for (DWORD nrWaits = 0; nrWaits + MAXIMUM_WAIT_OBJECTS < *data->nrPassengers; ) {
		offset = WaitForMultipleObjects(MAXIMUM_WAIT_OBJECTS, &hEvents[nrWaits], TRUE, INFINITE);

		// Um dos eventos estava assinalado (n�o deu timeout)
		if (offset != WAIT_TIMEOUT) {
			i = (nrWaits + offset) - WAIT_OBJECT_0;
			break;
		}

		// J� experimentei fazer wait a todos
		if (nrWaits + MAXIMUM_WAIT_OBJECTS == MAX_PASSENGERS - 1) {
			continueFlag = TRUE;
			break;
		}

		nrWaits += MAXIMUM_WAIT_OBJECTS;

		// No caso de o Wait ficar � espera de eventos que s�o lixo
		// Isto garante que todos s�o vistos
		// (alguns poder�o vistos 2x)
		if (nrWaits + MAXIMUM_WAIT_OBJECTS >= MAX_PASSENGERS) {
			nrWaits = MAX_PASSENGERS - MAXIMUM_WAIT_OBJECTS - 1;
		}
	}
	LeaveCriticalSection(data->criticalSectionPassengers);

	return 0;
}

DWORD WINAPI passengerThread(LPVOID param) {
	pPassengerThreadStruct thisPassenger = (pPassengerThreadStruct)param;
	BOOL ret, breakFlag;
	passenger aux;
	DWORD nBytes;
	unsigned int nrValidAirports = 0;
	TCHAR strWrite[STR_SIZE]= TEXT("");

	DWORD airplaneID = 0;

	HANDLE hGenericEvent;
	TCHAR genericEventName[STR_SIZE] = TEXT("");
	TCHAR tempStr1[STR_SIZE] = TEXT("");
	TCHAR tempStr2[STR_SIZE] = TEXT("");

	// fazer read da struct do passageiro na primeira vez
	ret = ReadFile(
		thisPassenger->namedPipe->hPipe, 
		&aux, 
		sizeof(passenger), 
		&nBytes, 
		NULL
	);

	if (!ret || !nBytes) {
		_ftprintf(stderr, TEXT("[ERRO] Erro ao fazer ReadFile %d %d..)\n"), ret, nBytes);

		free(thisPassenger);
		return -1;
	}

	// verifica se o aeroporto de destino e o aeroporto de origem existem
	EnterCriticalSection(thisPassenger->criticalSectionAirports);
	for (unsigned int i = 0; i < *thisPassenger->nrAirports; i++) {
		if (_tcsicmp(aux.destAirport, thisPassenger->airportsArray->name) == 0 || _tcsicmp(aux.srcAirport, thisPassenger->airportsArray->name) == 0) {
			nrValidAirports++;
		}
	}
	LeaveCriticalSection(thisPassenger->criticalSectionAirports);

	if (nrValidAirports != 2) {
		_ftprintf(stderr, TEXT("[ERRO] Passageiro %s introduziu um aeroporto inv�lido!\n"), aux.name);

		_stprintf_s(strWrite, _countof(strWrite) - 1, TEXT("%ld"), 0);

		ret = WriteFile(
			thisPassenger->namedPipe->hPipe,
			strWrite,
			_tcslen(strWrite) * sizeof(TCHAR),
			&nBytes,
			NULL
		);

		if (!ret || !nBytes) {
			_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel escrever no pipe do passageiro %s!\n"), aux.name);
			
			free(thisPassenger);
			return -1;
		}

		free(thisPassenger);
		return -1;
	}

	EnterCriticalSection(thisPassenger->criticalSectionPassengers);
	aux.id = getPassengerID();
	copyPassenger(&thisPassenger->passengersArray[(*thisPassenger->nrPassengers)++], aux);
	LeaveCriticalSection(thisPassenger->criticalSectionPassengers);
	
	thisPassenger->thisPassengerId = aux.id;

	_stprintf_s(strWrite, _countof(strWrite) - 1, TEXT("%ld"), aux.id);
	
	ret = WriteFile(
		thisPassenger->namedPipe->hPipe,
		strWrite,
		_tcslen(strWrite) * sizeof(TCHAR),
		&nBytes,
		NULL
	);

	if (!ret || !nBytes) {
		_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel escrever no pipe do passageiro %s!\n"), aux.name);
		
		removePassengerFromArray(thisPassenger->passengersArray, thisPassenger->nrPassengers, thisPassenger->thisPassengerId, thisPassenger->hThreadPassengersArray);

		free(thisPassenger);
		return -1;
	}

	_tcscpy_s(tempStr1, _countof(tempStr1), aux.srcAirport);
	_tcscpy_s(tempStr2, _countof(tempStr2), aux.destAirport);

	toUpperString(tempStr1);
	toUpperString(tempStr2);

	_stprintf_s(genericEventName, _countof(genericEventName) - 1, TEXT("ONBOARD_%s_%s"), tempStr1, tempStr2);

	hGenericEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		genericEventName
	);

	if (hGenericEvent == NULL) {
		_ftprintf(stderr, TEXT("[ERRO] O passageiro %s n�o conseguiu obter evento para embarcar!\n"), aux.name);
		
		removePassengerFromArray(thisPassenger->passengersArray, thisPassenger->nrPassengers, thisPassenger->thisPassengerId, thisPassenger->hThreadPassengersArray);

		free(thisPassenger);
		return -1;
	}

	EnterCriticalSection(thisPassenger->criticalSectionBool);
	while (!*thisPassenger->stop) {
		LeaveCriticalSection(thisPassenger->criticalSectionBool);

		if (WaitForSingleObject(hGenericEvent, 3000) == WAIT_TIMEOUT) {
			EnterCriticalSection(thisPassenger->criticalSectionBool);
			continue;
		}

		breakFlag = FALSE;

		EnterCriticalSection(thisPassenger->criticalSectionAirplanes);
		for (unsigned int i = 0; i < *thisPassenger->nrAirplanes; i++) {
			if (_tcsicmp(thisPassenger->airplanesArray[i].srcAirport, aux.srcAirport) == 0 && _tcsicmp(thisPassenger->airplanesArray[i].destAirport, aux.destAirport) == 0) {
				if (thisPassenger->airplanesArray[i].currentPassengers < thisPassenger->airplanesArray[i].capacity && thisPassenger->airplanesArray[i].stopped) {
					thisPassenger->airplanesArray[i].currentPassengers++;
					airplaneID = thisPassenger->airplanesArray[i].id;
					breakFlag = TRUE;
				}
				break;
			}
		}
		LeaveCriticalSection(thisPassenger->criticalSectionAirplanes);

		EnterCriticalSection(thisPassenger->criticalSectionBool);
		if (breakFlag) break;
	}
	LeaveCriticalSection(thisPassenger->criticalSectionBool);

	EnterCriticalSection(thisPassenger->criticalSectionBool);
	if (*thisPassenger->stop) {
		LeaveCriticalSection(thisPassenger->criticalSectionBool);

		removePassengerFromArray(thisPassenger->passengersArray, thisPassenger->nrPassengers, thisPassenger->thisPassengerId, thisPassenger->hThreadPassengersArray);

		free(thisPassenger);
		return -1;
	}
	LeaveCriticalSection(thisPassenger->criticalSectionBool);

	_stprintf_s(strWrite, _countof(strWrite) - 1, TEXT("Acabou de embarcar num avi�o! Prepare-se para a descolagem!"));

	ret = WriteFile(
		thisPassenger->namedPipe->hPipe,
		strWrite,
		_tcslen(strWrite) * sizeof(TCHAR),
		&nBytes,
		NULL
	);

	if (!ret || !nBytes) {
		_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel escrever no pipe do passageiro %s!\n"), aux.name);

		removePassengerFromArray(thisPassenger->passengersArray, thisPassenger->nrPassengers, thisPassenger->thisPassengerId, thisPassenger->hThreadPassengersArray);

		free(thisPassenger);
		return -1;
	}

	_stprintf_s(genericEventName, _countof(genericEventName) - 1, TEXT("MOVED_%ld"), airplaneID);

	hGenericEvent = CreateEvent(
		NULL,
		TRUE,
		FALSE,
		genericEventName
	);

	if (hGenericEvent == NULL) {
		_ftprintf(stderr, TEXT("[ERRO] O passageiro %s n�o conseguiria saber que se est� a mexer!"), aux.name);
		
		removePassengerFromArray(thisPassenger->passengersArray, thisPassenger->nrPassengers, thisPassenger->thisPassengerId, thisPassenger->hThreadPassengersArray);

		free(thisPassenger);
		return -1;
	}

	EnterCriticalSection(thisPassenger->criticalSectionBool);
	while (!*thisPassenger->stop) {
		LeaveCriticalSection(thisPassenger->criticalSectionBool);

		if (WaitForSingleObject(hGenericEvent, 3000) == WAIT_TIMEOUT) {
			EnterCriticalSection(thisPassenger->criticalSectionBool);
			continue;
		}

		breakFlag = TRUE;

		EnterCriticalSection(thisPassenger->criticalSectionAirplanes);
		for (unsigned int i = 0; i < *thisPassenger->nrAirplanes; i++) {
			if (thisPassenger->airplanesArray[i].id == airplaneID) {
				breakFlag = FALSE;

				// Se o voo tiver acabado (avi�o chegou ao destino)
				if (thisPassenger->airplanesArray[i].stopped) {
					_stprintf_s(strWrite, _countof(strWrite) - 1, PIPE_COMMAND_FLIGHT_ENDED);
					break;
				}

				_stprintf_s(strWrite, _countof(strWrite) - 1, TEXT("Est� agora a voar sobre (%i, %i)!"), thisPassenger->airplanesArray[i].currentCoordinates.x, thisPassenger->airplanesArray[i].currentCoordinates.y);
				break;
			}
		}
		LeaveCriticalSection(thisPassenger->criticalSectionAirplanes);

		if (!breakFlag) {
			ret = WriteFile(
				thisPassenger->namedPipe->hPipe,
				strWrite,
				_tcslen(strWrite) * sizeof(TCHAR),
				&nBytes,
				NULL
			);

			if (!ret || !nBytes) {
				_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel escrever no pipe do passageiro %s!\n"), aux.name);

				free(thisPassenger);
				return -1;
			}

			// Se o voo tiver acabado, sair do loop de escrita / espera por evento
			if (_tcscmp(strWrite, PIPE_COMMAND_FLIGHT_ENDED) == 0) {
				EnterCriticalSection(thisPassenger->criticalSectionBool);
				break;
			}

			// O voo ainda n� acabou; volta par ao in�cio
			EnterCriticalSection(thisPassenger->criticalSectionBool);
			continue;
		}

		_stprintf_s(strWrite, _countof(strWrite) - 1, PIPE_COMMAND_AIRPLANE_CLOSED);

		ret = WriteFile(
			thisPassenger->namedPipe->hPipe,
			strWrite,
			_tcslen(strWrite) * sizeof(TCHAR),
			&nBytes,
			NULL
		);

		if (!ret || !nBytes) {
			_ftprintf(stderr, TEXT("[ERRO] N�o foi poss�vel escrever no pipe do passageiro %s!\n"), aux.name);

			removePassengerFromArray(thisPassenger->passengersArray, thisPassenger->nrPassengers, thisPassenger->thisPassengerId, thisPassenger->hThreadPassengersArray);

			free(thisPassenger);
			return -1;
		}

		EnterCriticalSection(thisPassenger->criticalSectionBool);
		break;
	}
	LeaveCriticalSection(thisPassenger->criticalSectionBool);

	EnterCriticalSection(thisPassenger->criticalSectionPassengers);
	removePassengerFromArray(thisPassenger->passengersArray, thisPassenger->nrPassengers, thisPassenger->thisPassengerId, thisPassenger->hThreadPassengersArray);
	LeaveCriticalSection(thisPassenger->criticalSectionPassengers);

	free(thisPassenger);
	return 0;
}