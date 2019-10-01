

#include "Interfaz.h"
#include "NSK_Algo.h"
#include "thinkgear.h"
#include <stdio.h>
#include <string>
#include <WinUser.h>
#include <windowsx.h>
#include <windows.h>
#include <CommCtrl.h>
#include <time.h>
#include <pocketsphinx.h>
#include <sphinxbase/err.h>
#include <sphinxbase/ad.h>
#include <locale.h>

extern "C" {
	void *myMalloc(size_t size) {
		return malloc(size);
	}
};

void * operator new (size_t size) {
	return myMalloc(size);
}

#define MAX_LONGCADENA 100
#define MODELDIR "../Interfaz/pocketsphinx/model"

#define NSK_ALGO_CDECL(ret, func, args)		typedef ret (__cdecl *func##_Dll) args; func##_Dll func##Addr = NULL; char func##Str[] = #func;

NSK_ALGO_CDECL(eNSK_ALGO_RET,	NSK_ALGO_Init,				(eNSK_ALGO_TYPE type, const NS_STR dataPath));
NSK_ALGO_CDECL(eNSK_ALGO_RET,	NSK_ALGO_Uninit,			(NS_VOID));
NSK_ALGO_CDECL(eNSK_ALGO_RET,	NSK_ALGO_RegisterCallback,	(NskAlgo_Callback cbFunc, NS_VOID *userData));
NSK_ALGO_CDECL(NS_STR,			NSK_ALGO_SdkVersion,		(NS_VOID));
NSK_ALGO_CDECL(NS_STR,			NSK_ALGO_AlgoVersion,		(eNSK_ALGO_TYPE type));
NSK_ALGO_CDECL(eNSK_ALGO_RET,	NSK_ALGO_Start,				(NS_BOOL bBaseline));
NSK_ALGO_CDECL(eNSK_ALGO_RET,	NSK_ALGO_Pause,				(NS_VOID));
NSK_ALGO_CDECL(eNSK_ALGO_RET,	NSK_ALGO_Stop,				(NS_VOID));
NSK_ALGO_CDECL(eNSK_ALGO_RET,	NSK_ALGO_DataStream,		(eNSK_ALGO_DATA_TYPE type, NS_INT16 *data, NS_INT dataLenght));


HINSTANCE hInstancia;                                
WCHAR Titulo[MAX_LONGCADENA];                  
WCHAR NombreVentana[MAX_LONGCADENA];            

ATOM                RegistroObjetoVentana(HINSTANCE Instancia);
BOOL                InicializarInstancia(HINSTANCE, int);
LRESULT CALLBACK    ProcesosVentana(HWND, UINT, WPARAM, LPARAM);
INT_PTR CALLBACK    Informacion(HWND, UINT, WPARAM, LPARAM);

HWND  BotonInicio, BotonDetener, AbreCierraTick,TresTick,CuatroTick, VdeVictoria, Senalar, TextoEstado, TextoCalidadSenal, BotonVersion, TextoSalida, BotonDedos1, BotonDedos2, BotonDedos3, BotonDedos4, BotonDedos5;
HWND Interfaz;




int ModosSeleccionados = 0;


bool bIniciado = false;
bool bPausado = false;

#define MARGEN					10

#define ANCHO_ESTADO_TEXTO		300
#define ALTO_ESTADO_TEXTO		20

#define ANCHO_SENAL_TEXTO		200
#define ALTO_SENAL_TEXTO		20

#define ANCHO_BOTON			120
#define ALTO_BOTON			30


#define V_TEXTO_USUARIO			0x1000

#define PUERTO_COM					"COM14"



#ifdef _WIN64
#define ALGO_SDK_DLL			L"AlgoSdkDll64.dll"
#else
#define ALGO_SDK_DLL			L"AlgoSdkDll.dll"
#endif

HANDLE hComm = CreateFileA("\\\\.\\COM16", 
	GENERIC_READ | GENERIC_WRITE, 
	1,                            
	NULL,                        
	OPEN_EXISTING,
	0,            
	NULL);        



char *comPortName = NULL;
int   dllVersion = 0;
int   IdConexion = -1;
int   LecturaPaquetes = 0;
int   errCode = 0;
DWORD IdHilo = -1;
DWORD IdHilo2 = -1;
HANDLE Hilo = NULL;
HANDLE Hilo2 = NULL;
bool bConnectedHeadset = false;
int Iniciado = 0;

static void HiloCascoNeuronal(LPVOID lpdwThreadParam);
static void FuncionCallback(sNSK_ALGO_CB_PARAM param);
static bool obtenerDireccionFunciones(HINSTANCE hinstLib, HWND Interfaz);
static void *obtenerFunciones(HINSTANCE hinstLib, HWND hwnd, char *funcName, bool *bError);
static void SalidaTextoLog(LPCWSTR log);
static void ActualizarVentana(HWND Interfaz);
static wchar_t *HoraLocal();
static void SalidaTextoInterfaz(LPCWSTR Buffer);
int DetenerInterfazVoz();
int IniciarInterfazVoz();
int PausarInterfazVoz();
void cerrar_app();
int change = 0;
bool CloseOpen = false;
HWND Auxiliar;
bool Mode = true;
int ModeChange = 0;
using namespace std;


/* Método sleep proporcionado por PocketSphinx para el reconocedor*/
static void
sleep_msec(int32 ms)
{
#if (defined(_WIN32) && !defined(GNUWINCE)) || defined(_WIN32_WCE)
	Sleep(ms);
#else
	/* ------------------- Unix ------------------ */
	struct timeval tmo;

	tmo.tv_sec = 0;
	tmo.tv_usec = ms * 1000;

	select(0, NULL, NULL, NULL, &tmo);
#endif
}

static const arg_t cont_args_def[] = {
	POCKETSPHINX_OPTIONS,
	/* Argument file. */
	{"-argfile",
	 ARG_STRING,
	 NULL,
	 "Argument file giving extra arguments."},
	{"-adcdev",
	 ARG_STRING,
	 NULL,
	 "Name of audio device to use for input."},
	{"-infile",
	 ARG_STRING,
	 NULL,
	 "Audio file to transcribe."},
	{"-inmic",
	 ARG_BOOLEAN,
	 "no",
	 "Transcribe audio from microphone."},
	{"-time",
	 ARG_BOOLEAN,
	 "no",
	 "Print word times in file transcription."},
	CMDLN_EMPTY_OPTION
};

