# Guía paso a paso: firmware del ESP32-C6 (bring-up y verificación)

Formato de cada paso: **comando** (uno por bloque) → **salida esperada** → **criterio de aceptación** → **cuándo mostrarle la salida a Claude**. Anote cada resultado en la tabla del final (formato: afirmación → cómo se verificó → resultado), que sirve de registro para la declaración de uso de IA.

Siga las partes en orden y no avance si un criterio falla.

| Parte | Contenido | Tiempo estimado |
|---|---|---|
| 0 | Entorno: ESP-IDF, permisos, puertos, hello world | 45–60 min (la descarga es lo más lento) |
| 1 | Firmware + LEDs, en modo de prueba por USB | 30 min |
| 2 | Buzzer, relé, PIR y botón | 60 min |
| 3 | Pruebas unitarias del protocolo en el PC | 10 min |
| 4 | Fail-safe y pruebas automáticas de integración | 20 min |
| 5 | Telemetría y ajuste de stacks | 15 min |
| 6 | Conexión real con la Jetson (con su compañero) | 30–60 min |

---

## Parte 0: Entorno

### 0.1 Identificar la distribución
```bash
cat /etc/os-release
```
**Esperado:** líneas como `NAME="Ubuntu"` / `VERSION_ID="24.04"` o `NAME="Fedora Linux"`.
**Criterio:** saber si usará `apt` (Ubuntu/Debian) o `dnf` (Fedora).
**Mostrar a Claude:** siempre (es corto).

### 0.2 Instalar los prerrequisitos
Ubuntu/Debian:
```bash
sudo apt-get install -y git wget flex bison gperf python3 python3-pip python3-venv cmake ninja-build ccache libffi-dev libssl-dev dfu-util libusb-1.0-0 valgrind
```
Fedora:
```bash
sudo dnf install -y git wget flex bison gperf python3 python3-pip cmake ninja-build ccache dfu-util libusbx valgrind
```
**Esperado:** termina sin `E:` (apt) o con `Complete!` (dnf).
**Criterio:** el comando termina sin errores. Esta lista viene de la guía "Get Started" de Espressif para Linux; *no la verifiqué en su máquina*.
**Mostrar a Claude:** solo si hay errores.

### 0.3 Descargar ESP-IDF v5.5.6
```bash
mkdir -p ~/esp && cd ~/esp && git clone -b v5.5.6 --recursive https://github.com/espressif/esp-idf.git
```
**Esperado:** varios minutos de `Cloning into ...` y `Submodule path ...`. Ocupa unos 700 MB.
**Criterio:** existe `~/esp/esp-idf/export.sh`.
**Mostrar a Claude:** solo si falla.

**Por qué la v5.5.6:** es la versión más reciente de la rama 5.5 (hay una v6.1 más nueva, pero la mayoría de tutoriales y ejemplos todavía son de la 5.x). Con esta versión se compiló el firmware antes de entregárselo.

### 0.4 Instalar el compilador para el ESP32-C6
```bash
cd ~/esp/esp-idf && ./install.sh esp32c6
```
**Esperado:** al final, `All done! You can now run: . ./export.sh`.
**Criterio:** aparece esa línea.
**Mostrar a Claude:** solo si falla (copie las últimas ~30 líneas).

### 0.5 Cargar el entorno (en cada terminal nueva)
```bash
. ~/esp/esp-idf/export.sh
```
**Esperado:** termina con `Done! You can now compile ESP-IDF projects.`
**Criterio:** aparece `Done!`.
**Mostrar a Claude:** solo si falla.

### 0.6 Confirmar la versión
```bash
idf.py --version
```
**Esperado:** `ESP-IDF v5.5.6`.
**Criterio:** dice exactamente v5.5.6.
**Mostrar a Claude:** siempre (es evidencia de versión para el informe).

### 0.7 Permisos del puerto serie
```bash
sudo usermod -aG dialout $USER
```
**Esperado:** no imprime nada. **Cierre sesión y vuelva a entrar** (o reinicie).
**Verificación:**
```bash
groups
```
**Criterio:** en la lista aparece `dialout`.
**Mostrar a Claude:** si no aparece después de reiniciar.

