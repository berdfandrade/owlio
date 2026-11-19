#pragma once
#include <QMessageBox>
#include <QString>

// Textos do roteiro (nomes, cabeçalhos, sinopses) mostrados em dicas e perguntas. Esses widgets
// tratam como HTML qualquer texto que pareça HTML; um roteiro de outra pessoa com
// <img src="//servidor/x.png"> faria o Qt buscar a imagem na rede local (no Windows, via SMB).
namespace plaintext {

// Dica de ferramenta que mostra o texto como está escrito.
inline QString tip(const QString& text) {
    return text.isEmpty() ? QString() : QStringLiteral("<qt>") + text.toHtmlEscaped() + QStringLiteral("</qt>");
}

// Pergunta Sim/Não com o texto em formato simples. Devolve true para Sim.
inline bool ask(QWidget* parent, const QString& title, const QString& text) {
    QMessageBox box(QMessageBox::Question, title, text, QMessageBox::Yes | QMessageBox::No, parent);
    box.setTextFormat(Qt::PlainText);
    box.setDefaultButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}

}  // namespace plaintext
