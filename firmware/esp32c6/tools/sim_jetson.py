#!/usr/bin/env python3
"""
sim_jetson.py — Simulador de la Jetson Nano para probar el firmware del ESP32-C6
sin la Jetson. Habla el mismo protocolo que common/protocolo (docs/protocolo_uart.md).

Requisito: pip install pyserial   (en Fedora también: sudo dnf install python3-pyserial)

Uso:
  python3 sim_jetson.py /dev/ttyACM0            # modo interactivo
  python3 sim_jetson.py /dev/ttyACM0 --auto     # pruebas de integración automáticas
  python3 sim_jetson.py /dev/ttyUSB1 --sin-latido   # arrancar sin enviar latidos

Puertos en Linux:
  - Modo prueba por USB (sdkconfig.prueba_usb): conector "USB" de la DevKitC-1 -> /dev/ttyACM0
  - Adaptador USB-UART de 3,3 V en GPIO10/11:                              -> /dev/ttyUSB1 (o similar)
  El conector "UART" de la placa (/dev/ttyUSB0) es la CONSOLA de logs, no el protocolo.

Comandos interactivos (escriba y presione Enter):
  e N   ESTADO N  (0 reposo, 1 amarillo parpadeo, 2 amarillo fijo, 3 verde+relé, 4 rojo)
  b N   BUZZER N  (0 silencio, 1 bip corto, 2 bip doble, 3 ok, 4 error)
  t     pedir telemetría (y mostrarla completa)
  v     mostrar/ocultar la telemetría automática (cada ~1 s)
  c     CORTAR / reanudar el latido (prueba de fail-safe)
  x     enviar una trama con CRC corrupto
  g     enviar basura con un 0xAA falso seguido de una trama válida (resincronización)
  l     enviar una cabecera con LEN inválida (200)
  q     salir
"""
import argparse
import datetime
import queue
import struct
import sys
import threading
import time

try:
    import serial
except ImportError:
    sys.exit("Falta pyserial: pip install pyserial  (o: sudo dnf install python3-pyserial)")

# ----------------------------------------------------------------------------
# Protocolo (espejo de common/protocolo/include/protocolo.h)
# ----------------------------------------------------------------------------
SOF = 0xAA
MAX_DATOS = 48

LATIDO, ESTADO, BUZZER, PEDIR_TELEMETRIA = 0x01, 0x02, 0x03, 0x04
ACK, NACK = 0x10, 0x11
EVENTO_ARRANQUE, EVENTO_PIR, EVENTO_BOTON, TELEMETRIA, EVENTO_ENLACE = 0x80, 0x81, 0x82, 0x83, 0x84

NOMBRE_TIPO = {
    LATIDO: "LATIDO", ESTADO: "ESTADO", BUZZER: "BUZZER", PEDIR_TELEMETRIA: "PEDIR_TELEMETRIA",
    ACK: "ACK", NACK: "NACK", EVENTO_ARRANQUE: "EVENTO_ARRANQUE", EVENTO_PIR: "EVENTO_PIR",
    EVENTO_BOTON: "EVENTO_BOTON", TELEMETRIA: "TELEMETRIA", EVENTO_ENLACE: "EVENTO_ENLACE",
}
NOMBRE_ESTADO = ["REPOSO", "AMARILLO_PARPADEO", "AMARILLO_FIJO", "VERDE", "ROJO", "SIN_ENLACE"]
NOMBRE_NACK = {1: "TIPO_DESCONOCIDO", 2: "LONGITUD", 3: "VALOR", 4: "SIN_ENLACE"}
NOMBRE_RESET = {1: "encendido", 3: "esp_restart", 4: "panico", 5: "wdt_int", 6: "wdt_tareas",
                7: "otro_wdt", 9: "brownout", 2: "pin_ext", 11: "usb"}  # esp_reset_reason_t de ESP-IDF v5.5 (esp_system.h)
TAREAS = ["rx", "enlace", "salidas", "entradas", "telemetria"]


def crc16(data: bytes) -> int:
    """CRC-16/CCITT-FALSE: poly 0x1021, init 0xFFFF, sin reflexión, sin XOR final."""
    crc = 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            crc = ((crc << 1) ^ 0x1021) & 0xFFFF if crc & 0x8000 else (crc << 1) & 0xFFFF
    return crc


