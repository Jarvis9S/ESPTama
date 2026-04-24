#include "logic.h"

void aplicarGenetica(uint8_t nuevo_tipo)
{
  pet.type = nuevo_tipo;

  switch (nuevo_tipo)
  {
  case 1: // EL RAYITO (Niño Bueno)
    pet.hunger_interval = BASE_HUNGER * 1.2;
    pet.happiness_interval = BASE_HAPPINESS * 1.2;
    pet.poop_interval = BASE_POOP * 1.0;
    pet.tantrum_chance = BASE_TANTRUM * 0.5;
    if (pet.weight < pet.min_weight)
      pet.weight = pet.min_weight;
    pet.min_weight = 10;
    pet.sleep_hour = 20;
    pet.wake_hour = 8;
    Serial.println(">>> Genética Aplicada: El Rayito (1.2x)");
    break;

  case 2: // EL TIZÓN (Niño Asustadizo)
    pet.hunger_interval = BASE_HUNGER * 1.5;
    pet.happiness_interval = BASE_HAPPINESS * 0.7;
    pet.poop_interval = BASE_POOP * 1.0;
    pet.tantrum_chance = BASE_TANTRUM * 1.0;
    if (pet.weight < pet.min_weight)
      pet.weight = pet.min_weight;
    pet.min_weight = 8;
    pet.sleep_hour = 21;
    pet.wake_hour = 9;
    Serial.println(">>> Genética Aplicada: El Tizón (H:1.5x F:0.7x)");
    break;

  case 3: // EL BEBOTE (Glotón)
    pet.hunger_interval = BASE_HUNGER * 0.6;
    pet.happiness_interval = BASE_HAPPINESS * 1.5;
    pet.poop_interval = BASE_POOP * 0.6;
    pet.tantrum_chance = BASE_TANTRUM * 0.5;
    if (pet.weight < pet.min_weight)
      pet.weight = pet.min_weight;
    pet.min_weight = 20;
    pet.sleep_hour = 22;
    pet.wake_hour = 10;
    Serial.println(">>> Genética Aplicada: El Bebote (0.6x)");
    break;

  case 4: // EL TRASTO (Rebelde/Descuidado)
    pet.hunger_interval = BASE_HUNGER * 0.8;
    pet.happiness_interval = BASE_HAPPINESS * 0.8;
    pet.poop_interval = BASE_POOP * 1.0;
    pet.tantrum_chance = BASE_TANTRUM * 2.5;
    if (pet.weight < pet.min_weight)
      pet.weight = pet.min_weight;
    pet.min_weight = 7;
    pet.sleep_hour = 23;
    pet.wake_hour = 7;
    Serial.println(">>> Genética Aplicada: El Trasto (0.8x)");
    break;
  }
}

