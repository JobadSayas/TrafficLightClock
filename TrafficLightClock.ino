// VERSION: 1.60 - Nap Hour y Duration con Save

#include <Wire.h>
#include <RTClib.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#include <EEPROM.h>  // Nueva librería para persistencia

// Variables globales optimizadas
int ultimoMinutoImpreso = -1;
unsigned long ultimaInteraccion = 0;
const unsigned long TIEMPO_PANTALLA_ACTIVA = 30000; // 30 segundos
bool pantallaApagada = false;
unsigned long tiempoEncendidoPantalla = 0; // Para controlar el bloqueo temporal
const unsigned long TIEMPO_BLOQUEO_BOTONES = 300; // 300ms de bloqueo después de encender

// Direcciones EEPROM para configuración persistente
#define EE_MAGIC_BYTE 0          // Byte mágico para verificar si EEPROM está inicializada
#define EE_MAGIC_VALUE_V143 0x55 // Layout de la v1.43 (un solo horario de despertar)
#define EE_MAGIC_VALUE_V144 0x56 // Layout v1.44 (horarios compactos 1-3)
#define EE_MAGIC_VALUE 0x57      // Layout v1.54 (3 slots fijos, bit de ocupado)
#define EE_HORA_DESPERTAR 1      // Hora del horario 1 (se conserva la dirección de v1.43)
#define EE_MINUTOS_DESPERTAR 2   // Minutos del horario 1
#define EE_HORA_DORMIR 3         // Hora de dormir
#define EE_MINUTOS_DORMIR 4      // Minutos de dormir
#define EE_NAP_ACTIVO 5          // Estado Nap (0=Off, 1=On)
#define EE_NAP_HORA_INICIO 6     // Hora inicio nap
#define EE_NAP_MINUTO_INICIO 7   // Minuto inicio nap
#define EE_NAP_DURACION_HORAS 8  // Duración horas nap
#define EE_NAP_DURACION_MINUTOS 9 // Duración minutos nap
#define EE_WAKE0_RANGO 10        // Rango de días del horario 1
#define EE_WAKE_COUNT 11         // Bits 0-2: el slot correspondiente tiene hora
#define EE_WAKE1_HORA 12         // Horario 2
#define EE_WAKE1_MINUTOS 13
#define EE_WAKE1_RANGO 14
#define EE_WAKE2_HORA 15         // Horario 3
#define EE_WAKE2_MINUTOS 16
#define EE_WAKE2_RANGO 17
#define EE_TOTAL_CONFIG_BYTES 18 // Total de bytes usados en EEPROM

// Rangos de días. El orden es el que recorre el botón Change.
#define DAY_ALL 0
#define DAY_WEEK 1
#define DAY_WEEKEND 2
#define DAY_SUN 3
#define DAY_MON 4
#define DAY_TUE 5
#define DAY_WED 6
#define DAY_THU 7
#define DAY_FRI 8
#define DAY_SAT 9
#define DAY_RANGE_COUNT 10
#define MAX_WAKE_SLOTS 3

// Valores por defecto
const uint8_t DEFAULT_HORA_DESPERTAR = 8;
const uint8_t DEFAULT_MINUTOS_DESPERTAR = 0;
const uint8_t DEFAULT_HORA_DORMIR = 19;
const uint8_t DEFAULT_MINUTOS_DORMIR = 30;
const uint8_t DEFAULT_NAP_HORA_INICIO = 13;
const uint8_t DEFAULT_NAP_MINUTO_INICIO = 0;
const uint8_t DEFAULT_NAP_DURACION_HORAS = 1;
const uint8_t DEFAULT_NAP_DURACION_MINUTOS = 30;

// Horarios de despertar (hasta 3). Se cargan desde EEPROM en setup().
uint8_t wakeHora[MAX_WAKE_SLOTS];
uint8_t wakeMinutos[MAX_WAKE_SLOTS];
uint8_t wakeRango[MAX_WAKE_SLOTS];
uint8_t wakeOcupado = 1; // Bit i encendido: el slot i tiene hora
uint8_t horaDormir;
uint8_t minutosDormir;

// Textos en flash para no gastar RAM
const char dayRangeLabels[] PROGMEM =
  "All days [Ad]\0"
  "Week days [Wd]\0"
  "Week end [We]\0"
  "Sundays [Su]\0"
  "Mondays [M]\0"
  "Tuesdays [Tu]\0"
  "Wednesdays [W]\0"
  "Thursdays [Th]\0"
  "Fridays [Fr]\0"
  "Saturdays [Sa]\0";
const char dayRangeShort[] PROGMEM = "Ad\0Wd\0We\0Su\0M\0Tu\0W\0Th\0Fr\0Sa\0";
const char weekdayNames[] PROGMEM = "Sun\0Mon\0Tue\0Wed\0Thu\0Fri\0Sat\0";
const char weekdayFullNames[] PROGMEM =
  "Sunday\0Monday\0Tuesday\0Wednesday\0Thursday\0Friday\0Saturday\0";

// Variables para Nap Mode (nuevo sistema completo) - optimizado a uint8_t
bool napActivo; // Estado del nap
uint8_t napHoraInicio; // Hora de inicio del nap
uint8_t napMinutoInicio; // Minuto de inicio del nap
uint8_t napDuracionHoras; // Duración en horas (0-3)
uint8_t napDuracionMinutos; // Duración en minutos
bool enNapMenu = false; // Controlar si estamos en el menú de Nap
uint8_t napMenuOpcion = 0; // 0=Nap toggle, 1=Start, 2=Duration, 3=Back
bool napEditando = false; // Estamos editando Hour o Duration?
bool napEditandoStart = true; // true=Hour, false=Duration
bool napConfirmando = false;
uint8_t napConfirmOpcion = 0; // 0=Save, 1=Cancel
uint8_t napEditandoCampo = 0; // 0=hora/horas, 1=minuto/minutos
uint8_t napHoraTemporal = 0;
uint8_t napMinutoTemporal = 0;
uint8_t napDuracionHorasTemporal = 0;
uint8_t napDuracionMinutosTemporal = 0;
const uint8_t napTotalOpciones = 4; // Nap toggle, Start, Duration, Back

// NUEVAS VARIABLES PARA NAP MODE COMPLETO
unsigned long inicioSiestaReal = 0; // Cuando se activa Sleep Mode para siesta
bool modoSiestaActivo = false; // Indica si estamos en modo siesta (rojo tenue + verde tenue)
bool enVerdeExtraSiesta = false; // Indica si estamos en la hora extra de verde tenue
const uint8_t UMBRAL_HORA_SIESTA = 17; // 5:00 PM - límite entre siesta y noche
unsigned long tiempoFinSiesta = 0; // Cuando termina el rojo tenue de la siesta
unsigned long tiempoFinVerdeExtra = 0; // Cuando termina la hora extra verde

// Variables temporales para edición de reloj - optimizado a uint8_t
uint8_t horaTemporalReloj = 0;
uint8_t minutosTemporalReloj = 0;
uint8_t diaTemporalReloj = 0;

// Variables para 12-hour sleep y Force color
bool doceHorasSleep = false;
unsigned long inicioDoceHoras = 0;
const unsigned long DOCE_HORAS_MS = 12UL * 60UL * 60UL * 1000UL; // 12 horas en milisegundos
const unsigned long QUINCE_MINUTOS_MS = 15UL * 60UL * 1000UL;    // 15 minutos en milisegundos
const unsigned long DOCE_HORAS_QUINCE_MIN_MS = DOCE_HORAS_MS + QUINCE_MINUTOS_MS; // 12h 15min

// Force color: el modo decide cuánto dura y el color decide qué luz.
#define FORCE_MODE_OFF 0
#define FORCE_MODE_PERSISTENT 1
#define FORCE_MODE_1HOUR 2
#define FORCE_MODE_UNTIL 3

#define FORCE_COLOR_GREEN_HI 0
#define FORCE_COLOR_GREEN_LO 1
#define FORCE_COLOR_YELLOW_HI 2
#define FORCE_COLOR_YELLOW_LO 3
#define FORCE_COLOR_RED_HI 4
#define FORCE_COLOR_RED_LO 5
#define FORCE_COLOR_ALL_OFF 6

#define FC_ROOT 0
#define FC_MODE 1
#define FC_COLOR 2

uint8_t forceMode = FORCE_MODE_OFF;
uint8_t forceColorSel = FORCE_COLOR_GREEN_HI;
unsigned long tiempoInicioForceColor = 0;
uint8_t faseAlForzar = 0;
const unsigned long TIEMPO_FORCE_COLOR_MS = 60UL * 60UL * 1000UL;

// Variables para Game mode (nuevo)
bool gameMode = false;
unsigned long inicioGameMode = 0;
unsigned long ultimoCambioGameMode = 0;
uint8_t estadoGameMode = 0; // 0=Green, 1=Yellow, 2=Red

// ==================== CONFIGURACIÓN DE MODO JUEGO ====================
const unsigned long TIEMPO_VERDE_JUEGO = 20000;   // 20 segundos en verde
const unsigned long TIEMPO_AMARILLO_JUEGO = 3000; // 3 segundos en amarillo
const unsigned long TIEMPO_ROJO_JUEGO = 10000;    // 10 segundos en rojo
// =====================================================================

// ==================== VARIABLES PARA MODOS ESPECIALES ====================
bool modoDormirMenuActivo = false; // Indica si estamos en el menú especial de sleep mode
bool modoDoceHorasMenuActivo = false; // Indica si estamos en el menú especial de 12-hour sleep
uint8_t opcionEspecialSeleccionada = 0; // 0=Modo actual, 1=Force wakeup
const uint8_t totalOpcionesEspecial = 2; // Modo actual y Force wakeup
bool forceWakeupActivo = false; // Indica si el force wakeup está activo
unsigned long tiempoFinForceWakeup = 0; // Cuándo termina el force wakeup (1 hora)
const unsigned long TIEMPO_FORCE_WAKEUP_MS = 60UL * 60UL * 1000UL; // 1 hora en milisegundos
// ==========================================================================

// Pines
const int ledVerde = 9;
const int ledAmarillo = 10;
const int ledRojo = 11;
const int moveButton = 2;
const int selectButton = 3;

// Estados - optimizado a uint8_t
bool botonPresionado = false;
bool enMenuSettings = false;
bool enMenuAbout = false;           // Nueva variable para controlar pantalla About
bool editandoHora = false;
bool editandoReloj = false;          // Para diferenciar si estamos editando el reloj
uint8_t opcionSeleccionada = 0;      // 0=Sleep mode, 1=12-hour sleep, 2=Force color, 3=Game mode, 4=Settings
uint8_t settingOpcion = 0;           // 0=Wake up, 1=Sleep at, 2=Nap, 3=Clock setup, 4=About, 5=Back
uint8_t editandoCampo = 0;           // 0=hora, 1=minutos, 2=día en el reloj
bool confirmandoEdicionHora = false;
uint8_t horaConfirmOpcion = 0;       // 0=Save, 1=Cancel
uint8_t horaTemporalDormir = 0;
uint8_t minutosTemporalDormir = 0;

