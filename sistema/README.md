# Herramientas de sistema

- `epp.conf.example`: configuración del daemon (umbrales, tiempos, dispositivos). Se copia a `/etc/epp/epp.conf`.
- `systemd/epp-daemon.service`: servicio que arranca el daemon al encender la Jetson y lo relanza si se cae.
- `scripts/grabar_verificar_sd.sh`: **BORRA EL DISCO SELECCIONADO**. Graba una imagen `.img` con `dd` y la compara con `cmp`.
- `scripts/instalar_jetson.sh`: marcador; la instalación se hace a mano con los pasos de abajo.

## Instalar el daemon como servicio (en la Jetson)

Desde `daemon/build`, ya compilado:

```bash
sudo install -m 755 epp-daemon /usr/local/bin/epp-daemon
sudo install -d -m 755 /etc/epp
sudo install -m 644 ../../sistema/epp.conf.example /etc/epp/epp.conf
sudo install -d -o epp -g epp -m 750 /var/lib/epp
sudo install -m 644 ../../sistema/systemd/epp-daemon.service /etc/systemd/system/epp-daemon.service
sudo systemctl daemon-reload
sudo systemctl enable --now epp-daemon
journalctl -u epp-daemon -f
```

Antes, `nvgetty` debe estar deshabilitado (`sudo systemctl disable --now nvgetty`): usa `/dev/ttyTHS1` como consola.
