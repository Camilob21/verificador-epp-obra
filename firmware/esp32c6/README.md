# Firmware ESP32-C6: semáforo, relé y sensores del verificador de EPP

Firmware en C con ESP-IDF para la **ESP32-C6-DevKitC-1**. Maneja el semáforo (LEDs rojo, amarillo y verde), el buzzer, el relé, el sensor PIR y el botón de factory reset. Se comunica con la Jetson Nano por UART, con tramas protegidas por CRC, y tiene un fail-safe por latido.

- Protocolo: [`../../docs/protocolo_uart.md`](../../docs/protocolo_uart.md), código en [`../../common/protocolo/`](../../common/protocolo/)
- Guía paso a paso (bring-up): [`../../docs/firmware_guia_paso_a_paso.md`](../../docs/firmware_guia_paso_a_paso.md)
- Matriz de verificación: [`../../docs/firmware_matriz_pruebas.md`](../../docs/firmware_matriz_pruebas.md)

**Versión de ESP-IDF:** v5.5.6. Con esa versión se compiló y verificó este código, tanto en la variante UART1 como en la variante de prueba por USB.

---

## 1. Pines

Los pines se verificaron contra la [guía de usuario oficial de la ESP32-C6-DevKitC-1](https://docs.espressif.com/projects/esp-dev-kits/en/latest/esp32c6/esp32-c6-devkitc-1/user_guide.html). Se cambian en `idf.py menuconfig` → *EPP: firmware del semáforo* → *Pines*.

| Función | GPIO | Conector de la placa | Por qué este pin |
|---|---|---|---|
| LED rojo | 21 | J3-7 | GPIO libre, sin función de arranque |
| LED amarillo | 22 | J3-6 | ídem |
| LED verde | 23 | J3-5 | ídem; los tres LEDs quedan juntos |
| Buzzer | 20 | J3-8 | ídem |
| Relé (IN) | 19 | J3-9 | ídem; no es pin de arranque, así que su nivel durante el boot no afecta el arranque |
| PIR (OUT) o ECHO del HC-SR04 (por divisor) | 18 | J3-10 | ídem |
| TRIG del HC-SR04 | 2 | rotulado "2" en la placa | GPIO libre, sin función de arranque |
| Botón factory reset | 3 | J1-13 | ídem; pull-up interno |
| UART1 TX → Jetson pin 10 (RX) | 10 | J1-10 | libre; UART1 se asigna a cualquier pin por la matriz GPIO |
| UART1 RX ← Jetson pin 8 (TX) | 11 | J1-11 | ídem |

**Pines evitados y por qué:**
- GPIO4, 5, 8, 9 y 15 son de arranque (*strapping*). GPIO9 = botón BOOT; GPIO8 = LED RGB de la placa.
- GPIO12 y 13 son el USB nativo (D− y D+).
- GPIO16 y 17 son la UART0 de la consola, cableada al puente USB-UART.
- GPIO0 y 1 son las entradas del cristal de 32 kHz (se dejan libres por precaución).
- La memoria flash usa pines internos del módulo que no salen a los conectores.

## 2. Conexiones

> ⚠️ **Ningún pin del ESP32-C6 tolera 5 V.** Antes de conectar un módulo, mida con el multímetro el voltaje de su pin de salida o de entrada (ver guía, Parte 2).

```
                ESP32-C6-DevKitC-1
              ┌─────────────────────┐
  3V3 ────────┤ J1-1          J3-1  ├──── GND ─────────────┬── GND común (Jetson pin 6,
              │                     │                      │   PIR, relé, buzzer)
  Botón ──────┤ J1-13 GPIO3         │                      │
  (a GND)     │                     │                      │
  UART TX ────┤ J1-10 GPIO10        │                      │
  UART RX ────┤ J1-11 GPIO11        │                      │
  5V ─────────┤ J1-14         J3-5  ├ GPIO23 ─[330Ω]─▶|─ GND  LED verde
              │               J3-6  ├ GPIO22 ─[330Ω]─▶|─ GND  LED amarillo
              │               J3-7  ├ GPIO21 ─[330Ω]─▶|─ GND  LED rojo
              │               J3-8  ├ GPIO20 ── buzzer (ver 2.3)
              │               J3-9  ├ GPIO19 ── IN del relé (ver 2.4)
              │               J3-10 ├ GPIO18 ── OUT del PIR (ver 2.5)
              └─────────────────────┘
```

### 2.1 LEDs
GPIO → resistencia → ánodo (pata larga); cátodo → GND. Resistencia: `R = (3,3 V − Vf) / I`. Con LEDs rojos o amarillos (Vf ≈ 2 V) y unos 4 mA, **330 Ω** basta para una caja de prototipo. Si el verde es de alto brillo (Vf ≈ 3 V), 330 Ω deja menos de 1 mA y se verá tenue: use **100 Ω** (~3 mA). *Mida la Vf de sus LEDs con el multímetro en modo diodo antes de elegir.*

### 2.2 Botón
Un pulsador entre GPIO3 y GND. No necesita resistencia: el firmware activa el pull-up interno. Opcional: un condensador de 100 nF en paralelo si los cables son largos. El antirrebote es por software (50 ms).

### 2.3 Buzzer (tipo por confirmar)
**Cómo identificarlo:** conéctelo directo entre 5 V y GND durante 1 s. Si suena con un tono continuo, es **activo**; si solo hace "clic", es **pasivo**. Luego seleccione el tipo en `menuconfig` → *Actuadores*.

Los buzzers suelen consumir más corriente de la que conviene sacar de un GPIO, así que se manejan con un transistor:

```
 5V ──── (+) buzzer (−) ──┬── colector NPN (2N2222 / BC547)
                          │
               1N4148 ────┘ (cátodo a 5V; solo si es magnético)
 GPIO20 ──[1 kΩ]── base        emisor ── GND
```

Si es un **módulo de 3 pines** (VCC, GND, I/O), aliméntelo con 3,3 V y conecte I/O directo a GPIO20. Si suena al revés (cuando debería callar), es un módulo activo en bajo: avíseme y lo invertimos en el firmware.

### 2.4 Relé (modelo por confirmar)
El firmware deja el relé **apagado** al encender y ante cualquier falla. Para que eso también se cumpla en hardware:

**Caso A: módulo de 5 V con optoacoplador y jumper JD-VCC** (el más común; se activa con IN en bajo):
```
 Quitar el jumper JD-VCC.
 Conector de entrada:  VCC ← 3V3 del ESP32   IN ← GPIO19   GND ← GND del ESP32
 Conector JD-VCC:      JD-VCC ← 5V           GND ← GND de la fuente de 5 V
```
Así, el lado del optoacoplador funciona a 3,3 V: un "alto" del GPIO apaga el relé por completo y el pin nunca recibe 5 V. Durante el arranque el GPIO está como entrada y la resistencia del propio módulo mantiene IN en alto, o sea, relé apagado.

**Caso B: módulo sin optoacoplador, o con IN conectado a 5 V por una resistencia.** **No conecte IN directo al GPIO.** Use un transistor NPN como en 2.3 (con la bobina del relé en lugar del buzzer y el diodo 1N4148 **obligatorio**) y ponga una resistencia de 10 kΩ de GPIO19 a GND. En `menuconfig` desactive *El módulo de relé se activa con nivel BAJO*.

**Cómo verificarlo antes de conectar** (guía, paso 2.3): con el módulo alimentado y **sin** el ESP32, mida el voltaje del pin IN al aire. Si mide 5 V, está en el caso B.

Los contactos del relé (COM, NO, NC) se usan para la carga, por ejemplo un LED o una cerradura de 12 V con su propia fuente. **No conecte 110 V AC en el prototipo.**

### 2.5 Sensor de presencia: ultrasónico HC-SR04 (opción por defecto)

Se cambió el PIR por un HC-SR04 porque el PIR necesita calibrarse a mano (sensibilidad y tiempo) y solo dice "hay o no hay"; el ultrasónico **mide la distancia** (variable física continua) y la presencia se define por software: alguien entre 30 cm y 2,2 m (ajustable en `menuconfig` → *Entradas*).

```
 HC-SR04 VCC  ← 5V (J1-14)
 HC-SR04 GND  ← GND
 HC-SR04 TRIG ← GPIO2                (3,3 V del ESP32 alcanzan para disparar el sensor)
 HC-SR04 ECHO ─[1 kΩ]──●──[2 kΩ]── GND
                       └──────────► GPIO18   (≈ 3,3 V en alto)
```

> ⚠️ **ECHO entrega 5 V y ningún pin del ESP32-C6 los tolera: el divisor es obligatorio.** Con 1 kΩ arriba y 2 kΩ abajo, 5 V × 2/3 ≈ 3,3 V. Sirve cualquier par con relación 1:2 (2,2 kΩ / 4,7 kΩ, 10 kΩ / 20 kΩ). **No use dos resistencias iguales:** darían 2,5 V, justo en el límite de lo que el ESP32 reconoce como "alto". Antes de conectar GPIO18, mida con el multímetro el punto medio del divisor con un objeto a 10 cm del sensor: debe dar pulsos por debajo de 3,6 V.

El firmware dispara cada 60 ms, mide el ancho del eco con una interrupción, toma la mediana de las últimas 5 mediciones y aplica histéresis: 300 ms en rango para "presencia" y 1,5 s fuera de rango para "ausencia". La presencia viaja en el mismo `EVENTO_PIR` de siempre y la distancia en `EVENTO_DISTANCIA` (0x85), que el dashboard muestra en metros.

### 2.5 bis Sensor PIR (HC-SR501, opción alternativa)

Se elige en `menuconfig` → *EPP* → *Entradas* → *Sensor de presencia*.
```
 PIR VCC ← 5V      PIR GND ← GND      PIR OUT → GPIO18
```
El HC-SR501 se alimenta con 5 V, pero su salida suele ser de unos 3,3 V porque tiene un regulador interno. **Mídalo antes de conectar** (guía, paso 2.4): si en alto supera 3,6 V, ponga un divisor de 10 kΩ / 20 kΩ. Ajustes recomendados: jumper en **H** (redisparo), potenciómetro de tiempo al **mínimo** y sensibilidad a la mitad. Dura ~1 minuto de calentamiento con falsos disparos; el firmware lo ignora durante 60 s.

### 2.6 UART con la Jetson
```
 ESP32 GPIO10 (TX) ──────────► Jetson J41 pin 10 (RX)
 ESP32 GPIO11 (RX) ◄────────── Jetson J41 pin 8  (TX)
 ESP32 GND ─────────────────── Jetson J41 pin 6  (GND)
```
Los dos lados trabajan con 3,3 V: conexión directa, sin conversor de nivel.

### 2.7 Alimentación
Para el desarrollo, la placa se alimenta por USB. Del pin 5V (J1-14) salen los 5 V del USB hacia el PIR y la bobina del relé. *No verifiqué el consumo total ni la corriente máxima que entrega ese pin*: si el ESP32 se reinicia al activarse el relé (la telemetría reportará motivo = brownout), alimente el relé con una fuente de 5 V aparte, con GND común.

---

## 3. Arquitectura del firmware

| Tarea | Archivo | Prioridad | Stack | Qué hace |
|---|---|---|---|---|
| `enlace` | enlace.c | **6** | 3072 B | Cada 50 ms revisa el último latido; si pasaron > 800 ms, activa el fail-safe |
| `rx` | enlace.c | 5 | 4096 B | Lee el UART, pasa los bytes por el parser y atiende los comandos (responde ACK/NACK) |
| `salidas` | salidas.c | 4 | 3072 B | Ciclo de 10 ms: LEDs, patrones del buzzer y regla del relé |
| `entradas` | entradas.c | 3 | 3072 B | Muestreo cada 10 ms: filtro del PIR y antirrebote del botón |
| `telemetria` | telemetria.c | 1 | 4096 B | Cada 1 s (o a pedido): mide y envía la telemetría |

**Por qué estas prioridades:** la tarea de seguridad (`enlace`) tiene la mayor prioridad para que el fail-safe funcione aunque otras tareas estén ocupadas o colgadas. Le sigue `rx`, para no perder bytes; los 1024 B de buffer equivalen a ~89 ms de datos a 115200 baudios. Después vienen las salidas, que el usuario ve, y las entradas, que toleran algunos ms de retraso. La telemetría va de última. Ninguna tarea hace esperas activas: todas se bloquean (`vTaskDelayUntil`, lectura del UART con timeout o notificación), así que el uso de CPU es bajo y la tarea IDLE corre.

**Por qué estos tamaños de stack:** en ESP-IDF el stack se da en **bytes**. Elegí valores holgados porque `ESP_LOGx` usa formato tipo printf, que consume stack. La telemetría reporta el **high-water mark** (stack libre mínimo) de cada tarea. Con esos datos reales se puede ajustar: si a una tarea le sobran más de ~1500 B después de una prueba larga, se reduce dejando ~30 % de margen. *El ajuste final queda pendiente hasta tener mediciones en la placa.*

**Otras decisiones:**
- **Un solo dueño por GPIO:** solo `salidas` escribe en LEDs y buzzer; las demás tareas le envían comandos por una cola. La excepción es `salidas_rele_apagar_ya()`, que cualquier tarea puede llamar, porque apagar siempre es seguro.
- **Regla del relé:** relé ON ⇔ hay enlace **y** estado = VERDE **y** lleva menos de 8 s en verde.
- **Watchdog de tareas (2 s):** vigila `enlace`, `rx` y `salidas`. Si alguna se cuelga, el chip se reinicia, y al arrancar `salidas_init_seguro()` deja el relé apagado. La Jetson se entera del motivo por EVENTO_ARRANQUE.
- **Arranque:** lo primero que hace `app_main` es poner el relé en OFF y el LED rojo en ON. Después viene una prueba de lámparas (amarillo y verde 200 ms cada uno, más un bip) para verificar el cableado de un vistazo; se desactiva en menuconfig. Luego queda **rojo fijo** hasta el primer latido.
- **Indicación visual:** rojo fijo = arranque o estado ROJO. Rojo con un apagón corto cada segundo = **se perdió el enlace** con la Jetson. Rojo y amarillo alternando = botón mantenido. Los tres parpadeando = factory reset confirmado.
- **Transporte configurable:** UART1 (normal) o USB nativo (pruebas en el PC sin adaptador, ver 4.3).

## 4. Compilar, flashear y monitorear

Requisitos: ESP-IDF v5.5.6 instalado (guía, Parte 0) y el entorno cargado (`. ~/esp/esp-idf/export.sh`).

### 4.1 Compilar
```bash
cd firmware/esp32c6
idf.py set-target esp32c6      # solo la primera vez
idf.py build
```

### 4.2 Flashear y ver la consola
Use el conector **"UART"** de la placa (puente USB-UART, normalmente `/dev/ttyUSB0`):
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```
Para salir del monitor: `Ctrl+]`.

### 4.3 Modo de prueba por USB (sin la Jetson y sin adaptador)
El protocolo viaja por el conector **"USB"** (USB nativo, `/dev/ttyACM0`) y los logs siguen saliendo por el conector "UART". Se necesitan **dos cables USB-C**.
```bash
idf.py -B build_usb -D SDKCONFIG=sdkconfig_usb -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.prueba_usb" build
idf.py -B build_usb -D SDKCONFIG=sdkconfig_usb -p /dev/ttyUSB0 flash monitor
# en otra terminal:
python3 tools/sim_jetson.py /dev/ttyACM0
```

### 4.4 Configuración (`idf.py menuconfig` → *EPP: firmware del semáforo*)
Pines, polaridad del relé, tipo de buzzer, tiempo del fail-safe (800 ms), tiempo máximo en verde (8 s), calentamiento del PIR (60 s), tiempo del botón (5 s), periodo de la telemetría (1 s) y transporte.

**Por qué 800 ms para el fail-safe:** con latidos cada 200 ms, 800 ms tolera 3 latidos perdidos, que es un margen razonable para los retrasos de un daemon en Linux mientras corre la inferencia. Sumando el periodo de revisión (50 ms), la reacción queda en ≤ ~850 ms, dentro del requisito de 1 s. Si se perdieran latidos con frecuencia (telemetría: `fallos_enlace` > 0 sin cortes intencionales), primero conviene revisar el hilo de latido de la Jetson, no subir el tiempo.

## 5. Pruebas

| Nivel | Qué | Cómo |
|---|---|---|
| Unitarias (18) | Protocolo: CRC, codificación, parser, resincronización, robustez | `cmake -S common/protocolo -B build-proto && cmake --build build-proto && ctest --test-dir build-proto --output-on-failure` (desde la raíz del repo) |
| Memoria | Las mismas pruebas bajo valgrind | `valgrind --leak-check=full --error-exitcode=1 build-proto/test_protocolo` |
| Sanitizers | ASan + UBSan | `cmake -S common/protocolo -B build-asan -DPROTO_SANITIZERS=ON && cmake --build build-asan && build-asan/test_protocolo` |
| Integración (12 automáticas) | Firmware real en la placa, contra el simulador de la Jetson | `python3 tools/sim_jetson.py /dev/ttyACM0 --auto` (genera `resultados_integracion_*.md`) |
| Hardware / visual | LEDs, buzzer, relé, PIR, botón, arranque seguro | Guía paso a paso y matriz |

**¿Sirve el target "linux" de ESP-IDF para las pruebas en el host?** Sí existe, pero aquí no hace falta: el protocolo no depende de ESP-IDF, así que se compila directo con gcc y CMake. Eso es más simple, permite usar valgrind (lo recomendó el profesor) y es exactamente lo mismo que compila la Jetson.

## 6. Lo que NO está verificado (pendiente en la placa)

- El comportamiento real en la placa: todo se compiló, pero nada se ha ejecutado en hardware todavía.
- El modelo y la polaridad del relé, y el tipo de buzzer: hay que identificarlos (pasos 2.2 y 2.3 de la guía).
- El nivel de la salida del PIR (¿3,3 V?) y su tiempo de calentamiento real.
- El nivel del GPIO19 durante el reset y el bootloader: lo garantiza la resistencia externa; se comprueba con la prueba HW-01 (10 encendidos).
- El consumo total desde el pin 5V de la placa.
- El ajuste de los stacks: depende del high-water mark medido.
- En la Jetson: que `/dev/ttyTHS1` corresponda a los pines 8 y 10 en su JetPack 4.6.1 y que `nvgetty` esté desactivado. Le corresponde a su compañero.
