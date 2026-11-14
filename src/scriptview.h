#pragma once
#include <QAbstractScrollArea>
#include <QElapsedTimer>
#include <QTimer>

#include "editor.h"
#include "pagepainter.h"
#include "speller.h"

// Área de edição: mostra o roteiro como páginas reais e trata teclado/mouse.
class ScriptView : public QAbstractScrollArea {
    Q_OBJECT
public:
    explicit ScriptView(Editor* editor, QWidget* parent = nullptr);

    double zoom() const { return zoom_; }
    void setZoom(double z);
    bool sceneNumbers() const { return sceneNumbers_; }
    void setSceneNumbers(bool on);
    bool focusMode() const { return focusMode_; }
    void setFocusMode(bool on);
    void setSpellCheck(bool on);  // sublinha as palavras que não estão no dicionário
    void setSpellLanguage(Speller::Language l);
    PagePainter& painter() { return painter_; }

    int currentPage() const;
    void gotoParagraph(int para);
    void refresh();                   // depois de trocar o documento inteiro
    void refreshAfterExternalEdit();  // depois de mudanças feitas fora da view (localizar, etc.)

public slots:
    void undo();
    void redo();
    void cut();
    void copy();
    void paste();
    void selectAll();
    void setElement(Element e);
    bool toggleDualDialogue();  // false se o cursor não está numa fala com outra ao lado
    void zoomIn() { setZoom(zoom_ * 1.1); }
    void zoomOut() { setZoom(zoom_ / 1.1); }
    void zoomReset() { setZoom(1.0); }

signals:
    void documentChanged();
    void cursorMoved();
    void zoomChanged(double zoom);

protected:
    void paintEvent(QPaintEvent*) override;
    void resizeEvent(QResizeEvent*) override;
    void keyPressEvent(QKeyEvent*) override;
    void inputMethodEvent(QInputMethodEvent*) override;
    QVariant inputMethodQuery(Qt::InputMethodQuery q) const override;
    void mousePressEvent(QMouseEvent*) override;
    void mouseMoveEvent(QMouseEvent*) override;
    void mouseReleaseEvent(QMouseEvent*) override;
    void mouseDoubleClickEvent(QMouseEvent*) override;
    void wheelEvent(QWheelEvent*) override;
    void contextMenuEvent(QContextMenuEvent*) override;
    void focusInEvent(QFocusEvent*) override;
    void focusOutEvent(QFocusEvent*) override;
    bool focusNextPrevChild(bool) override { return false; }  // Tab é do roteiro
    void scrollContentsBy(int, int) override { viewport()->update(); }

private:
    double ppi() const { return 96.0 * zoom_; }
    double pageW() const { return ed_->paper().width * ppi(); }
    double pageH() const { return ed_->paper().height * ppi(); }
    double gap() const { return 28.0; }
    QPointF pageOrigin(int page) const;
    QRectF cursorRect() const;
    Pos posFromPoint(QPointF pt) const;
    void updateScrollBars();
    void ensureCursorVisible();
    void afterEdit(int oldRevision);
    void restartBlink();
    std::pair<int, int> focusRange() const;

    Editor* ed_;
    PagePainter painter_;
    Speller speller_;
    double zoom_ = 1.0;
    bool sceneNumbers_ = false;
    bool focusMode_ = false;
    bool cursorOn_ = true;
    bool dragging_ = false;
    QTimer blink_;
    QTimer autoScroll_;
    QPointF lastMouse_;
    std::u32string preedit_;
    QElapsedTimer lastDoubleClick_;
};