### 0.8 Identificar los puertos de la placa
Conecte **solo** el conector marcado **"UART"** de la DevKitC-1 y ejecute:
```bash
ls -l /dev/ttyUSB* /dev/ttyACM*
```
Luego conecte también el conector **"USB"** y repita el comando.
**Esperado:** con el conector UART aparece un puerto nuevo (normalmente `/dev/ttyUSB0`); con el conector USB aparece otro (normalmente `/dev/ttyACM0`).
**Criterio:** sabe qué puerto corresponde a cada conector. En el resto de la guía, `PUERTO_UART` es el del conector UART y `PUERTO_USB` el del conector USB.
**Mostrar a Claude:** siempre.

### 0.9 Hello world (comprobar placa y herramientas)
```bash
cp -r ~/esp/esp-idf/examples/get-started/hello_world ~/hello_world && cd ~/hello_world && idf.py set-target esp32c6 && idf.py build
```
**Esperado:** al final, `Project build complete. To flash, run: ...`.
```bash
idf.py -p /dev/ttyUSB0 flash monitor
```
**Esperado:** `Hello world!`, una línea `This is esp32c6 chip with 1 CPU core(s), WiFi/BLE, 802.15.4 (Zigbee/Thread), silicon revision v0.x, 8MB external flash` (la revisión puede variar) y una cuenta regresiva hasta reiniciar.
**Criterio:** ve `Hello world!` y el chip reportado es esp32c6. Salga con `Ctrl+]`.
**Mostrar a Claude:** la línea del chip y la de la flash (evidencia).

---

## Parte 1: Firmware y LEDs (modo de prueba por USB)

Conecte solo los 3 LEDs con sus resistencias (README, sección 2.1). Todavía no conecte relé, buzzer, PIR ni botón.

### 1.1 Copiar el código al repositorio
Descomprima el .zip entregado en la raíz del repositorio, de modo que queden `firmware/esp32c6/`, `common/protocolo/` y `docs/`.
```bash
ls firmware/esp32c6 common/protocolo
```
**Esperado:** `CMakeLists.txt  main  README.md  sdkconfig.defaults  sdkconfig.prueba_usb  tools` y `CMakeLists.txt  include  src  test`.
**Criterio:** están ambas carpetas.

### 1.2 Compilar la variante de prueba por USB
```bash
cd firmware/esp32c6 && idf.py -B build_usb -D SDKCONFIG=sdkconfig_usb -D SDKCONFIG_DEFAULTS="sdkconfig.defaults;sdkconfig.prueba_usb" build
```
**Esperado:** termina con `Project build complete.` y una línea `epp_esp32c6.bin binary size 0x2a... bytes ... (83%) free`.
**Criterio:** compila **sin warnings** (el código usa `-Werror`, así que un warning detendría la compilación).
**Mostrar a Claude:** la línea del tamaño del binario o, si falla, el error completo.

### 1.3 Flashear y abrir la consola
```bash
idf.py -B build_usb -D SDKCONFIG=sdkconfig_usb -p /dev/ttyUSB0 flash monitor
```
**Esperado (consola):**
```
I (...) salidas: Salidas en estado seguro: rele OFF (nivel 1), LED rojo ON
I (...) main: EPP firmware v1.0, protocolo v1. Motivo del reinicio: ...
I (...) main: Pines: R=21 A=22 V=23 BZ=20 RELE=19 PIR=18 BOTON=3 TX=10 RX=11
I (...) main: Fail-safe: 800 ms sin latido (revision cada 50 ms). Verde max: 8000 ms
W (...) enlace: MODO PRUEBA: protocolo por el USB nativo (USB Serial/JTAG)
I (...) entradas: PIR en calentamiento 60 s
I (...) main: Listo. Esperando el primer latido de la Jetson (LED rojo fijo).
```
**Esperado (LEDs):** rojo → amarillo 200 ms → verde 200 ms → **rojo fijo**.
**Criterio:** los tres LEDs encienden en la prueba de lámparas y al final queda el rojo fijo.
**Mostrar a Claude:** las primeras ~15 líneas de la consola.

