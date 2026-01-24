String version = "1.25";  // Version actualizada con corrección 12-hour sleep

#include <Wire.h>
#include <RTClib.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// Variables globales
String status = "--";
int ultimoMinutoImpreso = -1;
unsigned long ultimaInteraccion = 0;
const unsigned long TIEMPO_PANTALLA_ACTIVA = 30000; // 30 segundos
bool pantallaApagada = false;
unsigned long tiempoEncendidoPantalla = 0; // Para controlar el bloqueo temporal
const unsigned long TIEMPO_BLOQUEO_BOTONES = 300; // 300ms de bloqueo después de encender

// Horarios (editables)
int horaDespertar = 7;
int minutosDespertar = 30;
int horaDormir = 19;
int minutosDormir = 30;

// Variables temporales para edición de reloj
int horaTemporalReloj = 0;
int minutosTemporalReloj = 0;

// Variables para 12-hour sleep y Force color
bool doceHorasSleep = false;
unsigned long inicioDoceHoras = 0;
const unsigned long DOCE_HORAS_MS = 12UL * 60UL * 60UL * 1000UL; // CORREGIDO: 12 horas en milisegundos

int forceColor = 0; // 0=Off, 1=Green, 2=Yellow, 3=Red
const char* forceColorTexto[] = {"Off", "G", "Y", "R"};

// Variables para Game mode (nuevo)
bool gameMode = false;
unsigned long inicioGameMode = 0;
unsigned long ultimoCambioGameMode = 0;
int estadoGameMode = 0; // 0=Green, 1=Yellow, 2=Red

// ==================== CONFIGURACIÓN DE MODO JUEGO ====================
// Puedes ajustar estos tiempos según necesites
const unsigned long TIEMPO_VERDE_JUEGO = 20000;   // 30 segundos en verde
const unsigned long TIEMPO_AMARILLO_JUEGO = 3000; // 5 segundos en amarillo
const unsigned long TIEMPO_ROJO_JUEGO = 10000;    // 15 segundos en rojo
// =====================================================================

// Pines
const int ledVerde = 9;
const int ledAmarillo = 10;
const int ledRojo = 11;
const int moveButton = 2;
const int selectButton = 3;

// Estados
bool botonPresionado = false;
bool esSiesta = false;
bool enMenuSettings = false;
bool editandoHora = false;
bool editandoReloj = false;          // Para diferenciar si estamos editando el reloj
int opcionSeleccionada = 0;          // 0=Sleep mode, 1=12-hour sleep, 2=Force color, 3=Game mode, 4=Settings
int settingOpcion = 0;               // 0=Wake up, 1=Sleep at, 2=Clock setup, 3=Back
int editandoCampo = 0;               // 0=hora, 1=minutos
bool editandoWakeUp = true;          // true=Wake up, false=Sleep at
const int totalOpciones = 5;         // Cambiado de 4 a 5 (agregamos Game mode)
const int totalSettingOpciones = 4;
int settingScrollOffset = 0;         // Para controlar qué opciones mostrar en pantalla
const int MAX_OPCIONES_POR_PANTALLA = 2; // Máximo de opciones visibles a la vez

// Nueva variable para Force color menu
bool enForceColorMenu = false;
int forceColorOpcion = 0;            // 0=Green, 1=Yellow, 2=Red, 3=Off, 4=Back
const int totalForceColorOpciones = 5;

// RTC
RTC_DS3231 rtc;
DateTime now;
DateTime horaInicioSiesta;
const int duracionSiesta = 90;

// Intensidades
const int intensidadRojoMax = 100;
const int intensidadAmarilloMax = 255;
const int intensidadVerdeMax = 100;
const int intensidadRojoTenue = 3;
const int intensidadAmarilloTenue = 10;
const int intensidadVerdeTenue = 3;

// OLED
SSD1306AsciiWire oled;
#define OLED_ADDRESS 0x3C

void setup() {
  Serial.begin(9600);

  if (!rtc.begin()) {
    Serial.println("Error: No se encuentra el módulo RTC.");
    while (1);
  }

  // Verificar si el RTC perdió la hora (fecha muy antigua)
  now = rtc.now();
  if (now.year() < 2024) {
    Serial.println("ADVERTENCIA: RTC parece tener hora incorrecta. Usa Clock setup para ajustar.");
  }

  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  // rtc.adjust(DateTime(2025, 1, 12, 19, 15, 0)); // Formato: (año, mes, día, hora, minuto, segundo)

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
  
  Serial.println("=== SISTEMA INICIADO - VERSION 1.25 (12-HOUR SLEEP FIX) ===");
  Serial.print("12-hour sleep configurado a: ");
  Serial.print(DOCE_HORAS_MS / 1000 / 60 / 60);
  Serial.println(" horas");
  Serial.println("Configuración Game mode:");
  Serial.print("  Verde: "); Serial.print(TIEMPO_VERDE_JUEGO/1000); Serial.println(" segundos");
  Serial.print("  Amarillo: "); Serial.print(TIEMPO_AMARILLO_JUEGO/1000); Serial.println(" segundos");
  Serial.print("  Rojo: "); Serial.print(TIEMPO_ROJO_JUEGO/1000); Serial.println(" segundos");
}