// Menú Wake up: lista, submenu del slot y editor con Save/Cancel
#define WAKE_LISTA 0
#define WAKE_SLOT 1
#define WAKE_EDIT 2
#define WAKE_CONFIRM 3

bool enWakeUpMenu = false;
bool agregandoWake = false;
uint8_t wakePantalla = WAKE_LISTA;
uint8_t wakeMenuOpcion = 0;
uint8_t wakeScrollOffset = 0;
uint8_t wakeSlotMenuOpcion = 0;      // 0=Edit, 1=Delete, 2=Back
uint8_t wakeConfirmOpcion = 0;       // 0=Save, 1=Cancel
uint8_t wakeEditCampo = 0;           // 0=hora, 1=minutos, 2=rango de días
uint8_t wakeEditIndex = 0;
uint8_t wakeEditHora = DEFAULT_HORA_DESPERTAR;
uint8_t wakeEditMinutos = DEFAULT_MINUTOS_DESPERTAR;
uint8_t wakeEditRango = DAY_ALL;
const uint8_t totalOpciones = 5;     // Cambiado de 4 a 5 (agregamos Game mode)
const uint8_t totalSettingOpciones = 6;  // Cambiado de 4 a 6 (agregamos Nap y About)
uint8_t settingScrollOffset = 0;     // Para controlar qué opciones mostrar en pantalla
const uint8_t MAX_OPCIONES_POR_PANTALLA = 2; // Máximo de opciones visibles a la vez

// Force color: pantalla raíz, lista de modo y lista de color.
bool enForceColorMenu = false;
uint8_t forceColorPantalla = FC_ROOT;
uint8_t forceColorOpcion = 0;
const uint8_t totalForceColorRoot = 3;
const uint8_t totalForceColorModos = 5;
const uint8_t totalForceColorColores = 8;

// RTC
RTC_DS3231 rtc;
DateTime now;

// Intensidades (ahora con ambas versiones High y Low)
const uint8_t intensidadRojoMax = 100;
const uint8_t intensidadAmarilloMax = 255;
const uint8_t intensidadVerdeMax = 100;
const uint8_t intensidadRojoTenue = 3;
const uint8_t intensidadAmarilloTenue = 10;
const uint8_t intensidadVerdeTenue = 3;

// OLED
SSD1306AsciiWire oled;
#define OLED_ADDRESS 0x3C

// ======================================================
// FUNCIONES EEPROM
// ======================================================

void escribirSlotsWakeEnEEPROM() {
  EEPROM.update(EE_HORA_DESPERTAR, wakeHora[0]);
  EEPROM.update(EE_MINUTOS_DESPERTAR, wakeMinutos[0]);
  EEPROM.update(EE_WAKE0_RANGO, wakeRango[0]);
  EEPROM.update(EE_WAKE_COUNT, wakeOcupado);
  EEPROM.update(EE_WAKE1_HORA, wakeHora[1]);
  EEPROM.update(EE_WAKE1_MINUTOS, wakeMinutos[1]);
  EEPROM.update(EE_WAKE1_RANGO, wakeRango[1]);
  EEPROM.update(EE_WAKE2_HORA, wakeHora[2]);
  EEPROM.update(EE_WAKE2_MINUTOS, wakeMinutos[2]);
  EEPROM.update(EE_WAKE2_RANGO, wakeRango[2]);
}

void escribirDefaultsFabricaEnEEPROM() {
  EEPROM.update(EE_MAGIC_BYTE, EE_MAGIC_VALUE);
  EEPROM.update(EE_HORA_DESPERTAR, DEFAULT_HORA_DESPERTAR);
  EEPROM.update(EE_MINUTOS_DESPERTAR, DEFAULT_MINUTOS_DESPERTAR);
  EEPROM.update(EE_HORA_DORMIR, DEFAULT_HORA_DORMIR);
  EEPROM.update(EE_MINUTOS_DORMIR, DEFAULT_MINUTOS_DORMIR);
  EEPROM.update(EE_NAP_ACTIVO, 0);
  EEPROM.update(EE_NAP_HORA_INICIO, DEFAULT_NAP_HORA_INICIO);
  EEPROM.update(EE_NAP_MINUTO_INICIO, DEFAULT_NAP_MINUTO_INICIO);
  EEPROM.update(EE_NAP_DURACION_HORAS, DEFAULT_NAP_DURACION_HORAS);
  EEPROM.update(EE_NAP_DURACION_MINUTOS, DEFAULT_NAP_DURACION_MINUTOS);
  EEPROM.update(EE_WAKE0_RANGO, DAY_ALL);
  EEPROM.update(EE_WAKE_COUNT, 1);
  EEPROM.update(EE_WAKE1_HORA, DEFAULT_HORA_DESPERTAR);
  EEPROM.update(EE_WAKE1_MINUTOS, DEFAULT_MINUTOS_DESPERTAR);
  EEPROM.update(EE_WAKE1_RANGO, DAY_ALL);
  EEPROM.update(EE_WAKE2_HORA, DEFAULT_HORA_DESPERTAR);
  EEPROM.update(EE_WAKE2_MINUTOS, DEFAULT_MINUTOS_DESPERTAR);
  EEPROM.update(EE_WAKE2_RANGO, DAY_ALL);
}

void inicializarEEPROM() {
  uint8_t magic = EEPROM.read(EE_MAGIC_BYTE);

  if (magic == EE_MAGIC_VALUE) {
    return;
  }

  if (magic == EE_MAGIC_VALUE_V144) {
    // El byte era una cantidad compacta (1-3). Pasa a bits de slot.
    uint8_t count = EEPROM.read(EE_WAKE_COUNT);
    if (count < 1 || count > MAX_WAKE_SLOTS) count = 1;
    Serial.println(F("Migrando EEPROM 1.44 -> 1.54"));
    EEPROM.update(EE_WAKE_COUNT, (uint8_t)((1 << count) - 1));
    EEPROM.update(EE_MAGIC_BYTE, EE_MAGIC_VALUE);
    Serial.println(F("EEPROM migrada; cada horario queda en su slot"));
    return;
  }

  if (magic == EE_MAGIC_VALUE_V143) {
    // Conserva la hora ya guardada en los bytes 1 y 2 y la marca como All days.
    Serial.println(F("Migrando EEPROM 1.43 -> 1.54"));
    EEPROM.update(EE_WAKE0_RANGO, DAY_ALL);
    EEPROM.update(EE_WAKE_COUNT, 1);
    EEPROM.update(EE_WAKE1_HORA, DEFAULT_HORA_DESPERTAR);
    EEPROM.update(EE_WAKE1_MINUTOS, DEFAULT_MINUTOS_DESPERTAR);
    EEPROM.update(EE_WAKE1_RANGO, DAY_ALL);
    EEPROM.update(EE_WAKE2_HORA, DEFAULT_HORA_DESPERTAR);
    EEPROM.update(EE_WAKE2_MINUTOS, DEFAULT_MINUTOS_DESPERTAR);
    EEPROM.update(EE_WAKE2_RANGO, DAY_ALL);
    EEPROM.update(EE_MAGIC_BYTE, EE_MAGIC_VALUE);
    Serial.println(F("EEPROM migrada; el horario anterior queda como All days"));
    return;
  }

  Serial.println(F("EEPROM no inicializada - escribiendo valores por defecto"));
  escribirDefaultsFabricaEnEEPROM();
  Serial.println(F("Valores por defecto escritos en EEPROM"));
}

void cargarConfiguracionDesdeEEPROM() {
  Serial.println(F("Cargando configuración desde EEPROM..."));
  
  // Cargar horarios de despertar
  wakeHora[0] = EEPROM.read(EE_HORA_DESPERTAR);
  wakeMinutos[0] = EEPROM.read(EE_MINUTOS_DESPERTAR);
  wakeRango[0] = EEPROM.read(EE_WAKE0_RANGO);
  wakeOcupado = EEPROM.read(EE_WAKE_COUNT) & (uint8_t)((1 << MAX_WAKE_SLOTS) - 1);
  wakeHora[1] = EEPROM.read(EE_WAKE1_HORA);
  wakeMinutos[1] = EEPROM.read(EE_WAKE1_MINUTOS);
  wakeRango[1] = EEPROM.read(EE_WAKE1_RANGO);
  wakeHora[2] = EEPROM.read(EE_WAKE2_HORA);
  wakeMinutos[2] = EEPROM.read(EE_WAKE2_MINUTOS);
  wakeRango[2] = EEPROM.read(EE_WAKE2_RANGO);
  for (uint8_t i = 0; i < MAX_WAKE_SLOTS; i++) {
    if (wakeHora[i] > 23) wakeHora[i] = DEFAULT_HORA_DESPERTAR;
    if (wakeMinutos[i] > 59) wakeMinutos[i] = DEFAULT_MINUTOS_DESPERTAR;
    if (wakeRango[i] >= DAY_RANGE_COUNT) wakeRango[i] = DAY_ALL;
  }

  horaDormir = EEPROM.read(EE_HORA_DORMIR);
  minutosDormir = EEPROM.read(EE_MINUTOS_DORMIR);
  
  // Cargar configuración Nap
  napActivo = EEPROM.read(EE_NAP_ACTIVO) == 1;
  napHoraInicio = EEPROM.read(EE_NAP_HORA_INICIO);
  napMinutoInicio = EEPROM.read(EE_NAP_MINUTO_INICIO);
  napDuracionHoras = EEPROM.read(EE_NAP_DURACION_HORAS);
  napDuracionMinutos = EEPROM.read(EE_NAP_DURACION_MINUTOS);
  
  // Limitar napDuracionHoras a 0-3 como configuramos
  napDuracionHoras = napDuracionHoras % 4;
  
  Serial.println(F("Configuración cargada desde EEPROM:"));
  imprimirSlotsWakeSerial();
  Serial.print(F("  Sleep at: ")); Serial.print(horaDormir); Serial.print(F(":")); 
  if (minutosDormir < 10) Serial.print(F("0")); Serial.println(minutosDormir);
  Serial.print(F("  Nap: ")); Serial.println(napActivo ? F("On") : F("Off"));
  if (napActivo) {
    Serial.print(F("    Start: ")); Serial.print(napHoraInicio); Serial.print(F(":")); 
    if (napMinutoInicio < 10) Serial.print(F("0")); Serial.println(napMinutoInicio);
    Serial.print(F("    Duration: ")); Serial.print(napDuracionHoras); Serial.print(F("h "));
    if (napDuracionMinutos < 10) Serial.print(F("0")); Serial.print(napDuracionMinutos); Serial.println(F("m"));
  }
}

void guardarHorariosEnEEPROM() {
  Serial.println(F("Guardando horarios en EEPROM..."));
  escribirSlotsWakeEnEEPROM();
  EEPROM.update(EE_HORA_DORMIR, horaDormir);
  EEPROM.update(EE_MINUTOS_DORMIR, minutosDormir);
  Serial.println(F("Horarios guardados en EEPROM"));
}

void guardarWakeEnEEPROM() {
  Serial.println(F("Guardando horarios de despertar en EEPROM..."));
  escribirSlotsWakeEnEEPROM();
  imprimirSlotsWakeSerial();
  Serial.println(F("Horarios de despertar guardados en EEPROM"));
}