assert crc16(b"123456789") == 0x29B1, "CRC mal implementado"


def codificar(tipo: int, seq: int, datos: bytes = b"") -> bytes:
    cuerpo = bytes([tipo, seq & 0xFF, len(datos)]) + datos
    return bytes([SOF]) + cuerpo + struct.pack("<H", crc16(cuerpo))


def decodificar_telemetria(d: bytes) -> dict:
    (ver, est, flags, cpu, up, hl, hm, ok, ecrc, elen, etmo, euart, fallos) = struct.unpack_from(
        "<BBBBIIIIIHHHH", d, 0)
    stacks = struct.unpack_from("<5H", d, 32)
    return dict(version=ver, estado=est, flags=flags, cpu=cpu, uptime_s=up, heap_libre=hl,
                heap_min=hm, tramas_ok=ok, err_crc=ecrc, err_longitud=elen, err_timeout=etmo,
                err_uart=euart, fallos_enlace=fallos, stack_libre=dict(zip(TAREAS, stacks)))


class Parser:
    """Misma máquina de estados que protocolo.c, incluida la resincronización."""

    def __init__(self):
        self.buf = bytearray()
        self.err_crc = 0

    def feed(self, data: bytes):
        tramas = []
        pendientes = list(data)
        while pendientes:
            b = pendientes.pop(0)
            if not self.buf:
                if b == SOF:
                    self.buf.append(b)
                continue
            self.buf.append(b)
            error = False
            if len(self.buf) == 4 and self.buf[3] > MAX_DATOS:
                error = True
            elif len(self.buf) >= 4 and len(self.buf) == 4 + self.buf[3] + 2:
                n = self.buf[3]
                if crc16(bytes(self.buf[1:4 + n])) == struct.unpack_from("<H", self.buf, 4 + n)[0]:
                    tramas.append((self.buf[1], self.buf[2], bytes(self.buf[4:4 + n])))
                    self.buf.clear()
                else:
                    self.err_crc += 1
                    error = True
            if error:  # descartar solo el SOF y re-examinar lo demás
                pendientes = list(self.buf[1:]) + pendientes
                self.buf.clear()
        return tramas


# ----------------------------------------------------------------------------
# Simulador
# ----------------------------------------------------------------------------
def hora() -> str:
    return datetime.datetime.now().strftime("%H:%M:%S.%f")[:-3]


