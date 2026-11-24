#include "epp/posprocesamiento.hpp"

#include <algorithm>

namespace epp {

float iou(const Deteccion& a, const Deteccion& b) {
  const float ix = std::max(0.0f, std::min(a.x2, b.x2) - std::max(a.x1, b.x1));
  const float iy = std::max(0.0f, std::min(a.y2, b.y2) - std::max(a.y1, b.y1));
  const float inter = ix * iy;
  const float uni = (a.x2 - a.x1) * (a.y2 - a.y1) + (b.x2 - b.x1) * (b.y2 - b.y1) - inter;
  return uni > 0.0f ? inter / uni : 0.0f;
}

Detecciones decodificar(const float* out, const GeometriaRed& red, float umbral) {
  const int n = red.candidatos;
  const float pad = static_cast<float>(red.relleno_arriba);
  Detecciones cand;
  for (int a = 0; a < n; ++a) {
    int mejor = 0;
    float conf = out[4 * n + a];
    for (int c = 1; c < NUM_CLASES; ++c) {
      const float s = out[(4 + c) * n + a];
      if (s > conf) {
        conf = s;
        mejor = c;
      }
    }
    if (conf < umbral) continue;
    const float cx = out[a], cy = out[n + a];
    const float w = out[2 * n + a], h = out[3 * n + a];
    Deteccion d{cx - w / 2, cy - h / 2 - pad, cx + w / 2, cy + h / 2 - pad, conf, mejor};
    d.x1 = std::max(0.0f, d.x1);
    d.y1 = std::max(0.0f, d.y1);
    d.x2 = std::min(static_cast<float>(CAM_W), d.x2);
    d.y2 = std::min(static_cast<float>(CAM_H), d.y2);
    if (d.x2 > d.x1 && d.y2 > d.y1) cand.push_back(d);
  }
  // NMS por clase: estable para que el resultado no dependa del orden de empates
  std::stable_sort(cand.begin(), cand.end(),
                   [](const Deteccion& a, const Deteccion& b) { return a.conf > b.conf; });
  Detecciones final_;
  std::vector<bool> suprimida(cand.size(), false);
  for (size_t i = 0; i < cand.size(); ++i) {
    if (suprimida[i]) continue;
    final_.push_back(cand[i]);
    for (size_t j = i + 1; j < cand.size(); ++j)
      if (!suprimida[j] && cand[j].clase == cand[i].clase && iou(cand[i], cand[j]) > IOU_NMS)
        suprimida[j] = true;
  }
  return final_;
}

}  // namespace epp