void guardarNapConfigEnEEPROM() {
  Serial.println(F("Guardando configuración Nap en EEPROM..."));
  EEPROM.update(EE_NAP_ACTIVO, napActivo ? 1 : 0);
  EEPROM.update(EE_NAP_HORA_INICIO, napHoraInicio);
  EEPROM.update(EE_NAP_MINUTO_INICIO, napMinutoInicio);
  EEPROM.update(EE_NAP_DURACION_HORAS, napDuracionHoras);
  EEPROM.update(EE_NAP_DURACION_MINUTOS, napDuracionMinutos);
  Serial.println(F("Configuración Nap guardada en EEPROM"));
}

void copiarEntradaProgmem(const char* tabla, uint8_t index, char* buffer, uint8_t bufferSize) {
  const char *ptr = tabla;
  for (uint8_t i = 0; i < index; i++) {
    while (pgm_read_byte(ptr)) ptr++;
    ptr++;
  }
  uint8_t j = 0;
  char c;
  while ((c = pgm_read_byte(ptr++)) && j < bufferSize - 1) {
    buffer[j++] = c;
  }
  buffer[j] = '\0';
}

const char* getDayRangeLabel(uint8_t index) {
  static char buffer[20];
  if (index >= DAY_RANGE_COUNT) index = DAY_ALL;
  copiarEntradaProgmem(dayRangeLabels, index, buffer, sizeof(buffer));
  return buffer;
}

const char* getDayRangeShort(uint8_t index) {
  static char buffer[4];
  if (index >= DAY_RANGE_COUNT) index = DAY_ALL;
  copiarEntradaProgmem(dayRangeShort, index, buffer, sizeof(buffer));
  return buffer;
}

const char* getWeekdayName(uint8_t dow) {
  static char buffer[4];
  if (dow > 6) dow = 0;
  copiarEntradaProgmem(weekdayNames, dow, buffer, sizeof(buffer));
  return buffer;
}

const char* getWeekdayFullName(uint8_t dow) {
  static char buffer[10];
  if (dow > 6) dow = 0;
  copiarEntradaProgmem(weekdayFullNames, dow, buffer, sizeof(buffer));
  return buffer;
}

uint8_t especificidadRango(uint8_t rango) {
  if (rango >= DAY_SUN) return 2;
  if (rango == DAY_WEEK || rango == DAY_WEEKEND) return 1;
  return 0;
}

bool rangoCoincide(uint8_t rango, uint8_t dow) {
  if (rango == DAY_ALL) return true;
  if (rango == DAY_WEEK) return dow >= 1 && dow <= 5;
  if (rango == DAY_WEEKEND) return dow == 0 || dow == 6;
  return dow == (uint8_t)(rango - DAY_SUN);
}

bool slotWakeOcupado(uint8_t slot) {
  return slot < MAX_WAKE_SLOTS && (wakeOcupado & (1 << slot));
}

uint8_t contarSlotsWake() {
  uint8_t n = 0;
  for (uint8_t i = 0; i < MAX_WAKE_SLOTS; i++) {
    if (slotWakeOcupado(i)) n++;
  }
  return n;
}

uint8_t primerSlotWakeLibre() {
  for (uint8_t i = 0; i < MAX_WAKE_SLOTS; i++) {
    if (!slotWakeOcupado(i)) return i;
  }
  return 0xFF;
}

uint8_t slotDeOpcionWake(uint8_t opcion) {
  uint8_t vistos = 0;
  for (uint8_t i = 0; i < MAX_WAKE_SLOTS; i++) {
    if (!slotWakeOcupado(i)) continue;
    if (vistos == opcion) return i;
    vistos++;
  }
  return 0xFF;
}