class Jetson:
    def __init__(self, puerto: str, baudios: int, latido: bool, periodo_latido: float):
        self.ser = serial.Serial(puerto, baudios, timeout=0.05)
        self.lock = threading.Lock()
        self.seq = 0
        self.latido_activo = latido
        self.periodo_latido = periodo_latido
        self.t_ultimo_latido = 0.0
        self.mostrar_tel = False
        self.eventos = queue.Queue()  # (t, tipo, seq, datos) para las pruebas automáticas
        self.ultima_tel = None
        self.vivo = True
        self.parser = Parser()

    # --- envío ---
    def enviar_crudo(self, data: bytes):
        with self.lock:
            self.ser.write(data)

    def enviar(self, tipo: int, datos: bytes = b"") -> int:
        with self.lock:
            seq = self.seq
            self.seq = (self.seq + 1) & 0xFF
            self.ser.write(codificar(tipo, seq, datos))
        return seq

    # --- hilos ---
    def hilo_latido(self):
        while self.vivo:
            if self.latido_activo:
                self.enviar(LATIDO)
                self.t_ultimo_latido = time.monotonic()
            time.sleep(self.periodo_latido)

    def hilo_rx(self):
        while self.vivo:
            try:
                data = self.ser.read(256)
            except serial.SerialException as e:
                print(f"\n[{hora()}] ERROR de puerto: {e}")
                self.vivo = False
                return
            for tipo, seq, d in self.parser.feed(data):
                self.atender(tipo, seq, d)

    def atender(self, tipo, seq, d):
        t = time.monotonic()
        self.eventos.put((t, tipo, seq, d))
        if tipo == TELEMETRIA and len(d) == 42:
            self.ultima_tel = decodificar_telemetria(d)
            if self.mostrar_tel:
                self.imprimir_tel(self.ultima_tel, corta=True)
            return
        if tipo == ACK:
            print(f"[{hora()}] <- ACK de {NOMBRE_TIPO.get(d[0], hex(d[0]))} (seq {seq})")
        elif tipo == NACK:
            print(f"[{hora()}] <- NACK de {NOMBRE_TIPO.get(d[0], hex(d[0]))} (seq {seq}): "
                  f"{NOMBRE_NACK.get(d[1], d[1])}")
        elif tipo == EVENTO_ENLACE:
            if d[0] == 0:
                dt = (t - self.t_ultimo_latido) if self.t_ultimo_latido else float("nan")
                print(f"[{hora()}] <- EVENTO_ENLACE: FAIL-SAFE ({dt*1000:.0f} ms desde el último latido enviado)")
            else:
                print(f"[{hora()}] <- EVENTO_ENLACE: enlace restablecido")
        elif tipo == EVENTO_PIR:
            print(f"[{hora()}] <- EVENTO_PIR presencia={d[0]} listo={d[1]}")
        elif tipo == EVENTO_BOTON:
            print(f"[{hora()}] <- EVENTO_BOTON: FACTORY RESET solicitado -> respondo ACK")
            with self.lock:
                self.ser.write(codificar(ACK, seq, bytes([EVENTO_BOTON])))
        elif tipo == EVENTO_ARRANQUE:
            print(f"[{hora()}] <- EVENTO_ARRANQUE: fw v{d[1]}.{d[2]}, motivo del reinicio="
                  f"{NOMBRE_RESET.get(d[0], d[0])}")
        else:
            print(f"[{hora()}] <- {NOMBRE_TIPO.get(tipo, hex(tipo))} seq={seq} datos={d.hex()}")

    @staticmethod
    def imprimir_tel(tl, corta=False):
        est = NOMBRE_ESTADO[tl["estado"]] if tl["estado"] < len(NOMBRE_ESTADO) else tl["estado"]
        f = tl["flags"]
        banderas = " ".join(n for b, n in [(1, "ENLACE"), (2, "RELE"), (4, "PIR"), (8, "PIR_LISTO"),
                                            (16, "BOTON")] if f & b) or "-"
        cpu = "n/d" if tl["cpu"] == 255 else f"{tl['cpu']}%"
        if corta:
            print(f"[{hora()}] TEL up={tl['uptime_s']}s {est} [{banderas}] cpu={cpu} "
                  f"heap={tl['heap_libre']} ok={tl['tramas_ok']} crc={tl['err_crc']}")
            return
        print(f"--- TELEMETRIA ({hora()}) ---")
        print(f"  versión protocolo : {tl['version']}")
        print(f"  estado            : {est}   banderas: {banderas}")
        print(f"  uso de CPU        : {cpu}")
        print(f"  encendido         : {tl['uptime_s']} s")
        print(f"  heap libre / mín. : {tl['heap_libre']} / {tl['heap_min']} B")
        print(f"  tramas válidas    : {tl['tramas_ok']}")
        print(f"  errores CRC / LEN / timeout / UART : {tl['err_crc']} / {tl['err_longitud']} / "
              f"{tl['err_timeout']} / {tl['err_uart']}")
        print(f"  veces en fail-safe: {tl['fallos_enlace']}")
        print(f"  stack libre (B)   : " + ", ".join(f"{k}={v}" for k, v in tl["stack_libre"].items()))

    # --- utilidades para pruebas ---
    def vaciar_eventos(self):
        while not self.eventos.empty():
            self.eventos.get_nowait()

    def esperar(self, cond, timeout):
        """Espera un evento que cumpla cond(tipo, seq, datos). Devuelve (t, tipo, seq, d) o None."""
        fin = time.monotonic() + timeout
        while time.monotonic() < fin:
            try:
                ev = self.eventos.get(timeout=max(0.0, fin - time.monotonic()))
            except queue.Empty:
                return None
            if cond(ev[1], ev[2], ev[3]):
                return ev
        return None

    def pedir_tel(self, timeout=1.5):
        self.vaciar_eventos()
        self.enviar(PEDIR_TELEMETRIA)
        ev = self.esperar(lambda t, s, d: t == TELEMETRIA, timeout)
        return decodificar_telemetria(ev[3]) if ev else None


