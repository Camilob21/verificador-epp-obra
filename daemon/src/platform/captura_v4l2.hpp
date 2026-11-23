// captura_v4l2.hpp — webcam USB por V4L2 (YUYV 640x480, buffers mmap). Solo Linux.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace epp {

class CamaraV4L2 {
 public:
  // Abre el dispositivo, fija 640x480 YUYV y arranca la captura. Lanza std::runtime_error si falla.
  explicit CamaraV4L2(const std::string& dispositivo);
  ~CamaraV4L2();
  CamaraV4L2(const CamaraV4L2&) = delete;
  CamaraV4L2& operator=(const CamaraV4L2&) = delete;

  // Copia el siguiente cuadro (YUYV compacto, 640*480*2 bytes). false = sin cuadro en 2 s.
  bool capturar(std::vector<uint8_t>& yuyv);

 private:
  void xioctl(unsigned long req, void* arg, const char* nombre);
  struct Buffer {
    void* ptr;
    size_t len;
  };
  int fd_ = -1;
  unsigned bytes_por_linea_ = 0;
  std::vector<Buffer> buffers_;
};

}  // namespace epp
