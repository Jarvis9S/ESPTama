#include "ui.h"
#include "sprites.h"

// --- FUNCIONES AYUDANTES DE SPRITES ---

const unsigned char *obtenerSpriteBase()
{
  // --- CASO HUEVO ---
  if (pet.stage == 0)
    return epd_bitmap_egg1_1; // <--- ¡AÑADIDO!

  if (pet.stage == 1 && pet.baby_type == 2)
    return epd_bitmap_a2_1;
  if (pet.stage == 2)
  {
    switch (pet.type)
    {
    case 1:
      return epd_bitmap_b1_2; // El Rayito
    case 2:
      return epd_bitmap_b2_1; // El Tizón
    case 3:
      return epd_bitmap_b3_1; // El Bebote
    case 4:
      return epd_bitmap_b4_1; // El Trasto
    }
  }
  return epd_bitmap_a1_1; // Fallback (Bebé 1 y Huevos)
}

const unsigned char *obtenerSpriteComer(int fase)
{
  if (pet.stage == 1 && pet.baby_type == 2)
  {
    return (fase == 0 || fase == 2) ? epd_bitmap_a2_1_eat_0 : epd_bitmap_a2_1_eat_1;
  }

  if (pet.stage >= 2) // !!! REVISAR CUANDO SE AGREGUEN MÁS ETAPAS
  {
    switch (pet.type)
    {
    case 1:
      return (fase == 0 || fase == 2) ? epd_bitmap_b1_1_eat_0 : epd_bitmap_b1_1_eat_1;
    case 2:
      return (fase == 0 || fase == 2) ? epd_bitmap_b2_1_eat_0 : epd_bitmap_b2_1_eat_1;
    case 3:
      return (fase == 0 || fase == 2) ? epd_bitmap_b3_1_eat_0 : epd_bitmap_b3_1_eat_1;
    case 4:
      return (fase == 0 || fase == 2) ? epd_bitmap_b4_1_eat_0 : epd_bitmap_b4_1_eat_1;
    }
  }
  return (fase == 0 || fase == 2) ? epd_bitmap_a1_1_eat_0 : epd_bitmap_a1_1_eat_1;
}

const unsigned char *obtenerSpriteNope(bool anim)
{
  if (pet.stage == 1 && pet.baby_type == 2)
  {
    return anim ? epd_bitmap_a2_1_nope_1 : epd_bitmap_a2_1_nope_0;
  }

  if (pet.stage >= 2) // !!! REVISAR CUANDO SE AGREGUEN MÁS ETAPAS
  {
    switch (pet.type)
    {
    case 1:
      return anim ? epd_bitmap_b1_1_nope_1 : epd_bitmap_b1_1_nope_0;
    case 2:
      return anim ? epd_bitmap_b2_1_nope_1 : epd_bitmap_b2_1_nope_0;
    case 3:
      return anim ? epd_bitmap_b3_1_nope_1 : epd_bitmap_b3_1_nope_0;
    case 4:
      return anim ? epd_bitmap_b4_1_nope_1 : epd_bitmap_b4_1_nope_0;
    }
  }
  return anim ? epd_bitmap_a1_1_nope_1 : epd_bitmap_a1_1_nope_0;
}

const unsigned char *obtenerSpriteTriste()
{
  if (pet.stage == 1 && pet.baby_type == 2)
    return epd_bitmap_a2_1_sad;

  if (pet.stage >= 2) // !!! REVISAR CUANDO SE AGREGUEN MÁS ETAPAS
  {
    switch (pet.type)
    {
    case 1:
      return epd_bitmap_b1_1_sad;
    case 2:
      return epd_bitmap_b2_1_sad;
    case 3:
      return epd_bitmap_b3_1_sad;
    case 4:
      return epd_bitmap_b4_1_sad;
    }
  }
  return epd_bitmap_a1_1_sad;
}

const unsigned char *obtenerSpriteFeliz()
{
  if (pet.stage == 1 && pet.baby_type == 2)
    return epd_bitmap_a2_1_happy;

  if (pet.stage >= 2) // !!! REVISAR CUANDO SE AGREGUEN MÁS ETAPAS
  {
    switch (pet.type)
    {
    case 1:
      return epd_bitmap_b1_1_happy;
    case 2:
      return epd_bitmap_b2_1_happy;
    case 3:
      return epd_bitmap_b3_1_happy;
    case 4:
      return epd_bitmap_b4_1_happy;
    }
  }
  return epd_bitmap_a1_1_happy;
}

