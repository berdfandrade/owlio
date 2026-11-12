#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QScreen>
#include <QStyleFactory>
#include <QTimer>
#include <cstdio>

#include "i18n.h"
#include "mainwindow.h"
#include "scriptview.h"
#include "theme.h"

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);
    QApplication::setOrganizationName("Owlio");
    QApplication::setApplicationName("Owlio");
    QApplication::setApplicationVersion("0.1");
    QApplication::setStyle(QStyleFactory::create("Fusion"));
    QApplication::setWindowIcon(QIcon(":/owlio.png"));
    ThemeManager::instance().init();

    QCommandLineParser cli;
    cli.setApplicationDescription("Owlio, a screenwriting app");
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption pdfOpt("export-pdf", "Export the script to PDF and quit.", "out.pdf");
    QCommandLineOption shotOpt("screenshot", "Save a screenshot of the window and quit.", "image.png");
    QCommandLineOption themeOpt({"theme", "tema"}, "Force the theme for this run: light or dark.", "theme");
    QCommandLineOption langOpt({"language", "lang"}, "Interface language for this run: en or pt_BR.", "code");
    QCommandLineOption cardsOpt({"cards", "cartoes"}, "Open in the index cards view.");
    for (const QCommandLineOption& o : {themeOpt, langOpt, cardsOpt, pdfOpt, shotOpt}) cli.addOption(o);
    cli.addPositionalArgument("file", "A .fountain or .fdx script to open.");
    cli.process(app);

    i18n::install(app, cli.value(langOpt));

    if (cli.isSet(themeOpt)) {
        const QString t = cli.value(themeOpt).toLower();
        const bool dark = t == "dark" || t == "escuro";
        ThemeManager::instance().setMode(dark ? ThemeManager::Mode::Dark : ThemeManager::Mode::Light, false);
    }

    MainWindow w;
    const QStringList files = cli.positionalArguments();
    if (!files.isEmpty() && !w.openFile(files.first())) return 1;

    if (cli.isSet(pdfOpt)) {
        bool ok = w.exportPdfTo(cli.value(pdfOpt));
        std::printf("%s\n", ok ? "PDF exported." : "Failed to export PDF.");
        return ok ? 0 : 1;
    }

    w.show();
    if (cli.isSet(cardsOpt)) w.setCardsMode(true);
    if (cli.isSet(shotOpt)) {
        const QString out = cli.value(shotOpt);
        QTimer::singleShot(1200, &w, [&w, out] {
            if (!w.cardsMode()) w.view()->setFocus();
            QApplication::processEvents();
            w.grab().save(out);
            QApplication::exit(0);
        });
    }
    return app.exec();
}