void seleccionarSlotEnLista(uint8_t slot) {
  uint8_t idx = 0;
  for (uint8_t i = 0; i < slot && i < MAX_WAKE_SLOTS; i++) {
    if (slotWakeOcupado(i)) idx++;
  }
  wakeMenuOpcion = idx;
  wakeScrollOffset = (wakeMenuOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
}

void seleccionarFilaTrasBorrar(uint8_t slotBorrado) {
  uint8_t idx = 0;
  bool encontro = false;
  for (uint8_t i = 0; i < MAX_WAKE_SLOTS; i++) {
    if (!slotWakeOcupado(i)) continue;
    if (i > slotBorrado) {
      encontro = true;
      break;
    }
    idx++;
  }
  wakeMenuOpcion = encontro ? idx : contarSlotsWake();
  wakeScrollOffset = (wakeMenuOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
}

void imprimirSlotsWakeSerial() {
  Serial.print(F("  Wake slots: "));
  Serial.println(contarSlotsWake());
  for (uint8_t i = 0; i < MAX_WAKE_SLOTS; i++) {
    if (!slotWakeOcupado(i)) continue;
    Serial.print(F("    Time "));
    Serial.print(i + 1);
    Serial.print(F(": "));
    Serial.print(wakeHora[i]);
    Serial.print(F(":"));
    if (wakeMinutos[i] < 10) Serial.print(F("0"));
    Serial.print(wakeMinutos[i]);
    Serial.print(F(" ["));
    Serial.print(getDayRangeShort(wakeRango[i]));
    Serial.println(F("]"));
  }
}

// Devuelve false si hoy no hay ningún horario. En empate gana el de más arriba.
bool horarioDespertarDeHoy(uint8_t &hora, uint8_t &minuto) {
  int bestSpec = -1;
  int bestIndex = -1;
  uint8_t dow = now.dayOfTheWeek();
  for (uint8_t i = 0; i < MAX_WAKE_SLOTS; i++) {
    if (!slotWakeOcupado(i)) continue;
    if (!rangoCoincide(wakeRango[i], dow)) continue;
    uint8_t spec = especificidadRango(wakeRango[i]);
    if ((int)spec > bestSpec) {
      bestSpec = spec;
      bestIndex = i;
    }
  }
  if (bestIndex < 0) return false;
  hora = wakeHora[bestIndex];
  minuto = wakeMinutos[bestIndex];
  return true;
}

uint8_t totalOpcionesWakeMenu() {
  uint8_t ocupados = contarSlotsWake();
  return ocupados + (ocupados < MAX_WAKE_SLOTS ? 1 : 0) + 1;
}

void imprimirHoraConRango(uint8_t hora, uint8_t minutos, uint8_t rango) {
  oled.print(hora);
  oled.print(":");
  if (minutos < 10) oled.print("0");
  oled.print(minutos);
  oled.print("[");
  oled.print(getDayRangeShort(rango));
  oled.print("]");
}

const char* etiquetaModoForce() {
  switch (forceMode) {
    case FORCE_MODE_PERSISTENT: return "On Per";
    case FORCE_MODE_UNTIL: return "On Unt";
    case FORCE_MODE_1HOUR: return "On 1hr";
    default: return "Off";
  }
}

const char* etiquetaColorForce() {
  switch (forceColorSel) {
    case FORCE_COLOR_GREEN_HI: return "Green hi";
    case FORCE_COLOR_GREEN_LO: return "Green lo";
    case FORCE_COLOR_YELLOW_HI: return "Yellow hi";
    case FORCE_COLOR_YELLOW_LO: return "Yellow lo";
    case FORCE_COLOR_RED_HI: return "Red hi";
    case FORCE_COLOR_RED_LO: return "Red lo";
    default: return "All off";
  }
}

const char* etiquetaCortaForce() {
  if (forceMode == FORCE_MODE_OFF) return "Off";
  switch (forceColorSel) {
    case FORCE_COLOR_GREEN_HI: return "GH";
    case FORCE_COLOR_GREEN_LO: return "GL";
    case FORCE_COLOR_YELLOW_HI: return "YH";
    case FORCE_COLOR_YELLOW_LO: return "YL";
    case FORCE_COLOR_RED_HI: return "RH";
    case FORCE_COLOR_RED_LO: return "RL";
    default: return "AO";
  }
}

uint8_t totalOpcionesForceColor() {
  if (forceColorPantalla == FC_MODE) return totalForceColorModos;
  if (forceColorPantalla == FC_COLOR) return totalForceColorColores;
  return totalForceColorRoot;
}

// Fase del horario automático. Cambia en cada evento del ciclo.
uint8_t faseHorario() {
  int tiempoActual = now.hour() * 60 + now.minute();
  int tiempoDormir = horaDormir * 60 + minutosDormir;

  if (estaEnAvisoPrevioSiesta()) return 5;
  if (estaDentroPlaceholderSiesta()) return 6;
  if (tiempoActual >= tiempoDormir) return 8;

  uint8_t horaWake;
  uint8_t minWake;
  if (!horarioDespertarDeHoy(horaWake, minWake)) {
    if (tiempoActual < tiempoDormir - 30) return 9;
    return 7;
  }

  int tiempoDespertar = horaWake * 60 + minWake;
  if (tiempoActual < tiempoDespertar - 15) return 1;
  if (tiempoActual < tiempoDespertar) return 2;
  if (tiempoActual < tiempoDespertar + 60) return 3;
  if (tiempoActual < tiempoDormir - 30) return 4;
  return 7;
}

void aplicarColorForzado() {
  switch (forceColorSel) {
    case FORCE_COLOR_GREEN_HI:
      setLuz(ledVerde, intensidadVerdeMax);
      break;
    case FORCE_COLOR_GREEN_LO:
      setLuz(ledVerde, intensidadVerdeTenue);
      break;
    case FORCE_COLOR_YELLOW_HI:
      setLuz(ledAmarillo, intensidadAmarilloMax);
      break;
    case FORCE_COLOR_YELLOW_LO:
      setLuz(ledAmarillo, intensidadAmarilloTenue);
      break;
    case FORCE_COLOR_RED_HI:
      setLuz(ledRojo, intensidadRojoMax);
      break;
    case FORCE_COLOR_RED_LO:
      setLuz(ledRojo, intensidadRojoTenue);
      break;
    default:
      analogWrite(ledVerde, 0);
      analogWrite(ledAmarillo, 0);
      analogWrite(ledRojo, 0);
      break;
  }
}

void terminarForceColor() {
  forceMode = FORCE_MODE_OFF;
  if (pantallaApagada) return;
  if (enForceColorMenu) {
    mostrarForceColorMenu();
  } else if (!enMenuSettings && !editandoHora && !enMenuAbout && !enNapMenu && !enWakeUpMenu && !modoDormirMenuActivo && !modoDoceHorasMenuActivo) {
    mostrarPantallaPrincipal();
  }
}

void setup() {
  Serial.begin(9600);

  // Inicializar y cargar configuración desde EEPROM
  inicializarEEPROM();
  cargarConfiguracionDesdeEEPROM();

  if (!rtc.begin()) {
    Serial.println(F("Error: No se encuentra el módulo RTC."));
    while (1);
  }

  // Verificar si el RTC perdió la hora (fecha muy antigua)
  now = rtc.now();
  if (now.year() < 2024) {
    Serial.println(F("ADVERTENCIA: RTC parece tener hora incorrecta. Usa Clock setup para ajustar."));
  }

  pinMode(ledVerde, OUTPUT);
  pinMode(ledAmarillo, OUTPUT);
  pinMode(ledRojo, OUTPUT);
  pinMode(selectButton, INPUT_PULLUP);
  pinMode(moveButton, INPUT_PULLUP);

  Wire.begin();
  oled.begin(&Adafruit128x64, OLED_ADDRESS);
  oled.setFont(Arial14);
  mostrarPantallaPrincipal();
  pantallaApagada = false;
  
  Serial.println(F("=== SISTEMA INICIADO - VERSION 1.60 (HORARIOS DE DESPERTAR POR DIA) ==="));
  Serial.print(F("12-hour sleep configurado a: "));
  Serial.print(DOCE_HORAS_QUINCE_MIN_MS / 1000 / 60 / 60.0, 2);
  Serial.println(F(" horas totales (12h + 15min transición)"));
  Serial.println(F("Configuración Nap Mode:"));
  Serial.print(F("  Umbral siesta/noche: ")); Serial.print(UMBRAL_HORA_SIESTA); Serial.println(F(":00 (5:00 PM)"));
  Serial.println(F("  Hora extra verde tenue: 60 minutos"));
  Serial.println(F("Configuración persistente en EEPROM habilitada"));
  Serial.println(F("Force Color: modo (Off, Persistent, 1 hour, Until event) y color"));
  Serial.println(F("Force Color incluye All off"));
  Serial.println(F("NUEVO: Pantalla especial para 12-hour sleep + exclusión mutua"));
}

void loop() {
  now = rtc.now();
  int hora = now.hour();
  int minuto = now.minute();

  // Verificar si el force wakeup ha terminado
  if (forceWakeupActivo && millis() >= tiempoFinForceWakeup) {
    forceWakeupActivo = false;
    Serial.println(F("Force wakeup FINALIZADO - Volviendo a horario normal"));
  }

  // El modo de 1 hora vuelve solo. Until event vuelve al cambiar la fase.
  if (forceMode == FORCE_MODE_1HOUR && millis() >= tiempoInicioForceColor + TIEMPO_FORCE_COLOR_MS) {
    Serial.println(F("Force color FINALIZADO - 1 hora"));
    terminarForceColor();
  } else if (forceMode == FORCE_MODE_UNTIL && faseHorario() != faseAlForzar) {
    Serial.println(F("Force color FINALIZADO - Until event"));
    terminarForceColor();
  }

  if (minuto != ultimoMinutoImpreso) {
    Serial.print(F("Hora actual: "));
    Serial.print(hora);
    Serial.print(F(":"));
    if (minuto < 10) Serial.print(F("0"));
    Serial.println(minuto);

    ultimoMinutoImpreso = minuto;

    // Actualizar pantalla solo si no estamos en ningún menú y la pantalla está encendida
    if (!botonPresionado && !enMenuSettings && !editandoHora && !enForceColorMenu && !pantallaApagada && !enMenuAbout && !enNapMenu && !enWakeUpMenu && !modoDormirMenuActivo && !modoDoceHorasMenuActivo) {
      mostrarPantallaPrincipal();
    } else if (modoDormirMenuActivo && !pantallaApagada) {
      mostrarPantallaDormir();
    } else if (modoDoceHorasMenuActivo && !pantallaApagada) {
      mostrarPantallaDoceHoras();
    }
  }

  manejarBotones();
  manejarLuces();

  // Control de timeout de pantalla
  if (!pantallaApagada && (millis() - ultimaInteraccion > TIEMPO_PANTALLA_ACTIVA)) {
    oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
    pantallaApagada = true;
  }

  delay(100);
}

// ======================================================
// FUNCIONES PARA NAP MODE COMPLETO
// ======================================================

bool esAntesDeUmbralSiesta() {
  // Retorna true si la hora actual es antes de las 5:00 PM (17:00)
  return (now.hour() < UMBRAL_HORA_SIESTA);
}

bool estaDentroPlaceholderSiesta() {
  // Verifica si estamos dentro del placeholder de siesta (para avisos)
  if (!napActivo) return false;
  
  int tiempoActual = now.hour() * 60 + now.minute();
  int tiempoNapInicio = napHoraInicio * 60 + napMinutoInicio;
  int tiempoNapFin = tiempoNapInicio + (napDuracionHoras * 60) + napDuracionMinutos;
  
  return (tiempoActual >= tiempoNapInicio && tiempoActual < tiempoNapFin);
}

bool estaEnAvisoPrevioSiesta() {
  // Verifica si estamos en los 30 minutos previos a la siesta
  if (!napActivo) return false;
  
  int tiempoActual = now.hour() * 60 + now.minute();
  int tiempoNapInicio = napHoraInicio * 60 + napMinutoInicio;
  
  return (tiempoActual >= (tiempoNapInicio - 30) && tiempoActual < tiempoNapInicio);
}

void iniciarModoSiesta() {
  // Llamado cuando se activa Sleep Mode antes del umbral (5:00 PM)
  modoSiestaActivo = true;
  enVerdeExtraSiesta = false;
  inicioSiestaReal = millis();
  
  // Calcular tiempo de fin de siesta (duración configurada)
  unsigned long duracionSiestaMs = (napDuracionHoras * 60UL * 60UL * 1000UL) + 
                                   (napDuracionMinutos * 60UL * 1000UL);
  tiempoFinSiesta = inicioSiestaReal + duracionSiestaMs;
  
  Serial.print(F("MODO SIESTA INICIADO - "));
  Serial.print(F("Duración: "));
  Serial.print(napDuracionHoras);
  Serial.print(F("h "));
  Serial.print(napDuracionMinutos);
  Serial.print(F("m - "));
  Serial.print(F("Terminará rojo tenue a las "));
  Serial.print(tiempoFinSiesta);
  Serial.println(F(" ms"));
}

void finalizarModoSiesta() {
  modoSiestaActivo = false;
  Serial.println(F("MODO SIESTA FINALIZADO - Pasando a verde tenue por 1 hora"));
}

// ======================================================
// CORRECCIÓN: FUNCIÓN ACTUALIZADA PARA SALIR COMPLETAMENTE DE MODO SIESTA
// ======================================================

void manejarModoSiesta() {
  unsigned long tiempoActual = millis();
  
  if (enVerdeExtraSiesta) {
    // Estamos en la hora extra de verde tenue
    if (tiempoActual >= tiempoFinVerdeExtra) {
      // CORRECCIÓN AQUÍ: Terminó completamente el modo siesta, SALIR COMPLETAMENTE
      modoSiestaActivo = false;
      enVerdeExtraSiesta = false;
      
      // AQUÍ ESTÁ LA CLAVE: Desactivar Sleep Mode para volver a horario normal
      botonPresionado = false;
      
      Serial.println(F("HORA EXTRA VERDE FINALIZADA - SALIENDO COMPLETAMENTE DE MODO SIESTA"));
      Serial.println(F("Sleep mode DESACTIVADO automáticamente - Volviendo a horario normal"));
      
      // Actualizar pantalla para reflejar el cambio
      mostrarPantallaPrincipal();
    } else {
      // Mostrar verde tenue durante la hora extra
      setLuz(ledVerde, intensidadVerdeTenue);
    }
  } else if (modoSiestaActivo) {
    // Estamos en el período de rojo tenue de la siesta
    if (tiempoActual >= tiempoFinSiesta) {
      // Terminó el tiempo de siesta, iniciar hora extra verde
      enVerdeExtraSiesta = true;
      tiempoFinVerdeExtra = tiempoActual + (60UL * 60UL * 1000UL); // 1 hora
      Serial.print(F("SIESTA COMPLETADA - Iniciando hora extra verde hasta "));
      Serial.println(tiempoFinVerdeExtra);
    } else {
      // Mostrar rojo tenue durante la siesta
      setLuz(ledRojo, intensidadRojoTenue);
    }
  }
}

bool modoTenueActivo() {
  int tiempoActual = now.hour() * 60 + now.minute();
  int tiempoDormir = horaDormir * 60 + minutosDormir;
  uint8_t horaWake;
  uint8_t minWake;
  if (!horarioDespertarDeHoy(horaWake, minWake)) {
    return tiempoActual < tiempoDormir;
  }
  int tiempoDespertar = horaWake * 60 + minWake;
  
  return (tiempoActual < tiempoDespertar + 60) ||
         (tiempoActual >= tiempoDormir);
}

void manejarBotones() {
  static int lastSelect = HIGH;
  static int lastMove = HIGH;
  int currentSelect = digitalRead(selectButton);
  int currentMove = digitalRead(moveButton);
  
  // Verificar si estamos en período de bloqueo después de encender la pantalla
  static bool bloqueoActivo = false;
  if (bloqueoActivo && (millis() - tiempoEncendidoPantalla > TIEMPO_BLOQUEO_BOTONES)) {
    bloqueoActivo = false;
  }

  // Si la pantalla está apagada, cualquier botón solo la enciende
  if (pantallaApagada) {
    // Detectar flanco de bajada en cualquier botón
    bool botonPresionadoAhora = false;
    
    if (lastSelect == HIGH && currentSelect == LOW) {
      botonPresionadoAhora = true;
    }
    if (lastMove == HIGH && currentMove == LOW) {
      botonPresionadoAhora = true;
    }
    
    if (botonPresionadoAhora) {
      encenderPantalla();
      // Activar bloqueo temporal
      bloqueoActivo = true;
      tiempoEncendidoPantalla = millis();
      ultimaInteraccion = millis();
      // Actualizar estados para evitar procesamiento en siguiente ciclo
      lastSelect = currentSelect;
      lastMove = currentMove;
      return; // Salir sin procesar otras acciones
    }
  }
  
  // Si hay bloqueo activo, no procesar botones
  if (bloqueoActivo) {
    lastSelect = currentSelect;
    lastMove = currentMove;
    return;
  }
  
  // Procesar botones normalmente si la pantalla está encendida y no hay bloqueo
  if (lastSelect == HIGH && currentSelect == LOW) {
    procesarBotonSelect();
  }
  
  if (lastMove == HIGH && currentMove == LOW) {
    procesarBotonMove();
  }
  
  lastSelect = currentSelect;
  lastMove = currentMove;
}

void encenderPantalla() {
  oled.ssd1306WriteCmd(SSD1306_DISPLAYON);
  pantallaApagada = false;
  ultimaInteraccion = millis();
  
  if (!editandoHora && !enMenuSettings && !enForceColorMenu && !enMenuAbout && !enNapMenu && !enWakeUpMenu) {
    if (modoDormirMenuActivo) {
      mostrarPantallaDormir();
    } else if (modoDoceHorasMenuActivo) {
      mostrarPantallaDoceHoras();
    } else {
      mostrarPantallaPrincipal();
    }
  } else if (enMenuAbout) {
    mostrarMenuAbout();
  } else if (enNapMenu) {
    mostrarNapMenu();
  } else if (enWakeUpMenu) {
    if (wakePantalla == WAKE_EDIT || wakePantalla == WAKE_CONFIRM) {
      mostrarEdicionWake();
    } else if (wakePantalla == WAKE_SLOT) {
      mostrarWakeSlotMenu();
    } else {
      mostrarWakeUpMenu();
    }
  } else if (enForceColorMenu) {
    mostrarForceColorMenu();
  } else if (editandoHora) {
    mostrarEdicionHora();
  }
}

void cambiarValorEdicionHora() {
  if (editandoCampo == 0) {
    if (editandoReloj) {
      horaTemporalReloj = (horaTemporalReloj + 1) % 24;
    } else {
      horaTemporalDormir = (horaTemporalDormir + 1) % 24;
    }
  } else if (editandoReloj && editandoCampo == 2) {
    diaTemporalReloj = (diaTemporalReloj + 1) % 7;
  } else if (editandoReloj) {
    minutosTemporalReloj = (minutosTemporalReloj + 1) % 60;
  } else {
    minutosTemporalDormir = (minutosTemporalDormir + 5) % 60;
  }
}

void avanzarCampoEdicionHora() {
  uint8_t ultimoCampo = editandoReloj ? 2 : 1;
  if (editandoCampo < ultimoCampo) {
    editandoCampo++;
  } else {
    confirmandoEdicionHora = true;
    horaConfirmOpcion = 0;
  }
}

void salirEdicionHora() {
  editandoHora = false;
  editandoReloj = false;
  editandoCampo = 0;
  confirmandoEdicionHora = false;
  enMenuSettings = true;
  mostrarMenuSettings();
}

void guardarEdicionHora() {
  if (editandoReloj) {
    // La hora y el día se escriben juntos en el RTC.
    // dayOfTheWeek() sale de la fecha, así que se corre 0-6 días.
    uint8_t diaActual = now.dayOfTheWeek();
    uint8_t delta = (uint8_t)((diaTemporalReloj - diaActual + 7) % 7);
    DateTime nuevaHora = DateTime(now.year(), now.month(), now.day(),
                                  horaTemporalReloj, minutosTemporalReloj, 0);
    nuevaHora = nuevaHora + TimeSpan(delta, 0, 0, 0);
    rtc.adjust(nuevaHora);
    Serial.print(F("RTC ajustado a: "));
    Serial.print(nuevaHora.year());
    Serial.print(F("-"));
    Serial.print(nuevaHora.month());
    Serial.print(F("-"));
    Serial.print(nuevaHora.day());
    Serial.print(F(" "));
    Serial.print(getWeekdayFullName(nuevaHora.dayOfTheWeek()));
    Serial.print(F(" "));
    Serial.print(horaTemporalReloj);
    Serial.print(F(":"));
    if (minutosTemporalReloj < 10) Serial.print(F("0"));
    Serial.println(minutosTemporalReloj);
  } else {
    horaDormir = horaTemporalDormir;
    minutosDormir = minutosTemporalDormir;
    guardarHorariosEnEEPROM();
  }
  salirEdicionHora();
}

void procesarBotonSelect() {
  if (enNapMenu) {
    procesarBotonSelectNapMenu();
  }
  else if (enMenuAbout) {
    // En pantalla About, solo el botón Select sirve para volver atrás
    enMenuAbout = false;
    enMenuSettings = true;
    mostrarMenuSettings();
  }
  else if (enForceColorMenu) {
    procesarBotonSelectForceColor();
  }
  else if (enWakeUpMenu) {
    procesarBotonSelectWakeMenu();
  }
  else if (editandoHora) {
    if (confirmandoEdicionHora) {
      if (horaConfirmOpcion == 0) {
        guardarEdicionHora();
      } else {
        salirEdicionHora();
      }
      return;
    }
    cambiarValorEdicionHora();
    mostrarEdicionHora();
  }
  else if (enMenuSettings) {
    if (settingOpcion == 0) { // Wake up
      enWakeUpMenu = true;
      agregandoWake = false;
      wakePantalla = WAKE_LISTA;
      wakeMenuOpcion = 0;
      wakeScrollOffset = 0;
      mostrarWakeUpMenu();
    }
    else if (settingOpcion == 1) { // Sleep at
      editandoHora = true;
      editandoReloj = false;
      editandoCampo = 0;
      confirmandoEdicionHora = false;
      horaTemporalDormir = horaDormir;
      minutosTemporalDormir = minutosDormir;
      mostrarEdicionHora();
    }
    else if (settingOpcion == 2) { // Nap
      enNapMenu = true;
      napMenuOpcion = 0;
      napEditando = false;
      napEditandoCampo = 0;
      napConfirmando = false;
      napEditandoStart = true;
      mostrarNapMenu();
    }
    else if (settingOpcion == 3) { // Clock setup
      editandoHora = true;
      editandoReloj = true;
      editandoCampo = 0;
      confirmandoEdicionHora = false;
      // Usamos las variables temporales para no tocar los horarios guardados
      horaTemporalReloj = now.hour();
      minutosTemporalReloj = now.minute();
      diaTemporalReloj = now.dayOfTheWeek();
      mostrarEdicionHora();
    }
    else if (settingOpcion == 4) { // About
      enMenuAbout = true;
      mostrarMenuAbout();
    }
    else if (settingOpcion == 5) { // Back
      enMenuSettings = false;
      opcionSeleccionada = 0;
      mostrarPantallaPrincipal();
    }
  } else if (modoDormirMenuActivo) {
    // Menú especial cuando Sleep mode está activo
    if (opcionEspecialSeleccionada == 0) {
      // Sleep mode [On] -> Desactivar sleep mode
      botonPresionado = false;
      modoSiestaActivo = false;
      enVerdeExtraSiesta = false;
      modoDormirMenuActivo = false;
      Serial.println(F("Sleep mode DESACTIVADO desde menú especial"));
      mostrarPantallaPrincipal();
    } else if (opcionEspecialSeleccionada == 1) {
      // Force wakeup
      botonPresionado = false; // Desactivar sleep mode
      modoSiestaActivo = false;
      enVerdeExtraSiesta = false;
      forceWakeupActivo = true; // Activar force wakeup
      tiempoFinForceWakeup = millis() + TIEMPO_FORCE_WAKEUP_MS; // 1 hora
      modoDormirMenuActivo = false; // Salir del menú especial
      Serial.println(F("Force wakeup ACTIVADO - Verde por 1 hora"));
      mostrarPantallaPrincipal();
    }
  } else if (modoDoceHorasMenuActivo) {
    // Menú especial cuando 12-hour sleep está activo
    if (opcionEspecialSeleccionada == 0) {
      // 12-hour sleep [On] -> Desactivar 12-hour sleep
      doceHorasSleep = false;
      modoDoceHorasMenuActivo = false;
      Serial.println(F("12-hour sleep DESACTIVADO desde menú especial"));
      mostrarPantallaPrincipal();
    } else if (opcionEspecialSeleccionada == 1) {
      // Force wakeup
      doceHorasSleep = false; // Desactivar 12-hour sleep
      forceWakeupActivo = true; // Activar force wakeup
      tiempoFinForceWakeup = millis() + TIEMPO_FORCE_WAKEUP_MS; // 1 hora
      modoDoceHorasMenuActivo = false; // Salir del menú especial
      Serial.println(F("Force wakeup ACTIVADO desde 12-hour sleep - Verde por 1 hora"));
      mostrarPantallaPrincipal();
    }
  } else {
    // Menú principal
    if (opcionSeleccionada == 0) { // Sleep mode
      bool estadoAnterior = botonPresionado;
      
      // Si vamos a activar Sleep mode, desactivar 12-hour sleep
      if (!botonPresionado) {
        if (doceHorasSleep) {
          doceHorasSleep = false;
          modoDoceHorasMenuActivo = false;
          Serial.println(F("12-hour sleep DESACTIVADO automáticamente (Sleep mode activado)"));
        }
      }
      
      botonPresionado = !botonPresionado;
      
      if (botonPresionado) {
        // Sleep Mode ACTIVADO
        setLuz(ledRojo, intensidadRojoTenue);
        
        // Verificar si es modo siesta o modo noche
        if (napActivo && esAntesDeUmbralSiesta()) {
          // Es antes de las 5:00 PM → Modo Siesta
          iniciarModoSiesta();
          Serial.println(F("Sleep mode ACTIVADO - MODO SIESTA"));
        } else {
          // Es después de las 5:00 PM → Modo Noche normal
          modoSiestaActivo = false;
          enVerdeExtraSiesta = false;
          Serial.println(F("Sleep mode ACTIVADO - MODO NOCHE"));
        }
        
        // Activar el menú especial de dormir
        modoDormirMenuActivo = true;
        opcionEspecialSeleccionada = 0;
        mostrarPantallaDormir();
      } else {
        // Sleep Mode DESACTIVADO
        if (modoSiestaActivo || enVerdeExtraSiesta) {
          modoSiestaActivo = false;
          enVerdeExtraSiesta = false;
          Serial.println(F("Sleep mode DESACTIVADO - Modo siesta cancelado"));
        } else {
          Serial.println(F("Sleep mode DESACTIVADO"));
        }
        modoDormirMenuActivo = false;
        ultimoMinutoImpreso = -1;
        mostrarPantallaPrincipal();
      }
    }
    else if (opcionSeleccionada == 1) { // 12-hour sleep
      bool nuevoEstado = !doceHorasSleep;
      
      // Si vamos a activar 12-hour sleep, desactivar Sleep mode
      if (nuevoEstado && botonPresionado) {
        botonPresionado = false;
        modoSiestaActivo = false;
        enVerdeExtraSiesta = false;
        modoDormirMenuActivo = false;
        Serial.println(F("Sleep mode DESACTIVADO automáticamente (12-hour sleep activado)"));
      }
      
      doceHorasSleep = nuevoEstado;
      
      if (doceHorasSleep) {
        inicioDoceHoras = millis();
        Serial.print(F("12-hour sleep ACTIVADO - Inicio registrado a las "));
        Serial.print(millis());
        Serial.println(F(" ms"));
        
        // Activar el menú especial de 12-hour sleep
        modoDoceHorasMenuActivo = true;
        opcionEspecialSeleccionada = 0;
        mostrarPantallaDoceHoras();
      } else {
        Serial.println(F("12-hour sleep DESACTIVADO manualmente"));
        modoDoceHorasMenuActivo = false;
        mostrarPantallaPrincipal();
      }
    }
    else if (opcionSeleccionada == 2) { // Force color
      enForceColorMenu = true;
      forceColorPantalla = FC_ROOT;
      forceColorOpcion = 0;
      mostrarForceColorMenu();
    }
    else if (opcionSeleccionada == 3) { // Game mode
      gameMode = !gameMode;
      if (gameMode) {
        inicioGameMode = millis();
        ultimoCambioGameMode = millis();
        estadoGameMode = 0; // Comienza en verde
        Serial.println(F("Game mode ACTIVADO - Comienza en VERDE"));
      } else {
        Serial.println(F("Game mode DESACTIVADO"));
      }
      mostrarPantallaPrincipal();
    }
    else if (opcionSeleccionada == 4) { // Settings
      enMenuSettings = true;
      settingOpcion = 0;
      settingScrollOffset = 0; // Resetear scroll al entrar
      mostrarMenuSettings();
    }
  }
  ultimaInteraccion = millis();
}

void guardarEdicionNap() {
  if (napEditandoStart) {
    napHoraInicio = napHoraTemporal;
    napMinutoInicio = napMinutoTemporal;
  } else {
    napDuracionHoras = napDuracionHorasTemporal;
    napDuracionMinutos = napDuracionMinutosTemporal;
  }
  guardarNapConfigEnEEPROM();
  napEditando = false;
  napConfirmando = false;
  napEditandoCampo = 0;
  mostrarNapMenu();
}

void cancelarEdicionNap() {
  napEditando = false;
  napConfirmando = false;
  napEditandoCampo = 0;
  mostrarNapMenu();
}

void cambiarValorEdicionNap() {
  if (napEditandoStart) {
    if (napEditandoCampo == 0) {
      napHoraTemporal = (napHoraTemporal + 1) % 24;
    } else {
      napMinutoTemporal = (napMinutoTemporal + 5) % 60;
    }
  } else if (napEditandoCampo == 0) {
    napDuracionHorasTemporal = (napDuracionHorasTemporal + 1) % 4;
  } else {
    napDuracionMinutosTemporal = (napDuracionMinutosTemporal + 5) % 60;
  }
}

void procesarBotonSelectNapMenu() {
  if (napEditando) {
    if (napConfirmando) {
      if (napConfirmOpcion == 0) {
        guardarEdicionNap();
      } else {
        cancelarEdicionNap();
      }
      return;
    }
    cambiarValorEdicionNap();
    mostrarNapMenu();
    return;
  }

  switch (napMenuOpcion) {
    case 0: // Mode
      napActivo = !napActivo;
      guardarNapConfigEnEEPROM();
      Serial.print(F("Nap "));
      Serial.println(napActivo ? F("ACTIVADO") : F("DESACTIVADO"));
      break;
    case 1: // Hour
      napEditando = true;
      napEditandoStart = true;
      napConfirmando = false;
      napEditandoCampo = 0;
      napHoraTemporal = napHoraInicio;
      napMinutoTemporal = napMinutoInicio;
      break;
    case 2: // Duration
      napEditando = true;
      napEditandoStart = false;
      napConfirmando = false;
      napEditandoCampo = 0;
      napDuracionHorasTemporal = napDuracionHoras;
      napDuracionMinutosTemporal = napDuracionMinutos;
      break;
    case 3: // Back
      enNapMenu = false;
      enMenuSettings = true;
      mostrarMenuSettings();
      return;
  }
  mostrarNapMenu();
}

void empezarEdicionWake(bool nuevo) {
  if (nuevo) {
    uint8_t libre = primerSlotWakeLibre();
    if (libre == 0xFF) return;
    wakeEditIndex = libre;
    wakeEditHora = DEFAULT_HORA_DESPERTAR;
    wakeEditMinutos = DEFAULT_MINUTOS_DESPERTAR;
    wakeEditRango = DAY_ALL;
  } else {
    wakeEditHora = wakeHora[wakeEditIndex];
    wakeEditMinutos = wakeMinutos[wakeEditIndex];
    wakeEditRango = wakeRango[wakeEditIndex];
  }
  agregandoWake = nuevo;
  wakeEditCampo = 0;
  wakePantalla = WAKE_EDIT;
  mostrarEdicionWake();
}

void guardarEdicionWake() {
  wakeHora[wakeEditIndex] = wakeEditHora;
  wakeMinutos[wakeEditIndex] = wakeEditMinutos;
  wakeRango[wakeEditIndex] = wakeEditRango;
  wakeOcupado |= (uint8_t)(1 << wakeEditIndex);
  guardarWakeEnEEPROM();
  agregandoWake = false;
  wakeEditCampo = 0;
  wakePantalla = WAKE_LISTA;
  seleccionarSlotEnLista(wakeEditIndex);
  mostrarWakeUpMenu();
}

void cancelarEdicionWake() {
  bool eraNuevo = agregandoWake;
  agregandoWake = false;
  wakeEditCampo = 0;
  if (eraNuevo) {
    wakePantalla = WAKE_LISTA;
    mostrarWakeUpMenu();
  } else {
    wakePantalla = WAKE_SLOT;
    wakeSlotMenuOpcion = 0;
    mostrarWakeSlotMenu();
  }
}

void borrarSlotWake() {
  uint8_t slot = wakeEditIndex;
  wakeOcupado &= (uint8_t)~(1 << slot);
  guardarWakeEnEEPROM();
  wakePantalla = WAKE_LISTA;
  seleccionarFilaTrasBorrar(slot);
  mostrarWakeUpMenu();
}

void procesarBotonSelectWakeMenu() {
  if (wakePantalla == WAKE_CONFIRM) {
    if (wakeConfirmOpcion == 0) {
      guardarEdicionWake();
    } else {
      cancelarEdicionWake();
    }
    return;
  }

  if (wakePantalla == WAKE_EDIT) {
    if (wakeEditCampo == 0) {
      wakeEditHora = (wakeEditHora + 1) % 24;
    } else if (wakeEditCampo == 1) {
      wakeEditMinutos = (wakeEditMinutos + 5) % 60;
    } else {
      wakeEditRango = (wakeEditRango + 1) % DAY_RANGE_COUNT;
    }
    mostrarEdicionWake();
    return;
  }

  if (wakePantalla == WAKE_SLOT) {
    if (wakeSlotMenuOpcion == 0) {
      empezarEdicionWake(false);
    } else if (wakeSlotMenuOpcion == 1) {
      borrarSlotWake();
    } else {
      wakePantalla = WAKE_LISTA;
      mostrarWakeUpMenu();
    }
    return;
  }

  uint8_t total = totalOpcionesWakeMenu();
  uint8_t slot = slotDeOpcionWake(wakeMenuOpcion);
  if (slot != 0xFF) {
    wakeEditIndex = slot;
    wakeSlotMenuOpcion = 0;
    wakePantalla = WAKE_SLOT;
    mostrarWakeSlotMenu();
  } else if (contarSlotsWake() < MAX_WAKE_SLOTS && wakeMenuOpcion == contarSlotsWake()) {
    empezarEdicionWake(true);
  } else if (wakeMenuOpcion == total - 1) {
    enWakeUpMenu = false;
    agregandoWake = false;
    wakePantalla = WAKE_LISTA;
    enMenuSettings = true;
    mostrarMenuSettings();
  }
}

void procesarBotonMoveWakeMenu() {
  if (wakePantalla == WAKE_CONFIRM) {
    wakeConfirmOpcion = (wakeConfirmOpcion + 1) % 2;
    mostrarEdicionWake();
    return;
  }

  if (wakePantalla == WAKE_EDIT) {
    if (wakeEditCampo < 2) {
      wakeEditCampo++;
    } else {
      wakePantalla = WAKE_CONFIRM;
      wakeConfirmOpcion = 0;
    }
    mostrarEdicionWake();
    return;
  }

  if (wakePantalla == WAKE_SLOT) {
    wakeSlotMenuOpcion = (wakeSlotMenuOpcion + 1) % 3;
    mostrarWakeSlotMenu();
    return;
  }

  uint8_t total = totalOpcionesWakeMenu();
  wakeMenuOpcion = (wakeMenuOpcion + 1) % total;
  wakeScrollOffset = (wakeMenuOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  mostrarWakeUpMenu();
}

void procesarBotonMove() {
  if (enNapMenu) {
    procesarBotonMoveNapMenu();
  }
  else if (enMenuAbout) {
    // En pantalla About, el botón Move no hace nada
    return;
  }
  else if (enForceColorMenu) {
    // Navegación en Force color menu expandido
    forceColorOpcion = (forceColorOpcion + 1) % totalOpcionesForceColor();
    mostrarForceColorMenu();
  }
  else if (enWakeUpMenu) {
    procesarBotonMoveWakeMenu();
  }
  else if (editandoHora) {
    if (confirmandoEdicionHora) {
      horaConfirmOpcion = (horaConfirmOpcion + 1) % 2;
    } else {
      avanzarCampoEdicionHora();
    }
    mostrarEdicionHora();
  }
  else if (enMenuSettings) {
    uint8_t nuevaOpcion = (settingOpcion + 1) % totalSettingOpciones;
    settingOpcion = nuevaOpcion;
    
    // Calcular nuevo scroll offset basado en pares de opciones
    settingScrollOffset = (settingOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
    
    mostrarMenuSettings();
  } else if (modoDormirMenuActivo || modoDoceHorasMenuActivo) {
    // Navegación en menús especiales
    opcionEspecialSeleccionada = (opcionEspecialSeleccionada + 1) % totalOpcionesEspecial;
    if (modoDormirMenuActivo) {
      mostrarPantallaDormir();
    } else {
      mostrarPantallaDoceHoras();
    }
  } else {
    // Menú principal con scroll por pares
    uint8_t nuevaOpcion = (opcionSeleccionada + 1) % totalOpciones;
    opcionSeleccionada = nuevaOpcion;
    
    mostrarPantallaPrincipal();
  }
  ultimaInteraccion = millis();
}

void procesarBotonMoveNapMenu() {
  if (napEditando) {
    if (napConfirmando) {
      napConfirmOpcion = (napConfirmOpcion + 1) % 2;
    } else if (napEditandoCampo == 0) {
      napEditandoCampo = 1;
    } else {
      napConfirmando = true;
      napConfirmOpcion = 0;
    }
  } else {
    napMenuOpcion = (napMenuOpcion + 1) % napTotalOpciones;
  }
  mostrarNapMenu();
}

void manejarLuces() {
  // Verificar si es medianoche (00:00) para forzar salida del modo dormir
  if (now.hour() == 0 && now.minute() == 0) {
    botonPresionado = false;
    modoSiestaActivo = false;
    enVerdeExtraSiesta = false;
    modoDormirMenuActivo = false;
    modoDoceHorasMenuActivo = false;
  }

  // Force color activo queda fijo hasta que su modo termine.
  if (forceMode != FORCE_MODE_OFF) {
    aplicarColorForzado();
    return;
  }

  // Force wakeup
  if (forceWakeupActivo) {
    setLuz(ledVerde, intensidadVerdeTenue);
    return;
  }

  // 2. SLEEP MODE (con lógica dual siesta/noche)
  if (botonPresionado) {
    if (modoSiestaActivo || enVerdeExtraSiesta) {
      // Estamos en modo siesta (ya sea rojo tenue o verde tenue extra)
      manejarModoSiesta();
    } else {
      // Sleep Mode normal (noche) - rojo tenue siempre
      setLuz(ledRojo, intensidadRojoTenue);
    }
    return;
  }

  // 3. 12-HOUR SLEEP CORREGIDO (CON TRANSICIONES CORRECTAS)
  if (doceHorasSleep) {
    unsigned long tiempoTranscurrido = millis() - inicioDoceHoras;
    
    // Verificar si ya terminó completamente (12h 15min)
    if (tiempoTranscurrido >= DOCE_HORAS_QUINCE_MIN_MS) {
      doceHorasSleep = false;
      modoDoceHorasMenuActivo = false;
      Serial.print(F("12-hour sleep COMPLETADO - Desactivado automáticamente a las "));
      Serial.println(millis());
      return; // Salir para que continúe con lógica normal
    }
    
    // Después de 12 horas: VERDE TENUE por 15 minutos
    else if (tiempoTranscurrido >= DOCE_HORAS_MS) {
      // Mostrar verde tenue por 15 minutos después de las 12 horas
      setLuz(ledVerde, intensidadVerdeTenue);
      Serial.print(F("12-hour sleep VERDE TENUE - Transcurrido: "));
      Serial.print(tiempoTranscurrido / 1000 / 60.0, 1);
      Serial.println(F(" minutos"));
    }
    
    // Últimos 15 minutos de las 12 horas: AMARILLO TENUE
    else if (tiempoTranscurrido >= (DOCE_HORAS_MS - QUINCE_MINUTOS_MS)) {
      // Mostrar amarillo tenue los últimos 15 minutos
      setLuz(ledAmarillo, intensidadAmarilloTenue);
      Serial.print(F("12-hour sleep AMARILLO TENUE - Transcurrido: "));
      Serial.print(tiempoTranscurrido / 1000 / 60.0, 1);
      Serial.println(F(" minutos"));
    }
    
    // Primeras 11 horas 45 minutos: ROJO TENUE
    else {
      setLuz(ledRojo, intensidadRojoTenue);
    }
    return;
  }

  // 4. GAME MODE (nuevo - semáforo de juego)
  if (gameMode) {
    manejarGameMode();
    return;
  }

  // 5. NAP MODE AVISOS Y COMPORTAMIENTO (solo si Nap está ON y NO estamos en Sleep Mode)
  if (napActivo && !botonPresionado) {
    // 5.1. Aviso amarillo 30 minutos antes de la siesta
    if (estaEnAvisoPrevioSiesta()) {
      setLuz(ledAmarillo, intensidadAmarilloMax);
      return;
    }
    
    // 5.2. Dentro del placeholder de siesta (pero NO hemos activado Sleep Mode)
    if (estaDentroPlaceholderSiesta()) {
      // Estamos dentro del horario de siesta pero no se ha activado Sleep Mode
      // Mostrar rojo intenso para indicar que es hora de siesta
      setLuz(ledRojo, intensidadRojoMax);
      return;
    }
  }

  // 6. LÓGICA NORMAL DE HORARIOS
  int tiempoActual = now.hour() * 60 + now.minute();
  int tiempoDormir = horaDormir * 60 + minutosDormir;

  // Pasada la hora de dormir siempre es rojo fuerte, aunque el horario
  // de despertar de hoy caiga después o no exista.
  if (tiempoActual >= tiempoDormir) {
    setLuz(ledRojo, intensidadRojoMax);
    return;
  }

  uint8_t horaWake;
  uint8_t minWake;
  bool hayWake = horarioDespertarDeHoy(horaWake, minWake);

  // Sin horario para hoy: rojo tenue hasta 30 minutos antes de dormir.
  if (!hayWake) {
    if (tiempoActual < tiempoDormir - 30) {
      setLuz(ledRojo, intensidadRojoTenue);
    } else {
      setLuz(ledAmarillo, intensidadAmarilloMax);
    }
    return;
  }

  int tiempoDespertar = horaWake * 60 + minWake;

  // 12:00 AM - 7:15 AM (rojo tenue)
  if (tiempoActual < tiempoDespertar - 15) {
    setLuz(ledRojo, intensidadRojoTenue);
  } 
  // 7:15 AM - 7:30 AM (amarillo tenue)
  else if (tiempoActual < tiempoDespertar) {
    setLuz(ledAmarillo, intensidadAmarilloTenue);
  }
  // 7:30 AM - 8:30 AM (verde tenue)
  else if (tiempoActual < tiempoDespertar + 60) {
    setLuz(ledVerde, intensidadVerdeTenue);
  }
  // 8:30 AM - 7:00 PM (verde máximo)
  else if (tiempoActual < tiempoDormir - 30) {
    setLuz(ledVerde, intensidadVerdeMax);
  }
  // 7:00 PM - 7:30 PM (amarillo máximo)
  else {
    setLuz(ledAmarillo, intensidadAmarilloMax);
  }
}

void manejarGameMode() {
  unsigned long tiempoActual = millis();
  unsigned long tiempoTranscurrido = tiempoActual - ultimoCambioGameMode;
  
  switch(estadoGameMode) {
    case 0: // VERDE
      if (tiempoTranscurrido >= TIEMPO_VERDE_JUEGO) {
        // Cambiar a AMARILLO
        estadoGameMode = 1;
        ultimoCambioGameMode = tiempoActual;
        Serial.println(F("Game mode: VERDE -> AMARILLO"));
      } else {
        setLuz(ledVerde, intensidadVerdeMax);
      }
      break;
      
    case 1: // AMARILLO
      if (tiempoTranscurrido >= TIEMPO_AMARILLO_JUEGO) {
        // Cambiar a ROJO
        estadoGameMode = 2;
        ultimoCambioGameMode = tiempoActual;
        Serial.println(F("Game mode: AMARILLO -> ROJO"));
      } else {
        setLuz(ledAmarillo, intensidadAmarilloMax);
      }
      break;
      
    case 2: // ROJO
      if (tiempoTranscurrido >= TIEMPO_ROJO_JUEGO) {
        // Volver a VERDE (ciclo completo)
        estadoGameMode = 0;
        ultimoCambioGameMode = tiempoActual;
        Serial.println(F("Game mode: ROJO -> VERDE (ciclo completo)"));
      } else {
        setLuz(ledRojo, intensidadRojoMax);
      }
      break;
  }
}

void setLuz(int pin, int intensidad) {
  // Apagar las otras primero. digitalWrite después de analogWrite
  // cancela el PWM del LED que acabamos de encender.
  if (pin != ledVerde) analogWrite(ledVerde, 0);
  if (pin != ledAmarillo) analogWrite(ledAmarillo, 0);
  if (pin != ledRojo) analogWrite(ledRojo, 0);
  analogWrite(pin, intensidad);
}

void mostrarPantallaPrincipal() {
  oled.clear();
  
  // Primera línea: día, hora y marca. Dos espacios: el borde izquierdo recorta el primer carácter.
  oled.print("  ");
  oled.print(getWeekdayName(now.dayOfTheWeek()));
  oled.print(" ");
  oled.print(now.hour());
  oled.print(":");
  if (now.minute() < 10) oled.print("0");
  oled.print(now.minute());
  const char marca[] = "iTronnix";
  oled.setCursor(oled.displayWidth() - oled.strWidth(marca), 0);
  oled.println(marca);
  
  // Calcular qué opciones mostrar (siempre 2 opciones por pantalla)
  uint8_t opcionInicial = (opcionSeleccionada / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, totalOpciones);
  
  // Mostrar las opciones correspondientes
  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    if (i == opcionSeleccionada) {
      oled.print(">");
    } else {
      oled.print(" ");
    }
    
    switch(i) {
      case 0: // Sleep mode
        oled.print("Sleep mode [");
        oled.print(botonPresionado ? "On" : "Off");
        oled.println("]");
        break;
        
      case 1: // 12-hour sleep
        oled.print("12-hour sleep [");
        oled.print(doceHorasSleep ? "On" : "Off");
        oled.println("]");
        break;
        
      case 2: // Force color
        oled.print("Force color [");
        oled.print(etiquetaCortaForce());
        oled.println("]");
        break;
        
      case 3: // Game mode
        oled.print("Game mode [");
        oled.print(gameMode ? "On" : "Off");
        oled.println("]");
        break;
        
      case 4: // Settings
        oled.println("Settings");
        break;
    }
  }
  
  // Si hay menos opciones que el máximo, mostrar líneas vacías
  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }
  
  // Navegación con indicador de posición
  oled.print(" Move ");
  oled.print(opcionSeleccionada + 1);
  oled.print("/");
  oled.print(totalOpciones);
  if (opcionSeleccionada == 2 || opcionSeleccionada == 4) {
    oled.println("          Select");
  } else {
    oled.println("          Toggle");
  }
}

// Pantalla especial cuando Sleep mode está activo
void mostrarPantallaDormir() {
  oled.clear();
  
  // Primera línea: Hora y versión
  oled.print(" ");
  oled.print(now.hour());
  oled.print(":");
  if (now.minute() < 10) oled.print("0");
  oled.print(now.minute());
  oled.println("              iTronnix");
  
  // Calcular qué opciones mostrar (siempre 2 opciones por pantalla)
  uint8_t opcionInicial = (opcionEspecialSeleccionada / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, totalOpcionesEspecial);
  
  // Mostrar las opciones correspondientes
  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    if (i == opcionEspecialSeleccionada) {
      oled.print(">");
    } else {
      oled.print(" ");
    }
    
    switch(i) {
      case 0: // Sleep mode [On]
        oled.print("Sleep mode [On]");
        oled.println();
        break;
        
      case 1: // Force wakeup [Off]
        oled.print("Force wakeup [Off]");
        oled.println();
        break;
    }
  }
  
  // Si hay menos opciones que el máximo, mostrar líneas vacías
  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }
  
  // Navegación con indicador de posición
  oled.print(" Move ");
  oled.print(opcionEspecialSeleccionada + 1);
  oled.print("/");
  oled.print(totalOpcionesEspecial);
  oled.println("          Select");
}

