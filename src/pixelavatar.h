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

// Avatar em pixel art detalhado: grade de 64×64, formas em curvas, cada cor com brilho, luz, base,
// duas sombras e contorno.
// Cada campo é o índice de uma opção ou de uma cor da paleta.
struct PixelAvatar {
    int body = 0;        // tipo de corpo: 0 feminino, 1 masculino, 2 neutro
    int shape = 0;       // formato do rosto: oval, redondo, quadrado, coração, alongado
    int skin = 3;
    int face = 0;        // detalhes: nenhum, blush, sardas, pinta, cicatriz, rugas, curativo, maquiagem
    int eyes = 0;        // normal, feliz, arregalado, cílios, sonolento, fechado, amendoado, grande, cansado, piscando
    int eyeColor = 0;
    int brows = 0;       // natural, grossa, fina, erguida, brava, preocupada, reta, arqueada, curta
    int nose = 0;        // suave, arredondado, comprido, largo, pontudo
    int mouth = 0;       // sorriso, neutra, rindo, de lado, triste, dentes, lábios cheios, surpresa, língua, biquinho
    int lips = 0;
    int hairStyle = 1;   // 31 cortes; ver hairShape()
    int hairColor = 0;
    int facialHair = 0;  // nenhum, por fazer, bigode, cavanhaque, barba, barba cheia, guidão, mosca, costeletas, cavanhaque fechado
    int glasses = 0;     // nenhum, redondos, quadrados, escuros, monóculo, tapa-olho, gatinho, aviador, pequenos, meio aro
    int hat = 0;         // nenhum, gorro, boné, chapéu, faixa, laço, coroa, boina, caubói, cartola, bandana, fones, hijab
    int hatColor = 0;    // também a cor da gravata, do cachecol e dos elásticos
    int clothes = 0;     // 14 roupas; ver render()
    int shirt = 0;
    int accessory = 0;   // nenhum, brincos, argolas, pingentes, colar, gargantilha, piercing
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