// --- MOTOR BIOLÓGICO Y METABOLISMO ---
void procesarBiologia(uint32_t horaActual)
{
  // Si está muerto, el metabolismo se detiene por completo
  if (pet.estado_actual == ESTADO_MUERTE)
    return;

  // --- 1. EVOLUCIÓN (Bebé a Niño) ---
  static uint32_t ultimoCheckEvolucion = 0;

  if (ultimoCheckEvolucion == 0)
    ultimoCheckEvolucion = horaActual;

  if (horaActual - ultimoCheckEvolucion >= 1000)
  {
    uint32_t delta = horaActual - ultimoCheckEvolucion;
    
    if (pet.stage < 3) pet.evolution_timer += delta;
    
    // Sumar suciedad en tiempo real si hay cacas y está despierto cuanta más caca, más rápido sube la suciedad
    if (pet.poop_counter > 0 && !pet.is_sleeping) {
        pet.dirt_accumulation += (delta * pet.poop_counter);
    }

    ultimoCheckEvolucion = horaActual;
  }

  // 1.1 Reloj de Evolución: Bebé a Niño (1 min test)
  if (pet.stage == 0)
  {
    if (pet.evolution_timer >= 60000) 
    {
      pet.stage = 1;
      pet.evolution_timer = 0;

      if (pet.generation <= 1) {
        pet.baby_type = (random(0, 2) == 0) ? 1 : 2;
        if (pet.generation == 0) pet.generation = 1; 
      } else {
        if (random(0, 100) < 75) pet.baby_type = (pet.last_baby_type == 1) ? 2 : 1;
        else pet.baby_type = pet.last_baby_type;
      }

      pet.last_baby_type = pet.baby_type;
      pet.tantrum_chance = (pet.baby_type == 1) ? (BASE_TANTRUM * 2.5) : (BASE_TANTRUM * 1.5);
      
      pet.hunger = 0;
      pet.happiness = 0;
      pet.last_hunger_time = horaActual;
      pet.last_happiness_time = horaActual;
      pet.last_poop_time = horaActual;

      pet.estado_actual = ESTADO_EVOLUCION;
      pet.action_start = horaActual;
      pet.game_ticks = 0;
      Serial.println(">>> ¡Ha nacido un nuevo bicho! Generación: " + String(pet.generation));
    }
  }
  // 1.2 Reloj de Evolución: Niño a Adulto
  else if (pet.stage == 1)
  {
    if (pet.evolution_timer >= 3600000)
    {
      pet.stage = 2;          
      pet.evolution_timer = 0; 

      uint8_t tipo_elegido = 1;
      if (pet.weight >= 20) tipo_elegido = 3;
      else if (pet.care_mistakes == 0 && pet.discipline >= 25) tipo_elegido = 1;
      else if (pet.care_mistakes >= 2) tipo_elegido = (pet.baby_type == 1) ? 4 : 2;
      else tipo_elegido = (pet.baby_type == 1) ? 4 : 2;

      aplicarGenetica(tipo_elegido);

      pet.care_mistakes = 0;
      pet.estado_actual = ESTADO_EVOLUCION;
      pet.action_start = horaActual; // Usamos horaActual mejor que millis()
      pet.game_ticks = 0;
      Serial.println(">>> Evolución exclusiva aplicada según Baby Type: " + String(pet.baby_type));
    }
  }

  // --- 2. CÁMARA CRIO-GÉNICA (Motor de Supervivencia) ---
  if (pet.stage > 0 && pet.estado_actual != ESTADO_EVOLUCION)
  {
    // A. RELOJ METABÓLICO (Siempre funciona. x4 más lento si duerme)
    uint32_t mult = pet.is_sleeping ? 4 : 1;

    if (horaActual - pet.last_hunger_time >= (pet.hunger_interval * mult)) {
      if (pet.hunger > 0) {
        pet.hunger--;
        if (pet.hunger == 0) pet.mistake_processed = false; 
      }
      pet.last_hunger_time += (pet.hunger_interval * mult);
    }

    if (horaActual - pet.last_happiness_time >= (pet.happiness_interval * mult)) {
      if (pet.happiness > 0) {
        pet.happiness--;
        if (pet.happiness == 0) pet.mistake_processed = false;
      }
      pet.last_happiness_time += (pet.happiness_interval * mult);
    }

    // ========================================================
    // B. BLOQUE EXCLUSIVO PARA CUANDO ESTÁ DESPIERTO
    // ========================================================
    if (!pet.is_sleeping)
    {
      // B.1. Reloj Digestivo (Cacas)
      if (pet.poop_counter < 4 && horaActual - pet.last_poop_time >= pet.poop_interval) {
        pet.poop_counter++;
        pet.last_poop_time = horaActual;
        Serial.println("¡La mascota ha hecho caca! 💩");
      }

      // [NUEVO] B.2. Enfermedad por falta de higiene (La "Caca Vacuna")
      // 🔒 ARREGLO: Miramos el acumulador real de suciedad (14400000ms = 4 horas)
      if (pet.dirt_accumulation >= 14400000) {
        if (pet.health_status == 0) { // Solo iniciamos el timer si estaba sano
          pet.health_status = 1;
          pet.sickness_start = horaActual;
          Serial.println(">>> Enfermedad provocada por suciedad.");
        }
      }

      // [NUEVO] B.3. Berrinches y Disciplina
      static uint32_t ultimoCheckBerrinche = 0;
      if (ultimoCheckBerrinche == 0) ultimoCheckBerrinche = horaActual;

      if (horaActual - ultimoCheckBerrinche >= 60000) 
      {
        // Solo tira el dado si NO es una emergencia real (Hambre > 0 y Feliz > 0)
        if (!pet.needs_attention && pet.hunger > 0 && pet.happiness > 0 && pet.discipline < 100 && random(0, 10000) < pet.tantrum_chance) {
          pet.needs_attention = true;
          pet.attention_start = horaActual;
          pet.mistake_processed = false;
          Serial.println(">>> ¡Berrinche disparado!");
        }
        ultimoCheckBerrinche = horaActual;
      }

      // B.4. Disparador de Alarma por Supervivencia
      if ((pet.hunger == 0 || pet.happiness == 0) && !pet.needs_attention && !pet.mistake_processed) {
        pet.attention_start = horaActual;
        pet.needs_attention = true;
      }
    } // <--- FIN DEL BLOQUE DESPIERTO


    // ========================================================
    // C. BLOQUE GLOBAL (Funciona durmiendo y despierto)
    // ========================================================

    // C.1. Procesador de Caducidad de Alarma (15 minutos)
    if (pet.needs_attention && (horaActual - pet.attention_start >= 900000))
    {
      pet.needs_attention = false;
      pet.mistake_processed = true;

      if (pet.hunger == 0 || pet.happiness == 0 || (pet.is_sleeping && pet.is_light_on)) {
        pet.care_mistakes++;
        Serial.println(">>> Timeout: Care Mistake añadido. 😔");
      } else {
        Serial.println(">>> Timeout: Berrinche caducado. No hay penalización.");
      }
    }

    // C.2. CHEQUEOS LETALES (Muerte)
    
    // Muerte 1: Acumulación de Care Mistakes
    if (pet.care_mistakes >= 5 && pet.estado_actual != ESTADO_MUERTE) {
      pet.health_status = 2;
      pet.estado_actual = ESTADO_MUERTE;
      reproducirSonido(SND_MUERTE);
      return;
    }

    // Muerte 2: Inanición (12 horas con hambre 0)
    if (pet.hunger == 0) {
      if (pet.starvation_start == 0) pet.starvation_start = horaActual;
      else if (horaActual - pet.starvation_start >= 43200000) {
        pet.estado_actual = ESTADO_MUERTE;
        reproducirSonido(SND_MUERTE);
      }
    } else {
      pet.starvation_start = 0;
    }

    // Muerte 3: Enfermedad no curada (6 horas enfermo)
    if (pet.health_status == 1) {
      if (pet.sickness_start == 0) pet.sickness_start = horaActual;
      else if (horaActual - pet.sickness_start >= 21600000) {
        pet.estado_actual = ESTADO_MUERTE;
        reproducirSonido(SND_MUERTE);
      }
    } else {
      pet.sickness_start = 0;
    }

  } // Fin de cámara crio-génica
}