// NUEVA: Pantalla especial cuando 12-hour sleep está activo
void mostrarPantallaDoceHoras() {
  oled.clear();
  
  // Primera línea: Hora y versión
  oled.print(" ");
  oled.print(now.hour());
  oled.print(":");
  if (now.minute() < 10) oled.print("0");
  oled.print(now.minute());
  oled.println("              iTronnix");
  
  // Calcular qué opciones mostrar (siempre 2 opciones por pantalla)
  uint8_t opcionInicial = (opcionEspecialSeleccionada / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, totalOpcionesEspecial);
  
  // Mostrar las opciones correspondientes
  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    if (i == opcionEspecialSeleccionada) {
      oled.print(">");
    } else {
      oled.print(" ");
    }
    
    switch(i) {
      case 0: // 12-hour sleep [On]
        oled.print("12-hour sleep [On]");
        oled.println();
        break;
        
      case 1: // Force wakeup [Off]
        oled.print("Force wakeup [Off]");
        oled.println();
        break;
    }
  }
  
  // Si hay menos opciones que el máximo, mostrar líneas vacías
  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }
  
  // Navegación con indicador de posición
  oled.print(" Move ");
  oled.print(opcionEspecialSeleccionada + 1);
  oled.print("/");
  oled.print(totalOpcionesEspecial);
  oled.println("          Select");
}

