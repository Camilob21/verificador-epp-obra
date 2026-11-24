#include "inferencia_tensorrt.hpp"

#include <NvInfer.h>
#include <cuda_runtime_api.h>

#include <array>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

#include "epp/posprocesamiento.hpp"

namespace epp {

namespace {
void cudaCheck(cudaError_t e, const char* que) {
  if (e != cudaSuccess) throw std::runtime_error(std::string(que) + ": " + cudaGetErrorString(e));
}

class Logger : public nvinfer1::ILogger {
  void log(Severity s, const char* msg) noexcept override {
    if (s <= Severity::kWARNING) std::cerr << "[TRT] " << msg << "\n";
  }
};
}  // namespace

struct MotorTensorRT::Impl {
  Logger logger;
  // Orden de destruccion inverso a la declaracion: contexto, motor, runtime.
  std::unique_ptr<nvinfer1::IRuntime> runtime;
  std::unique_ptr<nvinfer1::ICudaEngine> engine;
  std::unique_ptr<nvinfer1::IExecutionContext> ctx;
  int in = -1, out = -1;
  std::array<void*, 2> buf{{nullptr, nullptr}};
  ~Impl() {
    for (void* p : buf)
      if (p) cudaFree(p);
  }
};

MotorTensorRT::MotorTensorRT(const std::string& ruta) : impl_(new Impl) {
  std::ifstream f(ruta, std::ios::binary);
  if (!f) throw std::runtime_error("No se pudo abrir el motor " + ruta);
  std::vector<char> datos((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());

  Impl& m = *impl_;
  m.runtime.reset(nvinfer1::createInferRuntime(m.logger));
  if (!m.runtime) throw std::runtime_error("createInferRuntime fallo");
  m.engine.reset(m.runtime->deserializeCudaEngine(datos.data(), datos.size()));
  if (!m.engine) throw std::runtime_error("No se pudo deserializar el motor (¿version de TensorRT distinta?)");
  m.ctx.reset(m.engine->createExecutionContext());
  if (!m.ctx) throw std::runtime_error("createExecutionContext fallo");

  m.in = m.engine->getBindingIndex("images");
  m.out = m.engine->getBindingIndex("output0");
  if (m.in < 0 || m.out < 0) throw std::runtime_error("El motor no tiene las entradas/salidas images/output0");
  const auto di = m.engine->getBindingDimensions(m.in);
  const auto dout = m.engine->getBindingDimensions(m.out);
  if (di.nbDims != 4 || di.d[1] != 3 || di.d[3] != CAM_W || di.d[2] < CAM_H || (di.d[2] - CAM_H) % 2 != 0)
    throw std::runtime_error("Entrada inesperada (se esperaba 1x3x480x640 o 1x3x640x640)");
  if (dout.nbDims != 3 || dout.d[1] != FILAS_SALIDA)
    throw std::runtime_error("Salida inesperada (se esperaba 1x13xN)");
  red_.alto = di.d[2];
  red_.ancho = di.d[3];
  red_.relleno_arriba = (red_.alto - CAM_H) / 2;
  red_.candidatos = dout.d[2];

  cudaCheck(cudaMalloc(&m.buf[static_cast<size_t>(m.in)], 3ull * red_.alto * red_.ancho * sizeof(float)),
            "cudaMalloc entrada");
  cudaCheck(cudaMalloc(&m.buf[static_cast<size_t>(m.out)], 1ull * FILAS_SALIDA * red_.candidatos * sizeof(float)),
            "cudaMalloc salida");
}

MotorTensorRT::~MotorTensorRT() = default;

void MotorTensorRT::inferir(const std::vector<float>& entrada, std::vector<float>& salida) {
  Impl& m = *impl_;
  salida.resize(static_cast<size_t>(FILAS_SALIDA) * static_cast<size_t>(red_.candidatos));
  cudaCheck(cudaMemcpy(m.buf[static_cast<size_t>(m.in)], entrada.data(), entrada.size() * sizeof(float),
                       cudaMemcpyHostToDevice),
            "copia a GPU");
  if (!m.ctx->executeV2(m.buf.data())) throw std::runtime_error("executeV2 fallo");
  cudaCheck(cudaMemcpy(salida.data(), m.buf[static_cast<size_t>(m.out)], salida.size() * sizeof(float),
                       cudaMemcpyDeviceToHost),
            "copia desde GPU");
}

}  // namespace epp
