#pragma once
#include <QImage>
#include <QString>
#include <string>

class QIODevice;
class QUrl;

// Leitura de imagens com limites, para fotos de personagem e imagens de beats. Elas chegam de
// arquivos escolhidos pelo usuário ou embutidas (em base64) em roteiros que podem vir de outras
// pessoas, então nada é decodificado sem conferir antes formato e tamanho:
//  - só JPEG, PNG e WebP (nada de SVG, que tem um parser inteiro por trás);
//  - no máximo kMaxPixels pixels declarados no cabeçalho (uma "image bomb" de 50000x50000 não aloca);
//  - já reduzida na leitura para caber em `maxSide`, sem alocar o bitmap inteiro antes.
namespace imageio {

constexpr qint64 kMaxPixels = 8192LL * 8192;
constexpr qsizetype kMaxBase64 = 4 * 1024 * 1024;  // base64 embutido acima disso é ignorado

// Lê do dispositivo (arquivo, buffer). Imagem nula se o formato ou o tamanho não forem aceitos.
QImage read(QIODevice* device, int maxSide, QString* error = nullptr);
QImage readFile(const QString& path, int maxSide, QString* error = nullptr);
// Decodifica base64 embutido no roteiro.
QImage fromBase64(const std::string& base64, int maxSide);
// O caminho de um arquivo solto ou colado, só se ele estiver no próprio computador. No Windows,
// QUrl::isLocalFile() aceita compartilhamentos de rede (file://servidor/pasta), e abri-los na hora
// pode travar a janela ou mandar as credenciais do usuário para a máquina. Vazio para esses casos.
QString localPath(const QUrl& url);
// true se o arquivo tem extensão de imagem aceita (para decidir se um arraste é para o Beat Board).
bool isImagePath(const QString& path);
// Filtro para o seletor de arquivos ("Images (*.jpg *.png ...)").
QString fileFilter();

}  // namespace imageio
