# Herramientas de sistema

- `epp.conf.example`: configuración del daemon (umbrales, tiempos, dispositivos). Se copia a `/etc/epp/epp.conf`.
- `uvcvideo-ngs.conf`: quirk del driver de la webcam (sin él, la cámara falla al abrirse con `Input/output error`). Se copia a `/etc/modprobe.d/`.
- `systemd/epp-daemon.service`: servicio que arranca el daemon al encender la Jetson y lo relanza si se cae.
- `scripts/grabar_verificar_sd.sh`: **BORRA EL DISCO SELECCIONADO**. Graba una imagen `.img` con `dd` y la compara con `cmp`.
- `scripts/instalar_jetson.sh`: marcador; la instalación se hace a mano con los pasos de abajo.

## Preparar la Jetson (una sola vez)

```bash
sudo systemctl disable --now nvgetty            # libera /dev/ttyTHS1 (era una consola serie)
sudo usermod -aG dialout epp                    # permiso para el UART al correr el daemon a mano
sudo install -m 644 sistema/uvcvideo-ngs.conf /etc/modprobe.d/uvcvideo-ngs.conf
sudo reboot
```

Comprobar tras reiniciar: `cat /sys/module/uvcvideo/parameters/quirks` debe dar `4` y `dmesg | grep -i "probe control"` no debe mostrar nada.

## Dashboard con nginx (en la Jetson)

```bash
sudo apt install nginx
sudo install -d /opt/epp/dashboard
sudo install -m 644 dashboard/index.html dashboard/app.js dashboard/styles.css /opt/epp/dashboard/
sudo install -m 644 dashboard/nginx-epp.conf /etc/nginx/sites-available/epp
sudo ln -sf /etc/nginx/sites-available/epp /etc/nginx/sites-enabled/epp
sudo rm -f /etc/nginx/sites-enabled/default
sudo nginx -t && sudo systemctl restart nginx
```

Luego abrir `http://epp-jetson.local/` desde un equipo de la misma red.

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

Para correrlo a mano (pruebas), primero `sudo systemctl stop epp-daemon`: la cámara solo la puede usar un programa a la vez. Tras cambiar `/etc/epp/epp.conf`: `sudo systemctl restart epp-daemon`.

Verificado el 24-nov: el servicio arranca solo tras `sudo reboot`, abre la cámara sin reconectarla y queda en `active (running)`.
