#ifndef UI_H
#define UI_H

#include <Arduino.h>
#include "globals.h"
#include "hardware.h" // Necesario para acceder a "display"

void actualizarPantalla();
void animacionArrastre(bool haciaReloj);

#endif