static void WordSpotting();
static void WordSpotting() {
	static wchar_t buffer[512];
	buffer[0] =0;
	ps_decoder_t *Reconocedor = NULL;
	cmd_ln_t *config = NULL;
	
	config = cmd_ln_init(NULL, ps_args(), TRUE,
		"-hmm", MODELDIR "/es-es/es-es",
		"-dict", MODELDIR "/es-es/mi.dict",
		"-kws", MODELDIR "/es-es/keywords.txt",
		NULL);
		
	if (config == NULL) {
		swprintf(buffer, 512, L"Fallo al crear el objecto de configuración.\n");
		SalidaTextoInterfaz(buffer);
		SalidaTextoLog(buffer);
		

	}
	ps_default_search_args(config);
	Reconocedor = ps_init(config);
	if (Reconocedor == NULL) {
		swprintf(buffer, 512, L" Fallo al crear el reconocedor.\n");
		SalidaTextoInterfaz(buffer);
		SalidaTextoLog(buffer);
		

	}

	ad_rec_t *ad;
	int16 adbuf[2048];
	uint8 utt_comenzada, intervaloSilencio;
	int32 k;
	char const *comando;

	if ((ad = ad_open_dev(cmd_ln_str_r(config, "-adcdev"), (int)cmd_ln_float32_r(config, "-samprate"))) == NULL) {
		swprintf(buffer, 512, L" Fallo al abrir el micrófono.\n");
		SalidaTextoInterfaz(buffer);
		SalidaTextoLog(buffer);
		E_FATAL("Fallo al abrir el micrófono.\n");
		
	}
	if (ad_start_rec(ad) < 0) {
		swprintf(buffer, 512, L" Fallo al empezar la grabación.\n");
		SalidaTextoInterfaz(buffer);
		SalidaTextoLog(buffer);
		E_FATAL("Fallo al empezar la grabación.\n");
	}

	if (ps_start_utt(Reconocedor) < 0) {
		swprintf(buffer, 512, L" Fallo al recibir señal.\n");
		SalidaTextoInterfaz(buffer);
		SalidaTextoLog(buffer);
		E_FATAL("Fallo al recibir señal.\n");
	}
	utt_comenzada = FALSE;
	E_INFO("Listo....\n");

	for (;;) {
		if ((k = ad_read(ad, adbuf, 2048)) < 0) {
			swprintf(buffer, 512, L" Fallo al leer sonido.\n");
			SalidaTextoInterfaz(buffer);
			SalidaTextoLog(buffer);
			E_FATAL("Fallo al leer sonido.\n");
		}
		ps_process_raw(Reconocedor, adbuf, k, FALSE, FALSE);
		intervaloSilencio = ps_get_in_speech(Reconocedor);
		if (intervaloSilencio && !utt_comenzada) {
			utt_comenzada = TRUE;
			E_INFO("Grabando...\n");
		}
		if (!intervaloSilencio && utt_comenzada) {
			
			ps_end_utt(Reconocedor);
			comando = ps_get_hyp(Reconocedor, NULL);
			if (comando != NULL) {
				printf("%s\n", comando);
				if (strcmp(comando, "modo abre cierra") == 0) {
					swprintf(buffer, 512, L"Ha dicho modo abre cierra.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);
					if (Button_GetCheck(AbreCierraTick) != BST_CHECKED) {
						Button_SetCheck(Auxiliar, false);

						Button_SetCheck(AbreCierraTick, true);
						Auxiliar = AbreCierraTick;
					}
					char cadena[] = "5";
					DWORD BytesEscribir;         
					DWORD BytesEscritos = 0;     
					BytesEscribir = (DWORD)sizeof(cadena);

					WriteFile(hComm,       
						cadena,     
						BytesEscribir,  
						&BytesEscritos, 
						NULL);
				}
				else if(strcmp(comando,"modo victoria")==0){
					swprintf(buffer, 512, L"Ha dicho modo victoria.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);
					if (Button_GetCheck(VdeVictoria) != BST_CHECKED) {
						Button_SetCheck(Auxiliar, false);

						Button_SetCheck(VdeVictoria, true);
						Auxiliar = VdeVictoria;
					}
					char cadena[] = "4";
					DWORD BytesEscribir;         
					DWORD BytesEscritos = 0;     
					BytesEscribir = (DWORD)sizeof(cadena);

					WriteFile(hComm,        
						cadena,     
						BytesEscribir,  
						&BytesEscritos, 
						NULL);
				}
				else if (strcmp(comando, "modo senalar") == 0) {
					swprintf(buffer, 512, L"Ha dicho modo señalar.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);
					if (Button_GetCheck(Senalar) != BST_CHECKED) {
						Button_SetCheck(Auxiliar, false);

						Button_SetCheck(Senalar, true);
						Auxiliar = Senalar;
					}
					char cadena[] = "3";
					DWORD BytesEscribir;    
					DWORD BytesEscritos = 0;     
					BytesEscribir = (DWORD)sizeof(cadena);

					WriteFile(hComm,       
						cadena,     
						BytesEscribir,  
						&BytesEscritos, 
						NULL);
				}
				else if (strcmp(comando, "abre mano") == 0) {
					CloseOpen = false;
					swprintf(buffer, 512, L"Ha dicho abre mano.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);

					swprintf(buffer, 512, L"Abriendo mano mediante voz.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);

					char cadena[] = "6";
					DWORD BytesEscribir;         
					DWORD BytesEscritos = 0;     
					BytesEscribir = (DWORD)sizeof(cadena);

					WriteFile(hComm,        
						cadena,     
						BytesEscribir,  
						&BytesEscritos, 
						NULL);
				}
				else if (strcmp(comando, "cierra mano") == 0) {
					CloseOpen = true;
					swprintf(buffer, 512, L"Ha dicho cierra mano.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);

					swprintf(buffer, 512, L"Cerrando mano mediante voz.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);


					char cadena[] = "7";
					DWORD BytesEscribir;        
					DWORD BytesEscritos = 0;     
					BytesEscribir = (DWORD)sizeof(cadena);

					WriteFile(hComm,        
						cadena,     
						BytesEscribir, 
						&BytesEscritos, 
						NULL);
				}
				else if (strcmp(comando, "senalar mano") == 0) {
					CloseOpen = true;
					swprintf(buffer, 512, L"Ha dicho señalar mano.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);
					swprintf(buffer, 512, L"Señalando mano mediante voz.\n");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);

					char cadena[] = "8";
					DWORD BytesEscribir;         
					DWORD BytesEscritos = 0;     
					BytesEscribir = (DWORD)sizeof(cadena);

					WriteFile(hComm,        
						cadena,     
						BytesEscribir, 
						&BytesEscritos, 
						NULL);

				}
				else if (strcmp(comando, "dejar de senalar")==0){
				CloseOpen = false;
				swprintf(buffer, 512, L"Ha dicho dejar de señalar.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				swprintf(buffer, 512, L"Dejando de señalar mediante voz.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "9";
				DWORD BytesEscribir;        
				DWORD BytesEscritos = 0;     
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,        
					cadena,     
					BytesEscribir,  
					&BytesEscritos, 
					NULL);
				}
				else if (strcmp(comando, "gesto victoria") == 0) {
				CloseOpen = true;
				swprintf(buffer, 512, L"Ha dicho gesto victoria.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				swprintf(buffer, 512, L"Realizando el gesto de Victoria mediante voz.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "10";
				DWORD BytesEscribir;         
				DWORD BytesEscritos = 0;     
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,       
					cadena,     
					BytesEscribir, 
					&BytesEscritos, 
					NULL);
				}
				else if (strcmp(comando, "finalizar gesto") == 0) {
				CloseOpen = false;
				swprintf(buffer, 512, L"Ha dicho finalizar gesto.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				swprintf(buffer, 512, L"Finalizar gesto victoria mediante voz.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "11";
				DWORD BytesEscribir;         
				DWORD BytesEscritos = 0;     
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,        
					cadena,     
					BytesEscribir,  
					&BytesEscritos, 
					NULL);
				}
				else if (strcmp(comando, "iniciar") == 0) {
				swprintf(buffer, 512, L"Ha dicho iniciar.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				IniciarInterfazVoz();
				}
				else if (strcmp(comando, "pausa") == 0) {
				swprintf(buffer, 512, L"Ha dicho pausa.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);
				PausarInterfazVoz();
				}
				else if (strcmp(comando, "detener") == 0) {
				swprintf(buffer, 512, L"Ha dicho detener.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);
				DetenerInterfazVoz();
				}
				else if (strcmp(comando, "cierra aplicacion") == 0) {
				CloseHandle(hComm);
				(NSK_ALGO_UninitAddr)();
				DestroyWindow(Interfaz);
				SendMessage(Interfaz, WM_DESTROY, 0, 0);
				}
				else if (strcmp(comando, "tres dedos") == 0) {
				CloseOpen = true;
				swprintf(buffer, 512, L"Ha dicho tres dedos.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				swprintf(buffer, 512, L"Realizando número tres mediante voz.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "12";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);

				
				}
				else if (strcmp(comando, "cierra aplicacion") == 0) {
				CloseHandle(hComm);
				(NSK_ALGO_UninitAddr)();
				DestroyWindow(Interfaz);
				SendMessage(Interfaz, WM_DESTROY, 0, 0);
				}
				else if (strcmp(comando, "cuatro dedos") == 0) {
				CloseOpen = true;
				swprintf(buffer, 512, L"Ha dicho cuatro dedos.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				swprintf(buffer, 512, L"Realizando número cuatro mediante voz.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "13";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);


				}
				else if (strcmp(comando, "modo tres") == 0) {
				swprintf(buffer, 512, L"Ha dicho modo tres.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);
				if (Button_GetCheck(TresTick) != BST_CHECKED) {
					Button_SetCheck(Auxiliar, false);

					Button_SetCheck(TresTick, true);
					Auxiliar = TresTick;
				}
				char cadena[] = "14";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);

				}
				else if (strcmp(comando, "modo cuatro") == 0) {
				swprintf(buffer, 512, L"Ha dicho modo cuatro.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);
				if (Button_GetCheck(CuatroTick) != BST_CHECKED) {
					Button_SetCheck(Auxiliar, false);

					Button_SetCheck(CuatroTick, true);
					Auxiliar = CuatroTick;
				}
				char cadena[] = "15";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);

				}
				else {
				swprintf(buffer, 512, L"No le he entendido. Repita el comando de voz, por favor.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);
					}
				fflush(stdout);
			}

			if (ps_start_utt(Reconocedor) < 0) {
				swprintf(buffer, 512, L" Fallo al recibir señal.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);
				E_FATAL("Fallo al recibir señal.\n");
			}
			utt_comenzada = FALSE;
			E_INFO("Listo....\n");
		}
		sleep_msec(100);
	}
	ad_close(ad);



	ps_free(Reconocedor);
	cmd_ln_free_r(config);
}



static void HiloCascoNeuronal(LPVOID lpdwThreadParam) {
	int contador = 0;
	wchar_t buffer[100];
	short Datos[512] = { 0 };
	int ultimoDato = 0;

	while (true) {
		
		LecturaPaquetes = TG_ReadPackets(IdConexion, 1);

		
		
		if (LecturaPaquetes == 1) {
			
			if (TG_GetValueStatus(IdConexion, TG_DATA_RAW) != 0) {
				
				Datos[contador++] = (short)TG_GetValue(IdConexion, TG_DATA_RAW);
				ultimoDato = contador;
				if (contador == 512) {
					(NSK_ALGO_DataStreamAddr)(NSK_ALGO_DATA_TYPE_EEG, Datos, contador);
					contador = 0;
				}
			}
			if (TG_GetValueStatus(IdConexion, TG_DATA_POOR_SIGNAL) != 0) {
				short pq = (short)TG_GetValue(IdConexion, TG_DATA_POOR_SIGNAL);
				SYSTEMTIME st;
				GetSystemTime(&st);
				swprintf(buffer, 100, L"%6d, PQ[%3d], [%d]", st.wSecond*1000 + st.wMilliseconds, pq, ultimoDato);
				contador = 0;
				SalidaTextoLog(buffer);
				(NSK_ALGO_DataStreamAddr)(NSK_ALGO_DATA_TYPE_PQ, &pq, 1);
			}
			if (TG_GetValueStatus(IdConexion, TG_DATA_ATTENTION) != 0) {
				short att = (short)TG_GetValue(IdConexion, TG_DATA_ATTENTION);
				(NSK_ALGO_DataStreamAddr)(NSK_ALGO_DATA_TYPE_ATT, &att, 1);
			}
			if (TG_GetValueStatus(IdConexion, TG_DATA_MEDITATION) != 0) {
				short med = (short)TG_GetValue(IdConexion, TG_DATA_MEDITATION);
				(NSK_ALGO_DataStreamAddr)(NSK_ALGO_DATA_TYPE_MED, &med, 1);
			}
		}
		
	}
}

static void SalidaTextoLog(LPCWSTR log) {
	OutputDebugStringW(log);
	OutputDebugStringW(L"\r\n");
}

static void *obtenerFunciones(HINSTANCE instancia, HWND ventana, char *funcNombre, bool *bError) {
	void *funcPtr = (void*)GetProcAddress(instancia, funcNombre);
	*bError = true;
	if (NULL == funcPtr) {
		wchar_t szBuff[100] = { 0 };
		swprintf(szBuff, 100, L"Fallo al obtener la función %s ", (wchar_t*)funcNombre);
		MessageBox(ventana, szBuff, L"Error", MB_OK);
		*bError = false;
	}
	return funcPtr;
}

static bool obtenerDireccionFunciones(HINSTANCE instancia, HWND ventana) {
	bool bError;

	NSK_ALGO_InitAddr = (NSK_ALGO_Init_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_InitStr, &bError);
	NSK_ALGO_UninitAddr = (NSK_ALGO_Uninit_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_UninitStr, &bError);
	NSK_ALGO_RegisterCallbackAddr = (NSK_ALGO_RegisterCallback_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_RegisterCallbackStr, &bError);
	NSK_ALGO_SdkVersionAddr = (NSK_ALGO_SdkVersion_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_SdkVersionStr, &bError);
	NSK_ALGO_AlgoVersionAddr = (NSK_ALGO_AlgoVersion_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_AlgoVersionStr, &bError);
	NSK_ALGO_StartAddr = (NSK_ALGO_Start_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_StartStr, &bError);
	NSK_ALGO_PauseAddr = (NSK_ALGO_Pause_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_PauseStr, &bError);
	NSK_ALGO_StopAddr = (NSK_ALGO_Stop_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_StopStr, &bError);
	NSK_ALGO_DataStreamAddr = (NSK_ALGO_DataStream_Dll)obtenerFunciones(instancia, ventana, NSK_ALGO_DataStreamStr, &bError);

	return bError;
}

static wchar_t *HoraLocal() {
	static wchar_t buffer[128];
	SYSTEMTIME lt;
	GetLocalTime(&lt);
	wsprintf(buffer, L"%2d/%2d/%4d %02d:%02d:%02d:%03d", lt.wDay, lt.wMonth, lt.wYear, lt.wHour, lt.wMinute, lt.wSecond, lt.wMilliseconds);
	return buffer;
}

static void SalidaTextoInterfaz(LPCWSTR Buffer) {
	if (Edit_GetLineCount(TextoSalida) >= 1024) {
		Edit_SetSel(TextoSalida, 0, -1);
		Edit_ReplaceSel(TextoSalida, L"");
	}
	wchar_t buffer[1024];
	int longitud = GetWindowTextLength(TextoSalida);
	
	Edit_SetSel(TextoSalida, longitud, longitud);
	wsprintf(buffer, L"  [%s]: %s\r\n\r\n", HoraLocal(), Buffer);
	Edit_ReplaceSel(TextoSalida, buffer);
	
}

static void FuncionCallback(sNSK_ALGO_CB_PARAM param) {
	static wchar_t buffer[512];
	static wchar_t sbuffer[512];
	static wchar_t qbuffer[512];
	buffer[0] = sbuffer[0] = qbuffer[0] = 0;
	switch (param.cbType) {
	case NSK_ALGO_CB_TYPE_STATE:
	{
		
		eNSK_ALGO_STATE estado = (eNSK_ALGO_STATE)(param.param.state & NSK_ALGO_STATE_MASK);
		eNSK_ALGO_STATE razon = (eNSK_ALGO_STATE)(param.param.state & NSK_ALGO_REASON_MASK);
		swprintf(sbuffer, 512, L"Estado: ");
		switch (estado) {
			
		case NSK_ALGO_STATE_PAUSE:
			swprintf(sbuffer, 512, L"%sPausa", sbuffer);
			bIniciado = false;

			PostMessageW(Interfaz, V_TEXTO_USUARIO, (WPARAM)BotonInicio, (LPARAM)L"Iniciar");
			PostMessageW(Interfaz, WM_ENABLE, (WPARAM)BotonDetener, (LPARAM)true);

			break;
		case NSK_ALGO_STATE_RUNNING:
			swprintf(sbuffer, 512, L"%sIniciado", sbuffer);
			bIniciado = true;

			PostMessageW(Interfaz, V_TEXTO_USUARIO, (WPARAM)BotonInicio, (LPARAM)L"Pausar");
			PostMessageW(Interfaz, WM_ENABLE, (WPARAM)BotonDetener, (LPARAM)true);

			break;
			
		case NSK_ALGO_STATE_STOP:
		{
			swprintf(sbuffer, 512, L"%sDetenido", sbuffer);
			bIniciado = false;

			PostMessageW(Interfaz, V_TEXTO_USUARIO, (WPARAM)BotonInicio, (LPARAM)L"Iniciar");
			PostMessageW(Interfaz, WM_ENABLE, (WPARAM)BotonInicio, (LPARAM)true);
			PostMessageW(Interfaz, WM_ENABLE, (WPARAM)BotonDetener, (LPARAM)false);
			
		}
			break;
			
		default:
			return;
		}
		switch (razon) {
		case NSK_ALGO_REASON_BY_USER:
			swprintf(sbuffer, 512, L"%s | Por usuario", sbuffer);
			break;
		case NSK_ALGO_REASON_CB_CHANGED:
			swprintf(sbuffer, 512, L"%s | CB changed", sbuffer);
			break;
		case NSK_ALGO_REASON_NO_BASELINE:
			swprintf(sbuffer, 512, L"%s | No baseline", sbuffer);
			break;
		case NSK_ALGO_REASON_SIGNAL_QUALITY:
			swprintf(sbuffer, 512, L"%s | Calidad de la señal", sbuffer);
			break;
		default:
			break;
		}
		PostMessageW(Interfaz, V_TEXTO_USUARIO, (WPARAM)TextoEstado, (LPARAM)sbuffer);
	}
		break;
	case NSK_ALGO_CB_TYPE_SIGNAL_LEVEL:
	{
		
		eNSK_ALGO_SIGNAL_QUALITY sq = (eNSK_ALGO_SIGNAL_QUALITY)param.param.sq;
		swprintf(qbuffer, 512, L"Calidad de la señal: ");
		switch (sq) {
		case NSK_ALGO_SQ_GOOD:
			swprintf(qbuffer, 512, L"%sBuena", qbuffer);
			break;
		case NSK_ALGO_SQ_MEDIUM:
			swprintf(qbuffer, 512, L"%sMedia", qbuffer);
			break;
		case NSK_ALGO_SQ_NOT_DETECTED:
			swprintf(qbuffer, 512, L"%sSeñal no detectada", qbuffer);
			break;
		case NSK_ALGO_SQ_POOR:
			swprintf(qbuffer, 512, L"%sPobre", qbuffer);
			break;
		}
		PostMessage(Interfaz, V_TEXTO_USUARIO, (WPARAM)TextoCalidadSenal, (LPARAM)qbuffer);
	}
		break;
	case NSK_ALGO_CB_TYPE_ALGO:
	{
		
		char str[100];
		char str2[100];
		char Special[100];
		memset(str, 0, 100);
		memset(str2, 0, 100);
		memset(Special, 0, 100);
		switch (param.param.index.type) {
			
			case NSK_ALGO_TYPE_BLINK:
			{
				int strength = param.param.index.value.group.eye_blink_strength;
				if (strength >= 80) {
					swprintf(buffer, 512, L"Parpadeo detectado. Fuerza del parpadeo = %4d", strength);
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);
					char strS[10];
					if (CloseOpen == false) {
						char strS2[] = "1";
						CloseOpen = true;
						if (Auxiliar == AbreCierraTick) {
							swprintf(buffer, 512, L"Cerrar Mano mediante parpadeo.");
						}
						else if (Auxiliar == VdeVictoria) {
							swprintf(buffer, 512, L"Realizando gesto victoria mediante parpadeo.");
						}
						else if (Auxiliar == Senalar) {
							swprintf(buffer, 512, L"Señalando mediante parpadeo.");
						}
						else if (Auxiliar == TresTick) {
							swprintf(buffer, 512, L"Realizando número tres mediante parpadeo.");
						}
						else if (Auxiliar == CuatroTick) {
							swprintf(buffer, 512, L"Realizando número mediante parpadeo.");
						}
						
						SalidaTextoInterfaz(buffer);
						SalidaTextoLog(buffer);
						strcpy(strS, strS2);
					}
					else {
						char strS2[] = "0";
						CloseOpen = false;
						if (Auxiliar == AbreCierraTick) {
							swprintf(buffer, 512, L"Abriendo Mano mediante parpadeo.");
						}
						else if (Auxiliar == VdeVictoria) {
							swprintf(buffer, 512, L"Finalizando gesto de victoria mediante parpadeo.");
						}
						else if (Auxiliar == Senalar) {
							swprintf(buffer, 512, L"Dejando de señalar mediante parpadeo.");
						}
						else if (Auxiliar == TresTick) {
							swprintf(buffer, 512, L"Abriendo mano mediante parpadeo.");
						}
						else if (Auxiliar == CuatroTick) {
							swprintf(buffer, 512, L"Abriendo mano mediante parpadeo.");
						}

						SalidaTextoInterfaz(buffer);
						SalidaTextoLog(buffer);
						strcpy(strS, strS2);
					}
					
					DWORD BytesEscribir;        
					DWORD BytesEscritos = 0;     
					BytesEscribir = (DWORD)sizeof(strS);

					WriteFile(hComm,       
						strS,     
						BytesEscribir, 
						&BytesEscritos, 
						NULL);


				}
				else {
					swprintf(buffer, 512, L"La fuerza del parpadeo ha sido insuficiente.");
					SalidaTextoInterfaz(buffer);
					SalidaTextoLog(buffer);
				}

				
				break;
			}
			
		}


		
	}
	

		break;
	}
}


int APIENTRY wWinMain(_In_ HINSTANCE Instancia, _In_opt_ HINSTANCE InstPrevia,
                     _In_ LPWSTR    LineaCommandos, _In_ int       MuestraVCmd)
{
    UNREFERENCED_PARAMETER(InstPrevia);
    UNREFERENCED_PARAMETER(LineaCommandos);

  
    LoadStringW(Instancia, IDS_APP_TITULO, Titulo, MAX_LONGCADENA);
    LoadStringW(Instancia, IDC_INTERFAZ, NombreVentana, MAX_LONGCADENA);
    RegistroObjetoVentana(Instancia);

 
    if (!InicializarInstancia (Instancia, MuestraVCmd))
    {
        return FALSE;
    }

    HACCEL Tabla = LoadAccelerators(Instancia, MAKEINTRESOURCE(IDC_INTERFAZ));

    MSG mensaje;

   
    while (GetMessage(&mensaje, nullptr, 0, 0))
    {
        if (!TranslateAccelerator(mensaje.hwnd, Tabla, &mensaje))
        {
            TranslateMessage(&mensaje);
            DispatchMessage(&mensaje);
        }
    }

    return (int) mensaje.wParam;
}





ATOM RegistroObjetoVentana(HINSTANCE Instancia)
{
    WNDCLASSEXW ObjetoRegistro;

    ObjetoRegistro.cbSize = sizeof(WNDCLASSEX);

    ObjetoRegistro.style          = CS_HREDRAW | CS_VREDRAW;
    ObjetoRegistro.lpfnWndProc    = ProcesosVentana;
    ObjetoRegistro.cbClsExtra     = 0;
    ObjetoRegistro.cbWndExtra     = 0;
    ObjetoRegistro.hInstance      = Instancia;
    ObjetoRegistro.hIcon          = LoadIcon(Instancia, MAKEINTRESOURCE(IDI_INTERFAZ));
    ObjetoRegistro.hCursor        = LoadCursor(nullptr, IDC_HAND);
    ObjetoRegistro.hbrBackground  = (HBRUSH)(COLOR_WINDOW+1);
    ObjetoRegistro.lpszMenuName   = MAKEINTRESOURCEW(IDC_INTERFAZ);
    ObjetoRegistro.lpszClassName  = NombreVentana;
    ObjetoRegistro.hIconSm        = LoadIcon(ObjetoRegistro.hInstance, MAKEINTRESOURCE(IDI_ICONO_PEQUENO));

    return RegisterClassExW(&ObjetoRegistro);
}


BOOL InicializarInstancia(HINSTANCE Instancia, int MuestraVCmd)
{
   hInstancia = Instancia; 
   setlocale(LC_ALL, "es_ES");

   Interfaz = CreateWindowW(NombreVentana, Titulo, WS_OVERLAPPEDWINDOW,
      CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, nullptr, nullptr, Instancia, nullptr);

   if (!Interfaz)
   {
      return FALSE;
   }
   ShowWindow(Interfaz, MuestraVCmd);
   UpdateWindow(Interfaz);
   if (hComm == INVALID_HANDLE_VALUE) {
	   MessageBox(Interfaz, L"Exoesqueleto activo de mano no detectado. Por favor, cierre la aplicación y conecte la placa Arduino en el puerto COM correspondiente.", L"Error", MB_OK);
   }
   else {
	 
	   DCB ParametrosSerial = { 0 };
	   ParametrosSerial.DCBlength = sizeof(ParametrosSerial);
	   GetCommState(hComm, &ParametrosSerial);
	   ParametrosSerial.BaudRate = CBR_9600;  
	   ParametrosSerial.ByteSize = 8;        
	   ParametrosSerial.StopBits = ONESTOPBIT;
	   ParametrosSerial.Parity = NOPARITY;
	   SetCommState(hComm, &ParametrosSerial);
	   COMMTIMEOUTS limiteTiempo = { 0 };
	   limiteTiempo.ReadIntervalTimeout = 50; 
	   limiteTiempo.ReadTotalTimeoutConstant = 50; 
	   limiteTiempo.ReadTotalTimeoutMultiplier = 10; 
	   limiteTiempo.WriteTotalTimeoutConstant = 50; 
	   limiteTiempo.WriteTotalTimeoutMultiplier = 10;
	   SetCommTimeouts(hComm, &limiteTiempo);
   }

   return TRUE;
}

static void ActualizarVentana(HWND Interfaz) {
	RECT rect;
	GetClientRect(Interfaz, &rect);
	
	SetWindowPos(BotonVersion, NULL, MARGEN + MARGEN + ANCHO_BOTON, MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);

	SetWindowPos(BotonDedos1, NULL, MARGEN + MARGEN + ANCHO_BOTON + MARGEN + ANCHO_BOTON * 2, MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);
	SetWindowPos(BotonDedos2, NULL, MARGEN + MARGEN + ANCHO_BOTON + MARGEN + ANCHO_BOTON * 2 + MARGEN + ANCHO_BOTON, MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);
	SetWindowPos(BotonDedos3, NULL, MARGEN + MARGEN + ANCHO_BOTON + MARGEN + ANCHO_BOTON * 2, MARGEN + ALTO_BOTON + MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);
	SetWindowPos(BotonDedos4, NULL, MARGEN + MARGEN + ANCHO_BOTON + MARGEN + ANCHO_BOTON * 2 + MARGEN + ANCHO_BOTON, MARGEN + ALTO_BOTON + MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);
	SetWindowPos(BotonDedos5, NULL, MARGEN + MARGEN + ANCHO_BOTON + MARGEN + ANCHO_BOTON * 2, MARGEN + ALTO_BOTON + MARGEN + ALTO_BOTON + MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);






	SetWindowPos(BotonInicio, NULL, MARGEN, MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);

	SetWindowPos(BotonDetener, NULL, MARGEN, (MARGEN + ALTO_BOTON) + MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);
	SetWindowPos(TextoEstado, NULL, MARGEN, rect.bottom - ALTO_ESTADO_TEXTO - MARGEN, ANCHO_ESTADO_TEXTO, ALTO_ESTADO_TEXTO, SWP_NOZORDER);
	SetWindowPos(TextoCalidadSenal, NULL, rect.right - ANCHO_SENAL_TEXTO - MARGEN, rect.bottom - ALTO_SENAL_TEXTO - MARGEN, ANCHO_SENAL_TEXTO, ALTO_SENAL_TEXTO, SWP_NOZORDER);

	SetWindowPos(VdeVictoria, NULL, rect.right - ANCHO_BOTON - MARGEN, MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);
	SetWindowPos(AbreCierraTick, NULL, rect.right - (ANCHO_BOTON + MARGEN) * 2, MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);
	SetWindowPos(Senalar, NULL, rect.right - ANCHO_BOTON - MARGEN, MARGEN + ALTO_BOTON + MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);

	SetWindowPos(TresTick, NULL, rect.right - (ANCHO_BOTON + MARGEN) * 2, MARGEN + ALTO_BOTON + MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);
	SetWindowPos(CuatroTick, NULL, rect.right - ANCHO_BOTON - MARGEN, MARGEN + ALTO_BOTON + MARGEN + ALTO_BOTON + MARGEN, ANCHO_BOTON, ALTO_BOTON, SWP_NOZORDER);

	SetWindowPos(TextoSalida, NULL, MARGEN, MARGEN + (ALTO_BOTON + MARGEN) * 4 + MARGEN, rect.right - (MARGEN * 2), rect.bottom - (MARGEN + (ALTO_BOTON + MARGEN) * 4 + MARGEN) - ALTO_BOTON - (MARGEN * 2), SWP_NOZORDER);
	
}


LRESULT CALLBACK ProcesosVentana(HWND Interfaz, UINT mensaje, WPARAM wParam, LPARAM lParam)
{
    switch (mensaje)
    {
	case WM_CREATE:
	{
		HINSTANCE InstanciaLibreria = LoadLibrary(ALGO_SDK_DLL);
		if (InstanciaLibreria == NULL) {
			MessageBox(Interfaz, L"Fallo al abrir la librería AlgoSdkDll.dll", L"Error", MB_OK);
			SalidaTextoLog(L"Fallo al abrir la librería AlgoSdkDll.dll");
		}
		else {
			SalidaTextoLog(L"Librería AlgoSdkDll.dll abierta correctamente");
			if (obtenerDireccionFunciones(InstanciaLibreria, Interfaz) == false) {
				FreeLibrary(InstanciaLibreria);
				return FALSE;
			}
			
		}

		
		IdConexion = TG_GetNewConnectionId();
		if (IdConexion < 0) {
			MessageBox(Interfaz, L"Fallo al recibir nuevo id de conexión", L"Error", MB_OK);
		}
		else {
			comPortName = "\\\\.\\" PUERTO_COM;
			errCode = TG_Connect(IdConexion,
				comPortName,
				TG_BAUD_57600,
				TG_STREAM_PACKETS);
			if (errCode < 0) {
				MessageBox(Interfaz, L"TG_Connect() falló.\n Casco neuronal no detectado.", L"Error", MB_OK);
				if ((Hilo2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WordSpotting, NULL, 0, &IdHilo2)) == NULL) {
					MessageBox(Interfaz, L"Fallo al crear el paquete de lectura del hilo", L"Error", MB_OK);
				}
			} else {
				wchar_t buffer[100];

				swprintf(buffer, 100, L"Conectado al casco neuronal en el puerto %S", PUERTO_COM);
				MessageBox(Interfaz, buffer, L"Información", MB_OK);
				bConnectedHeadset = true;
				if ((Hilo = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&HiloCascoNeuronal, NULL, 0, &IdHilo)) == NULL) {
					MessageBox(Interfaz, L"Fallo al crear el paquete de lectura del hilo", L"Error", MB_OK);
				}
				if ((Hilo2 = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)&WordSpotting, NULL, 0, &IdHilo2)) == NULL) {
					MessageBox(Interfaz, L"Fallo al crear el paquete de lectura del hilo", L"Error", MB_OK);
				}
			}
		}
		
		BotonInicio = CreateWindow(L"Button", L"Iniciar", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON , 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_INICIAR, hInstancia, NULL);
		BotonDetener = CreateWindow(L"Button", L"Detener", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_DETENER, hInstancia, NULL);
		
		TextoEstado = CreateWindow(L"Static", L"Estado: ", WS_VISIBLE | WS_CHILD, 0, 0, ANCHO_ESTADO_TEXTO, ALTO_ESTADO_TEXTO, Interfaz, (HMENU)IDD_ESTADO, hInstancia, NULL);
		TextoCalidadSenal = CreateWindow(L"Static", L"Calidad de señal: ", WS_VISIBLE | WS_CHILD, 0, 0, ANCHO_SENAL_TEXTO, ALTO_SENAL_TEXTO, Interfaz, (HMENU)IDD_SENAL, hInstancia, NULL);
		BotonVersion = CreateWindow(L"Button", L"Version", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_VERSION, hInstancia, NULL);
		TextoSalida = CreateWindow(L"Edit", L"", WS_VISIBLE | WS_CHILD | WS_VSCROLL | ES_LEFT | ES_MULTILINE | ES_AUTOVSCROLL, 0, 0, 10, 10, Interfaz, (HMENU)IDD_TEXTO, hInstancia, NULL);
		
		BotonDedos1 = CreateWindow(L"Button", L"Uno/Señalar", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_DEDOS1, hInstancia, NULL);
		BotonDedos2 = CreateWindow(L"Button", L"Dos/Victoria", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_DEDOS2, hInstancia, NULL);
		BotonDedos3 = CreateWindow(L"Button", L"Tres", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_DEDOS3, hInstancia, NULL);
		BotonDedos4 = CreateWindow(L"Button", L"Cuatro", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_DEDOS4, hInstancia, NULL);
		BotonDedos5 = CreateWindow(L"Button", L"Cinco/Abrir/Cerrar", WS_TABSTOP | WS_VISIBLE | WS_CHILD | BS_DEFPUSHBUTTON, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_DEDOS5, hInstancia, NULL);

		AbreCierraTick = CreateWindow(L"Button", L"AbreCierra", WS_VISIBLE | WS_CHILD | BS_CHECKBOX , 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_ABRECIERRA, hInstancia, NULL);
		VdeVictoria = CreateWindow(L"Button", L"VdeVictoria", WS_VISIBLE | WS_CHILD | BS_CHECKBOX, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_VdeVictoria, hInstancia, NULL);
		Senalar = CreateWindow(L"Button", L"Señalar", WS_VISIBLE | WS_CHILD | BS_CHECKBOX, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_Senalar, hInstancia, NULL);
		
		TresTick = CreateWindow(L"Button", L"Modo Tres", WS_VISIBLE | WS_CHILD | BS_CHECKBOX, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_MODOTRES, hInstancia, NULL);
		CuatroTick = CreateWindow(L"Button", L"Modo Cuatro", WS_VISIBLE | WS_CHILD | BS_CHECKBOX, 10, 10, ANCHO_BOTON, ALTO_BOTON, Interfaz, (HMENU)IDD_MODOCUATRO, hInstancia, NULL);




		Button_Enable(BotonDetener, false);
		//Edit_Enable(TextoSalida, FALSE);
	
		
		
		ActualizarVentana(Interfaz);
	}
		break;
	case V_TEXTO_USUARIO:
		if ((HWND)wParam == TextoEstado) {
			SalidaTextoLog((LPCWSTR)lParam);
		}
		SetWindowText((HWND)wParam, (LPCWSTR)lParam);
		break;
	case WM_ENABLE:
		EnableWindow((HWND)wParam, (BOOL)lParam);
		
		break;
	case WM_CTLCOLOREDIT:
	{
		HDC hdc = (HDC)wParam;
		
		SetTextColor(hdc, RGB(0, 0, 0));
		SetBkColor(hdc, RGB(200, 200, 200));

	}
		break;
	case WM_CTLCOLORBTN:
	{
		HDC hdc = (HDC)wParam;
		SetTextColor(hdc, RGB(255, 255, 255));
		SetBkColor(hdc, RGB(35, 85, 220));

		
	}
	break;
	
    case WM_COMMAND:
        {
            int wmId = LOWORD(wParam);
            
            switch (wmId)
            {
			
			case IDD_ABRECIERRA:
			{
				char cadena[] = "5";
				DWORD BytesEscribir;         
				DWORD BytesEscritos = 0;     
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,        
					cadena,     
					BytesEscribir,  
					&BytesEscritos, 
					NULL);
				if (Button_GetCheck((HWND)lParam) == BST_CHECKED) {
					Button_SetCheck((HWND)lParam, false);
				}
				else {

					Button_SetCheck(Auxiliar, false);

					Button_SetCheck((HWND)lParam, true);
					Auxiliar = (HWND)lParam;
				}
			}
			break;
			case IDD_VdeVictoria:
			{
				char cadena[] = "4";
				DWORD BytesEscribir;         
				DWORD BytesEscritos = 0;     
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,       
					cadena,     
					BytesEscribir,  
					&BytesEscritos, 
					NULL);
				if (Button_GetCheck((HWND)lParam) == BST_CHECKED) {
					Button_SetCheck((HWND)lParam, false);


				}
				else {

					Button_SetCheck(Auxiliar, false);

					Button_SetCheck((HWND)lParam, true);
					Auxiliar = (HWND)lParam;
				}
			}
			break;
			case IDD_Senalar:
			
			{
				char cadena[] = "3";
				DWORD BytesEscribir;         
				DWORD BytesEscritos = 0;    
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,        
					cadena,     
					BytesEscribir,  
					&BytesEscritos, 
					NULL);
				if (Button_GetCheck((HWND)lParam) == BST_CHECKED) {
					Button_SetCheck((HWND)lParam, false);
					

				} else {
					
						Button_SetCheck(Auxiliar, false);
					
					Button_SetCheck((HWND)lParam, true);
					Auxiliar = (HWND)lParam;
				}
			}
				break;
			case IDD_MODOTRES: 
			{
				char cadena[] = "14";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);
				if (Button_GetCheck((HWND)lParam) == BST_CHECKED) {
					Button_SetCheck((HWND)lParam, false);
				}
				else {

					Button_SetCheck(Auxiliar, false);

					Button_SetCheck((HWND)lParam, true);
					Auxiliar = (HWND)lParam;
				}
			}
			break;
			case IDD_MODOCUATRO:
			{
				char cadena[] = "15";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);
				if (Button_GetCheck((HWND)lParam) == BST_CHECKED) {
					Button_SetCheck((HWND)lParam, false);
				}
				else {

					Button_SetCheck(Auxiliar, false);

					Button_SetCheck((HWND)lParam, true);
					Auxiliar = (HWND)lParam;
				}
			}
			break;

			case IDD_INICIAR:		
			case IDD_DETENER:		
			
				if (((HWND)lParam) && (HIWORD(wParam) == BN_CLICKED)) {
					
					if ((HWND)lParam == BotonInicio) {
						wchar_t LecturaBuffer[1024] = { 0 };
						char charBuffer[1024] = { 0 };
						eNSK_ALGO_RET ret;
						wchar_t texto[20];
						Button_GetText(BotonInicio, texto, 20);
						ModosSeleccionados = 0;
						if (Button_GetCheck(AbreCierraTick)) {
							
							ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
							ModeChange = 3;
						}
						if (Button_GetCheck(VdeVictoria)) {
							
							ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
							ModeChange = 2;
						}
						if (Button_GetCheck(Senalar)) {
							ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
							ModeChange = 1;
						}
						if (Button_GetCheck(TresTick)) {
							ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
							ModeChange = 14;
						}
						if (Button_GetCheck(CuatroTick)) {
							ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
							ModeChange = 15;
						}

						if (ModosSeleccionados == 0) {
							MessageBox(Interfaz, L"Por favor, seleccione al menos un modo.", L"Error", MB_OK);
							
							return 0;
						}
						else {
							
						}
						if (Iniciado == 0 && lstrcmpW(texto, L"Iniciar") == 0) {
							ret = (NSK_ALGO_RegisterCallbackAddr)(&FuncionCallback, Interfaz);
						}
						else {
							Iniciado = 1;
						}
						ret = (NSK_ALGO_InitAddr)((eNSK_ALGO_TYPE)(ModosSeleccionados), charBuffer);
						if (lstrcmpW(texto, L"Iniciar") == 0) {
							ret = (NSK_ALGO_StartAddr)(NS_FALSE);
							if (NSK_ALGO_RET_SUCCESS == ret) {
								SalidaTextoLog(L"Se inició correctamente");
							} else {
								wchar_t buffer[100];
								swprintf(buffer, 100, L"El inicio falló con error: [%d]", ret);
								MessageBox(Interfaz, buffer, L"Error", MB_OK);
								SalidaTextoLog(buffer);
							}
						} else {
							ret = (NSK_ALGO_PauseAddr)();
							if (NSK_ALGO_RET_SUCCESS == ret) {
								SalidaTextoLog(L"Pausado");
							}
							else {
								wchar_t buffer[100];
								swprintf(buffer, 100, L"Fallo en la pausa: [%d]", ret);
								MessageBox(Interfaz, buffer, L"Error", MB_OK);
								SalidaTextoLog(buffer);
							}
						}
					} else if ((HWND)lParam == BotonDetener) {
						eNSK_ALGO_RET ret = (NSK_ALGO_StopAddr)();
						if (NSK_ALGO_RET_SUCCESS == ret) {
							
							SalidaTextoLog(L"Detenido correctamente");
						}
						else {
							wchar_t buffer[100];
							swprintf(buffer, 100, L"Fallo al detener: [%d]", ret);
							MessageBox(Interfaz, buffer, L"Error", MB_OK);
							SalidaTextoLog(buffer);
						}
					}
				}
				break;
			case IDD_VERSION:
				wchar_t buffer[1024];
				swprintf(buffer, 1024, L"Interfaz de comunicación entre Mindwave Mobile y Exoesqueleto activo de mano y WordSpotting.\nRealizado por Alejandro Díaz Sadoc.\nV 1.2");
					MessageBox(Interfaz,buffer,L"Información",MB_OK);

					break;
			case IDD_DEDOS1: {
				CloseOpen = true;
				swprintf(buffer, 512, L"Señalando mediante botón.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "8";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);

				
			}
				break;
			case IDD_DEDOS2:
			{
				CloseOpen = true;
				
				swprintf(buffer, 512, L"Realizando gesto victoria mediante botón.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "10";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);

			}
			break;
			case IDD_DEDOS3:
			{
				CloseOpen = true;

				swprintf(buffer, 512, L"Realizando número tres mediante botón.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "12";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);


			}
			break;
			case IDD_DEDOS4:
			{
				CloseOpen = true;

				swprintf(buffer, 512, L"Realizando número cuatro mediante botón.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "13";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);

			}
			break;
			case IDD_DEDOS5:
			{
				if (CloseOpen == true){
					CloseOpen = false;
				swprintf(buffer, 512, L"Abriendo mano mediante botón.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "6";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);
			}else if (CloseOpen == false) {
				CloseOpen = true;
				

				swprintf(buffer, 512, L"Cerrando mano mediante botón.\n");
				SalidaTextoInterfaz(buffer);
				SalidaTextoLog(buffer);

				char cadena[] = "7";
				DWORD BytesEscribir;
				DWORD BytesEscritos = 0;
				BytesEscribir = (DWORD)sizeof(cadena);

				WriteFile(hComm,
					cadena,
					BytesEscribir,
					&BytesEscritos,
					NULL);
			
			
			}
			}
			break;
            case IDM_INFO:
                DialogBox(hInstancia, MAKEINTRESOURCE(IDD_INFOVENTANA), Interfaz, Informacion);
                break;
            case IDM_SALIDA:
				CloseHandle(hComm);
				(NSK_ALGO_UninitAddr)();
                DestroyWindow(Interfaz);

                break;
            default:
                return DefWindowProc(Interfaz, mensaje, wParam, lParam);
            }
        }
        break;
    case WM_PAINT:
        {
            PAINTSTRUCT ps;
            HDC hdc = BeginPaint(Interfaz, &ps);
            EndPaint(Interfaz, &ps);
        }
        break;
	case WM_SIZING:
		{
			ActualizarVentana(Interfaz);
		}
		break;
	case WM_SIZE:
		ActualizarVentana(Interfaz);

		break;
	
	case WM_CTLCOLORSTATIC:
	{
		HDC hdcStatic = (HDC)wParam;
		//SetTextColor(hdcStatic, RGB(255, 255, 255));
		//SetBkColor(hdcStatic, RGB(35, 85, 220));
		//return (INT_PTR)CreateSolidBrush(RGB(230, 230, 230));
	}
	break;
    case WM_DESTROY:
		if (IdConexion >= 0) {
			TG_Disconnect(IdConexion); 

			TG_FreeConnection(IdConexion);

			IdConexion = -1;
		}
		if (Hilo != NULL) {
			TerminateThread(Hilo, 0);
			Hilo = NULL;
		}
		if (Hilo2 != NULL) {
			TerminateThread(Hilo2, 0);
			Hilo2 = NULL;
		}
		DestroyWindow(BotonInicio);
		DestroyWindow(BotonDetener);
		DestroyWindow(AbreCierraTick);
		DestroyWindow(TresTick);
		DestroyWindow(CuatroTick);
		DestroyWindow(VdeVictoria);
		DestroyWindow(Senalar);
		DestroyWindow(TextoEstado);
		DestroyWindow(TextoCalidadSenal);
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(Interfaz, mensaje, wParam, lParam);
    }
    return 0;
}


int DetenerInterfazVoz() {
	wchar_t texto[20];
	Button_GetText(BotonInicio, texto, 20);
	if (lstrcmpW(texto, L"Iniciar") == 0 && bPausado==false) {
		MessageBox(Interfaz, L"Por favor, inicie primero la ejecución.", L"Error", MB_OK);
	}
	else {
		eNSK_ALGO_RET ret = (NSK_ALGO_StopAddr)();
		if (NSK_ALGO_RET_SUCCESS == ret) {
			
			SalidaTextoLog(L"Detenido correctamente");
		}
		else {
			wchar_t buffer[100];
			swprintf(buffer, 100, L"Error en la detención: [%d]", ret);
			MessageBox(Interfaz, buffer, L"Error", MB_OK);
			SalidaTextoLog(buffer);
		}
	}
	return 0;
}

int PausarInterfazVoz() {
	
	wchar_t LecturaBuffer[1024] = { 0 };
	char charBuffer[1024] = { 0 };
	eNSK_ALGO_RET ret;
	wchar_t texto[20];
	Button_GetText(BotonInicio, texto, 20);
	ModosSeleccionados = 0;
	if (Button_GetCheck(AbreCierraTick)) {
		
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 3;
	}
	if (Button_GetCheck(VdeVictoria)) {
		
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 2;
	}
	if (Button_GetCheck(Senalar)) {
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 1;
	}
	if (Button_GetCheck(TresTick)) {
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 14;
	}
	if (Button_GetCheck(CuatroTick)) {
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 15;
	}
	
	if (lstrcmpW(texto, L"Iniciar") == 0) {
		MessageBox(Interfaz, L"Por favor, inicie primero la ejecución.", L"Error", MB_OK);
	}
	else if (ModosSeleccionados == 0) {
		MessageBox(Interfaz, L"Por favor, seleccione al menos un modo.", L"Error", MB_OK);
		
		return 0;
	}
	else {
		bPausado = true;
			
		
	}
	
	ret = (NSK_ALGO_InitAddr)((eNSK_ALGO_TYPE)(ModosSeleccionados), charBuffer);
	if (lstrcmpW(texto, L"Iniciar") == 0) {
	
	}
	else {
		ret = (NSK_ALGO_PauseAddr)();
		if (NSK_ALGO_RET_SUCCESS == ret) {
			SalidaTextoLog(L"Pausado correctamente");
		}
		else {
			wchar_t buffer[100];
			swprintf(buffer, 100, L"Error en la pausa: [%d]", ret);
			MessageBox(Interfaz, buffer, L"Error", MB_OK);
			SalidaTextoLog(buffer);
		}
	}
	return 0;
}

int IniciarInterfazVoz() {
	bPausado = false;
	wchar_t LecturaBuffer[1024] = { 0 };
	char charBuffer[1024] = { 0 };
	eNSK_ALGO_RET ret;
	wchar_t texto[20];
	Button_GetText(BotonInicio, texto, 20);
	ModosSeleccionados = 0;
	if (Button_GetCheck(AbreCierraTick)) {
		
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 3;
	}
	if (Button_GetCheck(VdeVictoria)) {
		
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 2;
	}
	if (Button_GetCheck(Senalar)) {
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 1;
	}
	if (Button_GetCheck(TresTick)) {
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 14;
	}
	if (Button_GetCheck(CuatroTick)) {
		ModosSeleccionados |= NSK_ALGO_TYPE_BLINK;
		ModeChange = 15;
	}
	

	if (ModosSeleccionados == 0) {
		MessageBox(Interfaz, L"Por favor, seleccione al menos un modo.", L"Error", MB_OK);
		
		return 0;
	}
	else {
		
	}
	if (Iniciado == 0) {
		ret = (NSK_ALGO_RegisterCallbackAddr)(&FuncionCallback, Interfaz);
	}
	else {
		Iniciado = 1;
	}
	ret = (NSK_ALGO_InitAddr)((eNSK_ALGO_TYPE)(ModosSeleccionados), charBuffer);
	if (lstrcmpW(texto, L"Iniciar") == 0) {
		ret = (NSK_ALGO_StartAddr)(NS_FALSE);
		if (NSK_ALGO_RET_SUCCESS == ret) {
			SalidaTextoLog(L"Iniciado correctamente");
		}
		else {
			wchar_t buffer[100];
			swprintf(buffer, 100, L"Error en el inicio: [%d]", ret);
			MessageBox(Interfaz, buffer, L"Error", MB_OK);
			SalidaTextoLog(buffer);
		}
	}
	else {
		
	}
	return 0;
}



INT_PTR CALLBACK Informacion(HWND ventana, UINT mensaje, WPARAM wParam, LPARAM lParam)
{
    UNREFERENCED_PARAMETER(lParam);
    switch (mensaje)
    {
    case WM_INITDIALOG:
        return (INT_PTR)TRUE;

    case WM_COMMAND:
        if (LOWORD(wParam) == IDOK || LOWORD(wParam) == IDCANCEL)
        {
            EndDialog(ventana, LOWORD(wParam));
            return (INT_PTR)TRUE;
        }
        break;
    }
    return (INT_PTR)FALSE;
}
