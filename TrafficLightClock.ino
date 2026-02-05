// VERSION: 1.35 - Force Color expandido con opciones High/Low

#include <Wire.h>
#include <RTClib.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>
#include <EEPROM.h>  // Nueva librería para persistencia

// Variables globales optimizadas
int ultimoMinutoImpreso = -1;
unsigned long ultimaInteraccion = 0;
const unsigned long TIEMPO_PANTALLA_ACTIVA = 3000000; // 30 segundos
bool pantallaApagada = false;
unsigned long tiempoEncendidoPantalla = 0; // Para controlar el bloqueo temporal
const unsigned long TIEMPO_BLOQUEO_BOTONES = 300; // 300ms de bloqueo después de encender

// Direcciones EEPROM para configuración persistente
#define EE_MAGIC_BYTE 0          // Byte mágico para verificar si EEPROM está inicializada
#define EE_MAGIC_VALUE 0x55      // Valor mágico
#define EE_HORA_DESPERTAR 1      // Hora de despertar
#define EE_MINUTOS_DESPERTAR 2   // Minutos de despertar
#define EE_HORA_DORMIR 3         // Hora de dormir
#define EE_MINUTOS_DORMIR 4      // Minutos de dormir
#define EE_NAP_ACTIVO 5          // Estado Nap (0=Off, 1=On)
#define EE_NAP_HORA_INICIO 6     // Hora inicio nap
#define EE_NAP_MINUTO_INICIO 7   // Minuto inicio nap
#define EE_NAP_DURACION_HORAS 8  // Duración horas nap
#define EE_NAP_DURACION_MINUTOS 9 // Duración minutos nap
#define EE_TOTAL_CONFIG_BYTES 10 // Total de bytes usados en EEPROM

// Valores por defecto
const uint8_t DEFAULT_HORA_DESPERTAR = 7;
const uint8_t DEFAULT_MINUTOS_DESPERTAR = 30;
const uint8_t DEFAULT_HORA_DORMIR = 19;
const uint8_t DEFAULT_MINUTOS_DORMIR = 30;
const uint8_t DEFAULT_NAP_HORA_INICIO = 13;
const uint8_t DEFAULT_NAP_MINUTO_INICIO = 0;
const uint8_t DEFAULT_NAP_DURACION_HORAS = 1;
const uint8_t DEFAULT_NAP_DURACION_MINUTOS = 30;

// Horarios (editables) - optimizado a uint8_t - ahora se cargan desde EEPROM en setup()
uint8_t horaDespertar;
uint8_t minutosDespertar;
uint8_t horaDormir;
uint8_t minutosDormir;

// Variables para Nap Mode (nuevo sistema completo) - optimizado a uint8_t
bool napActivo; // Estado del nap
uint8_t napHoraInicio; // Hora de inicio del nap
uint8_t napMinutoInicio; // Minuto de inicio del nap
uint8_t napDuracionHoras; // Duración en horas (0-3)
uint8_t napDuracionMinutos; // Duración en minutos
bool enNapMenu = false; // Controlar si estamos en el menú de Nap
uint8_t napMenuOpcion = 0; // 0=Nap toggle, 1=Start, 2=Duration, 3=Back
bool napEditando = false; // Estamos editando el horario?
bool napEditandoStart = true; // true=Editando Start, false=Editando Duration
uint8_t napEditandoCampo = 0; // 0=hora/horas, 1=minuto/minutos
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

// Variables para 12-hour sleep y Force color
bool doceHorasSleep = false;
unsigned long inicioDoceHoras = 0;
const unsigned long DOCE_HORAS_MS = 12UL * 60UL * 60UL * 1000UL; // 12 horas en milisegundos
const unsigned long QUINCE_MINUTOS_MS = 15UL * 60UL * 1000UL;    // 15 minutos en milisegundos
const unsigned long DOCE_HORAS_QUINCE_MIN_MS = DOCE_HORAS_MS + QUINCE_MINUTOS_MS; // 12h 15min

