#include <QApplication>
#include <QIcon>
#include <QCommandLineParser>
#include <QScreen>
#include <QStyleFactory>
#include <QTimer>
#include <cstdio>

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
    cli.setApplicationDescription("Owlio — editor de roteiros");
    cli.addHelpOption();
    cli.addVersionOption();
    QCommandLineOption pdfOpt("export-pdf", "Exporta o roteiro para PDF e sai.", "saida.pdf");
    QCommandLineOption shotOpt("screenshot", "Salva uma captura da janela e sai.", "imagem.png");
    QCommandLineOption themeOpt("tema", "Força o tema nesta execução: claro ou escuro.", "tema");
    cli.addOption(themeOpt);
    QCommandLineOption cardsOpt("cartoes", "Abre na tela de cartões.");
    cli.addOption(cardsOpt);
    cli.addOption(pdfOpt);
    cli.addOption(shotOpt);
    cli.addPositionalArgument("arquivo", "Roteiro .fountain ou .fdx para abrir.");
    cli.process(app);

    if (cli.isSet(themeOpt))
        ThemeManager::instance().setMode(cli.value(themeOpt) == "escuro" ? ThemeManager::Mode::Dark
                                                                         : ThemeManager::Mode::Light,
                                         false);

    MainWindow w;
    const QStringList files = cli.positionalArguments();
    if (!files.isEmpty() && !w.openFile(files.first())) return 1;

    if (cli.isSet(pdfOpt)) {
        bool ok = w.exportPdfTo(cli.value(pdfOpt));
        std::printf("%s\n", ok ? "PDF exportado." : "Falha ao exportar PDF.");
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