void mostrarMenuSettings() {
  oled.clear();
  // Título
  oled.println(" SETTINGS");
  
  // Calcular qué opciones mostrar (siempre 2 opciones por pantalla)
  uint8_t opcionInicial = settingScrollOffset;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, totalSettingOpciones);
  
  // Mostrar las opciones correspondientes
  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    if (i == settingOpcion) {
      oled.print(">");
    } else {
      oled.print(" ");
    }
    
    switch(i) {
      case 0: { // Wake up
        uint8_t nWake = contarSlotsWake();
        if (nWake == 0) {
          oled.print("Wake up");
        } else if (nWake == 1) {
          uint8_t slot = 0;
          for (uint8_t s = 0; s < MAX_WAKE_SLOTS; s++) {
            if (slotWakeOcupado(s)) {
              slot = s;
              break;
            }
          }
          oled.print("Wake up ");
          imprimirHoraConRango(wakeHora[slot], wakeMinutos[slot], wakeRango[slot]);
        } else {
          oled.print("Wake up (custom)");
        }
        break;
      }
        
      case 1: // Sleep at
        oled.print("Sleep ");
        oled.print(horaDormir);
        oled.print(":");
        if (minutosDormir < 10) oled.print("0");
        oled.print(minutosDormir);
        break;
        
      case 2: // Nap
        oled.print("Nap ");
        if (napActivo) {
          oled.print("[On] ");
          oled.print(napHoraInicio);
          oled.print(":");
          if (napMinutoInicio < 10) oled.print("0");
          oled.print(napMinutoInicio);
        } else {
          oled.print("[Off]");
        }
        break;
        
      case 3: // Clock setup
        oled.print("Clock setup");
        break;
        
      case 4: // About
        oled.print("About");
        break;
        
      case 5: // Back
        oled.print("Back");
        break;
    }
    
    oled.println();
  }
  
  // Si hay menos opciones que el máximo, mostrar líneas vacías
  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }
  
  // Navegación con indicador de posición
  oled.print(" Move ");
  oled.print(settingOpcion + 1);
  oled.print("/");
  oled.print(totalSettingOpciones);
  oled.println("          Select");
}

