# Verificador de EPP para control de acceso a obra

**Proyecto académico** de Ingeniería Electrónica, Universidad Nacional de Colombia, sede Manizales. **Entrega prevista: 1 de diciembre de 2026.**

Prototipo que comprueba, mediante cámara y un detector YOLO, la presencia de **casco y chaleco reflectivo** en una persona situada ante una marca de inspección. Gafas y guantes: **fase opcional no validada**. El prototipo **no es un sistema certificado de seguridad** ni sustituye una inspección humana.

## Arquitectura prevista (no implica funcionalidad integrada)

```text
Webcam USB (YUYV) ──USB──> Jetson Nano 4 GB
                              │ daemon C++17
                              │ V4L2 → RGB/letterbox → TensorRT YOLO → NMS
                              │ decisión multicuadro → estados → SQLite/API
                              ├── HTTP local ──> nginx ──> Dashboard JS/HTML/CSS
                              └── UART 3,3 V + CRC/latido ──> ESP32-C6
                                                            │ LEDs, buzzer, relé
Sensor PIR ──────────────────────────────────────────────────>│
Botón factory reset ─────────────────────────────────────────>│
```

**Política:** ninguna salida autoriza el acceso por defecto; pérdida de comunicación o incertidumbre deben producir rojo. La implementación y verificación integral de este comportamiento están pendientes.

## Hardware identificado

- NVIDIA Jetson Nano Developer Kit **4 GB A02**, JetPack 4.6.1, L4T 32.7.1, Ubuntu 18.04, TensorRT 8.2.
- Webcam USB NGS XPRESSCAM300 UVC (1903:8b28), YUYV 640 × 480, enfoque manual.
- ESP32-C6, semáforo LED, buzzer, relé, PIR y pulsador de reinicio (cableado pendiente de registrar).
- MicroSD 32 GB; fuente Jetson 5 V / 3 A por jack con jumper J48 (según pruebas reportadas por el equipo).

## Compilar el daemon

El daemon tiene tres partes (ver `daemon/CMakeLists.txt`):

- **`epp_core`** (`daemon/src/core`, C++17 puro): preprocesamiento YUYV→RGB→tensor, decodificación YOLOv8 + NMS, decisión con varios cuadros, máquina de estados e imágenes PPM de evidencia. Compila y se prueba en cualquier PC.
- **`epp_v4l2`** (`daemon/src/platform/captura_v4l2.cpp`): captura de la webcam; solo necesita las cabeceras del kernel, así que también se compila en la CI.
- **`epp-daemon`** (`main.cpp` + `inferencia_tensorrt.cpp`): **solo se compila si CMake encuentra CUDA y TensorRT**, es decir, en la Jetson.

### En un PC Linux (sin GPU): núcleo + pruebas

Instalar compilador C/C++17, CMake **3.10 o superior** y Google Test del sistema (Debian/Ubuntu: `sudo apt install build-essential cmake libgtest-dev`; Fedora: `sudo dnf install gcc-c++ cmake gtest-devel`).

```bash
cmake -S daemon -B daemon/build -DBUILD_TESTING=ON
cmake --build daemon/build -j2
ctest --test-dir daemon/build --output-on-failure
```

En un PC es normal el mensaje `TensorRT/CUDA no encontrados: epp-daemon se omite`.

### En la Jetson Nano (JetPack 4.6.1)

```bash
cmake -S daemon -B daemon/build -DBUILD_TESTING=OFF
cmake --build daemon/build -j2
```

Debe aparecer `TensorRT y CUDA encontrados: se compila epp-daemon`. El motor se genera **en la Jetson** a partir del ONNX (entrada 480×640) con `trtexec --onnx=best.onnx --saveEngine=best_480x640_fp16.engine --fp16`; no es portable entre versiones de TensorRT y no se sube al repositorio.

```bash
./daemon/build/epp-daemon best_480x640_fp16.engine /dev/video0        # cámara, Ctrl+C para terminar
./daemon/build/epp-daemon best_480x640_fp16.engine imagen.ppm         # una imagen PPM 640x480
```

**Estado:** el daemon captura, infiere y decide, pero **aún no tiene enlace UART con el ESP32**: no enciende LEDs ni relé (el relé solo aparece como texto). Cada decisión guarda una imagen `resultado_NN_<verde|rojo>.ppm`, que puede mostrar personas y por eso está excluida en `.gitignore`.

## ESP32-C6: compilar y flashear (ESP-IDF)

Requiere **ESP-IDF 5.x compatible con ESP32-C6**, instalado y exportado en el terminal (`idf.py --version`), hardware conectado y puerto serie conocido. Desde `firmware/esp32c6/`:

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p /dev/ttyACM0 flash monitor
```

**Cambiar** `/dev/ttyACM0` por el puerto real; en Windows usar `COMx`. `app_main` solo escribe un mensaje y calcula un CRC: **no activa LEDs, relé ni buzzer**. El proyecto independiente de pruebas Unity está en `firmware/esp32c6/tests/unity/`: construirlo y flashearlo por separado; consultar su README.

## Datos y modelos

Dataset: **[Construction-PPE de Ultralytics](https://docs.ultralytics.com/datasets/detect/construction-ppe)**, creador **Ultralytics**, licencia de imágenes/anotaciones **[CC BY 4.0](https://creativecommons.org/licenses/by/4.0/)**. Adaptación académica de las etiquetas de 12 a 9 clases por nombre; revisar versión exacta de los archivos utilizados en Colab. Véase `modelo/README.md`. Reetiquetado a nueve clases por **nombre**, no por ID. El dataset y las imágenes de personas **no** se suben. Los pesos `.pt`, `.onnx` y los motores TensorRT **no** se suben como Git ordinario.

Entrenamiento reportado (no recalculado aquí): YOLO26n mAP50 0,767 y YOLOv8n mAP50 0,782. **No está confirmada** la conversión ni inferencia sobre Jetson. El notebook, scripts originales y CSV completos se deben incorporar desde los archivos reales del equipo; las plantillas indican explícitamente lo que falta.

## Estado al 20-11-2026

- Arquitectura, caracterización de la cámara y entrenamiento: reportados como hechos.
- MicroSD regrabándose; TensorRT y cámara en Jetson: aún por verificar.
- Daemon: arranque + lógica mínima testeable, sin integración real.
- Firmware: hello world + componente CRC, sin actuadores.
- Dashboard: página estática de muestra; API/no nginx en producción.
- UART, estados completos, alarma, autorización, reset, instrumentación y carcasa: **pendientes**.

## Organización y contribución

Ver `CONTRIBUTING.md`, `docs/roles.md`, `docs/matriz_verificacion.md` y `docs/declaracion_ia.md`. **Licencia del repositorio pendiente de acuerdo del equipo**: consultar `docs/licencias.md` antes de elegirla o hacer público el código/modelo.