# ----------------------------------------------------------------------------
# Pruebas automáticas de integración
# ----------------------------------------------------------------------------
def pruebas_auto(j: Jetson, verde_max_s: float, timeout_ms: int):
    res = []

    def reg(id_, req, ok, detalle):
        res.append((id_, req, "PASA" if ok else "FALLA", detalle))
        print(f"  [{'PASA ' if ok else 'FALLA'}] {id_}: {req} — {detalle}")

    def orden(tipo, valor):
        j.vaciar_eventos()
        seq = j.enviar(tipo, bytes([valor]))
        return j.esperar(lambda t, s, d: t in (ACK, NACK) and s == seq, 1.0)

    print("\n=== Pruebas automáticas de integración ===")
    print("Observe la placa: algunas pruebas piden confirmar LEDs/buzzer a ojo (anótelo).\n")

    # PI-01: sin latido no se obedece ESTADO
    j.latido_activo = False
    time.sleep(timeout_ms / 1000 + 0.5)
    ev = orden(ESTADO, 3)
    reg("PI-01", "ESTADO sin latido se rechaza (NACK SIN_ENLACE)",
        ev is not None and ev[1] == NACK and ev[3][1] == 4,
        "respuesta: " + ("ninguna" if ev is None else f"{NOMBRE_TIPO[ev[1]]} {ev[3].hex()}"))

    # PI-02: al empezar los latidos se reporta enlace
    j.vaciar_eventos()
    t0 = time.monotonic()
    j.latido_activo = True
    ev = j.esperar(lambda t, s, d: t == EVENTO_ENLACE and d[0] == 1, 2.0)
    reg("PI-02", "Con latido se establece el enlace (EVENTO_ENLACE=1)", ev is not None,
        f"{(ev[0]-t0)*1000:.0f} ms" if ev else "no llegó")

    # PI-03: cada estado del semáforo
    for e in range(5):
        ev = orden(ESTADO, e)
        ok = ev is not None and ev[1] == ACK
        tel = j.pedir_tel()
        coincide = tel is not None and tel["estado"] == e
        rele_ok = tel is not None and bool(tel["flags"] & 2) == (e == 3)
        reg(f"PI-03.{e}", f"ESTADO {NOMBRE_ESTADO[e]}: ACK, telemetría y relé coinciden",
            ok and coincide and rele_ok,
            f"ACK={'sí' if ok else 'no'} estado_tel={tel['estado'] if tel else '?'} "
            f"rele={'ON' if tel and tel['flags'] & 2 else 'OFF'} (verifique LEDs a ojo)")
        time.sleep(1.2)

    # PI-04: patrones de buzzer
    for b in range(1, 5):
        ev = orden(BUZZER, b)
        reg(f"PI-04.{b}", f"BUZZER patrón {b}: ACK", ev is not None and ev[1] == ACK,
            "verifique el sonido a oído")
        time.sleep(1.5)

    # PI-05: valor fuera de rango
    ev = orden(ESTADO, 9)
    reg("PI-05", "ESTADO=9 se rechaza (NACK VALOR)", ev is not None and ev[1] == NACK and ev[3][1] == 3,
        "respuesta: " + ("ninguna" if ev is None else NOMBRE_TIPO[ev[1]]))

    # PI-06: tramas corruptas se cuentan
    t_a = j.pedir_tel()
    for _ in range(5):
        trama = bytearray(codificar(ESTADO, 0x33, bytes([4])))
        trama[-1] ^= 0xFF
        j.enviar_crudo(bytes(trama))
        time.sleep(0.05)
    t_b = j.pedir_tel()
    d_crc = (t_b["err_crc"] - t_a["err_crc"]) if t_a and t_b else -1
    reg("PI-06", "5 tramas con CRC corrupto -> err_crc aumenta en >= 5", d_crc >= 5, f"aumento={d_crc}")

    # PI-07: resincronización tras basura con SOF falso
    j.vaciar_eventos()
    seq = j.seq
    j.seq = (j.seq + 1) & 0xFF
    j.enviar_crudo(bytes([0x00, 0x13, 0xAA, 0x02, 0x07]) + codificar(ESTADO, seq, bytes([4])))
    ev = j.esperar(lambda t, s, d: t == ACK and s == seq, 1.0)
    reg("PI-07", "Basura con 0xAA falso + trama válida -> la trama válida se acepta", ev is not None,
        "ACK recibido" if ev else "sin ACK")

    # PI-08: LEN inválida
    t_a = j.pedir_tel()
    j.enviar_crudo(bytes([0xAA, 0x02, 0x00, 200]))
    time.sleep(0.1)
    t_b = j.pedir_tel()
    d_len = (t_b["err_longitud"] - t_a["err_longitud"]) if t_a and t_b else -1
    reg("PI-08", "Cabecera con LEN=200 -> err_longitud aumenta", d_len >= 1, f"aumento={d_len}")

    # PI-09: fail-safe al cortar el latido
    orden(ESTADO, 3)
    time.sleep(0.3)
    j.vaciar_eventos()
    j.latido_activo = False
    time.sleep(0.001)
    t_corte = j.t_ultimo_latido
    ev = j.esperar(lambda t, s, d: t == EVENTO_ENLACE and d[0] == 0, 3.0)
    dt = (ev[0] - t_corte) * 1000 if ev else float("inf")
    tel = j.pedir_tel()
    ok = ev is not None and dt <= 1000 and tel is not None and tel["estado"] == 5 and not (tel["flags"] & 2)
    reg("PI-09", "Cortar latido en VERDE -> fail-safe (rojo, relé OFF) en <= 1 s", ok,
        f"{dt:.0f} ms desde el último latido; estado_tel={tel['estado'] if tel else '?'} "
        f"rele={'ON' if tel and tel['flags'] & 2 else 'OFF'} (incluye latencia del USB/serial del PC)")

    # PI-10: recuperación
    j.vaciar_eventos()
    j.latido_activo = True
    ev = j.esperar(lambda t, s, d: t == EVENTO_ENLACE and d[0] == 1, 2.0)
    tel = j.pedir_tel()
    reg("PI-10", "Al volver el latido: enlace OK y estado ROJO hasta nueva orden",
        ev is not None and tel is not None and tel["estado"] == 4,
        f"estado_tel={NOMBRE_ESTADO[tel['estado']] if tel else '?'}")

    # PI-11: tiempo máximo en verde
    orden(ESTADO, 3)
    time.sleep(verde_max_s + 0.5)
    tel = j.pedir_tel()
    reg("PI-11", f"VERDE se corta solo tras ~{verde_max_s:.0f} s (protección local)",
        tel is not None and tel["estado"] == 4 and not (tel["flags"] & 2),
        f"estado_tel={NOMBRE_ESTADO[tel['estado']] if tel else '?'}")

    # PI-12: telemetría automática
    j.vaciar_eventos()
    evs = []
    fin = time.monotonic() + 3.5
    while time.monotonic() < fin:
        ev = j.esperar(lambda t, s, d: t == TELEMETRIA, fin - time.monotonic())
        if ev:
            evs.append(ev[0])
    periodo = (evs[-1] - evs[0]) / (len(evs) - 1) if len(evs) > 1 else float("nan")
    reg("PI-12", "Telemetría automática cada ~1 s", len(evs) >= 3 and 0.8 <= periodo <= 1.2,
        f"{len(evs)} tramas en 3.5 s, periodo medio {periodo:.2f} s")

    tel = j.pedir_tel()
    if tel:
        print()
        Jetson.imprimir_tel(tel)

    orden(ESTADO, 0)
    nombre = f"resultados_integracion_{datetime.datetime.now():%Y%m%d_%H%M%S}.md"
    with open(nombre, "w", encoding="utf-8") as f:
        f.write(f"# Resultados de integración (simulador) — {datetime.datetime.now():%Y-%m-%d %H:%M}\n\n")
        f.write("| ID | Requisito | Resultado | Detalle |\n|---|---|---|---|\n")
        for r in res:
            f.write(f"| {r[0]} | {r[1]} | {r[2]} | {r[3]} |\n")
        if tel:
            f.write(f"\nTelemetría final: `{tel}`\n")
    pasan = sum(1 for r in res if r[2] == "PASA")
    print(f"\n{pasan}/{len(res)} pruebas pasan. Resultados guardados en {nombre}")


