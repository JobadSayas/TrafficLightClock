String version = "22.0.2";  // Versión actualizada

#include <Wire.h>
#include <RTClib.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// Variables globales
String status = "--";
int ultimoMinutoImpreso = -1;

// Horarios
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
int opcionSeleccionada = 0;          // 0=Sleep mode, 1=Settings
int settingOpcion = 0;               // 0=Wake up, 1=Sleep at, 2=Exit
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

  // Configura el RTC con la hora actual directamente
  // rtc.adjust(DateTime(2025, 2, 16, 7, 30, 0)); // Año, Mes, Día, Hora, Minuto, Segundo
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  pinMode(ledVerde, OUTPUT);
  pinMode(ledAmarillo, OUTPUT);
  pinMode(ledRojo, OUTPUT);
  pinMode(selectButton, INPUT_PULLUP);
  pinMode(moveButton, INPUT_PULLUP);

  Wire.begin();
  oled.begin(&Adafruit128x64, OLED_ADDRESS);
  oled.setFont(Arial_14);
  mostrarPantallaPrincipal();
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

    if (!botonPresionado && !enMenuSettings) {
      mostrarPantallaPrincipal();
    }
  }

  manejarBotones();
  manejarLuces();

  delay(100);
}

void manejarBotones() {
  static int lastSelect = HIGH;
  static int lastMove = HIGH;
  int currentSelect = digitalRead(selectButton);
  int currentMove = digitalRead(moveButton);

  // Botón Select
  if (lastSelect == HIGH && currentSelect == LOW) {
    if (enMenuSettings) {
      if (settingOpcion == 2) { // Exit
        enMenuSettings = false;
      }
    } else {
      // Acción para Sleep mode (toggle)
      if (opcionSeleccionada == 0) {
        botonPresionado = !botonPresionado;
        if (botonPresionado) {
          oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
          setLuz(ledRojo, intensidadRojoTenue);
        } else {
          oled.ssd1306WriteCmd(SSD1306_DISPLAYON);
          ultimoMinutoImpreso = -1; // Forzar actualización
        }
      }
      // Entrar a Settings
      else if (opcionSeleccionada == 1) {
        enMenuSettings = true;
        settingOpcion = 0;
      }
    }
    
    if (enMenuSettings) {
      mostrarMenuSettings();
    } else {
      mostrarPantallaPrincipal();
    }
  }
  lastSelect = currentSelect;

  // Botón Move
  if (lastMove == HIGH && currentMove == LOW) {
    if (enMenuSettings) {
      settingOpcion = (settingOpcion + 1) % totalSettingOpciones;
      mostrarMenuSettings();
    } else {
      opcionSeleccionada = (opcionSeleccionada + 1) % totalOpciones;
      mostrarPantallaPrincipal();
    }
  }
  lastMove = currentMove;
}

void manejarLuces() {
  if (botonPresionado) {
    setLuz(ledRojo, intensidadRojoTenue);
    return;
  }

  int tiempoActual = now.hour() * 60 + now.minute();
  int tiempoDespertar = horaDespertar * 60 + minutosDespertar;
  int tiempoDormir = horaDormir * 60 + minutosDormir;

  if (tiempoActual < tiempoDespertar - 15) {
    oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
    setLuz(ledRojo, intensidadRojoTenue);
  } 
  else if (tiempoActual < tiempoDespertar) {
    oled.ssd1306WriteCmd(SSD1306_DISPLAYON);
    setLuz(ledAmarillo, intensidadAmarilloTenue);
  }
  else if (tiempoActual < tiempoDespertar + 60) {
    setLuz(ledVerde, intensidadVerdeTenue);
  }
  else if (tiempoActual < tiempoDormir - 30) {
    setLuz(ledVerde, intensidadVerdeMax);
  }
  else if (tiempoActual < tiempoDormir) {
    setLuz(ledAmarillo, intensidadAmarilloMax);
  }
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
  oled.print(" ");
  oled.print(now.hour());
  oled.print(":");
  if (now.minute() < 10) oled.print("0");
  oled.print(now.minute());
  oled.print("                  v ");
  oled.println(version);

  oled.println(opcionSeleccionada == 0 ? "> Sleep mode" : "  Sleep mode");
  oled.println(opcionSeleccionada == 1 ? "> Settings" : "  Settings");
  oled.println(" Move       Select");
}

void mostrarMenuSettings() {
  oled.clear();
  
  // Título con numeración
  oled.print(" Settings    ");
  oled.print(settingOpcion + 1);
  oled.println("/3");

  // Mostrar solo 2 opciones a la vez
  switch(settingOpcion) {
    case 0: // Wake up (1/3)
      oled.println("> Wake up at 7:30");
      oled.println("  Sleep at 19:30");
      break;
    case 1: // Sleep at (2/3)
      oled.println("  Wake up at 7:30");
      oled.println("> Sleep at 19:30");
      break;
    case 2: // Exit (3/3)
      oled.println("  Sleep at 19:30");
      oled.println("> Exit");
      break;
  }
  
  // Línea inferior fija
  oled.println(" Move       Select");
}