void loop() {
  now = rtc.now();
  int hora = now.hour();
  int minuto = now.minute();

  if (minuto != ultimoMinutoImpreso) {
    Serial.print("Hora actual: ");
    Serial.print(hora);
    Serial.print(":");
    if (minuto < 10) Serial.print("0");
    Serial.println(minuto);

    ultimoMinutoImpreso = minuto;

    if (!botonPresionado && !enMenuSettings && !editandoHora && !enForceColorMenu && !pantallaApagada) {
      mostrarPantallaPrincipal();
    }
  }

  manejarBotones();
  manejarLuces();

  // Control de timeout de pantalla
  if (!pantallaApagada && (millis() - ultimaInteraccion > TIEMPO_PANTALLA_ACTIVA)) {
    // Apagar pantalla después de 30 segundos de inactividad - sin condiciones adicionales
    oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
    pantallaApagada = true;
  }

  delay(100);
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
  
  if (!editandoHora && !enMenuSettings && !enForceColorMenu) {
    mostrarPantallaPrincipal();
  }
}

void procesarBotonSelect() {
  if (enForceColorMenu) {
    // Menú Force color
    if (forceColorOpcion == 0) { // Green
      forceColor = 1;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 1) { // Yellow
      forceColor = 2;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 2) { // Red
      forceColor = 3;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 3) { // Off
      forceColor = 0;
      enForceColorMenu = false;
      mostrarPantallaPrincipal();
    }
    else if (forceColorOpcion == 4) { // Back
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
        Serial.print("RTC ajustado a: ");
        Serial.print(horaTemporalReloj);
        Serial.print(":");
        if (minutosTemporalReloj < 10) Serial.print("0");
        Serial.println(minutosTemporalReloj);
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
    else if (settingOpcion == 2) { // Clock setup
      editandoHora = true;
      editandoReloj = true;
      editandoCampo = 0;
      // Usamos las variables temporales en lugar de horaDespertar/minutosDespertar
      horaTemporalReloj = now.hour();
      minutosTemporalReloj = now.minute();
      mostrarEdicionHora();
    }
    else if (settingOpcion == 3) { // Back
      enMenuSettings = false;
      opcionSeleccionada = 0;
      mostrarPantallaPrincipal();
    }
  } else {
    // Menú principal
    if (opcionSeleccionada == 0) { // Sleep mode
      botonPresionado = !botonPresionado;
      if (botonPresionado) {
        setLuz(ledRojo, intensidadRojoTenue);
        mostrarPantallaPrincipal(); // Mostrar inmediatamente el estado [On]
        Serial.println("Sleep mode ACTIVADO");
      } else {
        ultimoMinutoImpreso = -1;
        mostrarPantallaPrincipal();
        Serial.println("Sleep mode DESACTIVADO");
      }
    }
    else if (opcionSeleccionada == 1) { // 12-hour sleep
      doceHorasSleep = !doceHorasSleep;
      if (doceHorasSleep) {
        inicioDoceHoras = millis();
        Serial.print("12-hour sleep ACTIVADO - Inicio registrado a las ");
        Serial.print(millis());
        Serial.println(" ms");
      } else {
        Serial.println("12-hour sleep DESACTIVADO manualmente");
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
        Serial.println("Game mode ACTIVADO - Comienza en VERDE");
      } else {
        Serial.println("Game mode DESACTIVADO");
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

void procesarBotonMove() {
  if (enForceColorMenu) {
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
    int nuevaOpcion = (settingOpcion + 1) % totalSettingOpciones;
    settingOpcion = nuevaOpcion;
    
    // Calcular nuevo scroll offset basado en pares de opciones
    settingScrollOffset = (settingOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
    
    mostrarMenuSettings();
  } else {
    // Menú principal con scroll por pares
    int nuevaOpcion = (opcionSeleccionada + 1) % totalOpciones;
    opcionSeleccionada = nuevaOpcion;
    
    // Calcular scroll offset para menú principal (pares de 2)
    static int mainScrollOffset = 0;
    mainScrollOffset = (opcionSeleccionada / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
    
    mostrarPantallaPrincipal();
  }
  ultimaInteraccion = millis();
}

void manejarLuces() {
  // Verificar si es medianoche (00:00) para forzar salida del modo dormir
  if (now.hour() == 0 && now.minute() == 0) {
    botonPresionado = false;
  }

  // Verificar si 12-hour sleep ha terminado
  if (doceHorasSleep && (millis() - inicioDoceHoras >= DOCE_HORAS_MS)) {
    doceHorasSleep = false;
    Serial.print("12-hour sleep COMPLETADO - Desactivado automáticamente a las ");
    Serial.println(millis());
  }

  // 1. FORCE COLOR tiene prioridad máxima
  if (forceColor > 0) {
    switch(forceColor) {
      case 1: // Green
        setLuz(ledVerde, intensidadVerdeMax);
        break;
      case 2: // Yellow
        setLuz(ledAmarillo, intensidadAmarilloMax);
        break;
      case 3: // Red
        setLuz(ledRojo, intensidadRojoMax);
        break;
    }
    return;
  }

  // 2. SLEEP MODE (rojo tenue siempre)
  if (botonPresionado) {
    setLuz(ledRojo, intensidadRojoTenue);
    return;
  }

  // 3. 12-HOUR SLEEP (rojo tenue por 12 horas)
  if (doceHorasSleep) {
    setLuz(ledRojo, intensidadRojoTenue);
    return;
  }

  // 4. GAME MODE (nuevo - semáforo de juego)
  if (gameMode) {
    manejarGameMode();
    return;
  }

  // 5. LÓGICA NORMAL DE HORARIOS
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
        Serial.println("Game mode: VERDE -> AMARILLO");
      } else {
        setLuz(ledVerde, intensidadVerdeMax);
      }
      break;
      
    case 1: // AMARILLO
      if (tiempoTranscurrido >= TIEMPO_AMARILLO_JUEGO) {
        // Cambiar a ROJO
        estadoGameMode = 2;
        ultimoCambioGameMode = tiempoActual;
        Serial.println("Game mode: AMARILLO -> ROJO");
      } else {
        setLuz(ledAmarillo, intensidadAmarilloMax);
      }
      break;
      
    case 2: // ROJO
      if (tiempoTranscurrido >= TIEMPO_ROJO_JUEGO) {
        // Volver a VERDE (ciclo completo)
        estadoGameMode = 0;
        ultimoCambioGameMode = tiempoActual;
        Serial.println("Game mode: ROJO -> VERDE (ciclo completo)");
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
  oled.println("                iTronix");
  
  // Calcular qué opciones mostrar (siempre 2 opciones por pantalla)
  int opcionInicial = (opcionSeleccionada / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  int opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, totalOpciones);
  
  // Mostrar las opciones correspondientes
  for (int i = opcionInicial; i < opcionFinal; i++) {
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
        oled.print(forceColorTexto[forceColor]);
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
  for (int i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
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
  int opcionInicial = settingScrollOffset;
  int opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, totalSettingOpciones);
  
  // Mostrar las opciones correspondientes
  for (int i = opcionInicial; i < opcionFinal; i++) {
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
        
      case 2: // Clock setup
        oled.print("Clock setup");
        break;
        
      case 3: // Back
        oled.print("Back");
        break;
    }
    
    oled.println();
  }
  
  // Si hay menos opciones que el máximo, mostrar líneas vacías
  for (int i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
    oled.println();
  }
  
  // Navegación con indicador de posición
  oled.print(" Move ");
  oled.print(settingOpcion + 1);
  oled.print("/");
  oled.print(totalSettingOpciones);
  oled.println("          Select");
}

void mostrarForceColorMenu() {
  oled.clear();
  // Título
  oled.println(" FORCE COLOR");
  
  // Calcular qué opciones mostrar (siempre 2 opciones por pantalla)
  int opcionInicial = (forceColorOpcion / MAX_OPCIONES_POR_PANTALLA) * MAX_OPCIONES_POR_PANTALLA;
  int opcionFinal = min(opcionInicial + MAX_OPCIONES_POR_PANTALLA, totalForceColorOpciones);
  
  // Mostrar las opciones correspondientes
  for (int i = opcionInicial; i < opcionFinal; i++) {
    if (i == forceColorOpcion) {
      oled.print(">");
    } else {
      oled.print(" ");
    }
    
    switch(i) {
      case 0: // Green
        oled.println("Green");
        break;
        
      case 1: // Yellow
        oled.println("Yellow");
        break;
        
      case 2: // Red
        oled.println("Red");
        break;
        
      case 3: // Off
        oled.println("Off");
        break;
        
      case 4: // Back
        oled.println("Back");
        break;
    }
  }
  
  // Si hay menos opciones que el máximo, mostrar líneas vacías
  for (int i = opcionFinal - opcionInicial; i < MAX_OPCIONES_POR_PANTALLA; i++) {
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