#pragma once
#include <QDialog>
#include <QImage>
#include <QList>
#include <string>

class QButtonGroup;
class QGridLayout;
class QLabel;
class QListWidget;
class QPushButton;

// Avatar em pixel art no estilo 16 bits: grade de 32×32, cada cor com luz, base e duas sombras.
// Cada campo é o índice de uma opção ou de uma cor da paleta.
struct PixelAvatar {
    int body = 0;        // tipo de corpo: 0 feminino, 1 masculino, 2 neutro
    int skin = 2;
    int face = 0;        // detalhes: nenhum, blush, sardas, pinta, cicatriz, rugas
    int eyes = 0;        // normal, feliz, arregalado, com cílios, sonolento, fechado
    int eyeColor = 0;
    int brows = 0;       // natural, grossa, fina, erguida, brava, preocupada
    int mouth = 0;       // sorriso, neutra, rindo, de lado, triste, dentes, lábios cheios
    int lips = 0;
    int hairStyle = 1;
    int hairColor = 0;
    int facialHair = 0;  // nenhum, por fazer, bigode, cavanhaque, barba, barba cheia
    int glasses = 0;     // nenhum, redondos, quadrados, escuros, monóculo, tapa-olho
    int hat = 0;         // nenhum, gorro, boné, chapéu, faixa, laço, coroa
    int hatColor = 0;
    int clothes = 0;     // camiseta, camisa, terno, moletom, gola alta, alcinha, jaqueta
    int shirt = 0;
    int background = 0;

    QImage render(int size = 192) const;  // ampliada sem suavizar
    static PixelAvatar random(unsigned seed);
};

// Criador de avatar: prévia grande, tipo de corpo à vista, categorias com ícones, grade de opções
// desenhadas com o próprio avatar, cores da categoria, sorteio, desfazer e refazer.
class PixelAvatarDialog : public QDialog {
    Q_OBJECT
public:
    explicit PixelAvatarDialog(const QString& name, QWidget* parent = nullptr);
    std::string base64() const;  // PNG, para manter os pixels nítidos
    PixelAvatar avatar() const { return history_[pos_]; }
    void setCategory(int index);
    void setBody(int body);

private:
    void apply(const PixelAvatar& a);  // novo passo de desfazer
    void refresh();
    void fillOptions();

    QList<PixelAvatar> history_;
    int pos_ = 0;
    int category_ = 0;
    PixelAvatar initial_;
    QLabel* preview_;
    QButtonGroup* bodies_;
    QButtonGroup* categories_;
    QListWidget* options_;
    QLabel* optionsTitle_;
    QLabel* colorTitle_;
    QWidget* colorRow_;
    QGridLayout* colorLayout_;
    QPushButton* undo_;
    QPushButton* redo_;
};
