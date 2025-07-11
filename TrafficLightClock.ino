String version = "22.0.20";  // Version actualizada

#include <Wire.h>
#include <RTClib.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// Variables globales
String status = "--";
int ultimoMinutoImpreso = -1;
unsigned long ultimaInteraccion = 0;
const unsigned long TIEMPO_PANTALLA_ACTIVA = 5000; // 5 segundos
bool pantallaApagada = false;

// Horarios (editables)
int horaDespertar = 7;
int minutosDespertar = 30;
int horaDormir = 19;
int minutosDormir = 30;

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
int opcionSeleccionada = 0;          // 0=Sleep mode, 1=Settings
int settingOpcion = 0;               // 0=Wake up, 1=Sleep at, 2=Back
int editandoCampo = 0;               // 0=hora, 1=minutos
bool editandoWakeUp = true;          // true=Wake up, false=Sleep at
const int totalOpciones = 2;
const int totalSettingOpciones = 3;

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

    if (!botonPresionado && !enMenuSettings && !editandoHora && !pantallaApagada) {
      mostrarPantallaPrincipal();
    }
  }

  manejarBotones();
  manejarLuces();

  // Control de timeout de pantalla
  if (!pantallaApagada && (millis() - ultimaInteraccion > TIEMPO_PANTALLA_ACTIVA)) {
    if (botonPresionado || modoTenueActivo()) {
      oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
      pantallaApagada = true;
    }
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

  // Si la pantalla está apagada, cualquier botón solo la enciende
  if (pantallaApagada) {
    if ((lastSelect == HIGH && currentSelect == LOW) || 
        (lastMove == HIGH && currentMove == LOW)) {
      encenderPantalla();
      return; // Salir sin procesar otras acciones
    }
  }
  
  // Procesar botones normalmente si la pantalla está encendida
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
  
  if (!editandoHora && !enMenuSettings) {
    mostrarPantallaPrincipal();
  }
}

void procesarBotonSelect() {
  if (editandoHora) {
    if (editandoCampo == 0) {
      editandoCampo = 1; // Cambiar a editar minutos
    } else {
      // Guardar y salir al presionar Select en minutos
      editandoHora = false;
      editandoCampo = 0;
      enMenuSettings = true;
      mostrarMenuSettings();
      return;
    }
    mostrarEdicionHora();
  }
  else if (enMenuSettings) {
    if (settingOpcion == 0) { // Wake up
      editandoHora = true;
      editandoWakeUp = true;
      editandoCampo = 0;
      mostrarEdicionHora();
    }
    else if (settingOpcion == 1) { // Sleep at
      editandoHora = true;
      editandoWakeUp = false;
      editandoCampo = 0;
      mostrarEdicionHora();
    }
    else if (settingOpcion == 2) { // Back
      enMenuSettings = false;
      opcionSeleccionada = 0;
      mostrarPantallaPrincipal();
    }
  } else {
    if (opcionSeleccionada == 0) { // Sleep mode
      botonPresionado = !botonPresionado;
      if (botonPresionado) {
        setLuz(ledRojo, intensidadRojoTenue);
        mostrarPantallaPrincipal(); // Mostrar inmediatamente el estado [On]
      } else {
        ultimoMinutoImpreso = -1;
        mostrarPantallaPrincipal();
      }
    }
    else if (opcionSeleccionada == 1) { // Settings
      enMenuSettings = true;
      settingOpcion = 0;
      mostrarMenuSettings();
    }
  }
  ultimaInteraccion = millis();
}

