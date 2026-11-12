#pragma once
#include <QAbstractScrollArea>
#include <QDialog>

#include "editor.h"
#include "pagepainter.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QLineEdit;
class QPushButton;
class QSpinBox;
class QToolButton;

// Miniaturas das folhas que serão impressas (rosto + páginas escolhidas).
class SheetPreview : public QAbstractScrollArea {
    Q_OBJECT
public:
    SheetPreview(const Editor& ed, QWidget* parent = nullptr);
    void setOptions(const PrintOptions& opt);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void scrollContentsBy(int, int) override { viewport()->update(); }

private:
    void relayout();
    std::vector<int> sheets() const;  // -1 = página de rosto

    const Editor& ed_;
    PagePainter painter_;
    PrintOptions opt_;
    int cols_ = 2;
    double sheetW_ = 200, sheetH_ = 260;
};

// Janela de impressão e exportação para PDF.
class PrintDialog : public QDialog {
    Q_OBJECT
public:
    enum class Target { Printer, Pdf };
    PrintDialog(const Editor& ed, Target target, const QString& suggestedPdfPath, bool sceneNumbers,
                QWidget* parent = nullptr);
    PrintOptions options() const;
    QString exportedPath() const { return exported_; }

private:
    void setTarget(Target t);
    void refresh();
    void run(bool advanced);

    const Editor& ed_;
    Target target_;
    QString pdfPath_, exported_;
    SheetPreview* preview_;
    QLabel* heading_;
    QLabel* summary_;
    QToolButton* toPrinter_;
    QToolButton* toPdf_;
    QWidget* printerBox_;
    QComboBox* printer_;
    QSpinBox* copies_;
    QComboBox* range_;
    QSpinBox* from_;
    QSpinBox* to_;
    QWidget* rangeRow_;
    QCheckBox* titlePage_;
    QCheckBox* sceneNumbers_;
    QLineEdit* watermark_;
    QPushButton* go_;
    QPushButton* advanced_;
};
