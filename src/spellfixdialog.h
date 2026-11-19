#pragma once
#include <QDialog>
#include <string>
#include <vector>

#include "editor.h"

// Confirmação da correção ortográfica em lote: lista cada palavra errada com a correção que vai
// entrar (e quantas vezes aparece) e as que ficam como estão por falta de sugestão. Nada muda no
// roteiro até o usuário clicar em "Corrigir tudo"; depois, um Ctrl+Z desfaz a correção inteira.
class SpellFixDialog : public QDialog {
    Q_OBJECT
public:
    SpellFixDialog(const std::vector<Editor::TextFix>& fixes, const std::vector<std::u32string>& unfixable,
                   QWidget* parent = nullptr);
};
