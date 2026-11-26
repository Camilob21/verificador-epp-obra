# Manual de usuario — Verificador de EPP

Este equipo revisa, con una cámara, que la persona que va a entrar a la obra lleve **casco** y
**chaleco**. Si cumple, prende la luz verde y habilita el paso; si no, prende la roja.

> Es un prototipo académico de apoyo: **no** reemplaza la revisión de un supervisor.
> Ante cualquier duda o falla, el equipo da rojo.

## 1. Partes del equipo

| Parte | Para qué sirve |
|---|---|
| Computador (Jetson Nano) con cámara | Mira a la persona y decide |
| Caja con luces (rojo, amarillo, verde) | Le dice a la persona qué hacer |
| Sensor de distancia (dos "ojos" redondos) | Se acerca la mano para empezar |
| Botón de restauración | Vuelve el equipo a valores de fábrica (sección 6) |
| Cable de red (Ethernet) | Para ver el panel web desde un computador o celular |

## 2. Instalar y encender

1. Ubique el equipo a la entrada, con la cámara mirando hacia el sitio donde se parará la
   persona (unos 2 m) y **sin objetos a menos de 40 cm del sensor** (los confundiría con una mano).
2. Conecte el cable de red a un router con internet (sirve un celular compartiendo datos con un
   adaptador).
3. Conecte la alimentación. Al encender, las luces hacen una prueba: **rojo, amarillo, verde**,
   con un pitido corto, y quedan en rojo. Si alguna luz no prende en la prueba, está dañada o
   desconectada.
4. Espere alrededor de **1 minuto** mientras el equipo arranca.

## 3. Ver el panel web

Desde un computador o celular conectado a la **misma red**, abra el navegador en:

- `http://epp-jetson.local/` , o
- `http://<dirección IP del equipo>/` (la IP la da el router; también aparece al final de la instalación).

El panel muestra el estado, la cámara, los últimos eventos y el estado del equipo (memoria,
procesador, red). Es **solo de consulta**: desde el panel no se puede abrir la puerta ni borrar nada.

## 4. Puesta en marcha (primera vez y después de restaurar)

Un equipo nuevo o restaurado **no controla el acceso** hasta que alguien lo revise y lo confirme.
Mientras tanto la luz amarilla queda **fija** y el panel muestra el recuadro **"Puesta en marcha"**:

| Punto | Qué revisar si sale ✘ |
|---|---|
| Cámara: llegan imágenes | Cable USB de la cámara. Con ✔, acomode la cámara mirando la imagen del panel: debe verse a una persona **de cuerpo entero** en la marca del piso |
| Microcontrolador enlazado | Cable entre la Jetson y la caja de luces |
| Sensor: detectó la mano | Acerque la mano al sensor a menos de 40 cm |
| Hora sincronizada (opcional) | Internet en el cable de red. Sin internet el equipo funciona, pero las fechas de los eventos pueden estar mal |

**Para confirmar:** cuando los tres primeros puntos estén en ✔, **mantenga la mano frente al
sensor 3 segundos** (la barra del panel se llena). Suena un tono, las luces se apagan y el equipo
queda funcionando. Retire la mano.

## 5. Uso diario

1. La persona se acerca y **pone la mano frente al sensor** (a menos de 40 cm).
2. La luz amarilla **parpadea** durante **10 segundos**: en ese tiempo la persona se ubica frente
   a la cámara, de forma que se le vean **la cabeza y el torso**, con los brazos abajo.
3. La luz amarilla queda **fija**: el equipo está verificando. La persona debe quedarse quieta.
4. Resultado (unos 5 s):
   - **Verde**: cumple, puede pasar.
   - **Rojo**: falta el casco o el chaleco. Póngaselo y repita desde el paso 1.
5. Las luces se apagan y el equipo queda listo para la siguiente persona.

### Qué significa cada luz

| Luces | Significado |
|---|---|
| Todas apagadas | Listo, esperando la mano en el sensor |
| Amarillo parpadeando | Ubíquese frente a la cámara (cuenta de 10 s) |
| Amarillo fijo | Verificando, quédese quieto. En la puesta en marcha: falta confirmar (sección 4) |
| Verde | Cumple: puede pasar |
| Rojo | No cumple |
| Rojo fijo al encender | El equipo está arrancando (espere 1 minuto) |
| Rojo con un apagón corto cada segundo | **Falla de comunicación** entre el computador y la caja de luces. El paso queda bloqueado. Revise el cable; si no se arregla en 1 minuto, apague y encienda |
| Rojo y amarillo alternando rápido | Alguien está sosteniendo el botón de restauración (soltarlo antes de 5 s cancela) |
| Las tres luces parpadeando | Restauración de fábrica confirmada |

## 6. Restaurar valores de fábrica (factory reset)

Sirve cuando el equipo se va a instalar en otro sitio o quedó mal configurado.
**Borra** los eventos registrados y las fotos y **vuelve a la puesta en marcha** (sección 4).
No borra el programa ni el modelo.

1. **Sostenga el botón de restauración 5 segundos.** Al primer segundo, el rojo y el amarillo
   empiezan a alternar; si suelta antes de los 5 s no pasa nada.
2. A los 5 s las tres luces parpadean: suéltelo. El panel muestra "Restaurando valores de fábrica".
3. En unos 20 segundos el equipo reinicia el programa y queda en **puesta en marcha** (amarillo
   fijo). Siga la sección 4.

Sin botón (personal técnico, por SSH): `sudo epp-fabrica` y escribir `SI`.

## 7. Problemas comunes

| Problema | Qué hacer |
|---|---|
| El panel dice "sin conexión con el daemon" | El programa se está reiniciando; espere 1 minuto. Si sigue, apague y encienda el equipo |
| El panel no abre | Verifique que el computador esté en la misma red; pruebe con la IP en vez de `epp-jetson.local` |
| Da rojo llevando casco y chaleco | Párese de frente, a unos 2 m, con brazos abajo, sin contraluz ni luz directa muy fuerte |
| Empieza solo, sin poner la mano | Hay un objeto a menos de 40 cm del sensor: retírelo |
| Las fechas de los eventos dicen "hora sin confirmar" | El equipo no tuvo internet al arrancar; conéctelo a una red con internet |
