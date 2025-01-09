String version = "19.8";

#include <Wire.h>
#include <RTClib.h> // Biblioteca para manejar el RTC
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

String status = "--";

//Horarios
const int ajusteMinutos = 0;

// Pines de conexión de los LEDs
const int ledVerde = 9;
const int ledAmarillo = 10;
const int ledRojo = 11;

// Pin del botón
const int botonPin = 2;

// Estado del botón
bool botonPresionado = false;

// Control de siesta
bool esSiesta = false;
DateTime horaInicioSiesta;
const int duracionSiesta = 90; // 90 minutos de siesta

// Intensidades actualizadas para equilibrar la percepción visual
const int intensidadRojoMax = 100;
const int intensidadAmarilloMax = 255;
const int intensidadVerdeMax = 100;
const int intensidadRojoTenue = 3;
const int intensidadAmarilloTenue = 10;
const int intensidadVerdeTenue = 3;

// Instancia del módulo RTC
RTC_DS3231 rtc;

// Variable para almacenar la hora y los minutos actuales
DateTime now;

// Variables para registrar el estado anterior del botón
int estadoBotonActual = LOW;
int estadoBotonAnterior = LOW;

// Variable para almacenar el último minuto impreso
int ultimoMinutoImpreso = -1;


// Crear una instancia del objeto SSD1306Ascii con comunicación I2C
SSD1306AsciiWire oled;  

// Dirección I2C de la pantalla (puede ser 0x3C o 0x3D dependiendo del modelo)
#define OLED_ADDRESS 0x3C  


void setup() {
  // Inicializa la comunicación serial
  Serial.begin(9600);

  // Inicializa el RTC
  if (!rtc.begin()) {
    Serial.println("Error: No se encuentra el módulo RTC.");
    while (1);
  }

  // Configura el RTC con la hora actual directamente
  // rtc.adjust(DateTime(2024, 10, 14, 6, 52, 0)); // Año, Mes, Día, Hora, Minuto, Segundo
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Configura los pines de los LEDs como salida
  pinMode(ledVerde, OUTPUT);
  pinMode(ledAmarillo, OUTPUT);
  pinMode(ledRojo, OUTPUT);

  // Configura el pin del botón como entrada con resistencia pull-up interna
  pinMode(botonPin, INPUT_PULLUP);


  // Inicializar la comunicación I2C
  Wire.begin();

  // Inicializar la pantalla OLED
  oled.begin(&Adafruit128x64, OLED_ADDRESS);  // Usamos un controlador de pantalla estándar
  oled.setFont(Arial_14);  // Establece la fuente, puedes elegir otras fuentes también

}

void loop() {
  now = rtc.now(); // Obtiene la hora actual del RTC

  int hora = now.hour();
  int minuto = now.minute();

  // Muestra la hora en el monitor serial solo una vez por minuto
  if (minuto != ultimoMinutoImpreso) {
    Serial.print("Hora actual: ");
    Serial.print(hora);
    Serial.print(":");
    if (minuto < 10) {
      Serial.print("0"); // Agrega un cero si el minuto es menor a 10
    }
    Serial.println(minuto);

    // Actualiza el último minuto impreso
    ultimoMinutoImpreso = minuto;

    // Limpiar la pantalla y mostrar mensaje
    if (!botonPresionado) { // Solo actualiza la pantalla si no está en modo dormir
      oled.clear();
      oled.println("    " + String(hora) + ":" + String(minuto));  // Imprime el texto en la pantalla
      oled.println("    v" + version);
      oled.println("    " + status);
    }
  }

  // Verifica el estado del botón
  estadoBotonActual = digitalRead(botonPin);

  // Detecta si el botón ha sido presionado (cambio de estado de HIGH a LOW)
  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW) {
    botonPresionado = !botonPresionado; // Cambia el estado de la variable

    if (botonPresionado) {
        Serial.println("Modo dormir activado, luz roja tenue.");
        oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF); // Apaga la pantalla OLED
        setLuz(ledRojo, intensidadRojoTenue); // Luz roja tenue al entrar en modo dormir
    } else {
        Serial.println("Modo dormir desactivado, regresando a horario normal.");
        oled.ssd1306WriteCmd(SSD1306_DISPLAYON); // Enciende la pantalla OLED
        ultimoMinutoImpreso = -1; // Forzar actualización del OLED
    }
  }

  // Actualiza el estado anterior del botón
  estadoBotonAnterior = estadoBotonActual;

  // Control de LEDs según el horario, el estado del botón y la siesta
  if (botonPresionado) {
    if (esSiesta) {
      // Si es siesta y ha pasado la duración de la siesta, volver al horario normal
      if ((now.unixtime() - horaInicioSiesta.unixtime()) >= (duracionSiesta * 60)) {
        botonPresionado = false; // Desactiva el modo siesta
        esSiesta = false;
        Serial.println("Siesta terminada, volviendo a horario normal.");
      } else {
        setLuz(ledRojo, intensidadRojoTenue); // Luz roja tenue durante la siesta
      }
    } else {
      // Si no es siesta (modo dormir), luz roja tenue hasta las 12:00 am
      setLuz(ledRojo, intensidadRojoTenue);
      if (hora >= 0 && hora < 6) { // A las 12:00 am se desactiva modo dormir y se vuelve a horario normal
        botonPresionado = false;
        oled.ssd1306WriteCmd(SSD1306_DISPLAYON); // Enciende la pantalla OLED
        Serial.println("Modo dormir desactivado automáticamente a las 12:00 am, volviendo a horario normal.");
      }
    }
  }

  if (!botonPresionado) {
    // Control de LEDs según el horario establecido
    if (hora == 6 && minuto >= 45 && minuto < 60) {
        // 6:45 am a 6:59 am - Luz amarilla tenue
        setLuz(ledAmarillo, intensidadAmarilloTenue);
        status = "amarillo low (C1)";
    } 
    else if (hora == 7 && minuto >= 0 && minuto < 60) {
        // 7:00 am a 7:59 am - Luz verde tenue
        setLuz(ledVerde, intensidadVerdeTenue);
        status = "verde low (C2)";
    } 
    else if ((hora >= 8 && hora < 18) || (hora == 18 && minuto < 30)) {
        // 8:00 am a 6:29 pm - Luz verde intensa
        setLuz(ledVerde, intensidadVerdeMax);
        status = "verde max (C3)";
    } 
    else if (hora == 18 && minuto >= 30 && minuto < 60) {
        // 6:30 pm a 6:59 pm - Luz amarilla intensa
        setLuz(ledAmarillo, intensidadAmarilloMax);
        status = "amarillo max (C4)";
    } 
    else if (hora >= 19 && hora < 24) {
        // 7:00 pm a 11:59 pm - Luz roja intensa
        setLuz(ledRojo, intensidadRojoMax);
        status = "rojo max (C5)";
    } 
    else if ((hora >= 0 && hora < 6) || (hora == 6 && minuto < 45)) {
        // 12:00 am a 6:44 am - Luz roja tenue
        setLuz(ledRojo, intensidadRojoTenue);
        status = "rojo low (C6)";
    }
  }


  delay(100); // Pequeño retardo para evitar rebotes del botón
}

// Función para controlar la intensidad de la luz
void setLuz(int pin, int intensidad) {
  analogWrite(pin, intensidad);

  // Apaga los otros LEDs
  if (pin != ledVerde) {
    analogWrite(ledVerde, 0);
  }
  if (pin != ledAmarillo) {
    analogWrite(ledAmarillo, 0);
  }
  if (pin != ledRojo) {
    analogWrite(ledRojo, 0);
  }
}