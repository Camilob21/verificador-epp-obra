# App ESP-IDF independiente para pruebas Unity

Esta app se **flashea en lugar** del hello world, no junto con él. Desde este directorio, con ESP-IDF exportado:

```bash
idf.py set-target esp32c6
idf.py build
idf.py -p PUERTO_REAL flash monitor
```

Salida prevista: `2 Tests 0 Failures` o una variante equivalente según versión. **No comprobado aún en un ESP32-C6 real.** El proyecto usa el mismo componente C `epp_protocolo` del firmware principal. Registrar el puerto y la versión exacta de ESP-IDF en `docs/registro_pruebas.md`.
