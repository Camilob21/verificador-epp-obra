'use strict';
// Dashboard de solo lectura: consulta la API local del daemon (nginx reenvía /api/ a 127.0.0.1:8081).
// Si la API no responde, la pantalla lo dice: nunca se muestra un estado "verde" viejo.

const $ = (id) => document.getElementById(id);

const NOMBRES_ESTADO = {
  REPOSO: 'En reposo',
  ESPERANDO_PERSONA: 'Esperando persona',
  VERIFICANDO: 'Verificando',
  RESULTADO_VERDE: 'Cumple: puede pasar',
  RESULTADO_ROJO: 'No cumple',
};

function claseLuz(luz) {
  if (luz.startsWith('verde')) return 'luz-verde';
  if (luz.startsWith('rojo')) return 'luz-rojo';
  if (luz.includes('parpadeando')) return 'luz-amarillo parpadeo';
  if (luz.startsWith('amarillo')) return 'luz-amarillo';
  return 'luz-apagado';
}

function bytes(n) {
  const u = ['B', 'KB', 'MB', 'GB'];
  let i = 0;
  while (n >= 1024 && i < u.length - 1) { n /= 1024; i++; }
  return `${n.toFixed(i ? 1 : 0)} ${u[i]}`;
}

function duracion(s) {
  const h = Math.floor(s / 3600), m = Math.floor((s % 3600) / 60);
  return h ? `${h} h ${m} min` : `${m} min`;
}

function texto(id, valor) { $(id).textContent = valor; }

function celda(fila, valor, clase) {
  const td = document.createElement('td');
  td.textContent = valor;
  if (clase) td.className = clase;
  fila.appendChild(td);
}

async function pedir(ruta) {
  const r = await fetch(ruta, { cache: 'no-store' });
  const cuerpo = await r.json();
  if (!r.ok) throw new Error(cuerpo.error || `HTTP ${r.status}`);
  return cuerpo;
}

async function actualizarEstado() {
  try {
    const e = await pedir('/api/estado');
    $('conexion').textContent = 'en línea';
    $('conexion').className = 'chip chip-bien';
    texto('estado', NOMBRES_ESTADO[e.estado] || e.estado);
    texto('mensaje', e.mensaje || '—');
    $('luz').className = `luz ${claseLuz(e.luz)}`;
    texto('rele', e.rele ? 'activado' : 'apagado');
    texto('esp32', !e.esp32.configurado ? 'no configurado' : (e.esp32.enlace ? 'enlazado' : 'SIN ENLACE'));
    texto('pir', e.esp32.pir ? 'presencia' : 'sin presencia');
    texto('verdes', e.verdes);
    texto('rojos', e.rojos);
    texto('fps', e.fps.toFixed(1));
    texto('inferencia', e.inferencia_ms.toFixed(0));
    if (e.camara_en_vivo && $('panel-vivo').hidden) {
      $('panel-vivo').hidden = false;
      refrescarVivo();
    }
  } catch (err) {
    $('conexion').textContent = 'sin conexión con el daemon';
    $('conexion').className = 'chip chip-mal';
    texto('estado', 'Sin datos');
    texto('mensaje', 'No se puede consultar el estado. No interpretar esta pantalla como autorización.');
    $('luz').className = 'luz luz-apagado';
  }
}

async function actualizarEventos() {
  const cuerpo = $('eventos');
  try {
    const lista = await pedir('/api/eventos?limite=15');
    texto('eventos-aviso', lista.length ? '' : 'Todavía no hay eventos registrados.');
    cuerpo.replaceChildren();
    for (const ev of lista) {
      const tr = document.createElement('tr');
      celda(tr, ev.fecha.replace('T', ' ').slice(0, 19) + (ev.hora_confiable ? '' : ' (hora sin confirmar)'));
      celda(tr, ev.resultado, `res-${ev.resultado}`);
      const falta = [ev.falta_casco && 'casco', ev.falta_chaleco && 'chaleco'].filter(Boolean).join(', ');
      celda(tr, falta || '—');
      celda(tr, `${Math.round(ev.fraccion_casco * 100)} %`);
      celda(tr, `${Math.round(ev.fraccion_chaleco * 100)} %`);
      cuerpo.appendChild(tr);
    }
  } catch (err) {
    texto('eventos-aviso', `Eventos no disponibles: ${err.message}`);
  }
}

async function actualizarSistema() {
  try {
    const s = await pedir('/api/sistema');
    texto('cpu', s.cpu_pct === null ? '—' : s.cpu_pct.toFixed(0));
    texto('gpu', s.gpu_pct === null ? '—' : s.gpu_pct.toFixed(0));
    const m = s.memoria;
    texto('mem', m.total_kb ? (100 * (1 - m.disponible_kb / m.total_kb)).toFixed(0) : '—');
    texto('uptime', duracion(s.uptime_s));
    texto('temperaturas', s.temperaturas.map((t) => `${t.zona}: ${t.c.toFixed(1)} °C`).join(' · ') || '—');
    const red = $('red');
    red.replaceChildren();
    for (const r of s.red) {
      const tr = document.createElement('tr');
      celda(tr, r.interfaz);
      celda(tr, bytes(r.rx_bytes));
      celda(tr, bytes(r.tx_bytes));
      celda(tr, `${bytes(r.rx_bps)}/s`);
      celda(tr, `${bytes(r.tx_bps)}/s`);
      red.appendChild(tr);
    }
    texto('puertos', s.puertos_escucha.join(', ') || '—');
    const e = s.enlace;
    texto('enlace', !e.configurado ? 'UART no configurado' :
      `${e.ok ? 'enlazado' : 'SIN ENLACE'} · latidos ${e.latidos} · órdenes ${e.ordenes} · ACK ${e.acks} · ` +
      `NACK ${e.nacks} · reintentos ${e.reintentos} · sin ACK ${e.sin_ack} · tramas recibidas ${e.rx_tramas_ok} · ` +
      `errores CRC ${e.rx_err_crc}`);
    const c = s.mcu;
    texto('mcu', !c ? 'sin telemetría' :
      `encendido ${duracion(c.uptime_s)} · heap libre ${bytes(c.heap_libre)} (mín. ${bytes(c.heap_min)}) · ` +
      `CPU ${c.cpu_pct === null ? 'n/d' : c.cpu_pct + ' %'} · errores CRC ${c.err_crc} · fallos de enlace ${c.fallos_enlace} · ` +
      `stack libre ${c.stack_libre.join('/')} B`);
  } catch (err) {
    texto('enlace', `Sistema no disponible: ${err.message}`);
  }
}

// Vista en vivo: pide la siguiente imagen solo cuando terminó de cargar la anterior (~5 por segundo),
// así una red lenta no acumula peticiones.
function refrescarVivo() {
  const img = $('vivo');
  img.onload = () => { texto('vivo-aviso', ''); setTimeout(refrescarVivo, 200); };
  img.onerror = () => { texto('vivo-aviso', 'Sin imagen de la cámara en este momento.'); setTimeout(refrescarVivo, 1000); };
  img.src = `/api/camara.jpg?t=${Date.now()}`;
}

actualizarEstado();
actualizarEventos();
actualizarSistema();
setInterval(actualizarEstado, 500);
setInterval(actualizarEventos, 3000);
setInterval(actualizarSistema, 2000);
