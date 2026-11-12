#pragma once
#include <QDialog>
#include <QImage>
#include <QList>
#include <string>

class QButtonGroup;
class QHBoxLayout;
class QLabel;
class QListWidget;
class QPushButton;

// Avatar em pixel art montado por partes. Cada campo é o índice de uma opção ou de uma cor da paleta.
struct PixelAvatar {
    int body = 0;        // 0 feminino, 1 masculino, 2 neutro
    int skin = 2;
    int eyes = 0;        // normal, feliz, arregalado, com cílios
    int mouth = 0;       // sorriso, neutro, aberta, de lado
    int hairStyle = 1;
    int hairColor = 0;
    int facialHair = 0;  // nenhum, bigode, barba
    int glasses = 0;     // nenhum, redondos, escuros
    int clothes = 0;     // camiseta, gola, terno, moletom
    int shirt = 0;
    int background = 0;

    QImage render(int size = 192) const;  // grade de 16×16 ampliada sem suavizar
    static PixelAvatar random(unsigned seed);
};

// Criador de avatar: prévia grande, categorias com ícones, grade de opções desenhadas com o próprio
// avatar, cores da categoria, sorteio, desfazer e refazer.
class PixelAvatarDialog : public QDialog {
    Q_OBJECT
public:
    explicit PixelAvatarDialog(const QString& name, QWidget* parent = nullptr);
    std::string base64() const;  // PNG, para manter os pixels nítidos
    PixelAvatar avatar() const { return history_[pos_]; }
    void setCategory(int index);

private:
    void apply(const PixelAvatar& a);  // novo passo de desfazer
    void refresh();
    void fillOptions();

    QList<PixelAvatar> history_;
    int pos_ = 0;
    int category_ = 0;
    PixelAvatar initial_;
    QLabel* preview_;
    QButtonGroup* categories_;
    QListWidget* options_;
    QWidget* colorRow_;
    QHBoxLayout* colorLayout_;
    QPushButton* undo_;
    QPushButton* redo_;
};
