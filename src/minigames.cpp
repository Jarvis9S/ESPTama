#include "minigames.h"

void procesarMinijuego(uint32_t horaActual)
{
  // Variables locales para leer los botones solo durante el juego
  static bool estadoAnteriorA = HIGH;
  static uint32_t ultimoToqueA = 0;
  bool lecturaA;

  static bool estadoAnteriorB = HIGH;
  static uint32_t ultimoToqueB = 0;
  bool lecturaB;

  static bool estadoAnteriorC = HIGH;
  static uint32_t ultimoToqueC = 0;
  bool lecturaC;

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
    { 
      if (pet.game_wins >= 3) {
        bool era_emergencia_feliz = (pet.happiness == 0);
        if (pet.happiness < 4) pet.happiness++;
        
        
        if (era_emergencia_feliz && pet.hunger > 0) {
            pet.needs_attention = false;
            pet.mistake_processed = false;
        }
        
        Serial.println(">>> ¡Juego Ganado! 🏆");
      }
      if (pet.weight > pet.min_weight) pet.weight--;

      // --- ¡EL BUCLE MÁGICO! ---
      // En vez de volver a IDLE, reseteamos las variables de juego para otra partida
      pet.game_phase = 1;
      pet.game_round = 1;
      pet.game_wins = 0;
      pet.current_number = random(1, 9);
      pet.game_ticks = 0; // Reseteamos latidos
      
      // NOTA: Si el jugador quiere salir, solo tiene que pulsar el Botón C 
      // mientras espera el siguiente número (ya programado en la Fase 1).
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
      if (lecturaA == LOW && pet.game_phase == 1)
      {
        input_received = true;
        eligio_mayor = false;
      }
      ultimoToqueA = millis();
      estadoAnteriorA = lecturaA;
    }
    // Lectura Botón B (Mayor)
    lecturaB = digitalRead(BTN_B);
    if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
    {
      if (lecturaB == LOW && pet.game_phase == 1)
      {
        input_received = true;
        eligio_mayor = true;
      }
      ultimoToqueB = millis();
      estadoAnteriorB = lecturaB;
    }

    // Lectura Botón C (Cancelar)
    lecturaC = digitalRead(BTN_C);
    if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
    {
      if (lecturaC == LOW)
      {
        pet.estado_actual = ESTADO_IDLE;
        menu_index = -1; 
        actualizarPantalla();
      }
      ultimoToqueC = millis();
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
}