#include <Arduino.h>

#include "globals.h"
#include "hardware.h"
#include "ui.h"
#include "minigames.h"
#include "logic.h"

#include <Preferences.h>
Preferences memoria; // Creamos el objeto para manejar el guardado

// Paso del tiempo de la interfaz
uint32_t tiempoUltimoLatido = 0;

// Datos de la mascota (¡TODO EN UNO PARA FACILIDAD DE GUARDADO!)
MascotaData pet;

// Variables para el manejo de la pantalla y ahorro de energía
bool pantalla_encendida = true;
uint32_t ultimo_toque_global = 0;

// --- RELOJ DEL SISTEMA ---
uint8_t reloj_h = 12;          // Horas (0-23)
uint8_t reloj_m = 0;           // Minutos (0-59)
uint32_t ultimo_minuto_ms = 0; // Cronómetro para que pasen los minutos
uint8_t reloj_fase = 0;        // 0 = Ajustando Horas, 1 = Ajustando Minutos

// Variables para navegación de menús
bool frame_animation = false;
uint8_t idle_frame_index = 0; // Para ciclos de 3 frames
int8_t menu_index = -1;       // -1 significa que no hay nada seleccionado
int8_t submenu_index = 0;

// Función para guardar la partida actual en la memoria flash del ESP32
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

void setup()
{
  // !!! FLASHEAR Y BORRAR
  memoria.begin("tama_save", false);
  // memoria.clear(); // <--- Ejecuta esto solo 1 vez para sanear la partición
  memoria.end();

  Serial.begin(115200);
  iniciarTareaSonido();

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
    pet.min_weight = 5;
    pet.stage = 0;           // Huevo
    pet.evolution_timer = 0; // Tiempo de eclosión
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
    pet.hunger_interval = 300000;
    pet.happiness_interval = 450000;
    pet.poop_interval = 900000;
    pet.sleep_hour = 20;
    pet.wake_hour = 9;
    pet.snack_count = 0;
    pet.sickness_start = 0;
    pet.starvation_start = 0;
    pet.dirt_accumulation = 0;
    pet.baby_type = 0; // Se decidirá al nacer
    pet.tantrum_chance = 0;
    pet.is_rebelling = false;

    // --- LEER EL LEGADO ---
    memoria.begin("tama_save", true);
    // Leemos la generación guardada; si no existe (primera vez), ponemos 1
    pet.generation = memoria.getUInt("gen_legado", 1);
    // Leemos el tipo de bebé del ancestro para el cálculo del 75%
    pet.last_baby_type = memoria.getUInt("bebe_legado", 0);
    memoria.end();

    Serial.print(">>> Nueva Mascota. Generación: ");
    Serial.println(pet.generation);

    pet.x = 48; // Centro horizontal
    pet.y = 16; // Centro vertical (entre los menús)
  }

  // --- FORZAR SINCRONIZACIÓN DE HARDWARE ---
  display.ssd1306_command(SSD1306_DISPLAYON); // Aseguramos que el OLED reciba la orden física
  pantalla_encendida = true;
  ultimo_toque_global = millis(); // Inicializamos el temporizador de ahorro aquí

  actualizarPantalla();
}