# ----------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description="Simulador de la Jetson para el firmware EPP")
    ap.add_argument("puerto", help="p. ej. /dev/ttyACM0 (modo USB) o /dev/ttyUSB1 (adaptador)")
    ap.add_argument("--baudios", type=int, default=115200)
    ap.add_argument("--periodo-latido", type=float, default=0.2, help="segundos (defecto 0.2)")
    ap.add_argument("--sin-latido", action="store_true", help="arrancar sin enviar latidos")
    ap.add_argument("--auto", action="store_true", help="correr las pruebas de integración")
    ap.add_argument("--verde-max", type=float, default=8.0, help="CONFIG_EPP_VERDE_MAX_MS en s")
    ap.add_argument("--timeout-latido-ms", type=int, default=800, help="CONFIG_EPP_LATIDO_TIMEOUT_MS")
    a = ap.parse_args()

    try:
        j = Jetson(a.puerto, a.baudios, not a.sin_latido, a.periodo_latido)
    except serial.SerialException as e:
        sys.exit(f"No se pudo abrir {a.puerto}: {e}\n"
                 "¿Está en el grupo dialout? (sudo usermod -aG dialout $USER y cerrar sesión)")
    threading.Thread(target=j.hilo_rx, daemon=True).start()
    threading.Thread(target=j.hilo_latido, daemon=True).start()
    print(f"Conectado a {a.puerto} a {a.baudios} baudios. Latido {'ACTIVO' if j.latido_activo else 'APAGADO'}.")

    if a.auto:
        time.sleep(0.5)
        pruebas_auto(j, a.verde_max, a.timeout_latido_ms)
        j.vivo = False
        return

    print(__doc__.split("Comandos interactivos")[1])
    while j.vivo:
        try:
            linea = input().strip().split()
        except (EOFError, KeyboardInterrupt):
            break
        if not linea:
            continue
        c = linea[0].lower()
        arg = int(linea[1]) if len(linea) > 1 and linea[1].isdigit() else None
        if c == "q":
            break
        elif c == "e" and arg is not None:
            print(f"[{hora()}] -> ESTADO {arg} ({NOMBRE_ESTADO[arg] if arg < 5 else 'inválido'}) seq={j.enviar(ESTADO, bytes([arg]))}")
        elif c == "b" and arg is not None:
            print(f"[{hora()}] -> BUZZER {arg} seq={j.enviar(BUZZER, bytes([arg]))}")
        elif c == "t":
            tel = j.pedir_tel()
            Jetson.imprimir_tel(tel) if tel else print("sin respuesta de telemetría")
        elif c == "v":
            j.mostrar_tel = not j.mostrar_tel
            print(f"telemetría automática {'visible' if j.mostrar_tel else 'oculta'}")
        elif c == "c":
            j.latido_activo = not j.latido_activo
            print(f"[{hora()}] LATIDO {'REANUDADO' if j.latido_activo else 'CORTADO'}")
        elif c == "x":
            trama = bytearray(codificar(ESTADO, 0x55, bytes([4])))
            trama[-1] ^= 0xFF
            j.enviar_crudo(bytes(trama))
            print(f"[{hora()}] -> trama con CRC corrupto: {trama.hex(' ')}")
        elif c == "g":
            seq = j.seq
            j.seq = (j.seq + 1) & 0xFF
            j.enviar_crudo(bytes([0x00, 0x13, 0xAA, 0x02, 0x07]) + codificar(ESTADO, seq, bytes([4])))
            print(f"[{hora()}] -> basura + ESTADO ROJO válido (seq {seq}); debe llegar ACK")
        elif c == "l":
            j.enviar_crudo(bytes([0xAA, 0x02, 0x00, 200]))
            print(f"[{hora()}] -> cabecera con LEN=200")
        else:
            print("comando no reconocido (e N, b N, t, v, c, x, g, l, q)")
    j.vivo = False


if __name__ == "__main__":
    main()
