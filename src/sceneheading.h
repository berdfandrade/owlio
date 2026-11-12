#pragma once
#include <QColor>
#include <QString>
#include <cstring>
#include <initializer_list>

// Partes de um cabeçalho de cena e a cor do horário, usadas nos cartões e na ficha do personagem.
struct HeadingParts {
    QString prefix;    // INT. / EXT. / INT./EXT.
    QString location;  // CINE ODEON - SAGUÃO
    QString time;      // NOITE
};

inline HeadingParts splitHeading(const QString& heading) {
    HeadingParts h;
    QString rest = heading.trimmed();
    static const char* prefixes[] = {"INT./EXT.", "EXT./INT.", "INT/EXT", "I/E", "INT.", "EXT.", "EST.", "INT ", "EXT "};
    for (const char* p : prefixes) {
        if (rest.startsWith(QLatin1String(p), Qt::CaseInsensitive)) {
            h.prefix = rest.left(int(strlen(p))).trimmed();
            rest = rest.mid(int(strlen(p))).trimmed();
            break;
        }
    }
    int dash = rest.lastIndexOf(" - ");
    if (dash >= 0) {
        h.time = rest.mid(dash + 3).trimmed();
        rest = rest.left(dash).trimmed();
    }
    h.location = rest;
    return h;
}

// Cor da faixa do cartão conforme o horário da cena.
inline QColor timeColor(const QString& time) {
    const QString t = time.toUpper();
    auto has = [&](std::initializer_list<const char*> words) {
        for (const char* w : words)
            if (t.contains(QString::fromUtf8(w))) return true;
        return false;
    };
    if (has({"AMANHECER", "ANOITECER", "ENTARDECER", "PÔR DO SOL", "CREPÚSCULO", "DAWN", "DUSK", "SUNSET", "SUNRISE"}))
        return QColor("#f0855a");
    if (has({"NOITE", "MADRUGADA", "NIGHT"})) return QColor("#6e7bf2");
    if (has({"DIA", "MANHÃ", "TARDE", "DAY", "MORNING", "AFTERNOON"})) return QColor("#f2b13b");
    if (has({"CONTÍNUO", "CONTINUO", "DEPOIS", "MAIS TARDE", "CONTINUOUS", "LATER"})) return QColor("#36b39b");
    return QColor("#9aa0ab");
}
