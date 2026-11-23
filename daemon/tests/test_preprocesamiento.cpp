// Pruebas del preprocesamiento: conversion YUYV->RGB y armado del tensor de entrada.
#include <gtest/gtest.h>

#include "epp/deteccion.hpp"
#include "epp/preprocesamiento.hpp"

using namespace epp;

namespace {
std::vector<uint8_t> yuyvUniforme(uint8_t y, uint8_t u, uint8_t v) {
  std::vector<uint8_t> img(static_cast<size_t>(CAM_W) * CAM_H * 2);
  for (size_t i = 0; i < img.size(); i += 4) {
    img[i] = y;
    img[i + 1] = u;
    img[i + 2] = y;
    img[i + 3] = v;
  }
  return img;
}
}  // namespace

TEST(Preprocesamiento, NegroDeRangoLimitadoDaRgbCero) {
  std::vector<uint8_t> rgb;
  yuyvARgb(yuyvUniforme(16, 128, 128), rgb);
  ASSERT_EQ(rgb.size(), static_cast<size_t>(CAM_W) * CAM_H * 3);
  EXPECT_EQ(rgb.front(), 0);
  EXPECT_EQ(rgb.back(), 0);
}

TEST(Preprocesamiento, BlancoDeRangoLimitadoDa255) {
  std::vector<uint8_t> rgb;
  yuyvARgb(yuyvUniforme(235, 128, 128), rgb);
  EXPECT_EQ(rgb[0], 255);
  EXPECT_EQ(rgb[1], 255);
  EXPECT_EQ(rgb[2], 255);
}

TEST(Preprocesamiento, ValoresFueraDeRangoSeSaturanSinDesbordar) {
  std::vector<uint8_t> rgb;
  yuyvARgb(yuyvUniforme(255, 255, 255), rgb);  // R y B se pasarian de 255
  EXPECT_EQ(rgb[0], 255);
  EXPECT_EQ(rgb[2], 255);
  yuyvARgb(yuyvUniforme(0, 0, 0), rgb);        // G se pasaria de 255, R y B serian negativos
  EXPECT_EQ(rgb[0], 0);
  EXPECT_EQ(rgb[2], 0);
}

TEST(Preprocesamiento, RojoPuroBt601) {
  // Rojo BT.601 rango limitado: Y=81, U=90, V=240 -> R alto, G y B bajos
  std::vector<uint8_t> rgb;
  yuyvARgb(yuyvUniforme(81, 90, 240), rgb);
  EXPECT_GE(rgb[0], 250);
  EXPECT_LE(rgb[1], 5);
  EXPECT_LE(rgb[2], 5);
}

TEST(Preprocesamiento, TensorSinRellenoConRed480x640) {
  std::vector<uint8_t> rgb(static_cast<size_t>(CAM_W) * CAM_H * 3, 0);
  rgb[0] = 255;  // pixel (0,0), canal R
  rgb[(static_cast<size_t>(CAM_H - 1) * CAM_W + (CAM_W - 1)) * 3 + 2] = 51;  // ultimo pixel, canal B
  GeometriaRed red;  // 480x640 por defecto
  std::vector<float> t;
  rgbATensor(rgb, red, t);
  const size_t plano = static_cast<size_t>(CAM_W) * CAM_H;
  ASSERT_EQ(t.size(), 3 * plano);
  EXPECT_FLOAT_EQ(t[0], 1.0f);                  // R de (0,0)
  EXPECT_FLOAT_EQ(t[plano], 0.0f);              // G de (0,0)
  EXPECT_FLOAT_EQ(t[3 * plano - 1], 0.2f);      // B del ultimo pixel (51/255)
}

TEST(Preprocesamiento, TensorConRellenoGrisEnRed640x640) {
  std::vector<uint8_t> rgb(static_cast<size_t>(CAM_W) * CAM_H * 3, 255);
  GeometriaRed red{640, 640, 80, 8400};
  std::vector<float> t;
  rgbATensor(rgb, red, t);
  const size_t plano = 640u * 640u;
  ASSERT_EQ(t.size(), 3 * plano);
  const float gris = 114.0f / 255.0f;
  EXPECT_FLOAT_EQ(t[0], gris);                       // fila 0: relleno
  EXPECT_FLOAT_EQ(t[79 * 640], gris);                // fila 79: relleno
  EXPECT_FLOAT_EQ(t[80 * 640], 1.0f);                // fila 80: primera fila de la imagen
  EXPECT_FLOAT_EQ(t[559 * 640 + 639], 1.0f);         // fila 559: ultima fila de la imagen
  EXPECT_FLOAT_EQ(t[560 * 640], gris);               // fila 560: relleno
  EXPECT_FLOAT_EQ(t[2 * plano + 639 * 640], gris);   // canal B, ultima fila: relleno
}
