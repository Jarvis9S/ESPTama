#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "sprites.h"

#define SCREEN_WIDTH 128 // Ancho de la pantalla OLED
#define SCREEN_HEIGHT 64 // Alto de la pantalla OLED
#define OLED_RESET -1    // Reset (algunas pantallas no tienen, usamos -1)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BTN_A 13
#define BTN_B 12
#define BTN_C 14

// TAMAGOTCHI PDP - ALPHA 0.2

// Mapa de estados
enum EstadosMascota
{
  ESTADO_IDLE,           // Reposo/Pantalla principal
  ESTADO_MENU,           // Navegando por opciones
  ESTADO_SUBMENU_COMIDA, // Submenú de comida
  ESTADO_SUBMENU_STATS,  // Submenú de estadísticas
  ESTADO_ACCION,         // Comiendo, jugando, etc.
  ESTADO_EVOLUCION,      // Momento de la transformación
  ESTADO_MUERTE,         // La mascota ha muerto
  ESTADO_GAME            // Pantalla de juego
};

enum GamePhase
{
  SHOW_CURRENT,  // Muestra el número actual
  WAITING_INPUT, // Espera a que el usuario pulse "Mayor" o "Menor"
  SHOW_RESULT,   // Muestra el número oculto y si ganaste
  GAME_OVER      // Fin de la partida y entrega de premios
};

// Estructura de la mascota
struct MascotaData
{
  uint8_t stage;                // Edad o etapa de evolución (0, 1, 2...)
  uint8_t type;                 // Variante de la mascota (depende de los cuidados)
  uint32_t birth_time;          // Momento del nacimiento en milisegundos
  EstadosMascota estado_actual; // Estado actual de la mascota
  int8_t hunger;                // Hambre (0 a 4)
  int8_t happiness;             // Felicidad (0 a 4)
  uint16_t weight;              // Peso en gramos (0 a 9999)
  uint8_t health_status;        // Estado de salud (0 a 4)
  uint8_t poop_counter;         // Contador de cacas (0 a 4)
  uint8_t discipline;           // Nivel de disciplina (0 a 100)
  uint8_t race;                 // Raza o tipo específico (0 a 9)

  // Ritmos biológicos (¡NUEVO!)
  uint32_t hunger_interval;     // Cuánto tarda en bajar el hambre
  uint32_t happiness_interval;  // Cuánto tarda en bajar la felicidad
  uint32_t poop_interval;       // Cuánto tarda en hacerse caca
  bool poop_sickness_triggered; // Seguro anticiclos de enfermedad

  // Cronómetros
  uint32_t last_poop_time;
  uint32_t last_hunger_time;
  uint32_t last_happiness_time;

  // Variables para manejo de atención
  bool needs_attention;
  uint32_t attention_start;

  // Variables para manejo de errores de cuidado
  uint8_t care_mistakes;
  bool mistake_processed;

  // Cálculo de momento de inicio de acción actual
  uint32_t action_start;
  char current_action;

  // Juego para felicidad y menor peso
  uint8_t game_round;
  uint8_t game_wins;
  uint8_t current_number;
  uint8_t next_number;
  uint8_t game_phase;
  bool last_guess_won;
  uint8_t game_ticks;
};

// Paso del tiempo de la interfaz
uint32_t tiempoUltimoLatido = 0;
const uint32_t INTERVALO = 1000; // 1 segundo para refrescar la consola

MascotaData pet;

bool frame_animation = false;
int8_t menu_index = -1; // -1 significa que no hay nada seleccionado
int8_t submenu_index = 0;
bool estadoAnteriorA = HIGH; // Para evitar los rebotes del botón

