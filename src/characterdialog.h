#pragma once
#include <QDialog>
#include <QWidget>

#include "editor.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QListWidget;
class QPlainTextEdit;
class QPushButton;

// Foto redonda da ficha; clicar abre a escolha de imagem.
class AvatarButton : public QWidget {
    Q_OBJECT
public:
    explicit AvatarButton(QWidget* parent = nullptr);
    void set(const QString& name, const std::string& photo);
    QSize sizeHint() const override { return {136, 136}; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override { hover_ = false; update(); }

private:
    QString name_;
    std::string photo_;
    bool hover_ = false;
};

// Ficha do personagem: foto, nome, papel, idade, ocupação, características e sinopse,
// com os números dele no roteiro e as cenas em que fala.
class CharacterDialog : public QDialog {
    Q_OBJECT
public:
    CharacterDialog(const Editor& ed, const QString& name, QWidget* parent = nullptr);
    CharacterInfo info() const;
    int gotoParagraph() const { return gotoPara_; }  // -1 se não pediu para ir a uma cena

    void accept() override;

private:
    void choosePhoto();
    void updatePhoto();

    const Editor& ed_;
    QString original_;
    std::string photo_;
    int gotoPara_ = -1;

    AvatarButton* avatar_;
    QPushButton* removePhoto_;
    QLabel* nameLabel_;
    QLabel* roleLabel_;
    QLineEdit* name_;
    QComboBox* role_;
    QLineEdit* age_;
    QLineEdit* occupation_;
    QPlainTextEdit* traits_;
    QPlainTextEdit* synopsis_;
    QListWidget* scenes_;
};