Deje esta terminal abierta (la consola) y abra **otra terminal** para el simulador.

### 1.4 Instalar pyserial
```bash
python3 -m pip install --user pyserial
```
(En Fedora: `sudo dnf install python3-pyserial`. Si pip dice `externally-managed-environment`, use `sudo apt install python3-serial`.)
**Criterio:** `python3 -c "import serial; print(serial.VERSION)"` imprime una versión.

### 1.5 Simulador: enlace
```bash
python3 tools/sim_jetson.py /dev/ttyACM0
```
**Esperado (simulador):** `Conectado a /dev/ttyACM0 ... Latido ACTIVO.` y luego `<- EVENTO_ENLACE: enlace restablecido`.
**Esperado (consola):** `I (...) enlace: ENLACE OK (latido recibido)`.
**Criterio:** aparecen ambas líneas y el LED sigue en rojo fijo (regla: rojo hasta que llegue una orden).
**Mostrar a Claude:** si no llega el EVENTO_ENLACE.

### 1.6 Recorrer los estados
En el simulador escriba, uno por uno: `e 0`, `e 1`, `e 2`, `e 3`, `e 4`.
**Esperado:** después de cada uno, `<- ACK de ESTADO`. LEDs: apagados → amarillo parpadeando (0,5 s) → amarillo fijo → verde → rojo. Con `e 3` la consola muestra `rele -> ON`, y con `e 4`, `rele -> OFF`.
**Criterio:** cada LED corresponde a su estado. **Grabe un video corto con el celular**: sirve de evidencia para la matriz (HW-02).

---

## Parte 2: Buzzer, relé, PIR y botón

Desconecte el USB antes de cablear cada módulo.

### 2.1 Buzzer: identificar el tipo
Sin el ESP32: conecte el buzzer directo a 5 V y GND durante 1 s.
**Criterio:** tono continuo = **activo**; solo un clic = **pasivo**. Si es pasivo:
```bash
idf.py -B build_usb -D SDKCONFIG=sdkconfig_usb menuconfig
```
Vaya a *EPP: firmware del semáforo* → *Actuadores* → *Tipo de buzzer* → *Pasivo*, guarde con `S` y salga con `Q`.

### 2.2 Buzzer: probar
Cablee el buzzer según el README, sección 2.3, recompile y flashee (comando 1.3). En el simulador: `b 1`, `b 2`, `b 3`, `b 4`.
**Criterio:** se oye bip corto, bip doble, tono largo y tres tonos. Además, al arrancar suena un bip en la prueba de lámparas.
**Mostrar a Claude:** si no suena o suena al revés (continuo cuando debería callar).

### 2.3 Relé: medición de seguridad ANTES de conectarlo al ESP32
Alimente el módulo de relé (5 V y GND) **sin conectar IN al ESP32** y mida con el multímetro el voltaje entre IN y GND.
**Criterio:**
- ≈ 0 V o ≈ 3,3 V → seguro. Siga con el caso A del README si el módulo tiene JD-VCC.
- **≈ 5 V → caso B: NO lo conecte directo.** Use un transistor (README, sección 2.4).

**Mostrar a Claude:** **siempre**, con una foto del módulo (ambas caras). Así confirmamos el caso antes de conectar.

### 2.4 Relé: polaridad y estado al encender
Con el relé cableado según su caso, sin carga en los contactos:
1. Desconecte y vuelva a conectar el USB **10 veces**, mirando el LED del módulo y escuchando el relé.
   **Criterio (HW-01):** en ningún encendido el relé hace clic ni se enciende su LED.
2. En el simulador: `e 3`, luego `e 4`.
   **Criterio:** con `e 3` el relé hace clic y se enciende su LED; con `e 4` se apaga. Si ocurre al revés, la polaridad está invertida: en menuconfig cambie *El módulo de relé se activa con nivel BAJO* y repita desde el paso 1.

**Mostrar a Claude:** si en algún encendido el relé se activa.

