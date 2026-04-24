#include "hardware.h"

// Objeto de la pantalla física
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

// --- MOTOR DE SONIDO MULTITAREA ---
QueueHandle_t colaSonidos; // El "buzón" de pedidos

void taskSonido(void *pvParameters) {
    SonidosUI tipo;
    for (;;) {
        // Esperamos a que llegue un sonido al buzón (se queda pausado aquí sin gastar batería)
        if (xQueueReceive(colaSonidos, &tipo, portMAX_DELAY)) {
            // Cuando llega uno, ejecutamos el switch original con sus delays
            switch (tipo) {
                case SND_NAVEGAR: tone(PIN_BUZZER, 2000, 30); break;
                case SND_CONFIRMAR: tone(PIN_BUZZER, 1500, 80); break;
                case SND_CANCELAR: tone(PIN_BUZZER, 800, 80); break;
                case SND_ERROR: tone(PIN_BUZZER, 150, 300); break;
                case SND_LLAMADA:
                    for (int rep = 0; rep < 3; rep++) {
                        for (int i = 0; i < 3; i++) {
                            tone(PIN_BUZZER, 2500, 50); delay(80);
                            tone(PIN_BUZZER, 2000, 50); delay(80);
                        }
                        if (rep < 2) delay(500);
                    }
                    break;
                case SND_COMER: tone(PIN_BUZZER, 1800, 40); break;
                case SND_CURAR: tone(PIN_BUZZER, 2200, 150); break;
                case SND_ACIERTO: tone(PIN_BUZZER, 1200, 100); break;
                case SND_FALLO: tone(PIN_BUZZER, 300, 150); break;
                case SND_VICTORIA: tone(PIN_BUZZER, 2000, 400); break;
                case SND_DERROTA: tone(PIN_BUZZER, 200, 400); break;
                case SND_EVOLUCION_TICK: tone(PIN_BUZZER, 3000, 50); break;
                case SND_MUERTE: tone(PIN_BUZZER, 100, 800); break;
            }
        }
    }
}

void iniciarTareaSonido() {
    // Creamos el buzón para 10 sonidos máximo
    colaSonidos = xQueueCreate(10, sizeof(SonidosUI));
    // Creamos al "trabajador" en el núcleo 0 (el juego suele ir en el 1) y subimos de 2048 a 4096 para dar más aire al proceso de sonido
    xTaskCreatePinnedToCore(taskSonido, "TaskSonido", 4096, NULL, 1, NULL, 0);
}

void reproducirSonido(SonidosUI tipo) {
    // En lugar de tocar el sonido, lo enviamos al buzón y seguimos adelante
    xQueueSend(colaSonidos, &tipo, 0);
}