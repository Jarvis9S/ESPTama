#include <Arduino.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "sprites.h"

#include <Preferences.h>
Preferences memoria; // Creamos el objeto para manejar el guardado

#define SCREEN_WIDTH 128 // Ancho de la pantalla OLED
#define SCREEN_HEIGHT 64 // Alto de la pantalla OLED
#define OLED_RESET -1    // Reset (algunas pantallas no tienen, usamos -1)
Adafruit_SSD1306 display(SCREEN_WIDTH, SCREEN_HEIGHT, &Wire, OLED_RESET);

#define BTN_A 13
#define BTN_B 12
#define BTN_C 14

// --- AUDIO ---
#define PIN_BUZZER 25

enum SonidosUI
{
  SND_NAVEGAR,
  SND_CONFIRMAR,
  SND_CANCELAR,
  SND_ERROR,
  SND_LLAMADA,

  SND_COMER,          // "Ñam" corto
  SND_CURAR,          // Pitido mágico
  SND_ACIERTO,        // Acierto en minijuego
  SND_FALLO,          // Fallo en minijuego
  SND_VICTORIA,       // Ganar el minijuego / Evolucionar
  SND_DERROTA,        // Perder el minijuego
  SND_EVOLUCION_TICK, // Destello de evolución
  SND_MUERTE          // Tono fúnebre
};

// TAMAGOTCHI PDP - ALPHA 0.2

// Mapa de estados
enum EstadosMascota
{
  ESTADO_RELOJ,          // Configuración de la hora
  ESTADO_VER_RELOJ,      // Solo mirar la hora
  ESTADO_BOOT,           // Animación de inicio
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
  uint16_t age_days;            // Días reales de vida
  uint8_t type;                 // Variante de la mascota (depende de los cuidados)
  uint32_t birth_time;          // Momento del nacimiento en milisegundos
  EstadosMascota estado_actual; // Estado actual de la mascota
  int8_t hunger;                // Hambre (0 a 4)
  int8_t happiness;             // Felicidad (0 a 4)
  uint16_t weight;              // Peso en gramos (0 a 9999)
  uint8_t health_status;        // Estado de salud (0 a 4)
  uint8_t poop_counter;         // Contador de cacas (0 a 4)
  uint8_t discipline;           // Nivel de disciplina (0 a 100)
  uint8_t snack_count;          // Cuenta los snacks consecutivos
  uint32_t sickness_start;      // Cuándo empezó a estar enfermo
  uint32_t starvation_start;    // Cuándo llegó el hambre a 0
  bool is_sleeping;             // Si la mascota está durmiendo
  bool is_light_on;             // Si la luz está encendida
  uint32_t sleep_start;         // Para saber cuánto ha dormido
  uint32_t evolution_timer;     // Tiempo acumulado para la siguiente fase
  uint32_t last_session_time;   // Para calcular el delta al guardar

  // Ritmos biológicos (¡NUEVO!)
  uint32_t hunger_interval;    // Cuánto tarda en bajar el hambre
  uint32_t happiness_interval; // Cuánto tarda en bajar la felicidad
  uint32_t poop_interval;      // Cuánto tarda en hacerse caca
  uint32_t dirt_accumulation;  // Medidor de toxicidad ambiental (P1/P2)

  // --- Horarios ---
  uint8_t sleep_hour;
  uint8_t wake_hour;

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

  // Posición en la pantalla
  int16_t x; // Posición horizontal
  int16_t y; // Posición vertical
};

// --- CONSTANTES DE METABOLISMO BASE (1.0x) ---
// MODO DEBUG: Base = 1 minuto (60000 ms)
const uint32_t BASE_HUNGER = 3600000;    // 1 hora
const uint32_t BASE_HAPPINESS = 3600000; // 1 hora
const uint32_t BASE_POOP = 7200000;      // 2 horas

// Paso del tiempo de la interfaz
uint32_t tiempoUltimoLatido = 0;
const uint32_t INTERVALO = 1000; // 1 segundo para refrescar la consola

MascotaData pet;

bool pantalla_encendida = true;
uint32_t ultimo_toque_global = 0;

// --- RELOJ DEL SISTEMA ---
uint8_t reloj_h = 12;          // Horas (0-23)
uint8_t reloj_m = 0;           // Minutos (0-59)
uint32_t ultimo_minuto_ms = 0; // Cronómetro para que pasen los minutos
uint8_t reloj_fase = 0;        // 0 = Ajustando Horas, 1 = Ajustando Minutos

bool frame_animation = false;
uint8_t idle_frame_index = 0; // Para ciclos de 3 frames
int8_t menu_index = -1; // -1 significa que no hay nada seleccionado
int8_t submenu_index = 0;

