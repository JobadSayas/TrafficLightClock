//www.elegoo.com
//2018.10.24
#include <Wire.h>
#include <DS3231.h>

DS3231 clock;
RTCDateTime dt;

// Define Pins
#define GREEN 9
#define YELLOW 10
#define RED 11

// define variables
int greenValue;
int yellowValue;
int redValue;
const char lampButton = 4;
bool lampButtonPressed = false;
const char siestaButton = 13;
bool siestaButtonPressed = false;
const char modoButton = 12;
bool modoButtonPressed = false;
int guia = 5;
int lampara = 6;
int luzPrendida = 0;
float temporizador = 0;
bool siesta = true;

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
  pinMode(lampara, OUTPUT);
  pinMode(guia, OUTPUT);

}

void loop()
{

  // SEMAFORO------

  dt = clock.getDateTime();

  // For leading zero look to DS3231_dateformat example

  Serial.print("Raw data: ");
  Serial.print(dt.year);   Serial.print("-");
  Serial.print(dt.month);  Serial.print("-");
  Serial.print(dt.day);    Serial.print(" ");
  Serial.print(dt.hour);   Serial.print(":");
  Serial.print(dt.minute); Serial.print(":");
  Serial.print(dt.second); Serial.println("");

  greenValue = 0;
  yellowValue = 0;
  redValue = 0;

  if(dt.hour >= 5 && dt.hour < 7){
    // rojo (5:00 a 6:59)
    redValue = 1;
  }
  else if(dt.hour >= 7 && dt.hour < 8){
    // amarillo (7:00 a 7:59)
    yellowValue = 10;
  }
  else if(dt.hour >= 8 && dt.hour < 12){
    // verde  (8:00 a 11:59)
    greenValue = 50;

  }
  else if(dt.hour >= 12 && dt.hour < 13){
    // amarillo (12:00 a 12:59)
    yellowValue = 50;
  }
  else if(dt.hour >= 13 && dt.hour < 15){
    // rojo (13:00 a 14:59)
    redValue = 50;
  }
  else if(dt.hour >= 15 && dt.hour < 18){
    // verde (15:00 a 17:59)
    greenValue = 100;
  }
  else if(dt.hour >= 18 && dt.hour < 19 && siesta == true){
    // amarillo (18:00 a 18:59)
    greenValue = 100;
  }
  else if(dt.hour >= 18 && dt.hour < 19 && siesta == false){
    // amarillo (18:00 a 18:59)
    yellowValue = 255;
  }
  else if(dt.hour >= 19 && dt.hour < 20 && siesta == true){
    // amarillo (19:00 a 19:59)
    yellowValue = 255;
  }
  else if(dt.hour >= 19 && dt.hour < 20 && siesta == false){
    // amarillo (19:00 a 19:59)
    redValue = 200;
  }
  else if(dt.hour >= 20 && dt.hour < 21){
    // rojo (20:00 a 20:59)
    redValue = 100;
  }
  else if(dt.hour >= 21 && dt.hour < 24){
    // rojo (21:00 a 23:59)
    redValue = 5;
  }

  // greenValue = 0;
  // yellowValue = 0;
  // redValue = 100;

  analogWrite(GREEN, greenValue);
  analogWrite(YELLOW, yellowValue);
  analogWrite(RED, redValue);

  
  // LAMPARA------

  analogWrite(guia, 1);

  bool lampCurrentState = digitalRead(lampButton);
  
  if (lampCurrentState == lampButtonPressed) {
    Serial.println("boton lampara presionado");
    if(luzPrendida == 0){
      luzPrendida = 1;
      temporizador = 255;
    }
    else{
      luzPrendida = 0;
      temporizador = 0;
    }
    delay(500);
  }

  if(luzPrendida == 1){
    if (temporizador > 0){
      temporizador = temporizador - .5;
      Serial.println(temporizador);
    }
    else{
      luzPrendida = 0;
      temporizador = 0;
    }
  }

  analogWrite(lampara, temporizador);

  // SIESTA

  bool siestaCurrentState = digitalRead(siestaButton);

  if (siestaCurrentState == siestaButtonPressed) {
    Serial.println("boton siesta presionado");

    if(siesta == true){
      siesta = false;
      analogWrite(GREEN, 0);
      analogWrite(YELLOW, 250);
      analogWrite(RED, 100);
    }
    else{
      siesta = true;
      analogWrite(GREEN, 100);
      analogWrite(YELLOW, 250);
      analogWrite(RED, 0);
    }
    Serial.println(siesta);
    delay(500);
  }


  delay(100);

}