### 2.5 PIR: medir la salida
Alimente el PIR con 5 V y GND, **sin** conectar OUT al ESP32. Mueva la mano frente al sensor y mida el voltaje de OUT.
**Criterio:** en alto mide ≤ 3,6 V (se espera ~3,3 V). Si mide más, ponga un divisor de 10 kΩ / 20 kΩ.
**Mostrar a Claude:** el valor medido.

### 2.6 PIR: eventos
Conecte OUT a GPIO18, flashee y espere 60 s.
**Esperado (simulador):** a los ~60 s, `<- EVENTO_PIR presencia=0 listo=1`. Al mover la mano: `presencia=1`; al quedarse quieto, después del tiempo del potenciómetro: `presencia=0`.
**Criterio:** cada cambio de presencia genera **un solo** evento (sin ráfagas).

### 2.7 Botón de factory reset
Conecte el pulsador entre GPIO3 y GND.
1. Presiónelo menos de 1 s.
   **Criterio:** no pasa nada.
2. Manténgalo 3 s y suéltelo.
   **Criterio:** a partir de 1 s, rojo y amarillo alternan; al soltar, vuelve el estado anterior; la consola dice `cancelado`.
3. Manténgalo 6 s.
   **Criterio:** a los 5 s los tres LEDs parpadean, suena un tono largo y el simulador muestra `<- EVENTO_BOTON: FACTORY RESET solicitado -> respondo ACK`.

---

## Parte 3: Pruebas unitarias del protocolo (en el PC)

Desde la **raíz del repositorio**:

### 3.1 Compilar
```bash
cmake -S common/protocolo -B build-proto && cmake --build build-proto
```
**Esperado:** `[100%] Built target test_protocolo`.

### 3.2 Ejecutar
```bash
ctest --test-dir build-proto --output-on-failure
```
**Esperado:** `100% tests passed, 0 tests failed out of 1`.

### 3.3 Ver el detalle (18 pruebas)
```bash
./build-proto/test_protocolo
```
**Esperado:** 18 líneas `...:PASS` y al final `18 Tests 0 Failures 0 Ignored` y `OK`.
**Mostrar a Claude:** las últimas 3 líneas (evidencia).

### 3.4 Valgrind
```bash
valgrind --leak-check=full --error-exitcode=1 ./build-proto/test_protocolo
```
**Esperado:** `All heap blocks were freed -- no leaks are possible` y `ERROR SUMMARY: 0 errors from 0 contexts`.
**Criterio:** 0 errores (PU-19 de la matriz).

### 3.5 Sanitizers (opcional)
```bash
cmake -S common/protocolo -B build-asan -DPROTO_SANITIZERS=ON && cmake --build build-asan && ./build-asan/test_protocolo
```
**Esperado:** `18 Tests 0 Failures 0 Ignored`, sin reportes de `AddressSanitizer` ni `runtime error`.

---

## Parte 4: Fail-safe y pruebas automáticas

### 4.1 Fail-safe manual
En el simulador: `e 3` (verde, relé ON) y después `c` (cortar el latido).
**Esperado (simulador):** `<- EVENTO_ENLACE: FAIL-SAFE (8xx ms desde el último latido enviado)`.
**Esperado (placa):** el relé se apaga y queda rojo **con un apagón corto cada segundo**.
**Criterio:** ≤ 1000 ms. **Evidencia física (HW-07):** grabe en cámara lenta el LED verde y el relé mientras corta el latido.
Después escriba `c` otra vez.
**Criterio:** `enlace restablecido` y el LED en **rojo** (no vuelve a verde solo).

### 4.2 Pruebas de integración automáticas
Cierre el simulador interactivo (`q`) y ejecute:
```bash
python3 tools/sim_jetson.py /dev/ttyACM0 --auto
```
**Esperado:** unas 19 líneas `[PASA ] PI-..` y al final `19/19 pruebas pasan. Resultados guardados en resultados_integracion_AAAAMMDD_HHMMSS.md`. Dura ~45 s. Mientras corre, observe los LEDs y el buzzer.
**Criterio:** todas pasan.
**Mostrar a Claude:** el archivo `resultados_integracion_*.md` completo. Ese archivo va a la matriz.

---

## Parte 5: Telemetría

