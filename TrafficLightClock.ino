String version = "22.0.8";  // Versión actualizada

#include <Wire.h>
#include <RTClib.h>
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

// Variables globales
String status = "--";
int ultimoMinutoImpreso = -1;

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
bool editandoHoraDespertar = false;
int opcionSeleccionada = 0;          // 0=Sleep mode, 1=Settings
int settingOpcion = 0;               // 0=Wake up, 1=Sleep at, 2=Exit
int editandoCampo = 0;               // 0=hora, 1=minutos
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
  oled.setFont(Arial14);  // Fuente más compacta
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

    if (!botonPresionado && !enMenuSettings && !editandoHoraDespertar) {
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
    if (editandoHoraDespertar) {
      if (editandoCampo == 0) {
        editandoCampo = 1; // Cambiar a editar minutos
      } else {
        // Guardar y salir al presionar Select en minutos
        editandoHoraDespertar = false;
        editandoCampo = 0;
        enMenuSettings = true;
        mostrarMenuSettings();
        return;
      }
      mostrarEdicionHoraDespertar();
    }
    else if (enMenuSettings) {
      if (settingOpcion == 0) { // Wake up at
        editandoHoraDespertar = true;
        editandoCampo = 0;
        mostrarEdicionHoraDespertar();
      }
      else if (settingOpcion == 2) { // Exit
        enMenuSettings = false;
        opcionSeleccionada = 0; // Reset a Sleep mode
        mostrarPantallaPrincipal();
      }
    } else {
      if (opcionSeleccionada == 0) { // Sleep mode
        botonPresionado = !botonPresionado;
        if (botonPresionado) {
          oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
          setLuz(ledRojo, intensidadRojoTenue);
        } else {
          oled.ssd1306WriteCmd(SSD1306_DISPLAYON);
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
  }
  lastSelect = currentSelect;

  // Botón Move
  if (lastMove == HIGH && currentMove == LOW) {
    if (editandoHoraDespertar) {
      if (editandoCampo == 0) { // Editando hora
        horaDespertar = (horaDespertar + 1) % 24;
      } else { // Editando minutos
        minutosDespertar = (minutosDespertar + 5) % 60;
      }
      mostrarEdicionHoraDespertar();
    }
    else if (enMenuSettings) {
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
  oled.print("         v");
  oled.println(version);

  oled.println(opcionSeleccionada == 0 ? ">Sleep mode" : " Sleep mode");
  oled.println(opcionSeleccionada == 1 ? ">Settings" : " Settings");
  oled.println("Move     Select");
}

void mostrarMenuSettings() {
  oled.clear();
  
  // Mostrar solo 2 opciones a la vez con scroll
  switch(settingOpcion) {
    case 0: // Wake up (1/3)
      oled.print("Settings    1/3");
      oled.println();
      oled.print(settingOpcion == 0 ? ">" : " ");
      oled.print("Wake up ");
      oled.print(horaDespertar);
      oled.print(":");
      if (minutosDespertar < 10) oled.print("0");
      oled.println(minutosDespertar);
      
      oled.print(settingOpcion == 1 ? ">" : " ");
      oled.print("Sleep ");
      oled.print(horaDormir);
      oled.print(":");
      if (minutosDormir < 10) oled.print("0");
      oled.println(minutosDormir);
      break;
      
    case 1: // Sleep at (2/3)
      oled.print("Settings    2/3");
      oled.println();
      oled.print(settingOpcion == 0 ? ">" : " ");
      oled.print("Wake up ");
      oled.print(horaDespertar);
      oled.print(":");
      if (minutosDespertar < 10) oled.print("0");
      oled.println(minutosDespertar);
      
      oled.print(settingOpcion == 1 ? ">" : " ");
      oled.print("Sleep ");
      oled.print(horaDormir);
      oled.print(":");
      if (minutosDormir < 10) oled.print("0");
      oled.println(minutosDormir);
      break;
      
    case 2: // Exit (3/3)
      oled.print("Settings    3/3");
      oled.println();
      oled.print(settingOpcion == 1 ? ">" : " ");
      oled.print("Sleep ");
      oled.print(horaDormir);
      oled.print(":");
      if (minutosDormir < 10) oled.print("0");
      oled.println(minutosDormir);
      
      oled.println(settingOpcion == 2 ? ">Exit" : " Exit");
      break;
  }
  
  oled.println("Move     Select");
}

void mostrarEdicionHoraDespertar() {
  oled.clear();
  oled.println("Wake up at");
  
  if (editandoCampo == 0) {
    oled.print(">");
    oled.print(horaDespertar);
  } else {
    oled.print(" ");
    oled.print(horaDespertar);
  }
  
  oled.print(":");
  
  if (editandoCampo == 1) {
    oled.print(">");
    if (minutosDespertar < 10) oled.print("0");
    oled.print(minutosDespertar);
  } else {
    if (minutosDespertar < 10) oled.print("0");
    oled.print(minutosDespertar);
  }
  
  oled.println();
  oled.println();
  oled.println("Move     Select");
}