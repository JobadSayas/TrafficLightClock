// Version 12.15

#include <Wire.h>
#include <U8g2lib.h>

// Crear una instancia de la librería U8g2 para la pantalla OLED con rotación de 90 grados (U8G2_R1)
U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R1, U8X8_PIN_NONE, U8X8_PIN_NONE);

void setup() {
  u8g2.begin();  // Inicializar la pantalla
}

void loop() {
  u8g2.clearBuffer();  // Limpiar la memoria interna

  u8g2.drawBox(0, 0, 128, 20);  // Dibujar cuadro blanco que cubre la parte superior (encabezado)
  u8g2.setDrawColor(0);
  u8g2.setFont(u8g2_font_helvR08_tr);  // Establecer la fuente
  u8g2.drawStr(22, 8, "23:00");  // Dibujar el mensaje
  u8g2.setFont(u8g2_font_helvB08_tr);  // Establecer la fuente en negrita
  u8g2.drawStr(21, 18, "Menu");  // Dibujar el mensaje

  u8g2.setDrawColor(1);  // Establecer color de dibujo a blanco
  u8g2.setFont(u8g2_font_helvR08_tr);  // Establecer la fuente

  // u8g2.drawBox(0, 21, 128, 23);  // Dibujar cuadro blanco que cubre la parte superior (encabezado)
  // u8g2.setDrawColor(0);  // Establecer color de dibujo a blanco
  
  u8g2.drawStr(20, 31, "Sleep");  // Dibujar el mensaje
  u8g2.drawStr(20, 41, "mode");  // Dibujar el mensaje
  u8g2.drawDisc(8, 32, 5);  // Dibujar un círculo en el centro de la pantalla con un radio de 10 píxeles

  // Dibujar un círculo vacío (5 px de diámetro) junto a "Sleep mode"
  u8g2.drawCircle(110, 36, 5, U8G2_DRAW_ALL);

  u8g2.setDrawColor(1);  // Establecer color de dibujo a blanco
  u8g2.drawStr(0, 43, "_____________________________");  // Dibujar el mensaje

  // u8g2.drawBox(0, 45, 128, 23);  // Dibujar cuadro blanco que cubre la parte superior (encabezado)
  // u8g2.setDrawColor(0);  // Establecer color de dibujo a blanco

  u8g2.drawStr(20, 55, "Force");  // Dibujar el mensaje
  u8g2.drawStr(20, 65, "mode");  // Dibujar el mensaje

  u8g2.drawCircle(8, 56, 5);  // Dibujar un círculo en el centro de la pantalla con un radio de 10 píxeles


  u8g2.setDrawColor(1);  // Establecer color de dibujo a blanco
  u8g2.drawStr(0, 67, "_____________________________");  // Dibujar el mensaje

  // u8g2.drawBox(0, 69, 128, 23);  // Dibujar cuadro blanco que cubre la parte superior (encabezado)
  // u8g2.setDrawColor(0);  // Establecer color de dibujo a blanco

  u8g2.drawStr(20, 79, "Clock");  // Dibujar el mensaje
  u8g2.drawStr(20, 89, "mode");  // Dibujar el mensaje

  u8g2.drawCircle(8, 80, 5);  // Dibujar un círculo en el centro de la pantalla con un radio de 10 píxeles

  u8g2.setDrawColor(1);  // Establecer color de dibujo a blanco
  u8g2.drawStr(0, 91, "_____________________________");  // Dibujar el mensaje

  u8g2.drawBox(0, 93, 128, 19);  // Dibujar cuadro blanco que cubre la parte superior (encabezado)
  u8g2.setDrawColor(0);  // Establecer color de dibujo a blanco

  u8g2.drawStr(2, 106, "Settings");  // Dibujar el mensaje

  u8g2.setDrawColor(1);  // Establecer color de dibujo a blanco
  u8g2.setFont(u8g2_font_helvR08_tr);  // Establecer la fuente
  u8g2.drawStr(0, 111, "_____________________________");  // Dibujar el mensaje
  u8g2.drawStr(0, 124, "Move    Apply");  // Dibujar el mensaje

  u8g2.sendBuffer();  // Transferir la memoria interna a la pantalla
  delay(1000);  // Esperar 1 segundo
}