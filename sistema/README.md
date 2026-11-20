# Herramientas de sistema

- `scripts/grabar_verificar_sd.sh`: **BORRA EL DISCO SELECCIONADO**. Requiere imagen `.img` ya descomprimida; comprueba tamaño, montaje, tipo de dispositivo, pide confirmar escribiendo el nombre completo, graba con `dd` y compara con `cmp`. No se ha ejecutado sobre una microSD real en este entorno. La comparación se limita al tamaño de la imagen y **no** verifica el arranque ni la integridad previa del ZIP.
- Si se usa imagen ZIP, descomprimir o usar la comprobación original `unzip -p imagen.zip | sudo cmp -n ... - /dev/sdX` sobre la imagen adecuada. **Verificar** cuál imagen corresponde a la placa A02/B01 antes de grabar.
- `systemd/epp-daemon.service.example`: solo ejemplo; no activarlo todavía.
- `scripts/instalar_jetson.sh`: marcador explícito, aún **no instala** software ni modifica el equipo.