### 5.1 Ver la telemetría
En el simulador interactivo, escriba `t`.
**Esperado:** un bloque `--- TELEMETRIA ---` con estado, CPU (pocos %), heap libre (cientos de kB), errores y el stack libre de las 5 tareas.
**Criterio:** heap mínimo > 100 000 B y **ninguna tarea con stack libre < 512 B**.
**Mostrar a Claude:** el bloque completo, **después de las pruebas 4.2** (para que el high-water mark refleje uso real). Con esos números ajustamos los stacks.

### 5.2 Telemetría automática
Escriba `v` y espere 5 s.
**Criterio:** aparece una línea `TEL ...` por segundo y `up=` aumenta de 1 en 1.

---

## Parte 6: Conexión real con la Jetson

### 6.1 Compilar la variante normal (UART1)
```bash
idf.py build && idf.py -p /dev/ttyUSB0 flash monitor
```
**Esperado:** igual que en 1.3, pero **sin** la línea `MODO PRUEBA` y con `I (...) enlace: UART1 115200 8N1, TX=GPIO10 RX=GPIO11`.

### 6.2 En la Jetson (lo hace su compañero, comando por comando)
```bash
sudo systemctl stop nvgetty
```
```bash
sudo systemctl disable nvgetty
```
```bash
ls -l /dev/ttyTHS1
```
**Esperado:** `crw-rw---- 1 root dialout ... /dev/ttyTHS1`.
**Criterio:** el dispositivo existe. *Que corresponda a los pines 8 y 10 en JetPack 4.6.1 lo indica NVIDIA en sus foros y en JetsonHacks, pero no lo verifiqué en su Jetson.*

### 6.3 Cableado
Con **todo apagado**: GPIO10 → pin 10, GPIO11 ← pin 8, GND ↔ pin 6 (README, sección 2.6). Revise dos veces la numeración del conector J41: el pin 1 está marcado en la placa.

### 6.4 Primera prueba: el simulador corriendo en la Jetson
Copie `tools/sim_jetson.py` a la Jetson y, si hace falta, instale pyserial con `sudo apt install python3-serial`. Luego:
```bash
python3 sim_jetson.py /dev/ttyTHS1 --auto
```
**Criterio:** las mismas 19/19 de la Parte 4, ahora por el UART real. Esto valida el cableado y el puerto **antes** de integrar el daemon.
**Mostrar a Claude:** el archivo de resultados.

### 6.5 Integración con el daemon
El daemon C++ debe usar `common/protocolo` (docs/protocolo_uart.md, sección 8). Lo mínimo: un hilo que envíe el LATIDO cada 200 ms, enviar ESTADO según su máquina de estados y responder ACK al EVENTO_BOTON.

---

## Registro de verificación (para la declaración de uso de IA)

Copie esta tabla a su informe y llénela con lo que observó.

| # | Afirmación (generada con IA) | Cómo se verificó | Resultado | Fecha / quién |
|---|---|---|---|---|
| 1 | El entorno es ESP-IDF v5.5.6 | `idf.py --version` (0.6) | | |
| 2 | El firmware compila sin warnings | `idf.py build` (1.2, 6.1) | | |
| 3 | Al arrancar: relé OFF y rojo fijo | Observación, 10 encendidos (1.3, 2.4) | | |
| 4 | Cada estado enciende el LED correcto | Simulador + video (1.6) | | |
| 5 | El relé no se activa al encender | 10 encendidos (2.4) | | |
| 6 | La salida del PIR es ≤ 3,6 V | Multímetro (2.5) | | |
| 7 | IN del relé no tiene 5 V | Multímetro (2.3) | | |
| 8 | Botón de 5 s → EVENTO_BOTON | Simulador (2.7) | | |
| 9 | CRC("123456789") = 0x29B1 y 18 pruebas pasan | ctest (3.2, 3.3) | | |
| 10 | Sin errores de memoria | valgrind (3.4) | | |
| 11 | Fail-safe en ≤ 1 s | Simulador + video en cámara lenta (4.1) | | |
| 12 | 19/19 pruebas de integración | `--auto` (4.2, 6.4) | | |
| 13 | Ninguna tarea con stack libre < 512 B | Telemetría (5.1) | | |