const unsigned char *obtenerSpriteDormir(bool anim)
{
  if (pet.stage == 1 && pet.baby_type == 2)
  {
    return anim ? epd_bitmap_a2_1_sleep_1 : epd_bitmap_a2_1_sleep_0;
  }

  if (pet.stage >= 2) // !!! REVISAR CUANDO SE AGREGUEN MÁS ETAPAS
  {
    switch (pet.type)
    {
    case 1:
      return anim ? epd_bitmap_b1_1_sleep_1 : epd_bitmap_b1_1_sleep_0;
    case 2:
      return anim ? epd_bitmap_b2_1_sleep_1 : epd_bitmap_b2_1_sleep_0;
    case 3:
      return anim ? epd_bitmap_b3_1_sleep_1 : epd_bitmap_b3_1_sleep_0;
    case 4:
      return anim ? epd_bitmap_b4_1_sleep_1 : epd_bitmap_b4_1_sleep_0;
    }
  }
  return anim ? epd_bitmap_a1_1_sleep_1 : epd_bitmap_a1_1_sleep_0;
}

// --- FUNCIONES DE DIBUJO ---

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

    // Puntitos parpadeantes sincronizados con millis()
    if ((millis() / 500) % 2 == 0)
      display.print(":");
    else
      display.print(" ");

    if (reloj_m < 10)
      display.print("0");
    display.print(reloj_m);

    // --- BATERÍA (HUD) ---
    display.drawRect(2, 2, 16, 8, WHITE); // Cuerpo de la pila
    display.fillRect(18, 4, 2, 4, WHITE); // Borne positivo
    display.fillRect(4, 4, 12, 4, WHITE); // Relleno (batería llena)

    // --- VERSIÓN ---
    display.setTextSize(1);
    display.setCursor(100, 2);
    display.print("V.b1");
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
        display.drawBitmap(pet.x, 16, sleep_off_frame, 32, 32, WHITE);
      }
      else
      {
        // Despierto a oscuras: Ojos blancos que rebotan 2px
        int y_ojos = 16 + (frame_animation ? -2 : 0);
        display.drawBitmap(pet.x, y_ojos, epd_bitmap_eyes_dark_0, 32, 32, WHITE);
      }
    }
    else
    {
      // --- LUZ ENCENDIDA ---
      if (pet.is_sleeping)
      {
        // Duerme con la luz encendida (Mascota con ojos cerrados)
        const unsigned char *sleep_on_frame = obtenerSpriteDormir(frame_animation);
        display.drawBitmap(pet.x, 16, sleep_on_frame, 32, 32, WHITE);
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
            const unsigned char *sprite_actual;

            // --- FASE BEBÉ (STAGE 1) ---
            if (pet.stage == 1)
            {
              if (pet.baby_type == 2)
              { // BEBÉ 2
                if (idle_frame_index == 3)
                  sprite_actual = epd_bitmap_a2_3; // RARO
                else if (idle_frame_index == 1)
                  sprite_actual = epd_bitmap_a2_2;
                else
                  sprite_actual = epd_bitmap_a2_1;
              }
              else
              { // BEBÉ 1
                if (idle_frame_index == 3)
                  sprite_actual = epd_bitmap_a1_3; // RARO
                else if (idle_frame_index == 1)
                  sprite_actual = epd_bitmap_a1_2;
                else
                  sprite_actual = epd_bitmap_a1_1;
              }
            }
            // --- FASE NIÑO (STAGE 2) ---
            else if (pet.stage == 2)
            {
              if (pet.type == 1)
              { // EL RAYITO
                if (idle_frame_index == 3)
                  sprite_actual = epd_bitmap_b1_1; // RARO
                else if (idle_frame_index == 1)
                  sprite_actual = epd_bitmap_b1_3;
                else
                  sprite_actual = epd_bitmap_b1_2;
              }
              else if (pet.type == 2)
              { // EL TIZÓN
                if (idle_frame_index == 3)
                  sprite_actual = epd_bitmap_b2_3; // RARO
                else if (idle_frame_index == 1)
                  sprite_actual = epd_bitmap_b2_2;
                else
                  sprite_actual = epd_bitmap_b2_1;
              }
              else if (pet.type == 3)
              { // EL BEBOTE
                if (idle_frame_index == 3)
                  sprite_actual = epd_bitmap_b3_3; // RARO
                else if (idle_frame_index == 1)
                  sprite_actual = epd_bitmap_b3_2;
                else
                  sprite_actual = epd_bitmap_b3_1;
              }
              else if (pet.type == 4)
              { // EL TRASTO
                if (idle_frame_index == 3)
                  sprite_actual = epd_bitmap_b4_2; // RARO
                else if (idle_frame_index == 1)
                  sprite_actual = epd_bitmap_b4_3;
                else
                  sprite_actual = epd_bitmap_b4_1;
              }
            }
            else
            { // Fallback / Huevo
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

    if (pet.current_action == 'n') // Animación: Nope
    {
      const unsigned char *nope_frame = obtenerSpriteNope(frame_animation);
      display.drawBitmap(48, 16, nope_frame, 32, 32, WHITE);
    }
    else if (pet.current_action == 'm') // Animación: Medicina
    {
      const unsigned char *heal_icon = (fase == 0) ? epd_bitmap_heal_0 : (fase == 1 ? epd_bitmap_heal_1 : epd_bitmap_heal_2);
      display.drawBitmap(24, 24, heal_icon, 16, 16, WHITE);

      const unsigned char *p_med = (fase < 2) ? obtenerSpriteTriste() : obtenerSpriteFeliz();
      display.drawBitmap(48, 16, p_med, 32, 32, WHITE);
    }
    else if (pet.current_action == 'l') // Animación: Limpiar
    {
      int draw_x = pet.x;
      int sweep_x = 128 - (elapsed * 146 / 3000);

      // Efecto arrastre: Si la escoba choca con la mascota, la empuja
      if (sweep_x < draw_x + 24)
      {
        draw_x = sweep_x - 24;
        if (draw_x < 4)
          draw_x = 4; // Límite de la pared izquierda
      }

      const unsigned char *pet_frame;
      if (elapsed < 2500)
      {
        // Durante la limpieza alterna frames base (o uno triste si prefieres)
        pet_frame = frame_animation ? obtenerSpriteBase() : obtenerSpriteTriste();
      }
      else
      {
        // Solo sonríe al final del todo
        pet_frame = obtenerSpriteFeliz();
      }

      display.drawBitmap(draw_x, pet.y, pet_frame, 32, 32, WHITE);
      display.drawBitmap(sweep_x, 16, epd_bitmap_clean_lines, 18, 32, WHITE);
    }
    else if (pet.current_action == 'c' || pet.current_action == 's') // Animación: Comer
    {
      const unsigned char *pet_frame = obtenerSpriteComer(fase);
      const unsigned char *food_frame;

      if (fase == 0)
        food_frame = (pet.current_action == 'c') ? epd_bitmap_food_eat_0 : epd_bitmap_snack_eat_0;
      else if (fase == 1)
        food_frame = (pet.current_action == 'c') ? epd_bitmap_food_eat_1 : epd_bitmap_snack_eat_1;
      else
        food_frame = epd_bitmap_crumbs_eat;

      display.drawBitmap(48, 16, pet_frame, 32, 32, WHITE);
      display.drawBitmap(24, 24, food_frame, 16, 16, WHITE);
    }
    else if (pet.current_action == 'd') // Animación: Disciplina
    {
      display.drawBitmap(48, 16, obtenerSpriteTriste(), 32, 32, WHITE);

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

    if (submenu_index == 0) // PÁGINA 0: Edad, Peso y Generación
    {
      display.setCursor(8, 12);
      display.print("EDAD: ");
      display.print(pet.age_days);

      // --- FILA 2: PESO ---
      display.drawBitmap(4, 40, epd_bitmap_weight_0, 16, 16, WHITE);
      display.setCursor(22, 40);
      display.print(pet.weight);
      display.print("g");

      // --- FILA 2: GENERACIÓN ---
      display.setCursor(66, 40);
      display.print("Gen");
      display.print(pet.generation);
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
      display.drawBitmap(48, 16, obtenerSpriteBase(), 32, 32, WHITE);
    }
    // Fase 2: MOSTRAR RESULTADO DE LA RONDA
    else if (pet.game_phase == 2)
    {
      display.drawBitmap(16, 24, game_numbers[pet.current_number], 16, 16, WHITE); // Izq: Actual
      display.drawBitmap(96, 24, game_numbers[pet.next_number], 16, 16, WHITE);    // Der: Revelado

      // Centro: Animación Happy o Sad alternando con el frame base a1_1
      const unsigned char *pet_frame;
      if (pet.last_guess_won)
      { // (O game_wins >= 3 en la fase 3)
        pet_frame = frame_animation ? obtenerSpriteFeliz() : obtenerSpriteBase();
      }
      else
      {
        pet_frame = frame_animation ? obtenerSpriteTriste() : obtenerSpriteBase();
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
        pet_frame = frame_animation ? obtenerSpriteFeliz() : obtenerSpriteBase();
      }
      else
      {
        pet_frame = frame_animation ? obtenerSpriteTriste() : obtenerSpriteBase();
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
        const unsigned char *happy_frame = frame_animation ? obtenerSpriteFeliz() : obtenerSpriteBase();
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
        const unsigned char *happy_frame = frame_animation ? obtenerSpriteFeliz() : obtenerSpriteBase();
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
      int draw_x = (pet.stage == 0) ? 48 : pet.x; // 🔒 El huevo siempre en el centro

      if (pet.health_status == 1)
        pet_frame = obtenerSpriteTriste();
      else if (pet.is_sleeping)
        pet_frame = obtenerSpriteDormir(false);
      else
        pet_frame = obtenerSpriteBase();

      display.drawBitmap(draw_x, pet.y + y_idle, pet_frame, 32, 32, WHITE);

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