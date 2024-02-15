//www.elegoo.com
//2018.10.24
#include <Wire.h>
#include <DS3231.h>

DS3231 clock;
RTCDateTime dt;


// VARIABLES ----------

int modoSemaforo = 1;

int luzVerde = 9;
int luzAmarilla = 10;
int luzRoja = 11;

int valorVerde;
int valorAmarillo;
int valorRojo;

int lampButton = 4;
bool lampButtonPressed = false;

int siestaButton = 13;
bool siestaButtonPressed = false;

int modoButton = 12;
bool modoButtonPressed = false;

int luzGuia = 5;
int lampara = 6;
int luzPrendida = 0;
float intensidadLampara = 0;

bool siesta = true;

int ldr;
int intensidadLuces;


// SETUP -----------

void setup()
{
  Serial.begin(9600);

  Serial.println("Initialize RTC module");
  // Initialize DS3231
  clock.begin();

  
  // Manual (YYYY, MM, DD, HH, II, SS
  // clock.setDateTime(2016, 12, 9, 11, 46, 00);
  
  // Send sketch compiling time to Arduino
  // clock.setDateTime(__DATE__, __TIME__);    
  /*
  Tips:This command will be executed every time when Arduino restarts. 
       Comment this line out to store the memory of DS3231 module
  */

  // Setup pin modes
  pinMode(lampButton, INPUT_PULLUP);
  pinMode(siestaButton, INPUT_PULLUP);
  pinMode(modoButton, INPUT_PULLUP);

  pinMode(A0, INPUT);
  
  pinMode(luzRoja, OUTPUT);
  pinMode(luzAmarilla, OUTPUT);
  pinMode(luzVerde, OUTPUT);

  pinMode(lampara, OUTPUT);
  pinMode(luzGuia, OUTPUT);

}


// LOOP ----------

void loop()
{

  // MODO SEMAFORO SEMAFORO ----------

  if(modoSemaforo == 1){

    //Iniciar reloj
    dt = clock.getDateTime();

    Serial.print("Raw data: ");
    Serial.print(dt.year);   Serial.print("-");
    Serial.print(dt.month);  Serial.print("-");
    Serial.print(dt.day);    Serial.print(" ");
    Serial.print(dt.hour);   Serial.print(":");
    Serial.print(dt.minute); Serial.print(":");
    Serial.print(dt.second); Serial.println("");
    
    // Sensor luz
    ldr = analogRead(A0);
    
    if(ldr < 1000){
      intensidadLuces = 250;
    }
    else{
      intensidadLuces = 3;
    }

    //Cambiar luz segun hora
    valorVerde = 0;
    valorAmarillo = 0;
    valorRojo = 0;

    if(dt.hour >= 0 && dt.hour < 6){
      // rojo (0:00 a 5:59)
      valorRojo = intensidadLuces;
      siesta == true;
    }
    else if(dt.hour >= 6 && dt.hour < 7){
      // amarillo (6:00 a 6:59)
      valorAmarillo = intensidadLuces*2;
    }
    else if(dt.hour >= 7 && dt.hour < 11){
      // verde  (7:00 a 10:59)
      valorVerde = intensidadLuces;
    }
    else if(dt.hour >= 11 && dt.hour < 12){
      // amarillo (11:00 a 11:59)
      valorAmarillo = intensidadLuces*2;
    }
    else if(dt.hour >= 12 && dt.hour < 14){
      // rojo (12:00 a 13:59)
      valorRojo = intensidadLuces;
    }
    else if(dt.hour >= 14 && dt.hour < 17){
      // verde (14:00 a 16:59)
      valorVerde = intensidadLuces;
    }
    else if(dt.hour >= 17 && dt.hour < 18){ 
      // verde o amarillo (17:00 a 17:59)
      if(siesta == true){
        valorVerde = intensidadLuces;
      }
      else{
        valorAmarillo = intensidadLuces*2;
      }
    }
    else if(dt.hour >= 18 && dt.hour < 19){
      // amarillo o rojo (18:00 a 18:59)
      if(siesta == true){
        valorAmarillo = intensidadLuces*2;
      }
      else{
        valorRojo = intensidadLuces;
      }
    }
    else if(dt.hour >= 19 && dt.hour < 24){
      // rojo (19:00 a 23:00)
      valorRojo = intensidadLuces;
    }


    analogWrite(luzVerde, valorVerde);
    analogWrite(luzAmarilla, valorAmarillo);
    analogWrite(luzRoja, valorRojo);

  }


  // MODO JUEGO ----------

  if(modoSemaforo == 2){
    Serial.println("modo juego");
  }


  // MODO PARTY ----------

  if(modoSemaforo == 3){
    Serial.println("modo party");
  }
  
  
  // LAMPARA ----------

  analogWrite(luzGuia, 1);

  bool lampCurrentState = digitalRead(lampButton);
  
  if (lampCurrentState == lampButtonPressed) {
    Serial.println("boton lampara presionado");
    if(luzPrendida == 0){
      luzPrendida = 1;
      intensidadLampara = 255;
    }
    else{
      luzPrendida = 0;
      intensidadLampara = 0;
    }
    delay(500);
  }

  if(luzPrendida == 1){
    if (intensidadLampara > 0){
      intensidadLampara = intensidadLampara - .5;
      Serial.println(intensidadLampara);
    }
    else{
      luzPrendida = 0;
      intensidadLampara = 0;
    }
  }

  analogWrite(lampara, intensidadLampara);


  // SIESTA ----------

  bool siestaCurrentState = digitalRead(siestaButton);

  if (siestaCurrentState == siestaButtonPressed) {
    Serial.println("boton siesta presionado");

    if(siesta == true){
      siesta = false;
      analogWrite(luzVerde, 0);
      analogWrite(luzAmarilla, 250);
      analogWrite(luzRoja, 100);
    }
    else{
      siesta = true;
      analogWrite(luzVerde, 100);
      analogWrite(luzAmarilla, 250);
      analogWrite(luzRoja, 0);
    }
    Serial.println(siesta);
    delay(500);
  }

  delay(100);

}
