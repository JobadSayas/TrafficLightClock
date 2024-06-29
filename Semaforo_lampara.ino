//Version 10.0

#include <Wire.h>
#include <DS3231.h>

DS3231 clock;
RTCDateTime dt;


// Variables

int modo = 1;

int luzVerdeI = 9;
int luzAmarillaI = 10;
int luzRojaI = 11;

// int luzVerdeE = A1;
// int luzAmarillaE = A3;
// int luzRojaE = A2;

int botonA = 2;
int botonB = 3;
int botonC = 4;

long int timer = 0;
int timerToggle = 0;

int lampara = 3;
int luzPrendida = 0;
float intensidadLampara = 0;

bool siesta = true;


  // Funciones

void reiniciarLuces(){
  analogWrite(luzRojaI, 0);
  analogWrite(luzAmarillaI, 0);
  analogWrite(luzVerdeI, 0);

  // digitalWrite(luzRojaE, LOW);
  // digitalWrite(luzAmarillaE, LOW);
  // digitalWrite(luzVerdeE, LOW);
}

void rojo(int intensidad = 100){
  analogWrite(luzRojaI, intensidad);
  // digitalWrite(luzRojaE, HIGH);
}
void soloRojo(int intensidad = 100){
  reiniciarLuces();
  rojo(intensidad);
}

void amarillo(int intensidad = 255){
  analogWrite(luzAmarillaI, intensidad);
  // digitalWrite(luzAmarillaE, HIGH);
}
void soloAmarillo(int intensidad = 255){
  reiniciarLuces();
  amarillo(intensidad);
}

void verde(int intensidad = 100){
  analogWrite(luzVerdeI, intensidad);
  // digitalWrite(luzVerdeE, HIGH);
}
void soloVerde(int intensidad = 100){
  reiniciarLuces();
  verde(intensidad);
}


// SETUP -----------
void setup(){

  //INICIALIZAR ELECTRONICA

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
  pinMode(botonC, INPUT_PULLUP);
  pinMode(botonB, INPUT_PULLUP);
  pinMode(botonA, INPUT_PULLUP);

  pinMode(A0, INPUT);
  
  pinMode(luzRojaI, OUTPUT);
  pinMode(luzAmarillaI, OUTPUT);
  pinMode(luzVerdeI, OUTPUT);

  // pinMode(luzRojaE, OUTPUT);
  // pinMode(luzAmarillaE, OUTPUT);
  // pinMode(luzVerdeE, OUTPUT);

  pinMode(lampara, OUTPUT);

}


// LOOP ----------

void loop()
{

  //RELOJ
  //Iniciar e imprimir reloj
  dt = clock.getDateTime();

  Serial.print(dt.year);   Serial.print("-");
  Serial.print(dt.month);  Serial.print("-");
  Serial.print(dt.day);    Serial.print(" ");
  Serial.print(dt.hour);   Serial.print(":");
  Serial.print(dt.minute); Serial.print(":");
  Serial.print(dt.second); Serial.println("");

  //INICIADORES DEL DIA

  if(dt.hour == 0){
    modo = 1;
    siesta = false;
  }


  // MODO CAMBIO DE LUCES POR HORA ----------

  if(modo == 1){

      if(dt.hour >= 0 && dt.hour < 6){
        // rojo (0:00 a 5:59)
        soloRojo(3);
      }
      else if(dt.hour >= 6 && dt.hour < 7){
        // amarillo (6:00 a 6:59)
        soloAmarillo(10);
      }
      //HORA DE DESPERTAR <<<<<<<<<<<<<<<<<<<<<<<<<<<<
      else if(dt.hour >= 7 && dt.hour < 8){
        // verde  (7:00 a 7:59)
        soloVerde(3);
      }
      else if(dt.hour >= 8 && dt.hour < 11){
        // verde  (8:00 a 10:59)
        soloVerde();
      }
      else if(dt.hour >= 11 && dt.hour < 12){
        // amarillo (11:00 a 11:59)
        if(siesta == false){
          soloAmarillo();
        }
      }
      //HORA DE SIESTA <<<<<<<<<<<<<<<<<<<<<<<<<<<<
      else if(dt.hour >= 12 && dt.hour < 13){
        // rojo (12:00 a 12:59)
        if(siesta == false){
          soloRojo();
        }
      }
      else if(dt.hour >= 13 && dt.hour < 17){
        // verde (13:00 a 16:59)
        soloVerde();
      }
      else if(dt.hour >= 17 && dt.hour < 18){ 
        // verde o amarillo (17:00 a 17:59)
        if(siesta == true){
          soloVerde();
        }
        else{
          soloAmarillo();
        }
      }
      else if(dt.hour >= 18 && dt.hour < 19){
        // amarillo o rojo (18:00 a 18:59)
        if(siesta == true){
          soloAmarillo();
        }
        else{
          soloRojo();
        }
      }
      //HORA DE DORMIR <<<<<<<<<<<<<<<<<<<<<<<<<<<<
      else if(dt.hour >= 19 && dt.hour < 24){
        // rojo (19:00 a 23:00)
        soloRojo();
      }

  }


  // MODO DORMIR (ROJO)
  if(modo == 2){

    soloRojo(3);

    // Temporizador de siesta
    if(dt.hour >= 11 && dt.hour < 16){
      timerToggle = 1;
    }


    // Temporizador siesta

    //Incrementar
    if (timerToggle == 1){
      timer = timer + 100;
      Serial.println(timer);
    }

    //Alto a temporizador
    if (timer == 5400000){
    //1000*60*60 (1 seg = 1000 * 60 seg * 90 mins)
      
      timerToggle = 0;
      timer = 0;
      siesta = true;
      
      soloVerde(3);
      modo = 1;
    }

  }

  // MODO DESPERTAR (VERDE)
  if(modo == 3){
    
    soloVerde();

  }


  // MODO JUEGO
  if(modo == 4){

    timer = timer + 100;
    Serial.println(timer);

    if(timer > 0 && timer < 15000){
      soloVerde();
    }
    else if(timer > 15000 && timer < 20000){
      soloAmarillo();
    }
    else if(timer > 20000 && timer < 30000){
      soloRojo();
    }
    else if(timer >= 30000){
      timer = 0;
    }

  }
  
  
  // LAMPARA ----------

  bool botonCCurrentState = digitalRead(botonC);
  
  if (botonCCurrentState == LOW) {
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
  // Esto es solo para definir a que hora se pone amarillo o rojo

  // bool botonBCurrentState = digitalRead(botonB);

  // if (botonBCurrentState == LOW) {
  //   Serial.println("boton B (siesta) presionado");

  //   reiniciarLuces();

  //   if(siesta == true){
  //     siesta = false;
  //     rojo();
  //     amarillo();
  //   }
  //   else{
  //     siesta = true;
  //     verde();
  //     amarillo();
  //   }
  //   Serial.println(siesta);
  //   delay(500);
  // }


  // MODO DORMIR ----------
  // Esto forza a rojo y cuenta 1 hora cuando es siesta

  int botonACurrentState = digitalRead(botonA);
  
  if (botonACurrentState == LOW) {
    Serial.println("boton A (modo) presionado");
    //cambio de modos 1 2 3 aqui

    if(modo == 4){
      modo = 0;
    }

    modo++;
    Serial.println("Modo");
    Serial.println(modo);

    timer = 0;
    timerToggle = 0; 

    reiniciarLuces();
    if(modo == 1){
      rojo();
      amarillo();
      verde();
    }
    else if(modo == 4){
      rojo();
      verde();
    }

    delay(500);
  }



  delay(100);

}