void mostrarNapMenu() {
  oled.clear();

  if (napEditando) {
    oled.println(napEditandoStart ? " HOUR" : " DURATION");

    if (napConfirmando) {
      oled.print(napConfirmOpcion == 0 ? ">" : " ");
      oled.println("Save");
      oled.print(napConfirmOpcion == 1 ? ">" : " ");
      oled.println("Cancel");
      oled.print(" Move ");
      oled.print(napConfirmOpcion + 1);
      oled.print("/2");
      oled.println("          Select");
      return;
    }

    if (napEditandoStart) {
      oled.print(napEditandoCampo == 0 ? ">" : " ");
      oled.print(napHoraTemporal);
      oled.print(":");
      if (napEditandoCampo == 1) oled.print(">");
      if (napMinutoTemporal < 10) oled.print("0");
      oled.print(napMinutoTemporal);
      oled.println();
    } else {
      if (napEditandoCampo == 0) oled.print(">");
      else oled.print(" ");
      oled.print(napDuracionHorasTemporal);
      oled.print("h ");
      if (napEditandoCampo == 1) oled.print(">");
      if (napDuracionMinutosTemporal < 10) oled.print("0");
      oled.print(napDuracionMinutosTemporal);
      oled.print("m");
      oled.println();
    }

    oled.println();
    oled.println(" Move              Change");
    return;
  }

  oled.println(" NAP");

  uint8_t opcionInicial = (napMenuOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, napTotalOpciones);

  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    oled.print(i == napMenuOpcion ? ">" : " ");

    switch (i) {
      case 0:
        oled.print("Mode [");
        oled.print(napActivo ? "On " : "Off");
        oled.println("]");
        break;
      case 1:
        oled.print("Hour: ");
        oled.print(napHoraInicio);
        oled.print(":");
        if (napMinutoInicio < 10) oled.print("0");
        oled.print(napMinutoInicio);
        oled.println();
        break;
      case 2:
        oled.print("Duration: ");
        oled.print(napDuracionHoras);
        oled.print("h ");
        if (napDuracionMinutos < 10) oled.print("0");
        oled.print(napDuracionMinutos);
        oled.print("m");
        oled.println();
        break;
      default:
        oled.println("Back");
        break;
    }
  }

  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }

  oled.print(" Move ");
  oled.print(napMenuOpcion + 1);
  oled.print("/");
  oled.print(napTotalOpciones);
  if (napMenuOpcion == 0) {
    oled.println("          Toggle");
  } else if (napMenuOpcion == 1 || napMenuOpcion == 2) {
    oled.println("            Edit");
  } else {
    oled.println("          Select");
  }
}

