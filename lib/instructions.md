# Documento de Arquitectura y Especificación Técnica
**Proyecto:** TAMAGOTCHI PDP
**Versión Actual:** Alpha 0.2
**Plataforma Objetivo:** ESP32 DevKitC v4

---

## 1. Objetivo General del Proyecto
Desarrollar un sistema embebido interactivo de simulación biológica virtual (tipo "pet-sim" clásico). El sistema debe operar bajo una arquitectura **asíncrona y no bloqueante**, gestionando múltiples cronómetros metabólicos paralelos (hambre, felicidad, higiene, edad) basados en el reloj de hardware. La interacción del usuario se procesa a través de interrupciones lógicas por software (debounce por tiempo) leídas desde una botonera física de 3 vías, reflejando el estado del sistema en una matriz de píxeles OLED monocroma con renderizado de interfaz dinámica.

---

## 2. Lista de Materiales (BOM)

| Componente | Especificaciones Técnicas | Cantidad | Rol en el Sistema |
| :--- | :--- | :--- | :--- |
| **Microcontrolador** | ESP32 DevKitC v4 (Módulo WROOM-32) | 1 | Unidad Central de Procesamiento (Lógica y Renderizado). |
| **Pantalla** | Módulo OLED SSD1306 | 1 | Interfaz visual monocroma (128x64 px). Comunicación I2C. |
| **Pulsador A** | Switch Táctil (NA) | 1 | Control de navegación de la interfaz (Eje X cíclico). |
| **Pulsador B** | Switch Táctil (NA) | 1 | Disparador de ejecución (Confirmación de Comando). |
| **Pulsador C** | Switch Táctil (NA) | 1 | Interrupción de anulación (Cancelación / Salida). |
| **Resistencias** | Internas del ESP32 | 3 | Pull-up activado por software. |

---

## 3. Pinout y Mapa de Conexiones

**Topología Lógica:** Los pulsadores cierran circuito contra Tierra (GND). Se utiliza lógica **Activa en Baja (LOW)** mediante la configuración `INPUT_PULLUP`.

| Periférico | Pin Físico Componente | Pin Asignado ESP32 | Protocolo / Dirección / Modo |
| :--- | :--- | :--- | :--- |
| **OLED (I2C)** | SDA | `GPIO 21` | Bus de Datos I2C (Address: `0x3C`) |
| **OLED (I2C)** | SCL | `GPIO 22` | Señal de Reloj I2C |
| **OLED (Alimentación)**| VCC / GND | `3V3` / `GND` | Alimentación 3.3V lógica |
| **Botón A (Navegar)** | Terminal 1 (Terminal 2 a GND)| `GPIO 25` | `INPUT_PULLUP` (Activo en LOW) |
| **Botón B (Acción)** | Terminal 1 (Terminal 2 a GND)| `GPIO 26` | `INPUT_PULLUP` (Activo en LOW) |
| **Botón C (Cancelar)**| Terminal 1 (Terminal 2 a GND)| `GPIO 27` | `INPUT_PULLUP` (Activo en LOW) |

---

## 4. Arquitectura de Software

### 4.1. Dependencias y Controladores
* `<Arduino.h>`: Framework base.
* `<Wire.h>`: Stack del bus I2C.
* `<Adafruit_GFX.h>`: Rasterizador de primitivas gráficas y texto.
* `<Adafruit_SSD1306.h>`: Driver del hardware del display.
* `"sprites.h"`: Cabecera estática local. Almacena arrays de imágenes indexadas en la memoria Flash (`PROGMEM`) para optimización de RAM.

### 4.2. Modelo de Datos Central: `MascotaData` (Struct)
El estado de la máquina se mantiene en una instancia global estática llamada `pet`. Se compone de las siguientes variables:

**A. Variables Fisiológicas y de Estado:**
* `uint8_t stage`: Etapa evolutiva actual (0 = Base).
* `uint8_t type`: Variante morfológica basada en condiciones de cuidado.
* `EstadosMascota estado_actual`: Puntero principal de la FSM (Enum).
* `int8_t hunger` / `happiness`: Contadores de necesidades (Rango: 0 a 4).
* `uint16_t weight`: Peso acumulativo (Rango: 0 a 9999).
* `uint8_t health_status`: Vector de salud (0: Sano, 1: Enfermo, 2: Terminal).
* `uint8_t poop_counter`: Contador de higiene (Rango: 0 a 4).
* `uint8_t discipline`: Variable de comportamiento (Rango: 0 a 100).
* `uint8_t care_mistakes`: Acumulador de fallos (Condición Letal = 5).

**B. Cronometría Metabólica (`uint32_t` para `millis()`):**
* *Constantes de intervalo:* `hunger_interval` (20s), `happiness_interval` (25s), `poop_interval` (15s).
* *Marcadores de tiempo estáticos:* `last_poop_time`, `last_hunger_time`, `last_happiness_time`, `birth_time`.

