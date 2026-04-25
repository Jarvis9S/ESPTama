Tama32 - ESP32 Virtual Pet Firmware

Resumen del Proyecto

Tama32 es un firmware de código abierto desarrollado en C++ para microcontroladores basados en la arquitectura ESP32. Su objetivo es la simulación algorítmica de una mascota virtual basada en las lógicas de estado de los dispositivos hardware de la década de 1990 (específicamente iteraciones P1/P2).

El firmware implementa una máquina de estados finitos (FSM), persistencia de datos en memoria no volátil (NVS), procesamiento asíncrono de señales de audio mediante FreeRTOS y renderizado gráfico optimizado para displays I2C de 128x64 píxeles.

Arquitectura de Software

El sistema está segmentado funcionalmente en módulos para garantizar una baja latencia y el desacoplamiento de la lógica de presentación respecto al motor biológico.

1. Estructura de Datos Central (globals.h)

El estado completo de la simulación se encapsula en una única estructura de datos contigua (struct MascotaData). Esta aproximación permite una serialización y deserialización directa a nivel de bytes (block memory transfer) hacia la memoria flash. Contiene variables de estado (hambre, felicidad, edad), temporizadores (timestamps en milisegundos) e indicadores de banderas condicionales (is_rebelling, needs_attention).

2. Concurrencia y Audio (hardware.cpp)

Para evitar el bloqueo del bucle principal de renderizado (loop()) derivado de funciones con retrasos síncronos (delay(), tone()), el subsistema de audio opera de forma asíncrona.

Se inicializa una tarea dedicada en FreeRTOS (taskSonido) anclada al Núcleo 0 (xTaskCreatePinnedToCore).

La comunicación entre hilos se realiza mediante una cola de mensajes (xQueueCreate(10, sizeof(SonidosUI))).

El núcleo principal (Núcleo 1) emite peticiones no bloqueantes a la cola, permitiendo que la animación gráfica mantenga su tasa de refresco inalterada mientras se sintetizan las frecuencias piezoeléctricas.

3. Persistencia de Datos (NVS)

Utiliza la librería estandarizada Preferences.h del framework de Arduino para ESP32.

La persistencia se activa periódicamente (cada 60 minutos del sistema lógico) o durante transiciones críticas de estado (al iniciar ciclos de sueño).

El guardado opera bajo el namespace tama_save, almacenando el bloque de bytes completo de MascotaData y sincronizando variables globales de cronometría (reloj_h, reloj_m).

Modelado Biológico y Algoritmos (logic.cpp)

La rutina procesarBiologia() se ejecuta de forma síncrona en cada ciclo de inactividad de la FSM, calculando deltas de tiempo (horaActual - timestamp) para actualizar los estados fisiológicos.

Algoritmo de Toxicidad y Enfermedad

El sistema de higiene no es binario; emplea una función de acumulación de tiempo ponderada.

La variable dirt_accumulation se incrementa según el tiempo transcurrido (delta) multiplicado escalarmente por la cantidad de deposiciones activas (pet.poop_counter).

El umbral letal está fijado en 14400000 ms (4 horas lógicas). Bajo máxima carga de suciedad (4 unidades), el umbral se alcanza en 1 hora real.

Superar el umbral desencadena el flag health_status = 1 y congela temporalmente la caída de otros indicadores hasta administrar tratamiento.

Rama Genética y Evolución

Las transiciones de la variable stage están controladas por un temporizador acumulativo (evolution_timer). Al alcanzar la cota superior del Stage 1, se evalúa el árbol genético mediante prioridades escalonadas:

Prioridad de Masa Corporal: Si weight >= 20, el genotipo resultante será de tipo Glotón (Tipo 3).

Prioridad de Eficiencia Operativa: Si los fallos de atención (care_mistakes) son exactamente 0 y el índice de discipline >= 25, deriva en genotipo Óptimo (Tipo 1).

Prioridad de Descuido: Valores de care_mistakes >= 2 derivan en variables complejas (Tipos 2 o 4), alterando permanentemente multiplicadores metabólicos como tantrum_chance y hunger_interval.

Subsistema Probabilístico de Disciplina

La interacción de cuidados emplea rutinas de negación pseudoaleatorias.

Llamadas Injustificadas (Berrinches): Ocurren de forma periódica controlada (temporizador base de 60s) evaluando random(0, 10000) < pet.tantrum_chance. Requiere validación cruzada (hunger > 0 y happiness > 0) para confirmar la ausencia de emergencia biológica.

Rechazo Activo (Rebeldía): Ante intentos de alimentación con estados críticos, el sistema calcula una probabilidad de bloqueo: P(bloqueo) = 15 - (pet.discipline / 5).

La aplicación de corrección disciplinaria incrementa la variable discipline en tramos fijos de 25 unidades y resetea banderas de insubordinación (is_rebelling = false).

Interfaz Física y Hardware

Requerimientos de Componentes

MCU: Procesador Tensilica Xtensa Dual-Core 32-bit LX6 (Gama ESP32 WROOM/WROVER).

Display: OLED Monocromático 128x64 px. Interfaz I2C (Controlador SSD1306).

Señalización Acústica: Transductor piezoeléctrico pasivo sin oscilador interno.

Inputs: 3 pulsadores momentáneos configurados internamente con resistencias de pull-up activas (INPUT_PULLUP). Sistema de "Debouncing" implementado por software mediante deltas de 50ms para filtrado de transitorios electromagnéticos.