// CAMBIO: forceColor ahora tiene 8 valores (0-7)
uint8_t forceColor = 0; // 0=Off, 1=Green high, 2=Green low, 3=Yellow high, 4=Yellow low, 5=Red high, 6=Red low, 7=Back
// CAMBIO: Texto actualizado con todas las opciones nuevas
const char forceColorTexto[] PROGMEM = "Off\0Green high\0Green low\0Yellow high\0Yellow low\0Red high\0Red low\0Back";

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
uint8_t editandoCampo = 0;           // 0=hora, 1=minutos
bool editandoWakeUp = true;          // true=Wake up, false=Sleep at
const uint8_t totalOpciones = 5;     // Cambiado de 4 a 5 (agregamos Game mode)
const uint8_t totalSettingOpciones = 6;  // Cambiado de 4 a 6 (agregamos Nap y About)
uint8_t settingScrollOffset = 0;     // Para controlar qué opciones mostrar en pantalla
const uint8_t MAX_OPCIONES_POR_PANTALLA = 2; // Máximo de opciones visibles a la vez

// Nueva variable para Force color menu
bool enForceColorMenu = false;
uint8_t forceColorOpcion = 0;        // CAMBIO: 0=Green high, 1=Green low, 2=Yellow high, 3=Yellow low, 4=Red high, 5=Red low, 6=Off, 7=Back
const uint8_t totalForceColorOpciones = 8; // CAMBIO: Actualizado de 5 a 8

// Intensidades para los diferentes niveles
const uint8_t intensidadRojoMax = 100;      // Rojo high
const uint8_t intensidadAmarilloMax = 255;  // Amarillo high
const uint8_t intensidadVerdeMax = 100;     // Verde high
const uint8_t intensidadRojoTenue = 3;      // Rojo low (tenue)
const uint8_t intensidadAmarilloTenue = 10; // Amarillo low (tenue)
const uint8_t intensidadVerdeTenue = 3;     // Verde low (tenue)

// RTC
RTC_DS3231 rtc;
DateTime now;

// OLED
SSD1306AsciiWire oled;
#define OLED_ADDRESS 0x3C

// ======================================================
// FUNCIONES EEPROM
// ======================================================

void inicializarEEPROM() {
  // Verificar si la EEPROM ya está inicializada
  if (EEPROM.read(EE_MAGIC_BYTE) != EE_MAGIC_VALUE) {
    Serial.println(F("EEPROM no inicializada - escribiendo valores por defecto"));
    
    // Escribir byte mágico
    EEPROM.update(EE_MAGIC_BYTE, EE_MAGIC_VALUE);
    
    // Escribir valores por defecto
    EEPROM.update(EE_HORA_DESPERTAR, DEFAULT_HORA_DESPERTAR);
    EEPROM.update(EE_MINUTOS_DESPERTAR, DEFAULT_MINUTOS_DESPERTAR);
    EEPROM.update(EE_HORA_DORMIR, DEFAULT_HORA_DORMIR);
    EEPROM.update(EE_MINUTOS_DORMIR, DEFAULT_MINUTOS_DORMIR);
    EEPROM.update(EE_NAP_ACTIVO, 0); // Nap desactivado por defecto
    EEPROM.update(EE_NAP_HORA_INICIO, DEFAULT_NAP_HORA_INICIO);
    EEPROM.update(EE_NAP_MINUTO_INICIO, DEFAULT_NAP_MINUTO_INICIO);
    EEPROM.update(EE_NAP_DURACION_HORAS, DEFAULT_NAP_DURACION_HORAS);
    EEPROM.update(EE_NAP_DURACION_MINUTOS, DEFAULT_NAP_DURACION_MINUTOS);
    
    Serial.println(F("Valores por defecto escritos en EEPROM"));
  }
}

