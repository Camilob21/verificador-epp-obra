# Guía de contribución

## Ramas y PR

- `main` debe mantenerse estable; configurar protección de rama en GitHub cuando la cuenta/visibilidad lo permita.
- Una rama por funcionalidad: `feat/uart-crc`, `feat/vision-tensorrt`, `fix/camera-init`, `docs/manual`.
- Cada cambio se sube mediante **Pull Request** y su aprobación corresponde a **otra persona distinta de quien implementó**. Registrar quién implementa y quién verifica en `docs/matriz_verificacion.md`.
- Evitar mezclar cambios de firmware, daemon y documentación en un PR grande. En cambios de protocolo, actualizar ambos extremos y sus pruebas.
- No fusionar si falla el check `daemon-x86`; adjuntar evidencia y limitaciones. No considerar que CI garantiza seguridad física.

## Commits

Convención `tipo(área): descripción en imperativo`, por ejemplo `feat(daemon): agrega parser de trama`, `test(protocolo): cubre CRC de referencia`, `docs(manual): documenta reinicio`.
Tipos sugeridos: `feat`, `fix`, `test`, `docs`, `refactor`, `build`, `ci`, `chore`.

## Revisión mínima de PR

1. ¿Qué se cambió y por qué?
2. ¿Cómo se verificó? Comando, entorno, evidencia y resultado.
3. ¿Qué NO se verificó, en especial Jetson/ESP32/hardware?
4. ¿Hay riesgos de acceso no autorizado o datos personales?
5. ¿Implementador y revisor son diferentes?

No subir contraseñas, tokens, claves privadas, imágenes de personas, dataset o pesos por `git add -f`.
