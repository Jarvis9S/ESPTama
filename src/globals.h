#ifndef GLOBALS_H
#define GLOBALS_H

#include <Arduino.h>

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
  uint8_t baby_type;            // Tipo de bebé (0 a 4, dependiendo de la suerte al nacer)
  uint8_t last_baby_type;       // <--- ¡NUEVO! Para recordar la gen. anterior
  uint8_t generation;           // Generación de la mascota
  uint16_t age_days;            // Días reales de vida
  uint8_t type;                 // Variante de la mascota (depende de los cuidados)
  uint32_t birth_time;          // Momento del nacimiento en milisegundos
  EstadosMascota estado_actual; // Estado actual de la mascota
  int8_t hunger;                // Hambre (0 a 4)
  int8_t happiness;             // Felicidad (0 a 4)
  uint16_t weight;              // Peso en gramos (0 a 9999)
  uint16_t min_weight;          // Peso mínimo (por evolución)
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

  // Ritmos biológicos (¡NUEVO!)
  uint32_t hunger_interval;    // Cuánto tarda en bajar el hambre
  uint32_t happiness_interval; // Cuánto tarda en bajar la felicidad
  uint32_t poop_interval;      // Cuánto tarda en hacerse caca
  uint32_t dirt_accumulation;  // Medidor de toxicidad ambiental (P1/P2)
  uint16_t tantrum_chance;
  bool is_rebelling; // Recuerda si está en modo terco

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
const uint16_t BASE_TANTRUM = 130;         // 13.0% de probabilidad de berrinche

extern MascotaData pet; // Le dice a otros archivos que "pet" existe en main.cpp

// --- VARIABLES DE INTERFAZ Y RELOJ ---
extern uint8_t reloj_h;
extern uint8_t reloj_m;
extern uint8_t reloj_fase;
extern bool frame_animation;
extern uint8_t idle_frame_index;
extern int8_t menu_index;
extern int8_t submenu_index;

// --- VARIABLES DE INTERVALO Y LATIDO ---
const uint32_t INTERVALO = 1000;
extern uint32_t tiempoUltimoLatido;

#endif