void cargarConfiguracionDesdeEEPROM() {
  Serial.println(F("Cargando configuración desde EEPROM..."));
  
  // Cargar horarios
  horaDespertar = EEPROM.read(EE_HORA_DESPERTAR);
  minutosDespertar = EEPROM.read(EE_MINUTOS_DESPERTAR);
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
  Serial.print(F("  Wake up: ")); Serial.print(horaDespertar); Serial.print(F(":")); 
  if (minutosDespertar < 10) Serial.print(F("0")); Serial.println(minutosDespertar);
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
  EEPROM.update(EE_HORA_DESPERTAR, horaDespertar);
  EEPROM.update(EE_MINUTOS_DESPERTAR, minutosDespertar);
  EEPROM.update(EE_HORA_DORMIR, horaDormir);
  EEPROM.update(EE_MINUTOS_DORMIR, minutosDormir);
  Serial.println(F("Horarios guardados en EEPROM"));
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

// Función para obtener texto de forceColor desde PROGMEM
const char* getForceColorTexto(uint8_t index) {
  static char buffer[12]; // Aumentado para "Green high"
  const char *ptr = forceColorTexto;
  
  for(uint8_t i = 0; i < index; i++) {
    while(pgm_read_byte(ptr)) ptr++;
    ptr++; // saltar el null terminator
  }
  
  uint8_t j = 0;
  char c;
  while((c = pgm_read_byte(ptr++)) && j < 11) {
    buffer[j++] = c;
  }
  buffer[j] = '\0';
  return buffer;
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
  
  Serial.println(F("=== SISTEMA INICIADO - VERSION 1.35 (FORCE COLOR EXPANDIDO) ==="));
  Serial.print(F("12-hour sleep configurado a: "));
  Serial.print(DOCE_HORAS_QUINCE_MIN_MS / 1000 / 60 / 60.0, 2);
  Serial.println(F(" horas totales (12h + 15min transición)"));
  Serial.println(F("Configuración Nap Mode:"));
  Serial.print(F("  Umbral siesta/noche: ")); Serial.print(UMBRAL_HORA_SIESTA); Serial.println(F(":00 (5:00 PM)"));
  Serial.println(F("  Hora extra verde tenue: 60 minutos"));
  Serial.println(F("Configuración persistente en EEPROM habilitada"));
  Serial.println(F("Force Color expandido: Green high, Green low, Yellow high, Yellow low, Red high, Red low"));
}

void loop() {
  now = rtc.now();
  int hora = now.hour();
  int minuto = now.minute();

  if (minuto != ultimoMinutoImpreso) {
    Serial.print(F("Hora actual: "));
    Serial.print(hora);
    Serial.print(F(":"));
    if (minuto < 10) Serial.print(F("0"));
    Serial.println(minuto);

    ultimoMinutoImpreso = minuto;

    if (!botonPresionado && !enMenuSettings && !editandoHora && !enForceColorMenu && !pantallaApagada && !enMenuAbout && !enNapMenu) {
      mostrarPantallaPrincipal();
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

void manejarModoSiesta() {
  unsigned long tiempoActual = millis();
  
  if (enVerdeExtraSiesta) {
    // Estamos en la hora extra de verde tenue
    if (tiempoActual >= tiempoFinVerdeExtra) {
      // Terminó completamente el modo siesta
      modoSiestaActivo = false;
      enVerdeExtraSiesta = false;
      Serial.println(F("HORA EXTRA VERDE FINALIZADA - Comportamiento normal"));
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
  int tiempoDespertar = horaDespertar * 60 + minutosDespertar;
  
  return (tiempoActual < tiempoDespertar + 60) || // Hasta 8:30 AM
         (tiempoActual >= horaDormir * 60 + minutosDormir); // Después de 7:30 PM
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
  
  if (!editandoHora && !enMenuSettings && !enForceColorMenu && !enMenuAbout && !enNapMenu) {
    mostrarPantallaPrincipal();
  } else if (enMenuAbout) {
    mostrarMenuAbout();
  } else if (enNapMenu) {
    mostrarNapMenu();
  }
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
    // Menú Force color expandido
    if (forceColorOpcion == 0) { // Green high
      forceColor = 1;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 1) { // Green low
      forceColor = 2;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 2) { // Yellow high
      forceColor = 3;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 3) { // Yellow low
      forceColor = 4;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 4) { // Red high
      forceColor = 5;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 5) { // Red low
      forceColor = 6;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 6) { // Off
      forceColor = 0;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 7) { // Back
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
  }
  else if (editandoHora) {
    if (editandoCampo == 0) {
      editandoCampo = 1; // Cambiar a editar minutos
      mostrarEdicionHora();
    } else {
      // Guardar y salir al presionar Select en minutos
      if (editandoReloj) {
        // Aplicar la hora al RTC real usando las variables temporales
        DateTime nuevaHora = DateTime(now.year(), now.month(), now.day(), 
                                      horaTemporalReloj, minutosTemporalReloj, 0);
        rtc.adjust(nuevaHora);
        Serial.print(F("RTC ajustado a: "));
        Serial.print(horaTemporalReloj);
        Serial.print(F(":"));
        if (minutosTemporalReloj < 10) Serial.print(F("0"));
        Serial.println(minutosTemporalReloj);
      } else if (editandoWakeUp) {
        // Guardar Wake up en EEPROM
        guardarHorariosEnEEPROM();
      } else {
        // Guardar Sleep at en EEPROM
        guardarHorariosEnEEPROM();
      }
      
      editandoHora = false;
      editandoReloj = false;
      editandoCampo = 0;
      enMenuSettings = true;
      mostrarMenuSettings();
      return;
    }
  }
  else if (enMenuSettings) {
    if (settingOpcion == 0) { // Wake up
      editandoHora = true;
      editandoReloj = false;
      editandoWakeUp = true;
      editandoCampo = 0;
      mostrarEdicionHora();
    }
    else if (settingOpcion == 1) { // Sleep at
      editandoHora = true;
      editandoReloj = false;
      editandoWakeUp = false;
      editandoCampo = 0;
      mostrarEdicionHora();
    }
    else if (settingOpcion == 2) { // Nap
      enNapMenu = true;
      napMenuOpcion = 0;
      napEditando = false;
      napEditandoCampo = 0;
      napEditandoStart = true;
      mostrarNapMenu();
    }
    else if (settingOpcion == 3) { // Clock setup
      editandoHora = true;
      editandoReloj = true;
      editandoCampo = 0;
      // Usamos las variables temporales en lugar de horaDespertar/minutosDespertar
      horaTemporalReloj = now.hour();
      minutosTemporalReloj = now.minute();
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
  } else {
    // Menú principal
    if (opcionSeleccionada == 0) { // Sleep mode
      bool estadoAnterior = botonPresionado;
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
        mostrarPantallaPrincipal();
      } else {
        // Sleep Mode DESACTIVADO
        if (modoSiestaActivo || enVerdeExtraSiesta) {
          modoSiestaActivo = false;
          enVerdeExtraSiesta = false;
          Serial.println(F("Sleep mode DESACTIVADO - Modo siesta cancelado"));
        } else {
          Serial.println(F("Sleep mode DESACTIVADO"));
        }
        ultimoMinutoImpreso = -1;
        mostrarPantallaPrincipal();
      }
    }
    else if (opcionSeleccionada == 1) { // 12-hour sleep
      doceHorasSleep = !doceHorasSleep;
      if (doceHorasSleep) {
        inicioDoceHoras = millis();
        Serial.print(F("12-hour sleep ACTIVADO - Inicio registrado a las "));
        Serial.print(millis());
        Serial.println(F(" ms"));
      } else {
        Serial.println(F("12-hour sleep DESACTIVADO manualmente"));
      }
      mostrarPantallaPrincipal();
    }
    else if (opcionSeleccionada == 2) { // Force color
      enForceColorMenu = true;
      forceColorOpcion = 0;
      mostrarForceColorMenu();
    }
    else if (opcionSeleccionada == 3) { // Game mode (NUEVO)
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

void procesarBotonSelectNapMenu() {
  if (napEditando) {
    // Estamos editando los horarios
    if (napEditandoCampo == 0) {
      napEditandoCampo = 1; // Cambiar a editar minutos
    } else {
      // Último campo (minutos), salir del modo edición
      napEditando = false;
      napEditandoCampo = 0;
      
      // Guardar en EEPROM si estamos editando Start o Duration
      guardarNapConfigEnEEPROM();
      
      // Regresar a la opción que estábamos editando
      if (napEditandoStart) {
        napMenuOpcion = 1; // Volver a Start
      } else {
        napMenuOpcion = 2; // Volver a Duration
      }
    }
  } else {
    // No estamos editando, estamos en el menú principal
    switch (napMenuOpcion) {
      case 0: // Nap toggle
        napActivo = !napActivo;
        // Guardar estado en EEPROM inmediatamente
        guardarNapConfigEnEEPROM();
        Serial.print(F("Nap "));
        Serial.println(napActivo ? F("ACTIVADO") : F("DESACTIVADO"));
        break;
      case 1: // Start - Entrar a editar hora de inicio
        napEditando = true;
        napEditandoStart = true;
        napEditandoCampo = 0; // Comenzar con la hora
        break;
      case 2: // Duration - Entrar a editar duración
        napEditando = true;
        napEditandoStart = false;
        napEditandoCampo = 0; // Comenzar con las horas
        break;
      case 3: // Back - Volver a Settings
        enNapMenu = false;
        enMenuSettings = true;
        mostrarMenuSettings();
        return; // Salir para que no se muestre el menú Nap
    }
  }
  mostrarNapMenu();
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
    // Navegación en Force color menu
    forceColorOpcion = (forceColorOpcion + 1) % totalForceColorOpciones;
    mostrarForceColorMenu();
  }
  else if (editandoHora) {
    if (editandoCampo == 0) { // Editando hora
      if (editandoReloj) {
        // Para el reloj: incrementar hora normalmente usando variables temporales
        horaTemporalReloj = (horaTemporalReloj + 1) % 24;
      } else if (editandoWakeUp) {
        horaDespertar = (horaDespertar + 1) % 24;
      } else {
        horaDormir = (horaDormir + 1) % 24;
      }
    } else { // Editando minutos
      if (editandoReloj) {
        // Para el reloj: incrementar minuto a minuto usando variables temporales
        minutosTemporalReloj = (minutosTemporalReloj + 1) % 60;
      } else if (editandoWakeUp) {
        // Para wake up: incrementar de 5 en 5 minutos
        minutosDespertar = (minutosDespertar + 5) % 60;
      } else {
        // Para sleep: incrementar de 5 en 5 minutos
        minutosDormir = (minutosDormir + 5) % 60;
      }
    }
    mostrarEdicionHora();
  }
  else if (enMenuSettings) {
    uint8_t nuevaOpcion = (settingOpcion + 1) % totalSettingOpciones;
    settingOpcion = nuevaOpcion;
    
    // Calcular nuevo scroll offset basado en pares de opciones
    settingScrollOffset = (settingOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
    
    mostrarMenuSettings();
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
    // Estamos editando los horarios
    if (napEditandoStart) {
      // Editando START
      if (napEditandoCampo == 0) {
        // Editando hora de inicio
        napHoraInicio = (napHoraInicio + 1) % 24;
      } else {
        // Editando minuto de inicio (de 5 en 5 minutos)
        napMinutoInicio = (napMinutoInicio + 5) % 60;
      }
    } else {
      // Editando DURATION
      if (napEditandoCampo == 0) {
        // Editando horas de duración (0-3)
        napDuracionHoras = (napDuracionHoras + 1) % 4;
      } else {
        // Editando minutos de duración (de 5 en 5 minutos)
        napDuracionMinutos = (napDuracionMinutos + 5) % 60;
      }
    }
  } else {
    // No estamos editando horarios, cambiar entre opciones del menú
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
  }

  // 1. FORCE COLOR tiene prioridad máxima (expandido a 8 opciones)
  if (forceColor > 0) {
    switch(forceColor) {
      case 1: // Green high
        setLuz(ledVerde, intensidadVerdeMax);
        break;
      case 2: // Green low
        setLuz(ledVerde, intensidadVerdeTenue);
        break;
      case 3: // Yellow high
        setLuz(ledAmarillo, intensidadAmarilloMax);
        break;
      case 4: // Yellow low
        setLuz(ledAmarillo, intensidadAmarilloTenue);
        break;
      case 5: // Red high
        setLuz(ledRojo, intensidadRojoMax);
        break;
      case 6: // Red low
        setLuz(ledRojo, intensidadRojoTenue);
        break;
    }
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
  int tiempoDespertar = horaDespertar * 60 + minutosDespertar;
  int tiempoDormir = horaDormir * 60 + minutosDormir;

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
  else if (tiempoActual < tiempoDormir) {
    setLuz(ledAmarillo, intensidadAmarilloMax);
  }
  // Después de 7:30 PM (rojo máximo)
  else {
    setLuz(ledRojo, intensidadRojoMax);
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
  analogWrite(pin, intensidad);
  if (pin != ledVerde) digitalWrite(ledVerde, LOW);
  if (pin != ledAmarillo) digitalWrite(ledAmarillo, LOW);
  if (pin != ledRojo) digitalWrite(ledRojo, LOW);
}

void mostrarPantallaPrincipal() {
  oled.clear();
  
  // Primera línea: Hora y versión
  oled.print(" ");
  oled.print(now.hour());
  oled.print(":");
  if (now.minute() < 10) oled.print("0");
  oled.print(now.minute());
  oled.println("              iTronnix");
  
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
        oled.print(getForceColorTexto(forceColor));
        oled.println("]");
        break;
        
      case 3: // Game mode (NUEVO)
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
      case 0: // Wake up
        oled.print("Wake up ");
        oled.print(horaDespertar);
        oled.print(":");
        if (minutosDespertar < 10) oled.print("0");
        oled.print(minutosDespertar);
        break;
        
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
  // Título
  oled.println(" NAP SETUP");
  
  if (napEditando) {
    // MODO EDICIÓN
    if (napEditandoStart) {
      // Editando START
      oled.print(" Nap [");
      oled.print(napActivo ? "On" : "Off");
      oled.println("]");
      
      oled.print(" Start: ");
      if (napEditandoCampo == 0) {
        oled.print(">");
      } else {
        oled.print(" ");
      }
      oled.print(napHoraInicio);
      oled.print(":");
      
      if (napEditandoCampo == 1) {
        oled.print(">");
      } else {
        oled.print(" ");
      }
      if (napMinutoInicio < 10) oled.print("0");
      oled.print(napMinutoInicio);
      oled.println();
      
      oled.println();
      oled.print(" Change            Apply");
    } else {
      // Editando DURATION
      oled.print(" Duration: ");
      if (napEditandoCampo == 0) {
        oled.print(">");
      } else {
        oled.print(" ");
      }
      oled.print(napDuracionHoras);
      oled.print("h ");
      
      if (napEditandoCampo == 1) {
        oled.print(">");
      } else {
        oled.print(" ");
      }
      if (napDuracionMinutos < 10) oled.print("0");
      oled.print(napDuracionMinutos);
      oled.print("m");
      oled.println();
      
      oled.println(" Back");
      oled.println();
      oled.print(" Change            Apply");
    }
    
  } else {
    // MODO NORMAL DEL MENÚ
    // Calcular qué opciones mostrar (siempre 2 opciones por pantalla)
    uint8_t opcionInicial = (napMenuOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
    uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, napTotalOpciones);
    
    // Mostrar las opciones correspondientes
    for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
      if (i == napMenuOpcion) {
        oled.print(">");
      } else {
        oled.print(" ");
      }
      
      switch(i) {
        case 0: // Nap toggle
          oled.print( "Nap [");
          oled.print(napActivo ? "On" : "Off");
          oled.println("]");
          break;
          
        case 1: // Start
          oled.print(" Start: ");
          oled.print(napHoraInicio);
          oled.print(":");
          if (napMinutoInicio < 10) oled.print("0");
          oled.print(napMinutoInicio);
          oled.println();
          break;
          
        case 2: // Duration
          oled.print(" Duration: ");
          oled.print(napDuracionHoras);
          oled.print("h ");
          if (napDuracionMinutos < 10) oled.print("0");
          oled.print(napDuracionMinutos);
          oled.print("m");
          oled.println();
          break;
          
        case 3: // Back
          oled.println(" Back");
          break;
      }
    }
    
    // Si hay menos opciones que el máximo, mostrar líneas vacías
    for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
      oled.println();
    }
    
    // Mostrar instrucciones según la opción seleccionada
    oled.println();
    oled.print(" Move ");
    oled.print(napMenuOpcion + 1);
    oled.print("/");
    oled.print(napTotalOpciones);
    
    if (napMenuOpcion == 0) {
      oled.print("          Toggle");
    } else if (napMenuOpcion == 1 || napMenuOpcion == 2) {
      oled.print("          Edit");
    } else if (napMenuOpcion == 3) {
      oled.print("          Select");
    }
  }
}

void mostrarMenuAbout() {
  oled.clear();
  // Título
  oled.println(" ABOUT");
  
  // Versión
  oled.print(" Version: ");
  oled.println("1.35");

  // Website
  oled.println(" www.iTronnix.com");
  
  // Back
  oled.println(".                              Back");
}

void mostrarForceColorMenu() {
  oled.clear();
  // Título
  oled.println(" FORCE COLOR");
  
  // Calcular qué opciones mostrar (siempre 2 opciones por pantalla)
  uint8_t opcionInicial = (forceColorOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  uint8_t opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, totalForceColorOpciones);
  
  // Mostrar las opciones correspondientes
  for (uint8_t i = opcionInicial; i < opcionFinal; i++) {
    if (i == forceColorOpcion) {
      oled.print(">");
    } else {
      oled.print(" ");
    }
    
    switch(i) {
      case 0: // Green high
        oled.println("Green high");
        break;
        
      case 1: // Green low
        oled.println("Green low");
        break;
        
      case 2: // Yellow high
        oled.println("Yellow high");
        break;
        
      case 3: // Yellow low
        oled.println("Yellow low");
        break;
        
      case 4: // Red high
        oled.println("Red high");
        break;
        
      case 5: // Red low
        oled.println("Red low");
        break;
        
      case 6: // Off
        oled.println("Off");
        break;
        
      case 7: // Back
        oled.println("Back");
        break;
    }
  }
  
  // Si hay menos opciones que el máximo, mostrar líneas vacías
  for (uint8_t i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }
  
  // Navegación con indicador de posición
  oled.print(" Move ");
  oled.print(forceColorOpcion + 1);
  oled.print("/");
  oled.print(totalForceColorOpciones);
  oled.println("          Apply");
}

void mostrarEdicionHora() {
  oled.clear();
  
  // Título diferente según lo que estemos editando
  if (editandoReloj) {
    oled.println(" CLOCK SETUP");
  } else if (editandoWakeUp) {
    oled.println(" WAKE UP SETUP");
  } else {
    oled.println(" SLEEP SETUP");
  }
  
  // Mostrar hora y minutos con cursor
  if (editandoCampo == 0) {
    oled.print(">");
    if (editandoReloj) {
      oled.print(horaTemporalReloj);
    } else if (editandoWakeUp) {
      oled.print(horaDespertar);
    } else {
      oled.print(horaDormir);
    }
  } else {
    oled.print(" ");
    if (editandoReloj) {
      oled.print(horaTemporalReloj);
    } else if (editandoWakeUp) {
      oled.print(horaDespertar);
    } else {
      oled.print(horaDormir);
    }
  }
  
  oled.print(":");
  
  if (editandoCampo == 1) {
    oled.print(">");
    if (editandoReloj) {
      if (minutosTemporalReloj < 10) oled.print("0");
      oled.print(minutosTemporalReloj);
    } else if (editandoWakeUp) {
      if (minutosDespertar < 10) oled.print("0");
      oled.print(minutosDespertar);
    } else {
      if (minutosDormir < 10) oled.print("0");
      oled.print(minutosDormir);
    }
  } else {
    if (editandoReloj) {
      if (minutosTemporalReloj < 10) oled.print("0");
      oled.print(minutosTemporalReloj);
    } else if (editandoWakeUp) {
      if (minutosDespertar < 10) oled.print("0");
      oled.print(minutosDespertar);
    } else {
      if (minutosDormir < 10) oled.print("0");
      oled.print(minutosDormir);
    }
  }
  
  oled.println();
  oled.println();
  
  // Instrucciones en la parte inferior
  if (editandoCampo == 0) {
    oled.println(" Change             Apply");
  } else {
    oled.println(" Change              Apply");
  }
}