void procesarBotonMove() {
  if (editandoHora) {
    if (editandoCampo == 0) { // Editando hora
      if (editandoWakeUp) {
        horaDespertar = (horaDespertar + 1) % 24;
      } else {
        horaDormir = (horaDormir + 1) % 24;
      }
    } else { // Editando minutos
      if (editandoWakeUp) {
        minutosDespertar = (minutosDespertar + 5) % 60;
      } else {
        minutosDormir = (minutosDormir + 5) % 60;
      }
    }
    mostrarEdicionHora();
  }
  else if (enMenuSettings) {
    settingOpcion = (settingOpcion + 1) % totalSettingOpciones;
    mostrarMenuSettings();
  } else {
    opcionSeleccionada = (opcionSeleccionada + 1) % totalOpciones;
    mostrarPantallaPrincipal();
  }
  ultimaInteraccion = millis();
}

void manejarLuces() {
  // Verificar si es medianoche (00:00) para forzar salida del modo dormir
  if (now.hour() == 0 && now.minute() == 0) {
    botonPresionado = false;
  }

  if (botonPresionado) {
    setLuz(ledRojo, intensidadRojoTenue);
    return;
  }

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
  oled.print("       v");
  oled.println(version);
  
  // Opciones con estado [On]/[Off] actualizado inmediatamente
  if (opcionSeleccionada == 0) {
    oled.print(">Sleep mode [");
    oled.print(botonPresionado ? "On" : "Off");
    oled.println("]");
    oled.println(" Settings");
  } else {
    oled.print(" Sleep mode [");
    oled.print(botonPresionado ? "On" : "Off");
    oled.println("]");
    oled.println(">Settings");
  }
  
  // Navegación con indicador de posición
  oled.print("Move ");
  oled.print(opcionSeleccionada + 1);
  oled.print("/");
  oled.print(totalOpciones);
  oled.println("    Select");
}

void mostrarMenuSettings() {
  oled.clear();
  // Título sin versión
  oled.println("Settings");
  
  // Opciones
  if (settingOpcion == 0) {
    oled.print(">Wake up ");
    oled.print(horaDespertar);
    oled.print(":");
    if (minutosDespertar < 10) oled.print("0");
    oled.println(minutosDespertar);
    oled.print(" Sleep ");
    oled.print(horaDormir);
    oled.print(":");
    if (minutosDormir < 10) oled.print("0");
    oled.println(minutosDormir);
  }
  else if (settingOpcion == 1) {
    oled.print(" Wake up ");
    oled.print(horaDespertar);
    oled.print(":");
    if (minutosDespertar < 10) oled.print("0");
    oled.println(minutosDespertar);
    oled.print(">Sleep ");
    oled.print(horaDormir);
    oled.print(":");
    if (minutosDormir < 10) oled.print("0");
    oled.println(minutosDormir);
  }
  else if (settingOpcion == 2) {
    oled.println(">Back");
    oled.println();
  }
  
  // Navegación con indicador de posición
  oled.print("Move ");
  oled.print(settingOpcion + 1);
  oled.print("/");
  oled.print(totalSettingOpciones);
  oled.println("    Select");
}

void mostrarEdicionHora() {
  oled.clear();
  oled.println(editandoWakeUp ? "Wake up setup" : "Sleep setup");
  
  if (editandoCampo == 0) {
    oled.print(">");
    oled.print(editandoWakeUp ? horaDespertar : horaDormir);
  } else {
    oled.print(" ");
    oled.print(editandoWakeUp ? horaDespertar : horaDormir);
  }
  
  oled.print(":");
  
  if (editandoCampo == 1) {
    oled.print(">");
    if (editandoWakeUp) {
      if (minutosDespertar < 10) oled.print("0");
      oled.print(minutosDespertar);
    } else {
      if (minutosDormir < 10) oled.print("0");
      oled.print(minutosDormir);
    }
  } else {
    if (editandoWakeUp) {
      if (minutosDespertar < 10) oled.print("0");
      oled.print(minutosDespertar);
    } else {
      if (minutosDormir < 10) oled.print("0");
      oled.print(minutosDormir);
    }
  }
  
  oled.println();
  oled.println();
  oled.println(editandoCampo == 0 ? "Change    Apply" : "Change    Save");
}