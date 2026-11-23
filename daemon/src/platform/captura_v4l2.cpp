#include "captura_v4l2.hpp"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>

#include "epp/deteccion.hpp"

namespace epp {

CamaraV4L2::CamaraV4L2(const std::string& dev) {
  fd_ = ::open(dev.c_str(), O_RDWR);
  if (fd_ < 0) throw std::runtime_error("No se pudo abrir " + dev + ": " + std::strerror(errno));

  v4l2_format fmt{};
  fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  fmt.fmt.pix.width = CAM_W;
  fmt.fmt.pix.height = CAM_H;
  fmt.fmt.pix.pixelformat = V4L2_PIX_FMT_YUYV;
  fmt.fmt.pix.field = V4L2_FIELD_NONE;
  xioctl(VIDIOC_S_FMT, &fmt, "VIDIOC_S_FMT");
  if (fmt.fmt.pix.width != CAM_W || fmt.fmt.pix.height != CAM_H || fmt.fmt.pix.pixelformat != V4L2_PIX_FMT_YUYV)
    throw std::runtime_error("La camara no acepto 640x480 YUYV");
  bytes_por_linea_ = fmt.fmt.pix.bytesperline ? fmt.fmt.pix.bytesperline : CAM_W * 2;

  v4l2_requestbuffers req{};
  req.count = 4;
  req.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  req.memory = V4L2_MEMORY_MMAP;
  xioctl(VIDIOC_REQBUFS, &req, "VIDIOC_REQBUFS");
  for (unsigned i = 0; i < req.count; ++i) {
    v4l2_buffer b{};
    b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
    b.memory = V4L2_MEMORY_MMAP;
    b.index = i;
    xioctl(VIDIOC_QUERYBUF, &b, "VIDIOC_QUERYBUF");
    void* p = mmap(nullptr, b.length, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, b.m.offset);
    if (p == MAP_FAILED) throw std::runtime_error(std::string("mmap: ") + std::strerror(errno));
    buffers_.push_back({p, b.length});
    xioctl(VIDIOC_QBUF, &b, "VIDIOC_QBUF");
  }
  v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  xioctl(VIDIOC_STREAMON, &t, "VIDIOC_STREAMON");
}

CamaraV4L2::~CamaraV4L2() {
  if (fd_ < 0) return;
  v4l2_buf_type t = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  ioctl(fd_, VIDIOC_STREAMOFF, &t);
  for (auto& b : buffers_) munmap(b.ptr, b.len);
  ::close(fd_);
}

bool CamaraV4L2::capturar(std::vector<uint8_t>& yuyv) {
  fd_set fds;
  FD_ZERO(&fds);
  FD_SET(fd_, &fds);
  timeval tv{2, 0};
  if (select(fd_ + 1, &fds, nullptr, nullptr, &tv) <= 0) return false;

  v4l2_buffer b{};
  b.type = V4L2_BUF_TYPE_VIDEO_CAPTURE;
  b.memory = V4L2_MEMORY_MMAP;
  xioctl(VIDIOC_DQBUF, &b, "VIDIOC_DQBUF");
  const auto* src = static_cast<const uint8_t*>(buffers_[b.index].ptr);
  const size_t fila = static_cast<size_t>(CAM_W) * 2;
  yuyv.resize(fila * CAM_H);
  for (int y = 0; y < CAM_H; ++y)
    std::memcpy(yuyv.data() + static_cast<size_t>(y) * fila, src + static_cast<size_t>(y) * bytes_por_linea_, fila);
  xioctl(VIDIOC_QBUF, &b, "VIDIOC_QBUF");
  return true;
}

void CamaraV4L2::xioctl(unsigned long req, void* arg, const char* nombre) {
  int r;
  do {
    r = ioctl(fd_, req, arg);
  } while (r == -1 && errno == EINTR);
  if (r == -1) throw std::runtime_error(std::string(nombre) + ": " + std::strerror(errno));
}

}  // namespace epp