void loop()
{
  uint32_t horaActual = millis();

  // 0. EL MOTOR DEL TIEMPO (Lo movemos a la cima)
  // Se procesa antes que cualquier gráfico o botón para evitar "viajes en el tiempo"
  if (pet.estado_actual != ESTADO_RELOJ && pet.estado_actual != ESTADO_BOOT)
  {
    while (horaActual - ultimo_minuto_ms >= 60000)
    {
      reloj_m++;
      ultimo_minuto_ms += 60000;

      static uint8_t minutos_para_guardar = 0;
      minutos_para_guardar++;
      if (minutos_para_guardar >= 60)
      { // Autoguardado cada 60 mins
        guardarPartida();
        minutos_para_guardar = 0;
      }

      if (reloj_m >= 60)
      {
        reloj_m = 0;
        reloj_h++;
        if (reloj_h >= 24)
          reloj_h = 0;

        // Comprobación de sueño/despertar
        if (reloj_h == pet.sleep_hour && !pet.is_sleeping)
        {
          pet.is_sleeping = true;
          pet.sleep_start = horaActual;
          
          // Si la luz ya estaba apagada, ¡Premio! No hay alarma.
          if (pet.is_light_on) {
              pet.needs_attention = true;
              pet.attention_start = horaActual;
              pet.mistake_processed = false;
          } else {
              pet.needs_attention = false;
          }
        }
        else if (reloj_h == pet.wake_hour && pet.is_sleeping)
        {
          pet.is_sleeping = false;
          pet.is_light_on = true;
          pet.needs_attention = false;
          pet.age_days++;

          uint32_t sleep_duration = horaActual - pet.sleep_start;

          // Eliminamos la compensación de hambre y felicidad (el metabolismo ya lo ha procesado). Solo reseteamos la caca.
          pet.last_poop_time = horaActual;

          // MANTENEMOS la pausa de los cronómetros letales evitamos que muera súbitamente al despertar si se acostó malo.
          if (pet.sickness_start > 0)
            pet.sickness_start += sleep_duration;
          if (pet.starvation_start > 0)
            pet.starvation_start += sleep_duration;

          pet.attention_start += sleep_duration;

          // Reactivar alarmas si se despierta hambriento o triste
          if (pet.hunger == 0 || pet.happiness == 0)
          {
            pet.mistake_processed = false;
          }
        }
      }
    }
  }
  else
  {
    ultimo_minuto_ms = horaActual;
  }

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

  // --- 1. GUARDIÁN DEL DESPERTAR  ---
  if (digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW || digitalRead(BTN_C) == LOW)
  {
    // CASO A: La pantalla está apagada (Despertar)
    if (!pantalla_encendida)
    {
      display.ssd1306_command(SSD1306_DISPLAYON); // Comando físico al hardware
      pantalla_encendida = true;
      ultimo_toque_global = millis(); // Reiniciamos el tiempo de espera con un valor fresco

      Serial.println(">>> 💡 Pantalla activada por hardware.");

      // --- BLOQUEO DE SEGURIDAD ---
      // Esperamos a que sueltes el botón para que la pulsación de "despertar"
      // NO sea procesada por la lógica del juego (evita el bug del reloj).
      while (digitalRead(BTN_A) == LOW || digitalRead(BTN_B) == LOW || digitalRead(BTN_C) == LOW)
      {
        delay(10);
      }

      // Sincronizamos los estados de los botones para "limpiar" la memoria
      estadoAnteriorA = estadoAnteriorB = estadoAnteriorC = HIGH;
      ultimoToqueA = ultimoToqueB = ultimoToqueC = millis();

      return; // ¡FUNDAMENTAL! Salimos de esta vuelta del loop
    }

    // CASO B: La pantalla ya está encendida (Uso normal)
    // Solo refrescamos el temporizador de inactividad
    ultimo_toque_global = horaActual;

    /* =================================================================================
       TODO (VERSIÓN 1.0 FINAL - LIGHT SLEEP)
       ---------------------------------------------------------------------------------
       Para activar el ahorro extremo de batería (meses de duración), descomenta este
       bloque. ¡ATENCIÓN! Antes de hacerlo, debes buscar en TODO el proyecto (main.cpp,
       logic.cpp) la palabra "millis()" y sustituirla por:
       (uint32_t)(esp_timer_get_time() / 1000)
       Si no haces esto, el tiempo del juego se congelará al dormir.
       ================================================================================= */

    /* // --- INICIO DEL BLOQUE LIGHT SLEEP ---

    // 1. Despertar por reloj: Forzamos un despertar cada 60 segundos (60 millones de microsegundos)
    esp_sleep_enable_timer_wakeup(60000000ULL);

    // 2. Despertar por botones: Pines 12, 13 y 14 (A, B, C).
    // Usamos una máscara de bits (1ULL << pin)
    uint64_t mascara_botones = (1ULL << BTN_A) | (1ULL << BTN_B) | (1ULL << BTN_C);
    esp_sleep_enable_ext1_wakeup(mascara_botones, ESP_EXT1_WAKEUP_ALL_LOW);

    // 3. A DORMIR. (El código se pausa exactamente aquí y el consumo baja a 2mA)
    Serial.println(">>> Procesador en Light Sleep Zzz...");
    Serial.flush(); // Asegura que el texto se envía por USB antes de apagar

    esp_light_sleep_start();

    // 4. AL DESPERTAR. (El código continúa por aquí)
    // Cuando el usuario pulse un botón o pasen 60s, el loop() simplemente dará
    // su siguiente vuelta normal calculando el tiempo transcurrido perfectamente.

    // --- FIN DEL BLOQUE LIGHT SLEEP --- */
  }

  // --- 2. APAGAR PANTALLA POR INACTIVIDAD ---
  if (pantalla_encendida && (horaActual - ultimo_toque_global > 30000))
  {
    // NUEVO: Evitamos apagar la pantalla si hay una animación crítica en curso
    if (pet.estado_actual == ESTADO_EVOLUCION || pet.estado_actual == ESTADO_ACCION)
    {
      ultimo_toque_global = horaActual; // Le damos tiempo extra para que termine
    }
    else
    {
      // Forzamos el reinicio a IDLE para no dejar menús a medias.
      if (pet.estado_actual != ESTADO_IDLE && pet.estado_actual != ESTADO_RELOJ && pet.estado_actual != ESTADO_BOOT)
      {
        pet.estado_actual = ESTADO_IDLE;
      }
      menu_index = -1;
      display.ssd1306_command(SSD1306_DISPLAYOFF);
      pantalla_encendida = false;
      Serial.println(">>> 💤 Pantalla apagada por inactividad.");
    }
  }

  // --- MOTOR BIOLÓGICO ---
  // Solo procesamos la vida si NO estamos en menús críticos de inicio o configuración
  if (pet.estado_actual == ESTADO_IDLE || pet.estado_actual == ESTADO_MENU || pet.estado_actual == ESTADO_ACCION || pet.estado_actual == ESTADO_SUBMENU_COMIDA || pet.estado_actual == ESTADO_SUBMENU_STATS)
  {
    procesarBiologia(horaActual);
  }

  // --- MONITOR DE ALARMAS GLOBAL ---
  static bool alarmaProcesadaGlobal = false;

  if (pet.needs_attention == true)
  {
    // 🔒 NUEVO: La alarma espera si estamos en plena animación de nacimiento/evolución
    if (alarmaProcesadaGlobal == false && pet.estado_actual != ESTADO_EVOLUCION)
    {
      Serial.println(">>> 🚨 ALARMA CRÍTICA: Sonando en segundo plano.");
      reproducirSonido(SND_LLAMADA);
      alarmaProcesadaGlobal = true;
    }
  }
  else
  {
    alarmaProcesadaGlobal = false;
  }

  // --- ENCENDIDO AUTOMÁTICO POR EVENTOS CRÍTICOS ---
  // Si la mascota evoluciona o muere mientras la pantalla está apagada por inactividad, la despertamos.
  if ((pet.estado_actual == ESTADO_EVOLUCION || pet.estado_actual == ESTADO_MUERTE) && !pantalla_encendida)
  {
    display.ssd1306_command(SSD1306_DISPLAYON);
    pantalla_encendida = true;
    ultimo_toque_global = horaActual;
    Serial.println(">>> ✨ Pantalla encendida automáticamente para evento crítico.");
  }

// =========================================================================
  // --- TELEMETRÍA GLOBAL DETALLADA (BETA 1.1 - FULL DEBUG) ---
  // =========================================================================
  static uint32_t ultimo_print_hud = 0;
  if (horaActual - ultimo_print_hud >= 1000) // Se ejecuta cada 1 segundo
  {
    ultimo_print_hud = horaActual;

    Serial.println("\n>>> [--- " + String(reloj_h < 10 ? "0" : "") + String(reloj_h) + ":" + String(reloj_m < 10 ? "0" : "") + String(reloj_m) + " ---] <<<");

    // 1. ESTADO FISIOLÓGICO (CORAZONES Y ATRIBUTOS)
    Serial.print("FISIO:  [Hambre:");
    Serial.print(pet.hunger); // Muestra el valor actual de 0 a 4
    Serial.print("/4 | Feliz:");
    Serial.print(pet.happiness);
    Serial.print("/4 | Disc:");
    Serial.print(pet.discipline);
    Serial.print("% | Peso:");
    Serial.print(pet.weight);
    Serial.print("g | Edad:");
    Serial.print(pet.age_days);
    Serial.println("d]");

    // 2. IDENTIDAD Y GENÉTICA
    Serial.print("SISTEMA:[Gen:");
    Serial.print(pet.generation);
    Serial.print(" | Etapa:");
    Serial.print(pet.stage); // Muestra 0 (Huevo), 1 (Bebé) o 2 (Niño)
    Serial.print(" | Tipo:");
    Serial.print(pet.type);
    Serial.print(" | BabyType:");
    Serial.println(pet.baby_type);

    // 3. METABOLISMO (Tiempo restante / Intervalo activo)
    uint32_t mult = pet.is_sleeping ? 4 : 1; // El metabolismo es 4x más lento al dormir
    long tH = max(0L, (long)((pet.hunger_interval * mult) - (horaActual - pet.last_hunger_time)) / 1000);
    long tF = max(0L, (long)((pet.happiness_interval * mult) - (horaActual - pet.last_happiness_time)) / 1000);
    long tP = max(0L, (long)(pet.poop_interval - (horaActual - pet.last_poop_time)) / 1000);

    Serial.print("RELOJES:[T-Hambre:");
    Serial.print(tH);
    Serial.print("s | T-Feliz:");
    Serial.print(tF);
    Serial.print("s | T-Caca:");
    Serial.print(tP);
    Serial.print("s | Mult:");
    Serial.print(mult);
    Serial.println("x]");

    // 4. HIGIENE Y RIESGO SANITARIO
    long suciedad = pet.dirt_accumulation / 1000;
    uint8_t vel_suciedad = (pet.poop_counter > 0) ? pet.poop_counter : 0;
    // El límite es de 1800s (30min) para bebés y 14400s (4h) para el resto
    uint32_t limite_actual = (pet.stage == 1) ? 1800 : 14400; 

    Serial.print("RIESGO: [Suciedad:");
    Serial.print(suciedad);
    Serial.print("/");
    Serial.print(limite_actual);
    Serial.print("s | Vel: x"); // Velocidad de acumulación según número de cacas
    Serial.print(vel_suciedad);
    Serial.print(" | Cacas:");
    Serial.print(pet.poop_counter);
    Serial.print("/4 | Sick:");
    Serial.print(pet.health_status == 1 ? "SI" : "NO");
    if(pet.health_status == 1) {
        // Muestra cuánto tiempo queda antes de la muerte por enfermedad
        Serial.print(" (Muere en:");
        Serial.print(max(0L, (long)(21600000 - (horaActual - pet.sickness_start)) / 1000));
        Serial.print("s)");
    }
    Serial.println("]");

    // 5. ALERTAS Y TIMEOUTS DE CUIDADO
    Serial.print("STATUS: [Atencion:");
    Serial.print(pet.needs_attention ? "SI" : "NO");
    Serial.print(" | Rebelde:");
    Serial.print(pet.is_rebelling ? "SI" : "NO");
    Serial.print(" | Sueño:");
    Serial.print(pet.is_sleeping ? "Zzz" : "D");
    Serial.print(" | Luz:");
    Serial.print(pet.is_light_on ? "ON" : "OFF");
    Serial.print(" | CM:");
    Serial.print(pet.care_mistakes);
    Serial.print("/5 | Alarma:");
    if (pet.needs_attention && !pet.mistake_processed) {
        // Cuenta atrás de 15 minutos para atender la llamada
        Serial.print(max(0L, (long)(900000 - (horaActual - pet.attention_start)) / 1000));
        Serial.print("s");
    } else {
        Serial.print("--");
    }
    Serial.println("]");

    // 6. PRONÓSTICO DE EVOLUCIÓN
    String forecast = "???";
    if (pet.stage == 0) forecast = "HUEVO (Eclosion)";
    else if (pet.stage == 1) {
        // Lógica de evolución basada en peso, disciplina y errores
        if (pet.weight >= 20) forecast = "EL BEBOTE (T3)";
        else if (pet.care_mistakes == 0 && pet.discipline >= 25) forecast = "EL RAYITO (T1)";
        else forecast = (pet.baby_type == 1) ? "EL TRASTO (T4)" : "EL TIZON (T2)";
    }
    
    long tEvo = (pet.stage == 0) ? (60000 - pet.evolution_timer) : (3600000 - pet.evolution_timer);
    Serial.print("DESTINO:[T-Evo:");
    Serial.print(max(0L, tEvo / 1000));
    Serial.print("s | Ruta:");
    Serial.println(forecast + "]");
    Serial.println("===========================================================================");
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
      ultimoToqueA = millis();
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
      ultimoToqueC = millis();
      estadoAnteriorC = lecturaC;
    }
    break;

  case ESTADO_IDLE:

    // --- LECTURA DEL BOTÓN A (Navegar) ---
    lecturaA = digitalRead(BTN_A);

    // Si el estado cambia (al pulsar o al soltar) Y han pasado 50ms desde la última vibración
    if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
    {
      if (lecturaA == LOW)
      {
        if (pet.stage > 0) // El bicho ya ha nacido
        {
          reproducirSonido(SND_NAVEGAR);
          menu_index++;
          if (menu_index > 6)
            menu_index = 0;
          actualizarPantalla();
        }
        else // Sigue siendo un huevo
        {
          reproducirSonido(SND_NAVEGAR);
          menu_index++;
          if (menu_index > 6)
            menu_index = 0;
          actualizarPantalla();
        }
      }

      // Se actualiza la memoria tanto al pulsar (LOW) como al soltar (HIGH)
      ultimoToqueA = millis();
      estadoAnteriorA = lecturaA;
    }

    // --- LECTURA DEL BOTÓN B (Confirmar o ver reloj) ---
    lecturaB = digitalRead(BTN_B);

    // CERROJO EXTRA: Si la pantalla está apagada, ignoramos TODO el bloque
    if (!pantalla_encendida)
    {
      estadoAnteriorB = lecturaB; // Mantenemos sincronía pero no hacemos nada
    }
    else if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
    {
      if (lecturaB == LOW)
      {
        reproducirSonido(SND_CONFIRMAR);

        if (menu_index != -1)
        {
          // --- NUEVA LÓGICA SIN RETURN ---
          if (pet.stage == 0 && menu_index != 5)
          {
            reproducirSonido(SND_ERROR);
            menu_index = -1;
            // No hacemos nada más, pero NO salimos con return
          }
          else if (pet.is_sleeping == true && menu_index != 5 && menu_index != 6)
          {
            reproducirSonido(SND_ERROR);
            Serial.println(">>> Shhh... Acción bloqueada. La mascota duerme. 💤");
            menu_index = -1;
            actualizarPantalla();
          }
          else
          {
            // --- LÓGICA NORMAL DE MENÚS ---
            if (menu_index == 0)
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
                submenu_index = 0;
              }
            }
            else if (menu_index == 1)
            { // JUEGO
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
              }
            }
            else if (menu_index == 2)
            { // BAÑO
              if (pet.poop_counter > 0)
              {
                // 🔒 ARREGLO: Ya no reseteamos aquí. Solo activamos la animación.
                pet.current_action = 'l';
                pet.estado_actual = ESTADO_ACCION;
                pet.action_start = horaActual;
                reproducirSonido(SND_CONFIRMAR);
              }
            }
            else if (menu_index == 3)
            { // MEDICINA
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
            else if (menu_index == 4)
            {
              // DISCIPLINA acepta regañina por berrinche O por rebeldía
              bool es_berrinche = (pet.needs_attention && pet.hunger > 0 && pet.happiness > 0);
              bool es_rebeldia = pet.is_rebelling;

              if (es_berrinche || es_rebeldia)
              {
                pet.discipline = min((int)pet.discipline + 25, 100);
                pet.is_rebelling = false; // ¡Curamos la rebeldía!

                // No apagar el icono si se está muriendo
                if (pet.hunger > 0 && pet.happiness > 0)
                {
                  pet.needs_attention = false;
                  pet.mistake_processed = true;
                }
                else
                {
                  pet.needs_attention = true;    // Sigue necesitando ayuda vital
                  pet.mistake_processed = false; // La alarma puede volver a sonar
                }

                pet.current_action = 'd';
              }
              else
              {
                reproducirSonido(SND_ERROR);
                pet.current_action = 'n';
              }
              pet.estado_actual = ESTADO_ACCION;
              pet.action_start = horaActual;
            }
            else if (menu_index == 5)
            { // ESTADO (STATS)
              pet.estado_actual = ESTADO_SUBMENU_STATS;
              submenu_index = 0;
              actualizarPantalla();
              reproducirSonido(SND_CONFIRMAR);
            }
            else if (menu_index == 6)
            { // LUZ
              pet.is_light_on = !pet.is_light_on;
              if (pet.is_sleeping && !pet.is_light_on)
                pet.needs_attention = false;
            }
            menu_index = -1;
            actualizarPantalla();
          }
        }
        else // Ver reloj
        {
          animacionArrastre(true);
          pet.estado_actual = ESTADO_VER_RELOJ;
          actualizarPantalla();
        }
      }

      // ESTO SE EJECUTA SIEMPRE (Incluso para el huevo)
      ultimoToqueB = millis();
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
      ultimoToqueC = millis();
      estadoAnteriorC = lecturaC;
    }

    // 5. RELOJ DE INTERFAZ (Consola cada 1 segundo)
    if (horaActual - tiempoUltimoLatido >= INTERVALO)
    {

      // --- PASEO CON INTENCIÓN (PASOS LARGOS) ---
      // Solo se mueve si: está despierto, NO está enfermo y no hay menús abiertos
      if (pet.stage > 0 && !pet.is_sleeping && pet.health_status == 0)
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
      frame_animation = !frame_animation;
      // Ciclo de 4 para controlar la frecuencia de frames raros
      idle_frame_index = (idle_frame_index + 1) % 4;
      actualizarPantalla();

      tiempoUltimoLatido = horaActual;
    }
    break;

  case ESTADO_MENU:
    // Placeholder para futuras cosas como comidas, items...
    break;

  case ESTADO_ACCION:
    {
      // 1. Calculamos la duración: 5 segundos para limpiar (2s barrido + 3s happy), 3 para el resto
      uint32_t duracion = (pet.current_action == 'l') ? 5000 : 3000;

      // --- LECTURA DE BOTONES PARA EL SKIP ---
      lecturaB = digitalRead(BTN_B);
      if (lecturaB != estadoAnteriorB && (horaActual - ultimoToqueB > 50))
      {
        if (lecturaB == LOW)
          pet.action_start = horaActual - duracion; // Salta justo al final
        ultimoToqueB = millis();
        estadoAnteriorB = lecturaB;
      }

      lecturaC = digitalRead(BTN_C);
      if (lecturaC != estadoAnteriorC && (horaActual - ultimoToqueC > 50))
      {
        if (lecturaC == LOW)
          pet.action_start = horaActual - duracion; // Salta justo al final
        ultimoToqueC = millis();
        estadoAnteriorC = lecturaC;
      }

      // --- EL FINALIZADOR: Comprobamos si la acción ha terminado ---
      if (horaActual - pet.action_start >= duracion)
      {
        if (pet.current_action == 'l')
        {
          // AQUÍ ocurre la limpieza real de datos tras ver la animación
          pet.poop_counter = 0;
          pet.dirt_accumulation = 0;
          pet.last_poop_time = horaActual;
          
          pet.x = 48; // Reaparece en el centro para el paseo
          pet.y = 16;
          pet.estado_actual = ESTADO_IDLE;
        }
        else if (pet.current_action == 'c' || pet.current_action == 's')
        {
          pet.estado_actual = ESTADO_SUBMENU_COMIDA;
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
        // --- ANIMACIÓN Y SONIDO (Mientras la acción está en curso) ---
        
        // Latido rápido para refrescar la pantalla (20 FPS)
        static uint32_t ultimoFrameAnimacion = 0;
        if (horaActual - ultimoFrameAnimacion >= 50)
        {
          actualizarPantalla();
          ultimoFrameAnimacion = horaActual;
        }

        // Latido de 1 segundo para sonidos y lógica
        if (horaActual - tiempoUltimoLatido >= INTERVALO)
        {
          frame_animation = !frame_animation;
          long elapsed = horaActual - pet.action_start;

          if (pet.current_action == 'c' || pet.current_action == 's')
            reproducirSonido(SND_COMER);
          else if (pet.current_action == 'l') {
             // El sonido solo suena durante los 2s de movimiento
             if (elapsed < 2000) reproducirSonido(SND_NAVEGAR); 
          }
          else if (pet.current_action == 'm')
            reproducirSonido(SND_CURAR);
          else if (pet.current_action == 'd')
            reproducirSonido(SND_ERROR);

          tiempoUltimoLatido = horaActual;
        }
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
      ultimoToqueA = millis();
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
        else if (pet.is_rebelling || ((pet.hunger == 0 || pet.happiness == 0) && random(0, 100) < (15 - pet.discipline / 5)))
        {
          pet.is_rebelling = true;
          pet.needs_attention = true;
          pet.attention_start = horaActual;
          pet.mistake_processed = false;

          reproducirSonido(SND_ERROR);
          pet.current_action = 'n';
          pet.estado_actual = ESTADO_ACCION;
          pet.action_start = horaActual;
        }
        // 2. SI ESTÁ SANA: Miramos qué ha elegido 🍎🍬
        else
        {
          if (submenu_index == 0) // Ha elegido COMIDA
          {
            // Solo come si no está totalmente lleno (máximo 4)
            if (pet.hunger < 4)
            {
              bool era_emergencia_hambre = (pet.hunger == 0); // Chequeo previo

              pet.current_action = 'c';
              pet.hunger++;
              if (pet.weight < 99)
                pet.weight++;

              // SOLO se apaga si estábamos salvándolo de la inanición
              if (era_emergencia_hambre && pet.happiness > 0)
              {
                pet.needs_attention = false;
                pet.mistake_processed = false;
              }
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
            bool era_emergencia_feliz = (pet.happiness == 0); // 🔒 Chequeo previo

            pet.current_action = 's';
            if (pet.happiness < 4)
              pet.happiness++;
            if (pet.weight < 98)
              pet.weight += 2;
            else
              pet.weight = 99;
            pet.snack_count++;

            if (pet.snack_count >= 5)
            {
              pet.health_status = 1;
              pet.sickness_start = horaActual;
              pet.snack_count = 0;
            }

            // SOLO se apaga si estábamos salvándolo de la tristeza extrema
            if (era_emergencia_feliz && pet.hunger > 0)
            {
              pet.needs_attention = false;
              pet.mistake_processed = false;
            }
            pet.estado_actual = ESTADO_ACCION;
            pet.action_start = horaActual;
          }
        }

        // 2. FINALIZACIÓN
        menu_index = -1;      // Quitamos el cursor del menú por si acaso
        actualizarPantalla(); // Refrescamos para que desaparezca el menú y empiece el dibujo
      }
      ultimoToqueB = millis();
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
      ultimoToqueC = millis();
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
      ultimoToqueA = millis();
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
      ultimoToqueC = millis();
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
    lecturaA = digitalRead(BTN_A);
    lecturaC = digitalRead(BTN_C);

    if (lecturaA == LOW && lecturaC == LOW)
    {
      Serial.println(">>> REINICIO CON SUCESIÓN: Combinación A+C detectada. 🔄");

      // 1. Preparamos el legado
      uint8_t proxima_gen = pet.generation + 1;
      uint8_t ancestro = pet.baby_type; // El tipo de bebé que acaba de morir

      // 2. Limpiamos partida pero salvamos el linaje
      memoria.begin("tama_save", false);
      memoria.clear(); // Borramos la mascota muerta y sus stats
      memoria.putUInt("gen_legado", proxima_gen);
      memoria.putUInt("bebe_legado", ancestro);
      memoria.end();

      // Feedback visual
      display.clearDisplay();
      display.setCursor(30, 28);
      display.print("NUEVO HUEVO");
      display.display();
      delay(1500);

      ESP.restart(); // Reiniciamos para que setup() lea el nuevo linaje
    }
    break;

  case ESTADO_RELOJ:
    // BOTÓN A: Incrementar número (Con avance rápido)
    lecturaA = digitalRead(BTN_A);
    static bool estaPulsadoA = false;
    static uint32_t tiempoPulsadoA = 0;

    // 1. Detección del CLIC limpio
    if (lecturaA != estadoAnteriorA && (horaActual - ultimoToqueA > 50))
    {
      if (lecturaA == LOW)
      {
        estaPulsadoA = true;
        tiempoPulsadoA = horaActual;

        // Acción de un solo toque
        if (reloj_fase == 0)
          reloj_h = (reloj_h + 1) % 24;
        else
          reloj_m = (reloj_m + 1) % 60;
        actualizarPantalla();
      }
      else
      {
        estaPulsadoA = false; // Soltamos el botón
      }
      ultimoToqueA = horaActual;
      estadoAnteriorA = lecturaA;
    }

    // 2. Detección de MANTENER PULSADO (Avance rápido)
    if (estaPulsadoA && lecturaA == LOW && (horaActual - tiempoPulsadoA > 500))
    {
      if (horaActual - ultimoToqueA > 150)
      { // Velocidad del turbo
        if (reloj_fase == 0)
          reloj_h = (reloj_h + 1) % 24;
        else
          reloj_m = (reloj_m + 1) % 60;
        actualizarPantalla();
        ultimoToqueA = horaActual; // Reseteamos el tick del turbo
      }
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
          // Confirmamos minutos y EMPEZAMOS/VOLVEMOS AL JUEGO

          // --- 1. CONTINUIDAD BIOLÓGICA (EFECTO PAUSA) ---
          if (!pet.is_sleeping)
          {
            pet.last_hunger_time = horaActual;
            pet.last_happiness_time = horaActual;
            pet.last_poop_time = horaActual;
            if (pet.starvation_start > 0)
              pet.starvation_start = horaActual;
            if (pet.sickness_start > 0)
              pet.sickness_start = horaActual;
            // Pausar evita el Care Mistake
            if (pet.needs_attention)
              pet.attention_start = horaActual;
          }

          // --- 2. EVALUACIÓN INSTANTÁNEA DE SUEÑO ---
          // (Tu código de evaluación de sueño aquí, está impecable)
          bool deberia_dormir = false;
          if (pet.sleep_hour > pet.wake_hour)
          {
            if (reloj_h >= pet.sleep_hour || reloj_h < pet.wake_hour)
              deberia_dormir = true;
          }
          else
          {
            if (reloj_h >= pet.sleep_hour && reloj_h < pet.wake_hour)
              deberia_dormir = true;
          }

          if (deberia_dormir && !pet.is_sleeping)
          {
            pet.is_sleeping = true;
            pet.sleep_start = horaActual;
            pet.needs_attention = true;
            pet.attention_start = horaActual;
            pet.mistake_processed = false;
          }
          else if (!deberia_dormir && pet.is_sleeping)
          {
            pet.is_sleeping = false;
            pet.is_light_on = true;
            pet.needs_attention = false;

            // Solo reseteamos el reloj de la caca para empezar el día limpios.
            pet.last_poop_time = horaActual;

            if (pet.hunger == 0 || pet.happiness == 0)
            {
              pet.mistake_processed = false;
            }
          }
          // --- 3. RETORNO AL JUEGO Y SINCRONIZACIÓN TOTAL ---
          pet.estado_actual = ESTADO_IDLE;
          ultimo_toque_global = horaActual;
          pet.action_start = horaActual;

          // PARCHE DE SINCRONIZACIÓN DE INTERFAZ
          // Reseteamos todos los cronómetros de la pantalla para empezar de cero
          ultimo_minuto_ms = horaActual;
          ultimo_print_hud = horaActual;
          tiempoUltimoLatido = horaActual;

          guardarPartida(); // Salvamos la nueva hora y el estado
        }
        actualizarPantalla();
      }
      ultimoToqueB = millis();
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
      ultimoToqueC = millis();
      estadoAnteriorC = lecturaC;
    }
    break;

  case ESTADO_VER_RELOJ:
  {

    // Si cambia el minuto mientras miramos la pantalla, forzamos redibujo
    static uint8_t ultimo_segundo_visto = 60;
    uint8_t sec_actual = (millis() / 500) % 2; // Alterna entre 0 y 1 cada medio segundo
    if (sec_actual != ultimo_segundo_visto)
    {
      actualizarPantalla();
      ultimo_segundo_visto = sec_actual;
    }

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
        ultimoToqueB = millis();
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
        ultimoToqueC = millis();
        estadoAnteriorC = lecturaC;
      }
    }
  }
  break;

  case ESTADO_GAME:

    procesarMinijuego(horaActual);

    break;
  }
}
