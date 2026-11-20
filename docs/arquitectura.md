# Arquitectura acordada

`Jetson Nano → daemon C++17 → captura V4L2 → YUYV/RGB → letterbox → TensorRT FP16 → decode (1,13,8400) → NMS → decisión por prenda / cuadros → estados → SQLite + HTTP`

`Jetson ↔ ESP32-C6`: UART 3,3 V con CRC y latido (borrador en `common/protocolo/`). ESP: PIR, LEDs, buzzer, relé, reset ~5 s. nginx muestra dashboard HTML/CSS/JS y proxifica la API local.

Estados previstos: Reposo → espera PIR/persona → fase 1 casco+chaleco → fase 2 opcional → resultado → reposo. Fallo/timeout: rojo. **Ningún estado completo está implementado aún**.

**Separación tecnológica:** `daemon/src/core` solo lógica compilable en x86; `daemon/src/platform` sólo integración hardware. `common/protocolo` es C puro compartido. El CMake del daemon 3.10 **no** enlaza aún CUDA/TensorRT, ni intenta usarlos en x86. El CMake ESP-IDF 3.16+ es independiente.
