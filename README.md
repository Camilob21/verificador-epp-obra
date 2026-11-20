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

## Compilar el daemon en un PC Linux (sin GPU)

Instalar: compilador C/C++17, CMake **3.10 o superior** y Google Test **del sistema** (ejemplo Debian/Ubuntu: `sudo apt install build-essential cmake libgtest-dev`). En Fedora usar los paquetes equivalentes `gcc-c++ cmake gtest-devel`.

```bash
cd daemon
mkdir -p build
cd build
cmake .. -DBUILD_TESTING=ON
cmake --build . -j2
ctest --output-on-failure
./epp-daemon
```

El ejecutable es solo **un esqueleto seguro**; no captura vídeo, no ejecuta YOLO y no autoriza accesos. Los tests prueban decisión básica y CRC, **no** la detección real. En Jetson se puede compilar igual, o sin pruebas:

```bash
cd daemon
mkdir -p build
cd build
cmake .. -DBUILD_TESTING=OFF
cmake --build . -j2
```

Este CMake **no** usa `FetchContent`, `std::filesystem` ni banderas específicas de TensorRT. Los módulos de hardware están aislados en `daemon/src/platform/` y aún no se incluyen en la compilación. No activar un modo Jetson de inferencia que todavía no existe.

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