void guardarPartida()
{
  memoria.begin("tama_save", false);                       // Abrimos la carpeta "tama_save"
  memoria.putBytes("pet_data", &pet, sizeof(MascotaData)); // Guardamos TODO el struct
  // También guardamos la hora del sistema
  memoria.putUInt("reloj_h", reloj_h);
  memoria.putUInt("reloj_m", reloj_m);
  memoria.end();
  Serial.println(">>> 💾 Partida guardada en la memoria flash.");
}

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

  //  DIBUJO AUTOMÁTICO DEL ICONO DE ATENCIÓN
  if (pet.needs_attention && pet.estado_actual == ESTADO_IDLE)
  {
    // Dibujamos directamente el icono 7 en su posición, sea cual sea el estado del menú
    display.drawBitmap(menu_x[7], menu_y[7], menu_icons[7], 16, 16, WHITE);
  }

  // --- DIBUJAR LAS CACAS (Agrupadas a la derecha y animadas) ---
  // Solo visibles en IDLE y ACCIONES para no manchar menús
  // Solo se ven las cacas si la luz está encendida
  if ((pet.estado_actual == ESTADO_IDLE || pet.estado_actual == ESTADO_ACCION) && pet.is_light_on)
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
  if (pet.estado_actual == ESTADO_RELOJ)
  {
    display.setTextSize(2);
    display.setTextColor(WHITE);

    // Título "RELOJ" (5 letras = 60px) -> X = 34
    display.setCursor(34, 0);
    display.print("RELOJ");

    display.setCursor(34, 32);

    if (reloj_h < 10)
      display.print("0");
    display.print(reloj_h);
    display.print(":");
    if (reloj_m < 10)
      display.print("0");
    display.print(reloj_m);

    // Subrayamos horas o minutos (ajustado al nuevo tamaño)
    if (reloj_fase == 0)
      display.drawFastHLine(34, 50, 22, WHITE); // Subraya Horas
    else
      display.drawFastHLine(70, 50, 22, WHITE); // Subraya Minutos
  }
  else if (pet.estado_actual == ESTADO_VER_RELOJ)
  {
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(32, 24);

    if (reloj_h < 10)
      display.print("0");
    display.print(reloj_h);
    display.print(":");
    if (reloj_m < 10)
      display.print("0");
    display.print(reloj_m);

    // (Opcional) Indicador de si es AM o PM si lo quieres hacer estilo 12h,
    // pero con formato 24h es suficiente.
  }
  else if (pet.estado_actual == ESTADO_BOOT)
  {
    display.setTextSize(2);
    display.setTextColor(WHITE);

    // Título centrado (8 letras = 96px) -> X = 16
    display.setCursor(16, 0);
    display.print("GUARDADO");

    // Opciones directas
    display.setCursor(10, 24);
    display.print("A:SEGUIR");

    display.setCursor(10, 44);
    display.print("C:BORRAR");
  }

  // EL CONTENIDO DINÁMICO (Depende del estado)
  if (pet.estado_actual == ESTADO_IDLE)
  {
    if (pet.is_light_on == false)
    {
      // --- LUZ APAGADA (MODO OSCURO) ---
      // Ya no rellenamos con fillRect blanco. El fondo es negro por defecto.

      if (pet.is_sleeping)
      {
        // Duerme a oscuras: Sprites de Zzz animados
        const unsigned char *sleep_off_frame = frame_animation ? epd_bitmap_sleep_lightoff_1 : epd_bitmap_sleep_lightoff_0;

        // Dibujamos el bitmap en blanco sobre el fondo negro
        display.drawBitmap(48, 16, sleep_off_frame, 32, 32, WHITE);
      }
      else
      {
        // Despierto a oscuras: Ojos blancos que rebotan 2px
        int y_ojos = 16 + (frame_animation ? -2 : 0);
        display.drawBitmap(48, y_ojos, epd_bitmap_eyes_dark_0, 32, 32, WHITE);
      }
    }
    else
    {
      // --- LUZ ENCENDIDA ---
      if (pet.is_sleeping)
      {
        // Duerme con la luz encendida (Mascota con ojos cerrados)
        const unsigned char *sleep_on_frame = frame_animation ? epd_bitmap_a1_1_sleep_1 : epd_bitmap_a1_1_sleep_0;
        display.drawBitmap(48, 16, sleep_on_frame, 32, 32, WHITE);
      }
      else
      {
        // Despierto y normal (Sano o Enfermo)
        if (pet.health_status == 1)
        {
          // Mascota enferma (Usa pet.x y pet.y que el motor ha fijado en el centro)
          display.drawBitmap(pet.x, pet.y, frame_animation ? epd_bitmap_a1_1_sad : epd_bitmap_a1_2, 32, 32, WHITE);

          // --- CALAVERA A LA IZQUIERDA ---
          // Aleada de la zona de cacas (X=25) y atada a la Y de la mascota
          int y_sick = pet.y + (frame_animation ? -2 : 0);
          display.drawBitmap(25, y_sick, epd_bitmap_sick_0, 16, 16, WHITE);
        }
        else
        {
          if (pet.stage == 0)
          {
            // --- DIBUJO DEL HUEVO ---
            // Alternamos entre los dos sprites que has creado
            const unsigned char *egg_frame = frame_animation ? epd_bitmap_egg1_1 : epd_bitmap_egg1_0;

            // Le damos un bote vertical de 2 píxeles para que parezca que palpita
            int y_huevo = 16 + (frame_animation ? 0 : 2);
            display.drawBitmap(48, y_huevo, egg_frame, 32, 32, WHITE);
          }
          else 
          {
            // --- DIBUJO DINÁMICO POR TIPO ---
            const unsigned char* sprite_actual;

            if (pet.type == 2) // EL TIZÓN
            {
              if (idle_frame_index == 0) sprite_actual = epd_bitmap_a2_1;
              else if (idle_frame_index == 1) sprite_actual = epd_bitmap_a2_2;
              else sprite_actual = epd_bitmap_a2_3;
            }
            else if (pet.type == 3) // EL BEBOTE
            {
              if (idle_frame_index == 0) sprite_actual = epd_bitmap_a3_1;
              else if (idle_frame_index == 1) sprite_actual = epd_bitmap_a3_2;
              else sprite_actual = epd_bitmap_a3_3;
            }
            else // Tipo 1 (Rayito) o Bebé (Stage 1)
            {
              // Estos siguen usando 2 frames por ahora
              sprite_actual = frame_animation ? epd_bitmap_a1_2 : epd_bitmap_a1_1;
            }

            display.drawBitmap(pet.x, pet.y, sprite_actual, 32, 32, WHITE);
          }
        }
      }
    }
  }
  else if (pet.estado_actual == ESTADO_ACCION)
  {
    long elapsed = millis() - pet.action_start;
    int fase = elapsed / 1000; // 0, 1 o 2

    if (pet.current_action == 'n') // Animación: Nope (Negación)
    {
      const unsigned char *nope_frame = frame_animation ? epd_bitmap_a1_1_nope_1 : epd_bitmap_a1_1_nope_0;
      display.drawBitmap(48, 16, nope_frame, 32, 32, WHITE); // Vuelve al centro
    }
    else if (pet.current_action == 'm') // Animación: Medicina
    {
      // Icono de cura a la izquierda
      const unsigned char *heal_icon = (fase == 0) ? epd_bitmap_heal_0 : (fase == 1 ? epd_bitmap_heal_1 : epd_bitmap_heal_2);
      display.drawBitmap(24, 24, heal_icon, 16, 16, WHITE);

      // Mascota en el centro
      const unsigned char *p_med = (fase < 2) ? epd_bitmap_a1_1_sad : epd_bitmap_a1_1_happy;
      display.drawBitmap(48, 16, p_med, 32, 32, WHITE);
    }
    else if (pet.current_action == 'l') // Animación: Limpiar (¡LA ÚNICA RELATIVA!)
    {
      // La mascota se queda donde estaba (pet.x, pet.y) y se pone feliz al final
      const unsigned char *pet_frame;
      if (elapsed < 2000)
        pet_frame = frame_animation ? epd_bitmap_a1_2 : epd_bitmap_a1_1;
      else
        pet_frame = frame_animation ? epd_bitmap_a1_1_happy : epd_bitmap_a1_2;

      display.drawBitmap(pet.x, pet.y, pet_frame, 32, 32, WHITE);

      // La escoba cruza toda la pantalla barriendo
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

      // Mascota al centro, comida a su izquierda (Clásico)
      display.drawBitmap(48, 16, pet_frame, 32, 32, WHITE);
      display.drawBitmap(24, 24, food_frame, 16, 16, WHITE);
    }
    else if (pet.current_action == 'd') // Animación: Disciplina (Regañar)
    {
      // Mascota al centro, triste
      display.drawBitmap(48, 16, epd_bitmap_a1_1_sad, 32, 32, WHITE);

      // El icono de disciplina rebota a la derecha
      int y_disc = 16 + (frame_animation ? -2 : 0);
      display.drawBitmap(85, y_disc, epd_bitmap_menu16_discipline_1, 16, 16, WHITE);
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

    // Dibujamos el selector animado
    // Si frame_animation es true, desplazamos la flecha 2px para que "vibre"
    int offset_anim = frame_animation ? 0 : 2;
    display.drawBitmap(selector_x, 24 + offset_anim, epd_bitmap_selector_0, 16, 16, WHITE);
  }

  else if (pet.estado_actual == ESTADO_SUBMENU_STATS)
  {
    display.setTextSize(2);
    display.setTextColor(WHITE);

    if (submenu_index == 0) // PÁGINA 0: Edad y Peso
    {
      display.setCursor(8, 16);
      display.print("EDAD: ");
      display.print(pet.age_days);

      display.setCursor(8, 40);
      display.print("PESO: ");
      display.print(pet.weight);
      display.print("g");
    }
    else if (submenu_index == 1) // PÁGINA 1: Hambre
    {
      display.setCursor(28, 8);
      display.print("HAMBRE");

      for (int i = 0; i < 4; i++)
      {
        const unsigned char *heart = (i < pet.hunger) ? epd_bitmap_fullheart_0 : epd_bitmap_emptyheart_0;
        display.drawBitmap(24 + (i * 20), 32, heart, 16, 16, WHITE);
      }
    }
    else if (submenu_index == 2) // PÁGINA 2: Felicidad
    {
      display.setCursor(34, 8);
      display.print("FELIZ");

      for (int i = 0; i < 4; i++)
      {
        const unsigned char *heart = (i < pet.happiness) ? epd_bitmap_fullheart_0 : epd_bitmap_emptyheart_0;
        display.drawBitmap(24 + (i * 20), 32, heart, 16, 16, WHITE);
      }
    }
    else if (submenu_index == 3) // PÁGINA 3: Disciplina
    {
      display.setCursor(34, 8);
      display.print("DISC.");

      display.drawRect(16, 32, 96, 16, WHITE); // Barra más gruesa
      int fill_width = (pet.discipline * 96) / 100;
      display.fillRect(16, 32, fill_width, 16, WHITE);
    }
  }

  else if (pet.estado_actual == ESTADO_GAME)
  {
    // Fase 0: SPLASH SCREEN
    if (pet.game_phase == 0)
    {
      display.setTextSize(2);
      display.setTextColor(WHITE);
      // "A JUGAR!" (8 letras = 96px) -> X = 16
      display.setCursor(16, 24);
      display.print("A JUGAR!");
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
  else if (pet.estado_actual == ESTADO_EVOLUCION)
  {
    if (pet.stage == 1)
    {
      // --- ANIMACIÓN A: ROMPER EL HUEVO (De stage 0 a stage 1) ---
      const unsigned char *break_frames[] = {
          epd_bitmap_eggbreak_0, epd_bitmap_eggbreak_1,
          epd_bitmap_eggbreak_2, epd_bitmap_eggbreak_3};

      if (pet.game_ticks < 10)
      {
        // Dividimos los 10 segundos de tensión entre los 4 frames de grietas
        int frame_idx = 0;
        if (pet.game_ticks > 2)
          frame_idx = 1;
        if (pet.game_ticks > 5)
          frame_idx = 2;
        if (pet.game_ticks > 7)
          frame_idx = 3;

        display.drawBitmap(48, 16, break_frames[frame_idx], 32, 32, WHITE);
      }
      else
      {
        // ¡TA-DA! Nace el bebé
        const unsigned char *happy_frame = frame_animation ? epd_bitmap_a1_1_happy : epd_bitmap_a1_2;
        display.drawBitmap(48, 16, happy_frame, 32, 32, WHITE);
      }
    }
    else
    {
      // --- ANIMACIÓN B: CRECIMIENTO NORMAL (Bebé a Niño, etc) ---
      const unsigned char *evolve_frames[] = {
          epd_bitmap_evolve_0, epd_bitmap_evolve_1, epd_bitmap_evolve_2,
          epd_bitmap_evolve_3, epd_bitmap_evolve_4, epd_bitmap_evolve_5};

      int secuencia[] = {0, 1, 2, 3, 4, 5, 4, 5, 4, 5};

      if (pet.game_ticks < 10)
      {
        int frame_idx = secuencia[pet.game_ticks];
        display.drawBitmap(48, 16, evolve_frames[frame_idx], 32, 32, WHITE);
      }
      else
      {
        // ¡TA-DA! Muestra la nueva mascota
        const unsigned char *happy_frame = frame_animation ? epd_bitmap_a1_1_happy : epd_bitmap_a1_2;
        display.drawBitmap(48, 16, happy_frame, 32, 32, WHITE);
      }
    }
  }
  else if (pet.estado_actual == ESTADO_MUERTE)
  {
    // Dibujamos la tumba centrada (48, 16 es la misma posicíon que la mascota)
    display.drawBitmap(48, 16, epd_bitmap_tomb_0, 32, 32, WHITE);
  }

  display.display(); // Enviamos todo a la pantalla a la vez
}

// --- ANIMACIÓN DE TRANSICIÓN DEL RELOJ ---
void animacionArrastre(bool haciaReloj)
{
  for (int i = 0; i <= 64; i += 8)
  {
    display.clearDisplay();

    int y_idle = haciaReloj ? -i : -64 + i;
    int y_reloj = haciaReloj ? 64 - i : i;

    // --- DIBUJAR ESTADO SEGÚN LA LUZ ---
    if (pet.is_light_on)
    {
      // --- MASCOTA: Ahora usa pet.x y pet.y ---
      const unsigned char *pet_frame;
      if (pet.health_status == 1)
        pet_frame = epd_bitmap_a1_1_sad;
      else if (pet.is_sleeping)
        pet_frame = epd_bitmap_a1_1_sleep_0;
      else {
        // Elegimos el frame base según el tipo
        if (pet.type == 2) pet_frame = epd_bitmap_a2_1;
        else if (pet.type == 3) pet_frame = epd_bitmap_a3_1;
        else pet_frame = epd_bitmap_a1_1;
      }

      // Dibujamos en su posición actual + el desplazamiento de la animación
      display.drawBitmap(pet.x, pet.y + y_idle, pet_frame, 32, 32, WHITE);

      // --- CACAS: Mantienen su X pero se desplazan en Y ---
      const unsigned char *frame_caca = epd_bitmap_poop_0;
      if (pet.poop_counter >= 1)
        display.drawBitmap(105, 30 + y_idle, frame_caca, 16, 16, WHITE);
      if (pet.poop_counter >= 2)
        display.drawBitmap(87, 30 + y_idle, frame_caca, 16, 16, WHITE);
      if (pet.poop_counter >= 3)
        display.drawBitmap(105, 18 + y_idle, frame_caca, 16, 16, WHITE);
      if (pet.poop_counter >= 4)
        display.drawBitmap(87, 18 + y_idle, frame_caca, 16, 16, WHITE);
    }
    else
    {
      // Luz OFF: Usamos pet.x y pet.y para los ojos o el sueño en la oscuridad
      if (pet.is_sleeping)
      {
        display.drawBitmap(pet.x, pet.y + y_idle, epd_bitmap_sleep_lightoff_0, 32, 32, WHITE);
      }
      else
      {
        display.drawBitmap(pet.x, pet.y + y_idle, epd_bitmap_eyes_dark_0, 32, 32, WHITE);
      }
    }

    // --- DIBUJAR RELOJ ---
    display.setTextSize(2);
    display.setTextColor(WHITE);
    display.setCursor(32, 24 + y_reloj);
    if (reloj_h < 10)
      display.print("0");
    display.print(reloj_h);
    display.print(":");
    if (reloj_m < 10)
      display.print("0");
    display.print(reloj_m);

    display.display();
    delay(30);
  }
}

void reproducirSonido(SonidosUI tipo)
{
  switch (tipo)
  {
  case SND_NAVEGAR:
    tone(PIN_BUZZER, 2000, 30);
    break;
  case SND_CONFIRMAR:
    tone(PIN_BUZZER, 1500, 80);
    break;
  case SND_CANCELAR:
    tone(PIN_BUZZER, 800, 80);
    break;
  case SND_ERROR:
    tone(PIN_BUZZER, 150, 300);
    break;
  case SND_LLAMADA:
    for (int rep = 0; rep < 3; rep++) // Repetimos la melodía 3 veces
    {
      for (int i = 0; i < 3; i++) // Los 6 bips (3 pares)
      {
        tone(PIN_BUZZER, 2500, 50);
        delay(80);
        tone(PIN_BUZZER, 2000, 50);
        delay(80);
      }

      // Añadimos una pausa de medio segundo entre repeticiones
      // pero solo si no es la última (para no esperar al final)
      if (rep < 2)
        delay(500);
    }
    break;

  case SND_COMER:
    tone(PIN_BUZZER, 1800, 40);
    break; // Agudo y muy corto
  case SND_CURAR:
    tone(PIN_BUZZER, 2200, 150);
    break; // Brillante
  case SND_ACIERTO:
    tone(PIN_BUZZER, 1200, 100);
    break; // Positivo
  case SND_FALLO:
    tone(PIN_BUZZER, 300, 150);
    break; // Grave
  case SND_VICTORIA:
    tone(PIN_BUZZER, 2000, 400);
    break; // Largo y agudo
  case SND_DERROTA:
    tone(PIN_BUZZER, 200, 400);
    break; // Largo y grave
  case SND_EVOLUCION_TICK:
    tone(PIN_BUZZER, 3000, 50);
    break; // Chispa eléctrica
  case SND_MUERTE:
    tone(PIN_BUZZER, 100, 800);
    break; // Muy grave y prolongado
  }
}

void aplicarGenetica(uint8_t nuevo_tipo)
{
  pet.type = nuevo_tipo;

  switch (nuevo_tipo)
  {
  case 1: // EL RAYITO (Niño Bueno)
    pet.hunger_interval = BASE_HUNGER * 1.2;
    pet.happiness_interval = BASE_HAPPINESS * 1.2;
    pet.poop_interval = BASE_POOP * 1.0;
    pet.weight = 10;
    pet.sleep_hour = 20;
    pet.wake_hour = 8;
    Serial.println(">>> Genética Aplicada: El Rayito (1.2x)");
    break;

  case 2: // EL TIZÓN (Niño Asustadizo)
    pet.hunger_interval = BASE_HUNGER * 1.5;
    pet.happiness_interval = BASE_HAPPINESS * 0.7;
    pet.poop_interval = BASE_POOP * 1.0;
    pet.weight = 8;
    pet.sleep_hour = 21;
    pet.wake_hour = 9;
    Serial.println(">>> Genética Aplicada: El Tizón (H:1.5x F:0.7x)");
    break;

  case 3: // EL BEBOTE (Glotón)
    pet.hunger_interval = BASE_HUNGER * 0.6;
    pet.happiness_interval = BASE_HAPPINESS * 1.5;
    pet.poop_interval = BASE_POOP * 0.6;
    pet.weight = 15;
    pet.sleep_hour = 22;
    pet.wake_hour = 10;
    Serial.println(">>> Genética Aplicada: El Bebote (0.6x)");
    break;

  case 4: // EL TRASTO (Rebelde/Descuidado)
    pet.hunger_interval = BASE_HUNGER * 0.8;
    pet.happiness_interval = BASE_HAPPINESS * 0.8;
    pet.poop_interval = BASE_POOP * 1.0;
    pet.weight = 7;
    pet.sleep_hour = 23;
    pet.wake_hour = 7;
    Serial.println(">>> Genética Aplicada: El Trasto (0.8x)");
    break;
  }
}

// --- MOTOR BIOLÓGICO Y METABOLISMO ---
void procesarBiologia(uint32_t horaActual)
{
  // EVOLUCIÓN (Bebé a Niño)
  static uint32_t ultimoCheckEvolucion = 0;

  // --- PARCHE DE SINCRONIZACIÓN ---
  // Si es la primera vez que entramos (arranque), igualamos los tiempos
  if (ultimoCheckEvolucion == 0)
    ultimoCheckEvolucion = horaActual;

  // Sumamos el tiempo que ha pasado desde el último check al contador global
  if (horaActual - ultimoCheckEvolucion >= 1000)
  {
    // Solo sumamos si no ha habido un salto gigantesco de tiempo (ej. pausa de menú)
    if (horaActual - ultimoCheckEvolucion < 2000)
    {
      // etapa final Adulto = 3
      if (pet.stage < 3) 
      {
        pet.evolution_timer += (horaActual - ultimoCheckEvolucion);
      }
    }
    ultimoCheckEvolucion = horaActual;
  }

  // 1. RELOJ DE EVOLUCIÓN (Usa el tiempo acumulado guardado)
  if (pet.stage == 0)
  {
    if (pet.evolution_timer >= 10000) // 10 SEGUNDOS: El Huevo eclosiona
    {
      pet.stage = 1; // Nace el Bebé

      // --- INICIAMOS EL METABOLISMO DEL BEBÉ ---
      pet.hunger = 0;       // Nace famélico (urgencia máxima)
      pet.happiness = 0;    // Nace triste/llorando
      pet.poop_counter = 0; // Aseguramos que nace limpito

      pet.hunger_interval = BASE_HUNGER * 0.1;
      pet.happiness_interval = BASE_HAPPINESS * 0.1;
      pet.poop_interval = BASE_POOP * 0.15;

      // --- ¡EL MARGEN! Reiniciamos los relojes EXACTAMENTE al nacer ---
      pet.last_poop_time = horaActual;
      pet.last_hunger_time = horaActual;
      pet.last_happiness_time = horaActual;

      pet.estado_actual = ESTADO_EVOLUCION;
      pet.action_start = horaActual;
      pet.game_ticks = 0;
      Serial.println(">>> ¡El huevo se está rompiendo! 🥚✨");
    }
  }
  else if (pet.stage == 1)
  {
    if (pet.evolution_timer >= 360000) // 60 minutos de Bebé  
    {
      pet.stage = 2; // Pasa a Niño
      pet.evolution_timer = 0; // Reiniciamos el timer para la siguiente etapa

      // --- LÓGICA DE RAMIFICACIÓN EVOLUTIVA ---
      uint8_t tipo_elegido = 1;

      if (pet.care_mistakes == 0 && pet.discipline >= 50)
        tipo_elegido = 1;
      else if (pet.care_mistakes >= 3)
        tipo_elegido = 4;
      else if (pet.care_mistakes > 0 && pet.weight >= 15)
        tipo_elegido = 3;
      else
        tipo_elegido = 2;

      aplicarGenetica(tipo_elegido);

      pet.care_mistakes = 0;
      pet.estado_actual = ESTADO_EVOLUCION;
      pet.action_start = millis();
      pet.game_ticks = 0;
      Serial.println(">>> ¡El bebé está evolucionando a Niño! ✨");
    }
  }

  // --- INICIO DE LA CÁMARA CRIO-GÉNICA ---
  if (!pet.is_sleeping && pet.stage > 0)
  {
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

    // 4. CHEQUEOS DE SALUD Y ATENCIÓN
    if (pet.care_mistakes >= 5 && pet.estado_actual != ESTADO_MUERTE)
    {
      pet.health_status = 2;
      pet.estado_actual = ESTADO_MUERTE;
      Serial.println(">>> ¡La mascota ha muerto por descuido! ☠️");
      return;
    }

    // 4.1. CHEQUEO DE LLAMADA Y CARE MISTAKES
    if (pet.hunger == 0 || pet.happiness == 0)
    {
      if (pet.needs_attention == false && pet.mistake_processed == false)
      {
        pet.attention_start = horaActual;
        pet.needs_attention = true;
      }
      if (pet.needs_attention == true && (horaActual - pet.attention_start >= 900000))
      {
        pet.care_mistakes++;
        pet.needs_attention = false;
        pet.mistake_processed = true;
        Serial.println(">>> Care Mistake añadido. La luz se apagó. 😔");
      }
    }

    // 4.2. CHEQUEOS LETALES (Inanición y Enfermedad Prolongada)
    if (pet.hunger == 0)
    {
      if (pet.starvation_start == 0)
        pet.starvation_start = horaActual;
      else if (horaActual - pet.starvation_start >= 43200000)
      {
        pet.estado_actual = ESTADO_MUERTE;
        reproducirSonido(SND_MUERTE);
      }
    }
    else
    {
      pet.starvation_start = 0;
    }

    if (pet.health_status == 1)
    {
      if (pet.sickness_start == 0)
        pet.sickness_start = horaActual;
      else if (horaActual - pet.sickness_start >= 21600000)
      {
        pet.estado_actual = ESTADO_MUERTE;
        reproducirSonido(SND_MUERTE);
      }
    }
    else
    {
      pet.sickness_start = 0;
    }
  }
}

void setup()
{
  Serial.begin(115200);

  // 1. INICIALIZAR HARDWARE PRIMERO (Botones y Pantalla)
  pinMode(BTN_A, INPUT_PULLUP);
  pinMode(BTN_B, INPUT_PULLUP);
  pinMode(BTN_C, INPUT_PULLUP);

  if (!display.begin(SSD1306_SWITCHCAPVCC, 0x3C))
  {
    Serial.println("Fallo al iniciar la pantalla OLED");
    for (;;)
      ; // Bucle infinito si falla
  }
  display.clearDisplay();

  // 2. COMPROBAR MEMORIA
  memoria.begin("tama_save", false);               // <-- ¡CAMBIADO a false!
  bool existe_partida = memoria.isKey("pet_data"); // ¿Hay algo guardado?
  memoria.end();

  // 3. DECIDIR QUÉ HACER
  if (existe_partida)
  {
    pet.estado_actual = ESTADO_BOOT; // Vamos al menú de continuar
  }
  else
  {
    randomSeed(analogRead(0)); // Genera una semilla aleatoria
    Serial.println("Hello, Tama32! Creando nueva mascota...");

    // Estado inicial
    pet.hunger = 0;
    pet.happiness = 0;
    pet.health_status = 0;
    pet.weight = 5;
    pet.stage = 0;           // Huevo
    pet.evolution_timer = 0; // Tiempo de eclosión
    pet.last_session_time = 0;
    pet.action_start = 0;
    pet.current_action = ' ';
    pet.age_days = 0;
    pet.type = 0;
    pet.birth_time = millis();
    pet.care_mistakes = 0;
    pet.mistake_processed = false;
    pet.needs_attention = false;
    pet.poop_counter = 0;
    pet.discipline = 0;
    pet.is_sleeping = false;
    pet.is_light_on = true;
    pet.estado_actual = ESTADO_RELOJ;
    pet.last_poop_time = millis();
    pet.last_hunger_time = millis();
    pet.last_happiness_time = millis();
    pet.hunger_interval = 60000;
    pet.happiness_interval = 90000;
    pet.poop_interval = 120000;
    pet.sleep_hour = 20;
    pet.wake_hour = 9;
    pet.snack_count = 0;
    pet.sickness_start = 0;
    pet.starvation_start = 0;
    pet.dirt_accumulation = 0;

    pet.x = 48; // Centro horizontal
    pet.y = 16; // Centro vertical (entre los menús)
  }

  actualizarPantalla();
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

  // --- 1. GUARDIÁN DEL AHORRO DE ENERGÍA (Versión Anti-Fantasmas) ---
  if (digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW || digitalRead(BTN_C) == LOW)
  {
    ultimo_toque_global = horaActual; // Reseteamos el cronómetro de inactividad

    if (!pantalla_encendida)
    {
      display.ssd1306_command(SSD1306_DISPLAYON); // Despertamos la pantalla OLED
      pantalla_encendida = true;
      Serial.println(">>> 💡 Pantalla encendida.");

      // --- ¡LA CLAVE ESTÁ AQUÍ! ---
      // Esperamos activamente a que sueltes el botón antes de seguir.
      // Así el resto del código leerá los botones en HIGH y no hará nada.
      while (digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW || digitalRead(BTN_C) == LOW)
      {
        delay(10); // Pausa mínima para no estresar el procesador
      }

      delay(100); // Pequeño margen extra para evitar rebotes eléctricos
      return;     // Cortamos el loop aquí
    }
  }

  // --- 2. APAGAR PANTALLA POR INACTIVIDAD ---
  // Si pasan 30 segundos sin tocar nada y estamos en IDLE...
  if (pantalla_encendida && (horaActual - ultimo_toque_global > 30000) && pet.estado_actual == ESTADO_IDLE)
  {
    display.ssd1306_command(SSD1306_DISPLAYOFF); // Apaga físicamente el panel OLED
    pantalla_encendida = false;
    Serial.println(">>> 💤 Pantalla apagada para ahorrar energía.");
  }

  // --- MOTOR DEL RELOJ REAL ---
  if (pet.estado_actual != ESTADO_RELOJ && pet.estado_actual != ESTADO_BOOT && (horaActual - ultimo_minuto_ms >= 60000))
  {
    reloj_m++;

    // NUEVO: Contador para el autoguardado
    static uint8_t minutos_para_guardar = 0;
    minutos_para_guardar++;

    if (minutos_para_guardar >= 60)
    { // <-- Ajusta este número (ej: 10 = cada 10 minutos)
      guardarPartida();
      minutos_para_guardar = 0;
    }

    if (reloj_m >= 60)
    {
      reloj_m = 0;
      reloj_h++;
      if (reloj_h >= 24)
        reloj_h = 0;

      // --- COMPROBACIÓN DE HORARIOS (Solo se evalúa al cambiar la hora) ---
      if (reloj_h == pet.sleep_hour && !pet.is_sleeping)
      {
        pet.is_sleeping = true;
        pet.sleep_start = horaActual; // <-- ¡GUARDAMOS LA HORA EXACTA!

        pet.needs_attention = true;
        pet.attention_start = horaActual;
        pet.mistake_processed = false;
        Serial.println(">>> La mascota se ha dormido. ¡Apaga la luz! 🌙");
      }
      else if (reloj_h == pet.wake_hour && pet.is_sleeping)
      {
        pet.is_sleeping = false;
        pet.is_light_on = true;
        pet.needs_attention = false;
        pet.age_days++;

        // --- EL MILAGRO CRIO-GÉNICO ---
        // Desplazamos todos los relojes absolutos sumándoles el tiempo que ha estado durmiendo.
        // Así, para la mascota, la noche "no ha existido" y no se morirá ni se hará caca de golpe.
        uint32_t sleep_duration = horaActual - pet.sleep_start;
        pet.last_hunger_time += sleep_duration;
        pet.last_happiness_time += sleep_duration;
        pet.last_poop_time += sleep_duration;
        if (pet.sickness_start > 0)
          pet.sickness_start += sleep_duration;
        if (pet.starvation_start > 0)
          pet.starvation_start += sleep_duration;
        pet.attention_start += sleep_duration; // Por si se durmió con una rabieta pendiente

        Serial.println(">>> La mascota se ha despertado. ¡Buenos días! ☀️");
      }
    }
    ultimo_minuto_ms = horaActual;
  }

  // --- MOTOR BIOLÓGICO ---
  // Solo procesamos la vida si NO estamos en menús críticos de inicio o configuración
  if (pet.estado_actual != ESTADO_BOOT && pet.estado_actual != ESTADO_RELOJ)
  {
    procesarBiologia(horaActual);
  }

  switch (pet.estado_actual)
  {
  case ESTADO_BOOT:
    lecturaA = digitalRead(BTN_A);
    lecturaC = digitalRead(BTN_C);

    // --- CONTINUAR PARTIDA (Botón A) ---
    if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
    {
      if (lecturaA == LOW)
      {
        Serial.println(">>> Cargando partida...");
        memoria.begin("tama_save", true);
        memoria.getBytes("pet_data", &pet, sizeof(MascotaData));
        reloj_h = memoria.getUInt("reloj_h", 12);
        reloj_m = memoria.getUInt("reloj_m", 0);
        memoria.end();

        // Sincronizamos TODOS los relojes para que el tiempo empiece de cero en esta sesión
        pet.last_hunger_time = horaActual;
        pet.last_happiness_time = horaActual;
        pet.last_poop_time = horaActual;
        pet.birth_time = horaActual;
        pet.sleep_start = horaActual;
        if (pet.sickness_start > 0)
          pet.sickness_start = horaActual;
        if (pet.starvation_start > 0)
          pet.starvation_start = horaActual;
        if (pet.attention_start > 0)
          pet.attention_start = horaActual;

        pet.estado_actual = ESTADO_IDLE; // ¡A jugar!
        actualizarPantalla();
      }
      ultimoToqueA = horaActual;
      estadoAnteriorA = lecturaA;
    }

    // --- BORRAR PARTIDA (Botón C) ---
    if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
    {
      if (lecturaC == LOW)
      {
        Serial.println(">>> Borrando datos de guardado...");
        display.clearDisplay();
        display.setCursor(30, 28);
        display.print("BORRANDO...");
        display.display();
        delay(1000);

        memoria.begin("tama_save", false);
        memoria.clear(); // Destruye todos los datos
        memoria.end();

        ESP.restart(); // Reiniciamos la placa para nacer de nuevo
      }
      ultimoToqueC = horaActual;
      estadoAnteriorC = lecturaC;
    }
    break;

  case ESTADO_IDLE:

    // --- LECTURA DEL BOTÓN A (Navegar) ---
    lecturaA = digitalRead(BTN_A);

    // Si el estado cambia (al pulsar o al soltar) Y han pasado 50ms desde la última vibración
    if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
    {
      if (lecturaA == LOW) // Solo movemos el cursor si el cambio fue Hacia Abajo (Pulsar)
        if (pet.stage > 0) // <-- ¡NUEVO! El huevo no permite abrir menús
        {
          reproducirSonido(SND_NAVEGAR);
          menu_index++;
          if (menu_index > 6)
            menu_index = 0;
          actualizarPantalla();
        }
        else
        {
          reproducirSonido(SND_NAVEGAR);
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

    // --- LECTURA DEL BOTÓN B (Confirmar o ver reloj) ---
    lecturaB = digitalRead(BTN_B);

    // Comprobamos si ha habido un cambio (pulsar O soltar)
    if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
    {
      if (lecturaB == LOW) // Solo ejecutamos la acción si el cambio es "hacia abajo" (Pulsar)
      {
        reproducirSonido(SND_CONFIRMAR);
        if (menu_index != -1) // Si HAY un icono seleccionado
        {
          // Si está durmiendo, SOLO permitimos ver Estadísticas (5) y tocar la Luz (6)
          if (pet.is_sleeping == true && menu_index != 5 && menu_index != 6)
          {
            Serial.println(">>> Shhh... Acción bloqueada. La mascota duerme. 💤");
            menu_index = -1; // Deseleccionamos el menú
            actualizarPantalla();
          }
          else
          {
            if (menu_index == 0) // COMIDA
            {
              if (pet.health_status == 1)
              {
                reproducirSonido(SND_ERROR);
                pet.current_action = 'n';
                pet.estado_actual = ESTADO_ACCION;
                pet.action_start = horaActual;
              }
              else
              {
                pet.estado_actual = ESTADO_SUBMENU_COMIDA;
                submenu_index = 0; // <-- AÑADE ESTA LÍNEA AQUÍ
              }
            }
            else if (menu_index == 1) // JUEGO
            {
              if (pet.health_status == 1)
              {
                reproducirSonido(SND_ERROR);
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
                pet.dirt_accumulation = 0;

                // --- ¡PARCHE ANTI-DEUDA METABÓLICA! ---
                pet.last_poop_time = horaActual;

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
                pet.snack_count = 0;
                pet.current_action = 'm';
                pet.estado_actual = ESTADO_ACCION;
                pet.action_start = horaActual;
              }
            }
            else if (menu_index == 4) // DISCIPLINA
            {
              // Caso A: El bicho tiene un berrinche (necesita atención pero está lleno y feliz)
              if (pet.needs_attention && pet.hunger > 1 && pet.happiness > 1)
              {
                pet.discipline = min((int)pet.discipline + 25, 100);
                pet.needs_attention = false;
                pet.current_action = 'd'; // 'd' de Disciplina (Se pone triste/regañado)
              }
              // Caso B: No hay motivo para regañarle
              else
              {
                reproducirSonido(SND_ERROR);
                pet.current_action = 'n'; // 'n' de Nope (Te dice que no con la cabeza)
              }
              pet.estado_actual = ESTADO_ACCION;
              pet.action_start = horaActual;
            }
            else if (menu_index == 5) // ESTADÍSTICAS
            {
              pet.estado_actual = ESTADO_SUBMENU_STATS;
              submenu_index = 0;
            }
            else if (menu_index == 6) // LUZ
            {
              pet.is_light_on = !pet.is_light_on;
              if (pet.is_sleeping == true && pet.is_light_on == false)
              {
                pet.needs_attention = false;
                Serial.println(">>> Le has apagado la luz a tiempo. ¡Buenas noches!");
              }
            }
            menu_index = -1; // Apagamos el cursor tras elegir
            actualizarPantalla();
          }
        }
        else // Si NO HAY nada seleccionado (menu_index == -1)
        {
          animacionArrastre(true);
          pet.estado_actual = ESTADO_VER_RELOJ;
          actualizarPantalla();
        }
      }

      // ¡LA CLAVE ESTÁ AQUÍ! Esto debe ejecutarse SIEMPRE que haya un cambio, sea pulsando o soltando
      ultimoToqueB = horaActual;
      estadoAnteriorB = lecturaB;
    }

    // --- LECTURA DEL BOTÓN C (Cancelar) ---
    lecturaC = digitalRead(BTN_C);
    if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
    {
      if (lecturaC == LOW && menu_index != -1)
      {
        reproducirSonido(SND_CANCELAR);
        menu_index = -1;      // Deseleccionamos el menú
        actualizarPantalla(); // Borramos el fondo blanco del icono
        Serial.println(">>> Menú cancelado. ❌");
      }
      ultimoToqueC = horaActual;
      estadoAnteriorC = lecturaC;
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

      // --- LOGICA DE ALARMA POR EVENTO (UNA SOLA VEZ) ---
      static bool alarmaProcesada = false; // Variable para recordar si ya sonó

      if (pet.needs_attention == true)
      {
        // Solo suena si es el momento justo en que se activa (flanco de subida)
        if (alarmaProcesada == false)
        {
          Serial.println("¡AVISO: La mascota te necesita! 🚨");
          reproducirSonido(SND_LLAMADA); // Suena la melodía de 6 bips
          alarmaProcesada = true;        // Marcamos como sonada
        }
      }
      else
      {
        // Cuando la necesidad desaparece (se limpia caca, se da medicina, etc.)
        // reseteamos la variable para que pueda volver a sonar la próxima vez.
        alarmaProcesada = false;
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

      // --- SISTEMA DE TOXICIDAD POR CACA (P1/P2) ---
      // Cada segundo, sumamos toxicidad según el número de cacas.
      // 1 caca = +1 punto/segundo. 4 cacas = +4 puntos/segundo.
      if (!pet.is_sleeping) // ¡No se intoxica la habitación mientras duerme!
      {
        if (pet.poop_counter > 0 && pet.health_status == 0)
        {
          pet.dirt_accumulation += pet.poop_counter;

          // Límite: 3600 puntos.
          // Equivale a aguantar 1 caca durante 1 hora (60 mins),
          // o 4 cacas simultáneas durante 15 minutos.
          if (pet.dirt_accumulation >= 3600)
          {
            if (pet.health_status == 0)
            {                                  // Solo si estaba sano...
              pet.health_status = 1;           // Enferma
              pet.sickness_start = horaActual; // Iniciamos el reloj de la muerte
              Serial.println(">>> ¡La mascota ha enfermado por la suciedad! 🤢");
            }
          }
        }
      }

      // --- PASEO CON INTENCIÓN (PASOS LARGOS) ---
      // Solo se mueve si: está despierto, NO está enfermo y no hay menús abiertos
      if (!pet.is_sleeping && pet.health_status == 0 && menu_index == -1)
      {
        static int8_t direccionX = 0;
        static int8_t pasos_restantes = 0;

        // 1. Decidir un nuevo plan
        if (pasos_restantes <= 0)
        {
          if (random(0, 10) < 7) // 70% de probabilidad de echar a andar
          {
            direccionX = random(0, 2) == 0 ? -1 : 1;

            // Como damos pasos más grandes, cruzaremos la pantalla más rápido.
            // Con 3 a 8 pasos ya es suficiente para recorrer casi toda su zona.
            pasos_restantes = random(3, 8);
          }
          else // 30% de quedarse quieto mirando
          {
            direccionX = 0;
            pasos_restantes = random(1, 4); // Pausas cortas (1 a 3 segundos)
          }
        }

        // 2. Ejecutar el plan
        if (pasos_restantes > 0)
        {
          // --- ¡EL CAMBIO CLAVE! ---
          // Multiplicamos por 4 (o por 6 si lo quieres aún más loco).
          pet.x += (direccionX * 4);
          pasos_restantes--;

          // Si llega a los bordes, frena en seco y cancela el plan
          if (pet.x < 4)
          {
            pet.x = 4;
            pasos_restantes = 0;
          }
          if (pet.x > 54)
          {
            pet.x = 54;
            pasos_restantes = 0;
          }
        }

        // 3. Saltos
        if (pet.y < 16)
        {
          pet.y = 16; // Gravedad (cae al suelo)
        }
        else
        {
          if (random(0, 10) < 4)
            pet.y = 12; // 40% de probabilidad de dar un salto
        }
      }
      else if (pet.health_status == 1)
      {
        // Enfermo: clavado al suelo
        pet.x = 48;
        pet.y = 16;
      }

      // --- ACTUALIZACIÓN VISUAL ---
      frame_animation = !frame_animation; // Mantenemos el bool para cosas de 2 frames (menús, etc)
      idle_frame_index = (idle_frame_index + 1) % 3; // Ciclo 0, 1, 2 para la mascota
      actualizarPantalla();

      tiempoUltimoLatido = horaActual;
    }
    break;

  case ESTADO_MENU:
    // Placeholder para futuras cosas como comidas, items...
    break;

  case ESTADO_ACCION:
    // --- LECTURA DE BOTONES PARA EL SKIP (Con Antirrebote) ---
    lecturaB = digitalRead(BTN_B);
    if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
    {
      if (lecturaB == LOW)
        pet.action_start = horaActual - 3000; // Salta al final
      ultimoToqueB = horaActual;
      estadoAnteriorB = lecturaB;
    }

    lecturaC = digitalRead(BTN_C);
    if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
    {
      if (lecturaC == LOW)
        pet.action_start = horaActual - 3000; // Salta al final
      ultimoToqueC = horaActual;
      estadoAnteriorC = lecturaC;
    }

    // Comprobamos si la acción ha terminado (o ha sido saltada)
    if (horaActual - pet.action_start >= 3000)
    {
      // --- SALIDA INTELIGENTE AL SUBMENÚ ---
      if (pet.current_action == 'c' || pet.current_action == 's')
      {
        pet.estado_actual = ESTADO_SUBMENU_COMIDA;
        // NO reseteamos submenu_index aquí para que mantenga la posición (Food o Snack)
      }
      else
      {
        pet.estado_actual = ESTADO_IDLE;
      }

      actualizarPantalla();
      Serial.println(">>> Acción finalizada. Volviendo al menú.");
    }
    else
    {
      // --- 1. LATIDO RÁPIDO PARA ANIMACIÓN (20 FPS) ---
      static uint32_t ultimoFrameAnimacion = 0;
      if (horaActual - ultimoFrameAnimacion >= 50)
      {
        actualizarPantalla();
        ultimoFrameAnimacion = horaActual;
      }

      // --- 2. LATIDO DE SONIDO Y LÓGICA (1 segundo) ---
      if (horaActual - tiempoUltimoLatido >= INTERVALO)
      {
        frame_animation = !frame_animation;

        if (pet.current_action == 'c' || pet.current_action == 's')
          reproducirSonido(SND_COMER);
        else if (pet.current_action == 'l')
          reproducirSonido(SND_NAVEGAR);
        else if (pet.current_action == 'm')
          reproducirSonido(SND_CURAR);
        else if (pet.current_action == 'd')
          reproducirSonido(SND_ERROR);

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
          reproducirSonido(SND_ERROR);
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

              pet.snack_count = 0;

              pet.estado_actual = ESTADO_ACCION;
              pet.action_start = horaActual;
            }
            else
            {
              // Si está lleno, simplemente cancelamos y volvemos a la pantalla principal
              reproducirSonido(SND_ERROR);
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

            pet.snack_count++; // Sumamos un snack a la cuenta

            // Si come 5 snacks seguidos en un corto periodo, enferma
            if (pet.snack_count >= 5)
            {
              pet.health_status = 1;
              pet.sickness_start = horaActual;
              pet.snack_count = 0; // Reseteamos la cuenta al enfermar
              Serial.println("¡Se ha empachado de dulces y ha enfermado! 🤢");
            }

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
        if (submenu_index > 3)
          submenu_index = 0; // Si pasa de la pág 3, vuelve a la 0
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
    // --- EL LATIDO CENTRAL (Sincronizado con INTERVALO = 1 seg) ---
    if (horaActual - tiempoUltimoLatido >= INTERVALO)
    {
      frame_animation = !frame_animation; // Mantenemos el parpadeo global del sistema
      pet.game_ticks++;                   // Sumamos 1 fotograma (1 tick = 1 segundo)
      tiempoUltimoLatido = horaActual;    // Reseteamos el reloj maestro

      if (pet.game_ticks < 10)
      {
        reproducirSonido(SND_EVOLUCION_TICK); // <-- NUEVO: 10 latidos de carga
      }
      else if (pet.game_ticks == 10)
      {
        reproducirSonido(SND_VICTORIA); // <-- NUEVO: ¡TA-DAAA!
      }

      if (pet.game_ticks >= 13)
      {
        pet.estado_actual = ESTADO_IDLE;
      }

      actualizarPantalla(); // Se dibuja UNA sola vez por segundo, como el resto del juego
    }
    break;

  case ESTADO_MUERTE:
    // Leemos los botones A y C para el reinicio
    lecturaA = digitalRead(BTN_A);
    lecturaC = digitalRead(BTN_C);

    if (lecturaA == LOW && lecturaC == LOW)
    {
      Serial.println(">>> REINICIO TOTAL: Combinación A+C detectada. 🔄");

      // Feedback visual rápido antes de reiniciar
      display.clearDisplay();
      display.setCursor(30, 28);
      display.print("REINICIANDO...");
      display.display();
      delay(500); // Una pausa breve para que se lea el mensaje

      setup(); // Volvemos al estado inicial
    }
    break;

  case ESTADO_RELOJ:
    // BOTÓN A: Incrementar número
    lecturaA = digitalRead(BTN_A);
    if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
    {
      if (lecturaA == LOW)
      {
        if (reloj_fase == 0)
        {
          reloj_h++;
          if (reloj_h > 23)
            reloj_h = 0;
        }
        else
        {
          reloj_m++;
          if (reloj_m > 59)
            reloj_m = 0;
        }
        actualizarPantalla();
      }
      ultimoToqueA = horaActual;
      estadoAnteriorA = lecturaA;
    }

    // BOTÓN B: Confirmar / Siguiente
    lecturaB = digitalRead(BTN_B);
    if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
    {
      if (lecturaB == LOW)
      {
        if (reloj_fase == 0)
        {
          reloj_fase = 1; // Pasamos a minutos
        }
        else
        {
          // Confirmamos minutos y EMPEZAMOS EL JUEGO
          ultimo_minuto_ms = horaActual;     // Arranca el reloj real
          pet.birth_time = horaActual;       // Nace en este instante
          pet.last_hunger_time = horaActual; // Sincronizamos metaboilsmo
          pet.last_happiness_time = horaActual;
          pet.last_poop_time = horaActual;
          pet.estado_actual = ESTADO_IDLE;

          // Para evitar el salto de evolución al salir del reloj
          pet.action_start = horaActual; // Lo usaremos como ancla temporal general

          guardarPartida();
        }
        actualizarPantalla();
      }
      ultimoToqueB = horaActual;
      estadoAnteriorB = lecturaB;
    }

    // BOTÓN C: Volver atrás
    lecturaC = digitalRead(BTN_C);
    if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
    {
      if (lecturaC == LOW && reloj_fase == 1)
      {
        reloj_fase = 0; // Volvemos a editar horas
        actualizarPantalla();
      }
      ultimoToqueC = horaActual;
      estadoAnteriorC = lecturaC;
    }
    break;

  case ESTADO_VER_RELOJ:
    lecturaA = digitalRead(BTN_A);
    lecturaB = digitalRead(BTN_B);
    lecturaC = digitalRead(BTN_C);

    // --- A + C para editar la hora ---
    if (lecturaA == LOW && lecturaC == LOW)
    {
      pet.estado_actual = ESTADO_RELOJ;
      reloj_fase = 0; // Empezamos editando las horas
      actualizarPantalla();
      delay(200); // Pausa mágica para no arrastrar el rebote al menú de edición
    }
    else
    {
      // --- SALIR CON BOTÓN B (Con Antirrebote Fuerte) ---
      if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
      {
        if (lecturaB == LOW && lecturaA == HIGH) // Aseguramos que A no esté pulsado
        {
          animacionArrastre(false); // <-- ¡NUEVO! Animación de vuelta
          pet.estado_actual = ESTADO_IDLE;
          actualizarPantalla();
        }
        ultimoToqueB = horaActual;
        estadoAnteriorB = lecturaB;
      }

      // --- SALIR CON BOTÓN C (Con Antirrebote Fuerte) ---
      if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
      {
        if (lecturaC == LOW && lecturaA == HIGH)
        {
          animacionArrastre(false); // <-- ¡NUEVO! Animación de vuelta
          pet.estado_actual = ESTADO_IDLE;
          actualizarPantalla();
        }
        ultimoToqueC = horaActual;
        estadoAnteriorC = lecturaC;
      }
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

          if (pet.game_wins >= 3)
            reproducirSonido(SND_VICTORIA);
          else
            reproducirSonido(SND_DERROTA);
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
          if (pet.happiness < 4)
            pet.happiness++; // Límite de 4 corazones
          if (pet.weight > 5)
            pet.weight--; // Peso mínimo de 5g para no desaparecer

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
          reproducirSonido(SND_ACIERTO);
        }
        else
        {
          pet.last_guess_won = false;
          reproducirSonido(SND_FALLO);
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