void actualizarPantalla()
{
  display.clearDisplay(); // Limpiamos la pantalla entera

  // --- EL MENÚ FIJO (Solo se dibuja el seleccionado) ---
  const int menu_x[8] = {8, 40, 72, 104, 8, 40, 72, 104};
  const int menu_y[8] = {0, 0, 0, 0, 48, 48, 48, 48};

  // Array con tus nuevos nombres de sprites
  const unsigned char *menu_icons[8] = {
      epd_bitmap_menu16_food_1,       // 0: Comida
      epd_bitmap_menu16_game_1,       // 1: Juego
      epd_bitmap_menu16_toilet_1,     // 2: Baño
      epd_bitmap_menu16_meds_1,       // 3: Medicina
      epd_bitmap_menu16_discipline_1, // 4: Disciplina
      epd_bitmap_menu16_stats_1,      // 5: Estadísticas
      epd_bitmap_menu16_light_1,      // 6: Luz
      epd_bitmap_menu16_attention_1   // 7: Atención
  };

  // Array para acceder a los números por índice (0-9)
  const unsigned char *const game_numbers[] PROGMEM = {
      epd_bitmap_game_0, epd_bitmap_game_1, epd_bitmap_game_2, epd_bitmap_game_3, epd_bitmap_game_4,
      epd_bitmap_game_5, epd_bitmap_game_6, epd_bitmap_game_7, epd_bitmap_game_8, epd_bitmap_game_9};

  // Array para la animación del número oculto/interrogación
  const unsigned char *const game_guess_animation[] PROGMEM = {
      epd_bitmap_game_guess0, epd_bitmap_game_guess1};

  // Solo dibujamos si menu_index NO es -1
  if (menu_index != -1)
  {
    display.drawBitmap(menu_x[menu_index], menu_y[menu_index], menu_icons[menu_index], 16, 16, WHITE);
  }

  // --- ¡NUEVO! DIBUJO AUTOMÁTICO DEL ICONO DE ATENCIÓN ---
  if (pet.needs_attention == true)
  {
    // Dibujamos directamente el icono 7 en su posición, sea cual sea el estado del menú
    display.drawBitmap(menu_x[7], menu_y[7], menu_icons[7], 16, 16, WHITE);
  }

  // --- DIBUJAR LAS CACAS (Agrupadas a la derecha y animadas) ---
  // Solo visibles en IDLE y ACCIONES para no manchar menús
  if (pet.estado_actual == ESTADO_IDLE || pet.estado_actual == ESTADO_ACCION)
  {
    const unsigned char *frame_caca = frame_animation ? epd_bitmap_poop_1 : epd_bitmap_poop_0;
    if (pet.poop_counter >= 1)
      display.drawBitmap(105, 30, frame_caca, 16, 16, WHITE);
    if (pet.poop_counter >= 2)
      display.drawBitmap(87, 30, frame_caca, 16, 16, WHITE);
    if (pet.poop_counter >= 3)
      display.drawBitmap(105, 18, frame_caca, 16, 16, WHITE);
    if (pet.poop_counter >= 4)
      display.drawBitmap(87, 18, frame_caca, 16, 16, WHITE);
  }

  // EL CONTENIDO DINÁMICO (Depende del estado)
  if (pet.estado_actual == ESTADO_IDLE)
  {
    // Si está enfermo, usamos el sprite "sad"
    if (pet.health_status == 1)
    {
      // Usamos el frame triste alternando con el latido
      display.drawBitmap(48, 16, frame_animation ? epd_bitmap_a1_1_sad : epd_bitmap_a1_2, 32, 32, WHITE);
    }
    // Si está sano, usamos los frames normales
    else
    {
      display.drawBitmap(48, 16, frame_animation ? epd_bitmap_a1_2 : epd_bitmap_a1_1, 32, 32, WHITE);
    }
  }
  else if (pet.estado_actual == ESTADO_ACCION)
  {
    long elapsed = millis() - pet.action_start;
    int fase = elapsed / 1000; // 0, 1 o 2

    if (pet.current_action == 'n') // Animación: Nope (Negación)
    {
      const unsigned char *nope_frame = frame_animation ? epd_bitmap_a1_1_nope_1 : epd_bitmap_a1_1_nope_0;
      display.drawBitmap(48, 16, nope_frame, 32, 32, WHITE);
    }
    else if (pet.current_action == 'm') // Animación: Medicina
    {
      // Icono de cura a la izquierda (fases 0, 1, 2)
      const unsigned char *heal_icon = (fase == 0) ? epd_bitmap_heal_0 : (fase == 1 ? epd_bitmap_heal_1 : epd_bitmap_heal_2);
      display.drawBitmap(24, 24, heal_icon, 16, 16, WHITE);
      // Mascota: triste hasta que se cura al final (fase 2)
      const unsigned char *p_med = (fase < 2) ? epd_bitmap_a1_1_sad : epd_bitmap_a1_1_happy;
      display.drawBitmap(48, 16, p_med, 32, 32, WHITE);
    }
    else if (pet.current_action == 'l') // Animación: Limpiar
    {
      const unsigned char *pet_frame = frame_animation ? epd_bitmap_a1_1_happy : epd_bitmap_a1_1;
      display.drawBitmap(48, 16, pet_frame, 32, 32, WHITE);
      int sweep_x = 128 - (elapsed * 146 / 3000);
      display.drawBitmap(sweep_x, 16, epd_bitmap_clean_lines, 18, 32, WHITE);
    }
    else if (pet.current_action == 'c' || pet.current_action == 's') // Animación: Comer
    {
      const unsigned char *pet_frame;
      const unsigned char *food_frame;

      if (fase == 0)
      {
        pet_frame = epd_bitmap_a1_1_eat_0;
        food_frame = (pet.current_action == 'c') ? epd_bitmap_food_eat_0 : epd_bitmap_snack_eat_0;
      }
      else if (fase == 1)
      {
        pet_frame = epd_bitmap_a1_1_eat_1;
        food_frame = (pet.current_action == 'c') ? epd_bitmap_food_eat_1 : epd_bitmap_snack_eat_1;
      }
      else
      {
        pet_frame = epd_bitmap_a1_1_eat_0;
        food_frame = epd_bitmap_crumbs_eat;
      }

      display.drawBitmap(48, 16, pet_frame, 32, 32, WHITE);
      display.drawBitmap(24, 24, food_frame, 16, 16, WHITE);
    }
  }
  else if (pet.estado_actual == ESTADO_SUBMENU_COMIDA)
  {
    // Dibujamos el icono de la comida a la izquierda y el snack a la derecha
    display.drawBitmap(32, 24, epd_bitmap_food_eat_0, 16, 16, WHITE);
    display.drawBitmap(80, 24, epd_bitmap_snack_eat_0, 16, 16, WHITE);

    // Determinamos dónde dibujar la flecha selectora
    int selector_x;
    if (submenu_index == 0)
    {
      selector_x = 16;
    }
    else
    {
      selector_x = 64;
    }

    // Dibujamos el selector animado con tu sprite
    display.drawBitmap(selector_x, 24, epd_bitmap_selector_0, 16, 16, WHITE);
  }

  else if (pet.estado_actual == ESTADO_SUBMENU_STATS)
  {
    display.setTextSize(1);
    display.setTextColor(WHITE);

    if (submenu_index == 0) // PÁGINA 0: Edad y Peso
    {
      display.setCursor(16, 16);
      display.print("EDAD: ");
      display.print(pet.stage);

      display.setCursor(16, 36);
      display.print("PESO: ");
      display.print(pet.weight);
      display.print("g");
    }
    else if (submenu_index == 1) // PÁGINA 1: Hambre
    {
      display.setCursor(16, 16);
      display.print("HAMBRE:");

      // Dibujamos 4 corazones (separados por 20 píxeles para que respiren)
      for (int i = 0; i < 4; i++)
      {
        if (i < pet.hunger)
        {
          // Corazón Lleno
          display.drawBitmap(16 + (i * 20), 32, epd_bitmap_fullheart_0, 16, 16, WHITE);
        }
        else
        {
          // Corazón Vacío
          display.drawBitmap(16 + (i * 20), 32, epd_bitmap_emptyheart_0, 16, 16, WHITE);
        }
      }
    }
    else if (submenu_index == 2) // PÁGINA 2: Felicidad
    {
      display.setCursor(16, 16);
      display.print("FELICIDAD:");

      // Dibujamos 4 corazones
      for (int i = 0; i < 4; i++)
      {
        if (i < pet.happiness)
        {
          // Corazón Lleno
          display.drawBitmap(16 + (i * 20), 32, epd_bitmap_fullheart_0, 16, 16, WHITE);
        }
        else
        {
          // Corazón Vacío
          display.drawBitmap(16 + (i * 20), 32, epd_bitmap_emptyheart_0, 16, 16, WHITE);
        }
      }
    }
  }

  else if (pet.estado_actual == ESTADO_GAME)
  {
    // Fase 0: SPLASH SCREEN
    if (pet.game_phase == 0)
    {
      display.setTextSize(1);
      display.setTextColor(WHITE);
      display.setCursor(32, 28);
      display.print("GAME START");
    }
    // Fase 1: ESPERANDO INPUT
    else if (pet.game_phase == 1)
    {
      display.drawBitmap(16, 24, game_numbers[pet.current_number], 16, 16, WHITE);              // Izq: Actual
      display.drawBitmap(96, 24, game_guess_animation[frame_animation ? 1 : 0], 16, 16, WHITE); // Der: Hueco animado

      // Centro: Mascota Animación Idle (Alterna a1_1 y a1_2)
      display.drawBitmap(48, 16, frame_animation ? epd_bitmap_a1_2 : epd_bitmap_a1_1, 32, 32, WHITE);
    }
    // Fase 2: MOSTRAR RESULTADO DE LA RONDA
    else if (pet.game_phase == 2)
    {
      display.drawBitmap(16, 24, game_numbers[pet.current_number], 16, 16, WHITE); // Izq: Actual
      display.drawBitmap(96, 24, game_numbers[pet.next_number], 16, 16, WHITE);    // Der: Revelado

      // Centro: Animación Happy o Sad alternando con el frame base a1_1
      const unsigned char *pet_frame;
      if (pet.last_guess_won)
      {
        pet_frame = frame_animation ? epd_bitmap_a1_1_happy : epd_bitmap_a1_1;
      }
      else
      {
        pet_frame = frame_animation ? epd_bitmap_a1_1_sad : epd_bitmap_a1_1;
      }
      display.drawBitmap(48, 16, pet_frame, 32, 32, WHITE);
    }
    // Fase 3: FIN DEL JUEGO (Resumen)
    else if (pet.game_phase == 3)
    {
      uint8_t losses = (pet.game_round - 1) - pet.game_wins;                  // Calculamos los fallos
      display.drawBitmap(16, 24, game_numbers[pet.game_wins], 16, 16, WHITE); // Izq: Aciertos
      display.drawBitmap(96, 24, game_numbers[losses], 16, 16, WHITE);        // Der: Fallos

      // Centro: Happy si ganó 3 o más, Sad si perdió (Alternando)
      const unsigned char *pet_frame;
      if (pet.game_wins >= 3)
      {
        pet_frame = frame_animation ? epd_bitmap_a1_1_happy : epd_bitmap_a1_1;
      }
      else
      {
        pet_frame = frame_animation ? epd_bitmap_a1_1_sad : epd_bitmap_a1_1;
      }
      display.drawBitmap(48, 16, pet_frame, 32, 32, WHITE);
    }
  }

  // Líneas separadoras de la interfaz
  // display.drawLine(0, 17, 128, 17, WHITE);
  // display.drawLine(0, 46, 128, 46, WHITE);

  display.display(); // Enviamos todo a la pantalla a la vez
}

