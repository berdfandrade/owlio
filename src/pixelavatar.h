#pragma once
#include <QDialog>
#include <QImage>
#include <string>

class QComboBox;
class QLabel;

// Avatar em pixel art montado por partes (corpo, pele, cabelo, barba, óculos, roupa, fundo).
struct PixelAvatar {
    int body = 0;        // 0 feminino, 1 masculino, 2 neutro
    int skin = 2;
    int hairStyle = 1;
    int hairColor = 0;
    int facialHair = 0;  // 0 nenhum, 1 bigode, 2 barba
    int glasses = 0;     // 0 nenhum, 1 redondos, 2 escuros
    int shirt = 0;
    int background = 0;

    QImage render(int size = 192) const;  // grade de 16×16 ampliada sem suavizar
    static PixelAvatar random(unsigned seed);
};

// Janela para montar o avatar escolhendo as partes, com prévia ao vivo.
class PixelAvatarDialog : public QDialog {
    Q_OBJECT
public:
    explicit PixelAvatarDialog(const QString& name, QWidget* parent = nullptr);
    std::string base64() const;  // PNG, para manter os pixels nítidos

private:
    void load(const PixelAvatar& a);
    PixelAvatar current() const;
    void refresh();

    QLabel* preview_;
    QComboBox *body_, *skin_, *hairStyle_, *hairColor_, *facialHair_, *glasses_, *shirt_, *background_;
};
