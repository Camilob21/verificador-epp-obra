#include "epp/puesta_en_marcha.hpp"

namespace epp {

PuestaEnMarcha::PuestaEnMarcha(double segundos_confirmar) : segundos_(segundos_confirmar > 0 ? segundos_confirmar : 3.0) {}

EstadoPuesta PuestaEnMarcha::actualizar(double t, const EntradasPuesta& e) {
  EstadoPuesta s;
  s.camara = e.camara;
  s.esp32 = e.esp32;
  s.hora = e.hora;
  s.confirmacion_total_s = segundos_;

  // El sensor solo cuenta como probado si el aviso llego por un enlace vivo
  if (e.esp32 && e.presencia) sensor_visto_ = true;
  s.sensor = sensor_visto_;

  const bool listos = e.camara && e.esp32;
  if (!completa_) {
    if (listos && e.presencia) {
      if (!mano_puesta_) {
        mano_puesta_ = true;
        t_mano_ = t;
      }
      s.confirmacion_s = t - t_mano_;
      if (s.confirmacion_s >= segundos_) completa_ = true;
    } else {
      mano_puesta_ = false;  // retiro la mano o se cayo algo: empieza de nuevo
    }
  }
  if (completa_) s.confirmacion_s = segundos_;
  s.completa = completa_;

  if (completa_)
    s.mensaje = "Puesta en marcha completa";
  else if (!e.camara)
    s.mensaje = "Puesta en marcha: revise la camara (no llegan imagenes)";
  else if (!e.esp32)
    s.mensaje = "Puesta en marcha: revise el cable entre la Jetson y el ESP32 (sin enlace)";
  else if (mano_puesta_)
    s.mensaje = "Puesta en marcha: mantenga la mano frente al sensor";
  else
    s.mensaje = "Puesta en marcha: acomode la camara y luego mantenga la mano frente al sensor 3 s";
  return s;
}

}  // namespace epp
