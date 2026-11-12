#pragma once
#include <QColor>
#include <QPainter>
#include <QString>
#include <string>

// Fotos dos personagens (guardadas no documento em base64).
namespace avatars {

// Desenha o avatar no círculo `r`: a foto, se houver, ou as iniciais sobre uma cor derivada do nome.
// `ring` > 0 desenha um anel na cor `ringColor` (para separar avatares sobrepostos).
void draw(QPainter& p, const QRectF& r, const QString& name, const std::string& base64,
          double ring = 0, const QColor& ringColor = QColor());

// Lê uma imagem, recorta o quadrado central, reduz e devolve em base64 (JPEG).
// Em caso de erro devolve vazio e preenche `error`.
std::string fromImageFile(const QString& path, QString* error);

// Filtro para QFileDialog com os formatos de imagem suportados.
QString fileFilter();

}  // namespace avatars
