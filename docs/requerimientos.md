# Requerimientos preliminares

| ID | Requisito | Criterio de aceptación a definir/verificar | Estado |
|---|---|---|---|
| RF-01 | Detectar casco y chaleco | Ensayo con personas y EPP real, cuadro temporal y umbrales medidos | Pendiente |
| RF-02 | Negar ante ausencia de EPP o tiempo agotado | Nunca dar verde en escenarios adversos | Pendiente |
| RF-03 | Intercambio UART Jetson–ESP32 con CRC | Pruebas de integridad, corrupción y reconexión | Pendiente |
| RF-04 | Falla de latido implica rojo | Desconectar Jetson y medir el tiempo hasta el rojo de enlace perdido | Pendiente |
| RF-05 | Bring-up y factory reset | Arranque guiado y retorno desde estado configurado | Pendiente |
| RF-06 | Administrador web + interfaz física | Pruebas de autenticación, estado y trazabilidad | Pendiente |
| RF-07 | Instrumentación software | CPU, memoria, periféricos, tráfico y puertos visibles | Pendiente |
| RF-08 | Carcasa prototipo | Inspección física y seguridad eléctrica básica | Pendiente |
| RF-09 | Pruebas unitarias e integración | Evidencias con responsable diferente al implementador | En curso |
| RNF-01 | Compatibilidad JetPack 4.6.1 | Compilación real en CMake/GCC de la Jetson | Pendiente |
| RNF-02 | No subir material personal o secretos | Revisión de `git status`, PR y contenidos | En curso |

Revisar/ajustar valores cuantitativos con el equipo antes de declarar cumplimiento.