void setup()
{
  Serial.begin(115200);
  randomSeed(analogRead(0)); // Genera una semilla aleatoria leyendo ruido analógico
  Serial.println("Hello, Tama32!");

  // Estado inicial
  pet.hunger = 2;
  pet.happiness = 2;
  pet.health_status = 0;
  pet.weight = 5;
  pet.stage = 0;
  pet.type = 1; // Tipo base
  pet.birth_time = millis();

  pet.care_mistakes = 0;
  pet.mistake_processed = false;
  pet.needs_attention = false;
  pet.poop_counter = 0;
  pet.discipline = 0;
  pet.estado_actual = ESTADO_IDLE;

  // Inicializamos los relojes
  pet.last_poop_time = millis();
  pet.last_hunger_time = millis();
  pet.last_happiness_time = millis();

  // Configuramos la velocidad del metabolismo del bebé (en ms)
  pet.hunger_interval = 20000;    // Baja stats cada 20s
  pet.happiness_interval = 25000; // Baja stats cada 25s
  pet.poop_interval = 15000;      // Caca cada 15s

  // Configuración de botones
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_C, INPUT_PULLUP);

  // INICIALIZACIÓN DE LA PANTALLA OLED
  // El 0x3C es la dirección I2C estándar de estas pantallas
  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("Fallo al iniciar la pantalla OLED");
    for (;;)
      ; // Bucle infinito si falla
  }
  display.clearDisplay(); // Limpia la memoria de la pantalla
}

