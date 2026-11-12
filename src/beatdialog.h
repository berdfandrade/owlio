#pragma once
#include <QDialog>
#include <QImage>
#include <QWidget>
#include <memory>
#include <string>

#include "document.h"

class QButtonGroup;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;

// Imagens dos beats: reduzidas para no máximo 720 px e guardadas como JPEG em base64.
namespace beatimage {
std::string fromImage(QImage img);
std::string fromFile(const QString& path, QString* error);
QImage decode(const std::shared_ptr<const std::string>& data);  // com cache
QString fileFilter();
}  // namespace beatimage

// Prévia da imagem do beat; clicar escolhe outra.
class BeatImagePreview : public QWidget {
    Q_OBJECT
public:
    explicit BeatImagePreview(QWidget* parent = nullptr);
    void setImage(const QImage& img, const QColor& band);
    QSize sizeHint() const override { return {260, 190}; }

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent*) override;
    void mouseReleaseEvent(QMouseEvent* e) override;
    void enterEvent(QEnterEvent*) override { hover_ = true; update(); }
    void leaveEvent(QEvent*) override { hover_ = false; update(); }

private:
    QImage image_;
    QColor band_;
    bool hover_ = false;
};

// Janela de um beat: título, texto, cor e imagem.
class BeatDialog : public QDialog {
    Q_OBJECT
public:
    BeatDialog(const Beat& beat, QWidget* parent = nullptr);
    Beat beat() const;

private:
    void chooseImage();
    void refreshPreview();

    Beat beat_;
    BeatImagePreview* preview_;
    QPushButton* removeImage_;
    QLineEdit* title_;
    QPlainTextEdit* text_;
    QButtonGroup* colors_;
};
