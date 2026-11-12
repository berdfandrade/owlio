#pragma once
#include <QString>

#include "document.h"
#include "editor.h"

class QApplication;

// Idioma da interface e convenções do roteiro, guardados nas configurações.
// O idioma oficial do Owlio é o inglês; o português vem de translations/owlio_pt_BR.ts.
namespace i18n {

struct Language {
    QString code;   // "en", "pt_BR"
    QString name;   // no próprio idioma: "English", "Português (Brasil)"
};

QList<Language> uiLanguages();
QString uiLanguage();                       // código salvo (padrão "en")
void setUiLanguage(const QString& code);    // vale a partir do próximo início
// Instala os tradutores. `override` (ex.: da linha de comando) tem prioridade sobre a configuração.
void install(QApplication& app, const QString& override = {});
QString activeLanguage();                   // idioma carregado nesta execução

ScriptLanguage scriptLanguage();
void setScriptLanguage(ScriptLanguage lang);

QString elementLabel(Element e);            // nome do elemento no idioma da interface

}  // namespace i18n