void loop()
{
  uint32_t horaActual = millis();

  // --- VARIABLES PARA EL BOTÓN A ---
  static bool estadoAnteriorA = HIGH; // 'static' recuerda el valor entre vueltas del loop
  static uint32_t ultimoToqueA = 0;   // Cronómetro para el antirrebote
  bool lecturaA;

  // --- VARIABLES PARA EL BOTÓN B ---
  static bool estadoAnteriorB = HIGH;
  static uint32_t ultimoToqueB = 0;
  bool lecturaB;

  // --- VARIABLES PARA EL BOTÓN C ---
  static bool estadoAnteriorC = HIGH;
  static uint32_t ultimoToqueC = 0;
  bool lecturaC;

  switch (pet.estado_actual)
  {
  case ESTADO_IDLE:

    // --- LECTURA DEL BOTÓN A (Navegar) ---
    lecturaA = digitalRead(BTN_A);

    // Si el estado cambia (al pulsar o al soltar) Y han pasado 50ms desde la última vibración
    if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
    {
      if (lecturaA == LOW) // Solo movemos el cursor si el cambio fue Hacia Abajo (Pulsar)
      {
        menu_index++;
        // LIMITAMOS A 6: Así el icono 7 (Atención) nunca se selecciona
        if (menu_index > 6)
        {
          menu_index = 0;
        }
        actualizarPantalla(); // Forzamos el redibujado instantáneo
      }

      // Actualizamos cronómetro y estado para AMBOS movimientos (pulsar y soltar)
      ultimoToqueA = horaActual;
      estadoAnteriorA = lecturaA;
    }

    // --- LECTURA DEL BOTÓN B (Confirmar) ---
    lecturaB = digitalRead(BTN_B);
    if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
    {
      if (lecturaB == LOW && menu_index != -1)
      {
        if (menu_index == 0) // COMIDA
        {
          if (pet.health_status == 1)
          { // ¡NUEVO! Comprobamos ANTES de entrar
            pet.current_action = 'n';
            pet.estado_actual = ESTADO_ACCION;
            pet.action_start = horaActual;
          }
          else
          {
            pet.estado_actual = ESTADO_SUBMENU_COMIDA;
            submenu_index = 0;
          }
        }
        else if (menu_index == 1) // JUEGO
        {
          if (pet.health_status == 1)
          { // Enfermo = Nope
            pet.current_action = 'n';
            pet.estado_actual = ESTADO_ACCION;
            pet.action_start = horaActual;
          }
          else
          {
            pet.estado_actual = ESTADO_GAME;
            pet.game_round = 1;
            pet.game_wins = 0;
            pet.current_number = random(1, 9);
            pet.game_phase = 0;
            pet.action_start = horaActual;
            Serial.println(">>> ¡Entrando al juego de números! 🎮");
          }
        }
        else if (menu_index == 2) // BAÑO
        {
          if (pet.poop_counter > 0)
          {
            pet.poop_counter = 0;
            pet.poop_sickness_triggered = false; // ¡Reseteamos el seguro al limpiar!
            pet.current_action = 'l';
            pet.estado_actual = ESTADO_ACCION;
            pet.action_start = horaActual;
          }
        }
        else if (menu_index == 3) // MEDICINA
        {
          if (pet.health_status > 0)
          {
            pet.health_status = 0;

            pet.needs_attention = false;
            pet.mistake_processed = false;

            pet.current_action = 'm';
            pet.estado_actual = ESTADO_ACCION;
            pet.action_start = horaActual;
          }
        }
        else if (menu_index == 4) // DISCIPLINA
        {
          if (pet.needs_attention && pet.hunger > 1 && pet.happiness > 1)
          {
            pet.discipline = min((int)pet.discipline + 25, 100);
            pet.needs_attention = false;
            pet.current_action = 'd';
            pet.estado_actual = ESTADO_ACCION;
            pet.action_start = horaActual;
          }
        }
        else if (menu_index == 5) // ESTADÍSTICAS
        {
          pet.estado_actual = ESTADO_SUBMENU_STATS;
          submenu_index = 0; // Usamos esta misma variable para saber en qué "página" estamos
        }

        menu_index = -1; // Apagamos el cursor tras elegir
        actualizarPantalla();
      }
      ultimoToqueB = horaActual;
      estadoAnteriorB = lecturaB;
    }

    // --- LECTURA DEL BOTÓN C (Cancelar) ---
    lecturaC = digitalRead(BTN_C);
    if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
    {
      if (lecturaC == LOW && menu_index != -1)
      {
        menu_index = -1;      // Deseleccionamos el menú
        actualizarPantalla(); // Borramos el fondo blanco del icono
        Serial.println(">>> Menú cancelado. ❌");
      }
      ultimoToqueC = horaActual;
      estadoAnteriorC = lecturaC;
    }

    // 1. RELOJ DE EVOLUCIÓN (Bebé a Niño)
    if (horaActual - pet.birth_time >= 10000 && pet.stage == 0)
    {
      pet.stage = 1;
      Serial.println(">>> ¡Tu mascota ha evolucionado a etapa 1! 🐣");
    }

    // 2. RELOJ METABÓLICO (Hambre y Felicidad)
    if (horaActual - pet.last_hunger_time >= pet.hunger_interval)
    {
      if (pet.hunger > 0)
        pet.hunger--;
      pet.last_hunger_time = horaActual;
      Serial.println("¡El metabolismo avanza! (-1 Hambre) 📉");
    }
    if (horaActual - pet.last_happiness_time >= pet.happiness_interval)
    {
      if (pet.happiness > 0)
        pet.happiness--;
      pet.last_happiness_time = horaActual;
      Serial.println("¡El metabolismo avanza! (-1 Felicidad) 📉");
    }

    // 3. RELOJ DIGESTIVO (Cacas)
    if (pet.poop_counter < 4 && horaActual - pet.last_poop_time >= pet.poop_interval)
    {
      pet.poop_counter++;
      pet.last_poop_time = horaActual;
      Serial.println("¡La mascota ha hecho caca! 💩");
    }

    // 4. CHEQUEOS DE SALUD Y ATENCIÓN (Se evalúan constantemente)
    if (pet.care_mistakes == 5 && pet.estado_actual != ESTADO_MUERTE)
    {
      pet.health_status = 2;
      pet.estado_actual = ESTADO_MUERTE;
      Serial.println(">>> ¡La mascota ha muerto por descuido! ☠️");
      return;
    }

    if (pet.poop_counter == 4 && pet.health_status == 0 && !pet.poop_sickness_triggered)
    {
      pet.health_status = 1;
      pet.poop_sickness_triggered = true; // Se activa el seguro
      Serial.println("¡La mascota está enferma por la suciedad! 🤢");
    }

    if (pet.hunger == 0 || pet.happiness == 0)
    {
      if (pet.needs_attention == false)
      {
        pet.attention_start = horaActual;
      }
      pet.needs_attention = true;

      // Care Mistakes si pasan 10s ignorándola
      if (horaActual - pet.attention_start >= 10000 && pet.mistake_processed == false)
      {
        pet.care_mistakes++;
        pet.mistake_processed = true;
      }
    }

    // 5. RELOJ DE INTERFAZ (Consola cada 1 segundo)
    if (horaActual - tiempoUltimoLatido >= INTERVALO)
    {
      Serial.println("------------------------------");
      Serial.print("Hambre: ");
      Serial.print(pet.hunger);
      Serial.print(" | Felicidad: ");
      Serial.println(pet.happiness);
      Serial.print("Disciplina: ");
      Serial.println(pet.discipline);
      Serial.print("Care Mistakes: ");
      Serial.println(pet.care_mistakes);
      Serial.print("Cacas: ");
      Serial.println(pet.poop_counter);
      Serial.print("Salud: ");
      Serial.println(pet.health_status);
      Serial.print("Peso: ");
      Serial.println(pet.weight);
      Serial.print("Etapa: ");
      Serial.print(pet.stage);
      Serial.print(" | Tipo: ");
      Serial.println(pet.type);

      // Aviso de atención
      if (pet.needs_attention == true)
      {
        Serial.println("¡AVISO: La mascota te necesita! 🚨");
      }

      // DISCIPLINA (0.1%)
      if (random(0, 1000) < 1)
      {
        if (pet.hunger > 1 && pet.happiness > 1)
        {
          pet.needs_attention = true;
          Serial.println("¡Berrinche! La mascota quiere atención sin motivo. 😤");
        }
      }

      // --- ACTUALIZACIÓN VISUAL ---
      frame_animation = !frame_animation;
      actualizarPantalla();

      tiempoUltimoLatido = horaActual;
    }

  case ESTADO_MENU:
    break;

  case ESTADO_ACCION:
    if (horaActual - pet.action_start >= 3000)
    {
      // --- SALIDA DEL ESTADO ---
      pet.estado_actual = ESTADO_IDLE;
      actualizarPantalla();

      if (pet.current_action == 'c' || pet.current_action == 's')
        Serial.println(">>> La mascota ha terminado de comer. 🥣");
      else if (pet.current_action == 'l')
        Serial.println(">>> ¡Todo limpio y reluciente! ✨");
      else if (pet.current_action == 'm')
        Serial.println(">>> La mascota se ha curado. ❤️");
      else if (pet.current_action == 'd')
        Serial.println(">>> La mascota se ha disciplinado. 📏");
    }
    else
    {
      // --- 1. NUEVO LATIDO RÁPIDO (Animación fluida a 20 FPS) ---
      static uint32_t ultimoFrameAnimacion = 0;
      if (horaActual - ultimoFrameAnimacion >= 50)
      {
        actualizarPantalla(); // Esto llama a la matemática de la escoba constantemente
        ultimoFrameAnimacion = horaActual;
      }

      // --- 2. TU LATIDO NORMAL (1 segundo) ---
      if (horaActual - tiempoUltimoLatido >= INTERVALO)
      {
        frame_animation = !frame_animation; // Alternamos la cara de la mascota

        if (pet.current_action == 'c' || pet.current_action == 's')
          Serial.println("¡Ñam!");
        else if (pet.current_action == 'l')
          Serial.println("¡Fiu, fiu! 🧽");
        else if (pet.current_action == 'm')
          Serial.println("Curando... 💊");
        else if (pet.current_action == 'd')
          Serial.println("Disciplinando... 📏");

        tiempoUltimoLatido = horaActual;
      }
    }
    break;

  case ESTADO_SUBMENU_COMIDA:
    // --- LECTURA BOTÓN A (Navegar 0 -> 1 -> 0) ---
    lecturaA = digitalRead(BTN_A);
    if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
    {
      if (lecturaA == LOW)
      {
        submenu_index = (submenu_index == 0) ? 1 : 0; // Alterna entre 0 (Comida) y 1 (Snack)
        actualizarPantalla();
      }
      ultimoToqueA = horaActual;
      estadoAnteriorA = lecturaA;
    }

    // --- LECTURA BOTÓN B (Confirmar selección de comida) ---
    lecturaB = digitalRead(BTN_B);
    if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
    {
      if (lecturaB == LOW)
      {
        // 1. PRIMERO: ¿Está enferma la mascota? 🤢
        if (pet.health_status == 1)
        {
          // Si está enfermo, no importa lo que elijas: la acción será "Nope"
          pet.current_action = 'n';          // 'n' activará los sprites de negación
          pet.estado_actual = ESTADO_ACCION; // Saltamos a la animación
          pet.action_start = horaActual;     // Empezamos a contar los 3 segundos
        }
        // 2. SI ESTÁ SANA: Miramos qué ha elegido 🍎🍬
        else
        {
          if (submenu_index == 0) // Ha elegido COMIDA
          {
            // Solo come si no está totalmente lleno (máximo 4)
            if (pet.hunger < 4)
            {
              pet.current_action = 'c'; // Acción de comer bol
              pet.hunger++;             // Aumentamos saciedad
              pet.weight++;             // El alimento base sube 1g

              pet.needs_attention = false;   // Si le das de comer, se le quita el aviso de atención (si lo tenía)
              pet.mistake_processed = false; // Reseteamos el proceso de care mistake para que no cuente como fallo si se le da de comer tras tener hambre 0

              pet.estado_actual = ESTADO_ACCION;
              pet.action_start = horaActual;
            }
            else
            {
              // Si está lleno, simplemente cancelamos y volvemos a la pantalla principal
              pet.current_action = 'n';          // 'n' activará los sprites de negación
              pet.estado_actual = ESTADO_ACCION; // Saltamos a la animación
              pet.action_start = horaActual;     // Empezamos a contar los 3 segundos
            }
          }
          else if (submenu_index == 1) // Ha elegido SNACK
          {
            // El snack siempre lo acepta si está sano, ¡es un capricho!
            pet.current_action = 's'; // Acción de comer snack
            if (pet.happiness < 4)
              pet.happiness++; // Sube felicidad
            pet.weight += 2;   // Pero el azúcar engorda más (2g)

            pet.needs_attention = false;
            pet.mistake_processed = false;

            pet.estado_actual = ESTADO_ACCION;
            pet.action_start = horaActual;
          }
        }

        // 2. FINALIZACIÓN
        menu_index = -1;      // Quitamos el cursor del menú por si acaso
        actualizarPantalla(); // Refrescamos para que desaparezca el menú y empiece el dibujo
      }
      ultimoToqueB = horaActual;
      estadoAnteriorB = lecturaB;
    }

    // --- LECTURA BOTÓN C (Cancelar/Volver) ---
    lecturaC = digitalRead(BTN_C);
    if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
    {
      if (lecturaC == LOW)
      {
        pet.estado_actual = ESTADO_IDLE; // Volvems a la pantalla principal
        actualizarPantalla();
      }
      ultimoToqueC = horaActual;
      estadoAnteriorC = lecturaC;
    }
    break;

  case ESTADO_SUBMENU_STATS:
    // --- Botón A: Pasar de página (0, 1, 2) ---
    lecturaA = digitalRead(BTN_A);
    if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
    {
      if (lecturaA == LOW)
      {
        submenu_index++;
        if (submenu_index > 2)
          submenu_index = 0; // Si pasa de la pág 2, vuelve a la 0
        actualizarPantalla();
      }
      ultimoToqueA = horaActual;
      estadoAnteriorA = lecturaA;
    }

    // --- Botón C: Salir al menú principal ---
    lecturaC = digitalRead(BTN_C);
    if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
    {
      if (lecturaC == LOW)
      {
        pet.estado_actual = ESTADO_IDLE;
        actualizarPantalla();
      }
      ultimoToqueC = horaActual;
      estadoAnteriorC = lecturaC;
    }
    // (Opcional: puedes hacer que el botón B también sirva para salir o pasar página si quieres)
    break;

  case ESTADO_EVOLUCION:
    break;

  case ESTADO_MUERTE:
    // --- LÓGICA DE REINICIO POR BOTONES (A + C) ---
    lecturaA = digitalRead(BTN_A);
    lecturaC = digitalRead(BTN_C);

    // Faltaba este 'if':
    if (lecturaA == LOW && lecturaC == LOW)
    {
      Serial.println(">>> REINICIO TOTAL: Combinación A+C detectada. 🔄");
      setup();
    }
    break;

  case ESTADO_GAME:

    // --- EL LATIDO CENTRAL (El único reloj que manda) ---
    if (horaActual - tiempoUltimoLatido >= INTERVALO)
    {
      frame_animation = !frame_animation;
      pet.game_ticks++; // Contamos un latido más
      tiempoUltimoLatido = horaActual;

      // Evaluamos las transiciones de fase en perfecta sincronía con el dibujo
      if (pet.game_phase == 0 && pet.game_ticks >= 1)
      {
        pet.game_phase = 1; // Terminó el Splash
      }
      else if (pet.game_phase == 2 && pet.game_ticks >= 2)
      { // Han pasado 2 latidos
        pet.game_round++;
        if (pet.game_wins >= 3 || pet.game_round > 5)
        {
          pet.game_phase = 3; // Fin del juego
          pet.game_ticks = 0; // Reiniciamos ticks para la pantalla final
          frame_animation = true;
        }
        else
        {
          pet.current_number = pet.next_number;
          pet.game_phase = 1;
        }
      }
      else if (pet.game_phase == 3 && pet.game_ticks >= 3)
      { // Han pasado 3 latidos
        if (pet.game_wins >= 3)
        {
          pet.happiness++;
          if (pet.weight > 0)
            pet.weight--;

          pet.needs_attention = false;
          pet.mistake_processed = false;

          Serial.println(">>> ¡Juego Ganado! 🏆");
        }
        pet.estado_actual = ESTADO_IDLE;
      }

      actualizarPantalla(); // Se dibuja UNA sola vez
    }

    // --- FASE 1: ESPERANDO INPUT (Respuesta instantánea) ---
    if (pet.game_phase == 1)
    {
      bool input_received = false;
      bool eligio_mayor = false;

      // Lectura Botón A (Menor)
      lecturaA = digitalRead(BTN_A);
      if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
      {
        if (lecturaA == LOW)
        {
          input_received = true;
          eligio_mayor = false;
        }
        ultimoToqueA = horaActual;
        estadoAnteriorA = lecturaA;
      }
      // Lectura Botón B (Mayor)
      lecturaB = digitalRead(BTN_B);
      if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
      {
        if (lecturaB == LOW)
        {
          input_received = true;
          eligio_mayor = true;
        }
        ultimoToqueB = horaActual;
        estadoAnteriorB = lecturaB;
      }

      // Lectura Botón C (Cancelar)
      lecturaC = digitalRead(BTN_C);
      if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
      {
        if (lecturaC == LOW)
        {
          pet.estado_actual = ESTADO_IDLE;
          actualizarPantalla();
        }
        ultimoToqueC = horaActual;
        estadoAnteriorC = lecturaC;
      }

      if (input_received)
      {
        do
        {
          pet.next_number = random(0, 10);
        } while (pet.next_number == pet.current_number);

        if ((eligio_mayor && pet.next_number > pet.current_number) ||
            (!eligio_mayor && pet.next_number < pet.current_number))
        {
          pet.last_guess_won = true;
          pet.game_wins++;
        }
        else
        {
          pet.last_guess_won = false;
        }

        pet.game_phase = 2;

        // --- REINICIO MAESTRO INSTANTÁNEO ---
        pet.game_ticks = 0;
        frame_animation = true;          // Forzamos la cara de reacción
        tiempoUltimoLatido = horaActual; // Reseteamos el reloj maestro
        actualizarPantalla();            // Dibujo inmediato
      }
    }
    break;
  }
}