**C. Lógica de Interfaz y Variables Auxiliares:**
* `bool frame_animation`: Alternador de animación global a 1Hz.
* `int8_t menu_index`: Puntero del Menú OSD (-1 = Inactivo, 0-7 = Elementos UI).
* *Debounce:* `estadoAnteriorA/B/C`, `ultimoToqueA/B/C`, `lecturaA/B/C`.

### 4.3. Máquinas de Estados Finitos (FSM)

El ciclo `loop()` rige un switch maestro evaluando `pet.estado_actual`:

1.  **`ESTADO_IDLE`:** Bucle núcleo. Escucha inputs de hardware (Debounce: 50ms), evalúa cronómetros metabólicos, calcula deterioro biológico, verifica condiciones letales (enfermedad por cacas = 4 o care_mistakes = 5) e imprime la UI dinámica.
2.  **`ESTADO_ACCION`:** Interrupción de estado temporal (3000ms). Evalúa `pet.current_action`. Bloquea la interacción por botones, pero no detiene los relojes biológicos que corren en background.
3.  **`ESTADO_GAME`:** Desvía el flujo de control hacia una sub-FSM indexada por `pet.game_phase` para procesar el minijuego interno (adivinanza Mayor/Menor aleatoria de 0 a 10).
4.  **`ESTADO_MUERTE`:** Termina la simulación biológica. Solo escucha la bandera de hardware de reinicio lógico (`comando = 'r' -> setup()`).

### 4.4. Motor de Renderizado (`actualizarPantalla()`)
Se utiliza un enfoque de redibujado de buffer completo. La matriz se divide de la siguiente forma:
* **Layer 1 (Menú Activo):** Mapeo de `menu_index` contra un vector de coordenadas fijas (`menu_x[8]`, `menu_y[8]`). Dibuja bitmaps de 16x16 px alojados en PROGMEM de manera condicional (solo renderiza si el índice coincide).
* **Layer 2 (Entorno/Higiene):** Dibuja sprites de residuos (`epd_bitmap_poop`) en formato matriz 2x2 anclados en el sector inferior derecho (X: 87 a 105, Y: 18 a 30).
* **Layer 3 (Actor Principal):** Renderizado en el centro absoluto (X: 48, Y: 16) condicionado a `frame_animation`.

---

## 5. Roadmap: Problemas Pendientes y Tareas Futuras (Para IA / Desarrollo)

El sistema ha superado la viabilidad mecánica. El siguiente ciclo iterativo (Alpha 0.3) requiere resolver la representación gráfica de subsistemas existentes:

1.  **Refactorización de Interfaz del Minijuego (`ESTADO_GAME`):**
    * *Problema:* Actualmente opera exclusivamente mediante salida `Serial`.
    * *Objetivo:* Sobrescribir `actualizarPantalla()` en la condición `ESTADO_GAME`. Imprimir en OLED el número base (`pet.current_number`), iconos indicadores de "Up/Down" para lectura del usuario y UI de victorias (`pet.game_wins`).

2.  **Módulo de Renderizado de Stats (Menú Índice 5):**
    * *Problema:* El comando interno para acceder a los stats no está mapeado a una visualización de hardware.
    * *Objetivo:* Programar rutina gráfica que limpie el lienzo del Actor Principal e imprima: corazones vacíos/llenos proporcionales a las variables `hunger` y `happiness` (escala 0-4) y el valor `int` literal interpolado en un string de `weight` + "g".

3.  **Sistema Visual de Alertas Biomédicas:**
    * *Problema:* La enfermedad está operando lógicamente (`health_status > 0`) pero no posee "call to action" visual.
    * *Objetivo:* Implementar sprite de calavera (16x16 px) reservando el cuadrante inferior izquierdo (ej. `X=8, Y=30`) para alertar al usuario. Debe renderizarse obligatoriamente si `health_status == 1`.

4.  **Desacoplamiento Dinámico del Icono "Atención" (Índice 7):**
    * *Problema:* Actualmente solo es visible al navegar.
    * *Objetivo:* Modificar la rutina del layer del HUD para que evalúe `if (pet.needs_attention == true)`. En este caso afirmativo, se debe renderizar el icono de alarma de la posición 7 (HUD inferior derecho) haciendo un *bypass* de la validación del vector `menu_index`.

5.  **Animación Dinámica de Estados Transicionales:**
    * *Problema:* `ESTADO_ACCION` renderiza por defecto un rectángulo blanco hueco `fillRect`.
    * *Objetivo:* Asociar un puntero lógico que cambie el bitmap renderizado durante el bloqueo asíncrono basado en la letra guardada en la variable temporal `current_action`.