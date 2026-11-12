#pragma once
#include <QDialog>
#include <QWidget>

#include "editor.h"
#include "pagepainter.h"

class QComboBox;
class QLineEdit;
class QPlainTextEdit;

// Miniatura de uma página de rosto, redesenhada a cada alteração.
class TitlePagePreview : public QWidget {
    Q_OBJECT
public:
    explicit TitlePagePreview(QWidget* parent = nullptr);
    void setDocument(const Document& doc);
    QSize sizeHint() const override { return {380, 492}; }
    bool hasHeightForWidth() const override { return true; }
    int heightForWidth(int w) const override { return static_cast<int>(w * 11.0 / 8.5); }

protected:
    void paintEvent(QPaintEvent*) override;

private:
    Editor ed_;
    PagePainter painter_;
};

// Janela para editar a página de rosto (título, crédito, autores, contato...).
class TitlePageDialog : public QDialog {
    Q_OBJECT
public:
    TitlePageDialog(const Document& doc, QWidget* parent = nullptr);
    // Pares chave/valor no formato Fountain (Title, Credit, Author, ...).
    std::vector<std::pair<std::string, std::string>> values() const;

private:
    void updatePreview();

    Document base_;
    TitlePagePreview* preview_;
    QLineEdit* title_;
    QComboBox* credit_;
    QPlainTextEdit* authors_;
    QLineEdit* source_;
    QLineEdit* revision_;
    QLineEdit* draftDate_;
    QPlainTextEdit* contact_;
    QPlainTextEdit* notes_;
};