void mostrarMenuAbout() {
  oled.clear();
  // Título
  oled.println(" ABOUT");
  
  // Versión
  oled.print(" Version: ");
  oled.println("1.60");

  // Website
  oled.println(" www.iTronnix.com");
  
  // Back
  oled.println(".                              Back");
}

void procesarBotonSelectForceColor() {
  if (forceColorPantalla == FC_ROOT) {
    if (forceColorOpcion == 0) {
      forceColorPantalla = FC_MODE;
      forceColorOpcion = forceMode;
    } else if (forceColorOpcion == 1) {
      forceColorPantalla = FC_COLOR;
      forceColorOpcion = forceColorSel;
    } else {
      enForceColorMenu = false;
      forceColorPantalla = FC_ROOT;
      mostrarPantallaPrincipal();
      return;
    }
    mostrarForceColorMenu();
    return;
  }

  if (forceColorPantalla == FC_MODE) {
    if (forceColorOpcion < 4) {
      forceMode = forceColorOpcion;
      if (forceMode == FORCE_MODE_1HOUR) {
        tiempoInicioForceColor = millis();
      } else if (forceMode == FORCE_MODE_UNTIL) {
        faseAlForzar = faseHorario();
      }
    }
    forceColorPantalla = FC_ROOT;
    forceColorOpcion = 0;
    mostrarForceColorMenu();
    return;
  }

  if (forceColorOpcion < 7) {
    forceColorSel = forceColorOpcion;
  }
  forceColorPantalla = FC_ROOT;
  forceColorOpcion = 1;
  mostrarForceColorMenu();
}

void imprimirOpcionForceColor(uint8_t i) {
  if (forceColorPantalla == FC_MODE) {
    switch (i) {
      case 0: oled.println("Off"); break;
      case 1: oled.println("Persistent"); break;
      case 2: oled.println("1 hour"); break;
      case 3: oled.println("Until event"); break;
      default: oled.println("Back"); break;
    }
    return;
  }

  if (forceColorPantalla == FC_COLOR) {
    switch (i) {
      case 0: oled.println("Green Hi"); break;
      case 1: oled.println("Green Lo"); break;
      case 2: oled.println("Yellow Hi"); break;
      case 3: oled.println("Yellow Lo"); break;
      case 4: oled.println("Red Hi"); break;
      case 5: oled.println("Red Lo"); break;
      case 6: oled.println("All off"); break;
      default: oled.println("Back"); break;
    }
    return;
  }

  switch (i) {
    case 0:
      oled.print("Mode [");
      oled.print(etiquetaModoForce());
      oled.println("]");
      break;
    case 1:
      oled.print("Color [");
      oled.print(etiquetaColorForce());
      oled.println("]");
      break;
    default:
      oled.println("Back");
      break;
  }
}

void mostrarForceColorMenu() {
  oled.clear();
  if (forceColorPantalla == FC_MODE) {
    oled.println(" MODE");
  } else if (forceColorPantalla == FC_COLOR) {
    oled.println(" COLOR");
  } else {
    oled.println(" FORCE COLOR");
  }

  uint8_t total = totalOpcionesForceColor();
  uint8_t opcionInicial = (forceColorOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, total);

  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    oled.print(i == forceColorOpcion ? ">" : " ");
    imprimirOpcionForceColor(i);
  }

  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }

  oled.print(" Move ");
  oled.print(forceColorOpcion + 1);
  oled.print("/");
  oled.print(total);
  if (forceColorPantalla == FC_ROOT) {
    oled.println("          Select");
  } else {
    oled.println("           Apply");
  }
}

void mostrarWakeUpMenu() {
  oled.clear();
  oled.println(" WAKEUP");

  uint8_t total = totalOpcionesWakeMenu();
  uint8_t ocupados = contarSlotsWake();
  uint8_t opcionInicial = wakeScrollOffset;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, total);

  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    oled.print(i == wakeMenuOpcion ? ">" : " ");

    uint8_t slot = slotDeOpcionWake(i);
    if (slot != 0xFF) {
      oled.print("Time ");
      oled.print(slot + 1);
      oled.print(": ");
      imprimirHoraConRango(wakeHora[slot], wakeMinutos[slot], wakeRango[slot]);
    } else if (ocupados < MAX_WAKE_SLOTS && i == ocupados) {
      oled.print("Add new");
    } else {
      oled.print("Back");
    }
    oled.println();
  }

  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }

  oled.print(" Move ");
  oled.print(wakeMenuOpcion + 1);
  oled.print("/");
  oled.print(total);
  oled.println("          Select");
}

void mostrarWakeSlotMenu() {
  oled.clear();
  oled.print(" TIME ");
  oled.println(wakeEditIndex + 1);

  const uint8_t total = 3;
  uint8_t opcionInicial = (wakeSlotMenuOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, total);

  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    oled.print(i == wakeSlotMenuOpcion ? ">" : " ");
    if (i == 0) oled.println("Edit");
    else if (i == 1) oled.println("Delete");
    else oled.println("Back");
  }

  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }

  oled.print(" Move ");
  oled.print(wakeSlotMenuOpcion + 1);
  oled.print("/");
  oled.print(total);
  oled.println("          Select");
}

void mostrarEdicionWake() {
  oled.clear();
  oled.println(agregandoWake ? " ADD NEW" : " EDIT");

  if (wakePantalla == WAKE_CONFIRM) {
    oled.print(wakeConfirmOpcion == 0 ? ">" : " ");
    oled.println("Save");
    oled.print(wakeConfirmOpcion == 1 ? ">" : " ");
    oled.println("Cancel");
    oled.print(" Move ");
    oled.print(wakeConfirmOpcion + 1);
    oled.print("/2");
    oled.println("          Select");
    return;
  }

  if (wakeEditCampo == 0) {
    oled.print(">");
  } else {
    oled.print(" ");
  }
  oled.print(wakeEditHora);
  oled.print(":");
  if (wakeEditCampo == 1) {
    oled.print(">");
  }
  if (wakeEditMinutos < 10) oled.print("0");
  oled.print(wakeEditMinutos);
  oled.println();

  if (wakeEditCampo == 2) {
    oled.print(">");
  } else {
    oled.print(" ");
  }
  oled.println(getDayRangeLabel(wakeEditRango));

  oled.println(" Move              Change");
}

void mostrarEdicionHora() {
  oled.clear();

  if (editandoReloj) {
    oled.println(" CLOCK SETUP");
  } else {
    oled.println(" SLEEP");
  }

  if (confirmandoEdicionHora) {
    oled.print(horaConfirmOpcion == 0 ? ">" : " ");
    oled.println("Save");
    oled.print(horaConfirmOpcion == 1 ? ">" : " ");
    oled.println("Cancel");
    oled.print(" Move ");
    oled.print(horaConfirmOpcion + 1);
    oled.print("/2");
    oled.println("          Select");
    return;
  }

  uint8_t hora = editandoReloj ? horaTemporalReloj : horaTemporalDormir;
  uint8_t minutos = editandoReloj ? minutosTemporalReloj : minutosTemporalDormir;

  oled.print(editandoCampo == 0 ? ">" : " ");
  oled.print(hora);
  oled.print(":");
  if (editandoCampo == 1) oled.print(">");
  if (minutos < 10) oled.print("0");
  oled.print(minutos);
  oled.println();

  if (editandoReloj) {
    oled.print(editandoCampo == 2 ? ">" : " ");
    oled.println(getWeekdayFullName(diaTemporalReloj));
  } else {
    oled.println();
  }

  oled.println(" Move              Change");
}