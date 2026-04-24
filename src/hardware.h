#ifndef HARDWARE_H
#define HARDWARE_H

#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "globals.h" // Para que reconozca los SonidosUI

// --- DEFINICIÓN DE PANTALLA ---
#define SCREEN_WIDTH 128 
#define SCREEN_HEIGHT 64 
#define OLED_RESET -1    

// --- PINES ---
#define BTN_A 13
#define BTN_B 12
#define BTN_C 14
#define PIN_BUZZER 25

// Avisamos a todo el proyecto de que la pantalla existe
extern Adafruit_SSD1306 display;

// Declaramos que existe esta función
void reproducirSonido(SonidosUI tipo);

void iniciarTareaSonido();

#endif