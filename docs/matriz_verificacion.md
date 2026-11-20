# Matriz de verificación (inicial)

| ID | Prueba | Nivel | Implementa | Verifica (otra persona) | Herramienta / evidencia | Estado |
|---|---|---|---|---|---|---|
| RF-09 | Regla básica de denegación Fase 1 | Unitaria PC | Por asignar | Por asignar | CTest + Google Test | Preparada, ejecución local pendiente de documentar |
| RF-03 | CRC16 123456789 = 0x29B1 | Unitaria PC/ESP | Por asignar | Por asignar | Google Test / Unity | Preparada, ESP sin probar |
| RF-03 | Trama truncada/corrupta | Integración UART | Por asignar | Por asignar | Analizador lógico, UART logs | Pendiente |
| RF-04 | Sin latido = rojo/relé no energizado | Integración | Por asignar | Por asignar | Osciloscopio / multímetro | Pendiente |
| RF-01 | Detección real con casco+chaleco | Sistema | Por asignar | Por asignar | Vídeo y CSV anonimizados | Pendiente |
| RF-05 | Factory reset retorna al bring-up | Sistema | Por asignar | Por asignar | Protocolo manual | Pendiente |
| RF-07 | Instrumentación completa | Sistema | Por asignar | Por asignar | Capturas sin datos privados | Pendiente |

La CI `daemon-x86` **no prueba** cámara, modelo, GPIO, UART real, relé, temporización ni integridad del sistema completo.
