#include "i18n.h"

#include <QApplication>
#include <QLibraryInfo>
#include <QLocale>
#include <QSettings>
#include <QTranslator>

namespace i18n {

namespace {
QString g_active = "en";

// Para o lupdate encontrar os nomes dos elementos (vêm do núcleo, sem Qt).
[[maybe_unused]] const char* const kElementNames[] = {
    QT_TRANSLATE_NOOP("Element", "Scene Heading"), QT_TRANSLATE_NOOP("Element", "Action"),
    QT_TRANSLATE_NOOP("Element", "Character"),     QT_TRANSLATE_NOOP("Element", "Parenthetical"),
    QT_TRANSLATE_NOOP("Element", "Dialogue"),      QT_TRANSLATE_NOOP("Element", "Transition"),
};
}  // namespace

QList<Language> uiLanguages() {
    return {{"en", "English"}, {"pt_BR", QString::fromUtf8("Português (Brasil)")}};
}

QString uiLanguage() {
    const QString code = QSettings().value("language", "en").toString();
    for (const Language& l : uiLanguages())
        if (l.code == code) return code;
    return "en";
}

void setUiLanguage(const QString& code) { QSettings().setValue("language", code); }

QString activeLanguage() { return g_active; }

void install(QApplication& app, const QString& override) {
    QString code = override.isEmpty() ? uiLanguage() : override;
    if (code.startsWith("pt")) code = "pt_BR";
    else code = "en";
    g_active = code;

    // Mesmo em inglês carregamos owlio_en: ele só traz as formas de plural ("1 scene", "2 scenes").
    auto* app_tr = new QTranslator(&app);
    if (app_tr->load(":/i18n/owlio_" + code + ".qm")) app.installTranslator(app_tr);

    if (code != "en") {
        // Textos do próprio Qt (botões Salvar/Cancelar, diálogos de arquivo).
        auto* qt_tr = new QTranslator(&app);
        if (qt_tr->load("qtbase_" + code, QLibraryInfo::path(QLibraryInfo::TranslationsPath)))
            app.installTranslator(qt_tr);
        QLocale::setDefault(QLocale(code));
    } else {
        QLocale::setDefault(QLocale(QLocale::English, QLocale::UnitedStates));
    }
}

ScriptLanguage scriptLanguage() {
    return QSettings().value("scriptLanguage", "en").toString() == "pt" ? ScriptLanguage::Portuguese
                                                                        : ScriptLanguage::English;
}

void setScriptLanguage(ScriptLanguage lang) {
    QSettings().setValue("scriptLanguage", lang == ScriptLanguage::Portuguese ? "pt" : "en");
}

QString elementLabel(Element e) { return QCoreApplication::translate("Element", elementName(e)); }

}  // namespace i18n
