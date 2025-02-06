String version = "21.1";

#include <Wire.h>
#include <RTClib.h> // Biblioteca para manejar el RTC
#include <SSD1306Ascii.h>
#include <SSD1306AsciiWire.h>

String status = "--";

//Horarios
const int horaDespertar = 7;
const int minutosDespertar = 30;
const int horaDormir = 20;
const int minutosDormir = 0;

// Pines de conexión de los LEDs
const int ledVerde = 9;
const int ledAmarillo = 10;
const int ledRojo = 11;

// Botones
const int moveButton = 2;
const int selectButton = 3;

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
  // rtc.adjust(DateTime(2025, 2, 16, 7, 30, 0)); // Año, Mes, Día, Hora, Minuto, Segundo
  // rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));

  // Configura los pines de los LEDs como salida
  pinMode(ledVerde, OUTPUT);
  pinMode(ledAmarillo, OUTPUT);
  pinMode(ledRojo, OUTPUT);

  // Configura el pin del botón como entrada con resistencia pull-up interna
  pinMode(selectButton, INPUT_PULLUP);


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

    // Contenido oled
    if (!botonPresionado) { // Solo actualiza la pantalla si no está en modo dormir
      oled.clear();
      oled.println("             iTronix");

      // Formato con ceros a la izquierda solo para los minutos
      String minutoStr = (minuto < 10) ? "0" + String(minuto) : String(minuto);

      oled.println(" " + String(hora) + ":" + minutoStr + "                  v " + String(version));  // Imprime el texto en la pantalla
      oled.println(" " + status);
      oled.println(" w: " + String(horaDespertar) + ":" + String(minutosDespertar) + " |  s: " + String(horaDormir) + ":" + String(minutosDormir));
    }
    
  }

  // Verifica el estado del botón
  estadoBotonActual = digitalRead(selectButton);

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
    // Conversión de tiempos a minutos totales desde la medianoche
    int tiempoActual = hora * 60 + minuto;
    int tiempoDespertar = horaDespertar * 60 + minutosDespertar;
    int tiempoDormir = horaDormir * 60 + minutosDormir;

    // Control de la pantalla OLED según la hora
    if (tiempoActual < (tiempoDespertar)) {
        // Apaga la pantalla OLED entre las 12:00 AM y 15 minutos antes de despertar
        oled.ssd1306WriteCmd(SSD1306_DISPLAYOFF);
    } else {
        // Enciende la pantalla OLED a partir de 15 minutos antes de la hora de despertar
        oled.ssd1306WriteCmd(SSD1306_DISPLAYON);
    }

    // Control de LEDs según el horario establecido
    if (tiempoActual < (tiempoDespertar - 15)) {
        // 12:00 AM hasta 15 minutos antes de despertar - Luz roja tenue
        setLuz(ledRojo, intensidadRojoTenue);
        status = "rojo low (C1)";
    } 
    else if (tiempoActual >= (tiempoDespertar - 15) && tiempoActual < tiempoDespertar) {
        // 15 minutos antes de despertar hasta la hora de despertar - Luz amarilla tenue
        setLuz(ledAmarillo, intensidadAmarilloTenue);
        status = "amarillo low (C2)";
    } 
    else if (tiempoActual >= tiempoDespertar && tiempoActual < (tiempoDespertar + 60)) {
        // Hora de despertar hasta 1 hora después - Luz verde tenue
        setLuz(ledVerde, intensidadVerdeTenue);
        status = "verde low (C3)";
    } 
    else if (tiempoActual >= (tiempoDespertar + 60) && tiempoActual < (tiempoDormir - 30)) {
        // 1 hora después de despertar hasta 30 minutos antes de dormir - Luz verde intensa
        setLuz(ledVerde, intensidadVerdeMax);
        status = "verde max (C4)";
    } 
    else if (tiempoActual >= (tiempoDormir - 30) && tiempoActual < tiempoDormir) {
        // 30 minutos antes de dormir hasta la hora de dormir - Luz amarilla intensa
        setLuz(ledAmarillo, intensidadAmarilloMax);
        status = "amarillo max (C5)";
    } 
    else {
        // Hora de dormir hasta las 12:00 AM - Luz roja intensa
        setLuz(ledRojo, intensidadRojoMax);
        status = "rojo max (C6)";
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