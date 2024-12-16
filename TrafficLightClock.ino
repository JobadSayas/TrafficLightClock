//Version 18.0

#include <Wire.h>
#include <RTClib.h> // Biblioteca para manejar el RTC

//Horarios
int horaDespertar[2] = {7, 0}; // 7:30 am
int horaSiesta[2] = {12, 30};    // 1:00 pm
int horaDormir[2] = {20, 0};    // 8:00 pm

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
  }

  // Verifica el estado del botón
  estadoBotonActual = digitalRead(botonPin);

  // Detecta si el botón ha sido presionado (cambio de estado de HIGH a LOW)
  if (estadoBotonAnterior == HIGH && estadoBotonActual == LOW) {
    botonPresionado = !botonPresionado; // Cambia el estado de la variable
    if (botonPresionado) {
      // Verifica si es siesta o modo noche
      if (hora < 17) { // Si es antes de las 5:00 pm, entra en modo siesta
        esSiesta = true;
        horaInicioSiesta = now; // Registra la hora de inicio de la siesta
        Serial.println("Siesta activada, luz roja tenue por 90 minutos.");
      } else {
        esSiesta = false;
        Serial.println("Modo dormir nocturno activado, luz roja tenue hasta las 12:00 am.");
      }
    } else {
      esSiesta = false; // Si se desactiva el botón, se sale del modo siesta
      Serial.println("Modo manual desactivado, volviendo a horario normal.");
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
        Serial.println("Modo dormir desactivado automáticamente a las 12:00 am, volviendo a horario normal.");
      }
    }
  }

  //Control de LEDs usando horarios configurables
  if (!botonPresionado) {
    // Rojo tenue: desde la medianoche hasta 15 mins antes de despertar
    if ((hora < horaDespertar[0]) || (hora == horaDespertar[0] && minuto < (horaDespertar[1] - 15))) {
      setLuz(ledRojo, intensidadRojoTenue);
    }
    // Amarillo tenue: desde 15 mins antes de despertar hasta la hora de despertar
    else if ((hora == horaDespertar[0] && minuto >= (horaDespertar[1] - 15)) && (hora == horaDespertar[0] && minuto < horaDespertar[1])) {
      setLuz(ledAmarillo, intensidadAmarilloTenue);
    }
    // Verde tenue: desde la hora de despertar hasta 60 mins después
    else if ((hora == horaDespertar[0] && minuto >= horaDespertar[1]) || (hora == horaDespertar[0] + 1 && minuto < horaDespertar[1])) {
      setLuz(ledVerde, intensidadVerdeTenue);
    }
    // Verde intenso: desde 60 mins después de despertar hasta 30 mins antes de la siesta
    else if ((hora > horaDespertar[0] + 1) && (hora < horaSiesta[0] || (hora == horaSiesta[0] && minuto < (horaSiesta[1] - 30)))) {
      setLuz(ledVerde, intensidadVerdeMax);
    }
    // Amarillo máximo: 30 mins antes de la siesta
    else if ((hora == horaSiesta[0] && minuto >= (horaSiesta[1] - 30)) && (hora == horaSiesta[0] && minuto < horaSiesta[1])) {
      setLuz(ledAmarillo, intensidadAmarilloMax);
    }
    // Rojo máximo: durante la siesta
    else if ((hora == horaSiesta[0] && minuto >= horaSiesta[1]) || (hora == horaSiesta[0] + 1 && minuto < horaSiesta[1])) {
      setLuz(ledRojo, intensidadRojoMax);
    }
    // Verde tenue: 60-90 mins después de la siesta
    else if ((hora == horaSiesta[0] + 1 && minuto >= horaSiesta[1]) || (hora == horaSiesta[0] + 1 && minuto < (horaSiesta[1] + 30))) {
      setLuz(ledVerde, intensidadVerdeTenue);
    }
    // Verde intenso: desde 90 mins después de la siesta hasta 30 mins antes de dormir
    else if ((hora > horaSiesta[0] + 1 && minuto >= (horaSiesta[1] + 30)) && (hora < horaDormir[0] || (hora == horaDormir[0] && minuto < (horaDormir[1] - 30)))) {
      setLuz(ledVerde, intensidadVerdeMax);
    }
    // Amarillo máximo: 30 mins antes de dormir
    else if ((hora == horaDormir[0] && minuto >= (horaDormir[1] - 30)) && (hora == horaDormir[0] && minuto < horaDormir[1])) {
      setLuz(ledAmarillo, intensidadAmarilloMax);
    }
    // Rojo intenso: desde la hora de dormir hasta las 11:59 pm
    else if ((hora >= horaDormir[0] && hora < 24)) {
      setLuz(ledRojo, intensidadRojoMax);
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

