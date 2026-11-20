# Declaración del uso de IA — plantilla

En la organización inicial del repositorio se utilizó asistencia de IA para proponer la estructura, borradores de CMake/ESP-IDF, fuentes de ejemplo, documentos, flujos GitHub y criterios de pruebas. **La IA no ejecutó pruebas sobre la Jetson ni el ESP32, no midió la cámara ni entrenó modelos**. No se debe declarar como verificado un paso sólo porque haya sido sugerido.

| Recurso generado con IA | Objetivo | Comprobación humana obligatoria | Evidencia |
|---|---|---|---|
| CMake C++17 + Google Test | Validar lógica sin GPU | Compilar en Fedora/Jetson; ejecutar CTest | Logs, versiones, revisor |
| Componente CRC compartido | Evitar implementación divergente | Comparar CRC conocido en PC y ESP, probar tramas corruptas | Log Unity/CTest |
| Firmware hello world | Base ESP32-C6 | `idf.py build`, flash, monitor | Log y board real |
| CI GitHub | Comprobación repetible | Ver primer run verde y branch protection | URL del job |
| Ejemplo script de SD | dd + cmp | Revisar dispositivo manualmente y ejecutar sólo bajo supervisión | Foto de `lsblk`, log `cmp` |

Añadir prompt/fecha, versión de archivos y correcciones humanas antes de la entrega. No guardar secretos ni nombres de personas en evidencias públicas.