Subrutinas de HUD (Navegación)

La FSM permite interactuar con las rutinas biológicas mediante 8 índices de menú mapeados a sub-estados condicionales:
0. Alimentación (ESTADO_SUBMENU_COMIDA): Procesa modificaciones en hunger (+1) o happiness (+1), acopladas invariablemente a sumatorios en weight (+1g / +2g). Un exceso de snacks (snack_count >= 5) fuerza una alteración de health_status.

Minijuego Lógico (ESTADO_GAME): Operativa R.N.G. de predicción mayor/menor a 5 rondas. Actúa como mecanismo de compensación térmica (reducción de masa corporal).

Saneamiento: Restablecimiento de variables de polución ambiental (poop_counter, dirt_accumulation).

Asistencia Médica: Purga del estado de enfermedad general.

Corrector de Comportamiento: Único vector de modificación de la variable discipline.

Telemetría / HUD: Renderizado numérico directo del struct en VRAM.

Ciclos de Luz: Alteración booleana requerida obligatoriamente durante el cruce de ventanas de sueño (sleep_hour).

Procedimiento de Despliegue (Flashing)

Entorno de Compilación Recomendado

Herramienta: PlatformIO IDE (Extensión VSCode).

Plataforma: espressif32.

Framework: arduino.

Dependencias Externas: Adafruit SSD1306, Adafruit GFX Library.

Precaución de Primera Inicialización (Critical Initialization)

Al efectuar un despliegue de firmware sobre un silicio virgen, o al requerir un borrado maestro, es perentorio formatear la tabla NVS:

Localizar el archivo de entrada main.cpp.

En el bloque lógico setup(), eliminar el marcado de comentario de la instrucción memoria.clear();.

Compilar y transferir el binario al microcontrolador.

Verificar por terminal Serie (Baudrate 115200) la instrucción de arranque inicial.

Comentar de nuevo la línea memoria.clear(); en el código fuente.

Recompilar y re-desplegar. La omisión de este último paso provocará pérdida de datos catastrófica (Data Loss) en cada pérdida de tensión (Cold Boot).

Roadmap Técnico y Proyecciones Futuras

Fase Beta 1.0: Versión actual. Estabilidad certificada del bucle principal y sistema de guardado.

Fase Beta 1.1: Migración de llamadas de cronometría estándar (millis()) a resoluciones altas vinculadas al temporizador de sistema (esp_timer_get_time()).

Fase RC 1.0: Implementación completa y acoplamiento de las interrupciones del controlador de energía esp_light_sleep_start() para reducir el consumo estático (Idle Draw) al orden de microamperios.


Registro de Cambios Actualizado (Versión 1.1)

Sustituye la sección correspondiente en tu README.md por esta versión técnica actualizada:
Subsistema de Interfaz y Gráficos (ui.cpp)

    Sprites de Enfermedad Dinámicos: Corrección en la lógica de renderizado para estados de salud comprometidos; el sistema consulta ahora los punteros obtenerSpriteTriste() y obtenerSpriteBase() de la evolución activa.

    Coreografía de Limpieza por Fases: Refactorización de la rutina de mantenimiento en una secuencia de dos etapas: barrido dinámico (2000ms) y celebración rítmica (3000ms).

    Animación de Celebración Dinámica: Se ha corregido el estado estático post-limpieza implementando una alternancia de frames entre los estados "Feliz" y "Base" sincronizada con el intervalo global frame_animation.

    Física de Arrastre de la Escoba: Implementación de detección de colisión visual; la mascota mantiene su posición original hasta ser alcanzada por la herramienta, momento en el que es desplazada fuera del área visible.

    Reubicación del Icono de Disciplina: Desplazamiento de la coordenada de renderizado del icono al sector izquierdo (X=25) para prevenir la superposición con deposiciones activas.

    Ocultación de Sprites Estáticos: Inhabilitación del renderizado de suciedad en coordenadas de origen durante la acción de limpieza para evitar parpadeos visuales.

Lógica de Control y Máquina de Estados (main.cpp)

    Secuenciación de Finalización de Acción: Implementación de un finalizador asíncrono que extiende la limpieza a 5000ms, asegurando que el reseteo de poop_counter y dirt_accumulation ocurra tras la animación.

    Sincronización Acústica: Coordinación del sonido SND_NAVEGAR para que se ejecute exclusivamente durante la ventana de movimiento de la escoba.

    Habilitación de Movimiento Autónomo: Eliminación de la condicional que restringía el desplazamiento de la mascota durante la navegación por menús.

    Gestión de Iluminación Preventiva: Validación en la rutina de sueño que suprime alarmas y errores de cuidado si el usuario apaga la luz antes de la hora programada.

Motor Biológico y Evolución (logic.cpp)

    Umbrales de Higiene Dinámicos: Límite de acumulación de suciedad reducido a 30 minutos para la etapa de bebé (Stage 1) para garantizar el impacto de la higiene en la evolución temprana.

    Exclusión de Berrinches por Salud: Modificación del generador de eventos para bloquear disparos de needs_attention si health_status es diferente de cero.

    Ajuste del Ciclo Digestivo Inicial: Aplicación de un desplazamiento temporal en el nacimiento que reduce el intervalo de la primera deposición a 15 minutos aproximadamente.

    Reseteo de Atributos en Evolución: Reinicio de las variables pet.care_mistakes y pet.discipline en las transiciones de etapa para asegurar curvas de dificultad independientes.