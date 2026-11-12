#include "autosave.h"

#include <QCryptographicHash>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLockFile>
#include <QSaveFile>
#include <QStandardPaths>
#include <QUuid>
#include <algorithm>

namespace autosave {

namespace {
QString dataDir() { return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation); }

bool writeFile(const QString& path, const QByteArray& data) {
    QSaveFile f(path);
    if (!f.open(QIODevice::WriteOnly)) return false;
    f.write(data);
    return f.commit();
}

QByteArray readFile(const QString& path) {
    QFile f(path);
    return f.open(QIODevice::ReadOnly) ? f.readAll() : QByteArray();
}
}  // namespace

QString recoveryDir() { return dataDir() + "/recovery"; }
QString backupRoot() { return dataDir() + "/backups"; }

QString backupDir(const QString& scriptPath) {
    const QFileInfo fi(scriptPath);
    // O hash do caminho separa roteiros com o mesmo nome em pastas diferentes.
    const QByteArray hash =
        QCryptographicHash::hash(fi.absoluteFilePath().toUtf8(), QCryptographicHash::Sha1).toHex().left(8);
    return backupRoot() + "/" + fi.completeBaseName() + "-" + QString::fromLatin1(hash);
}

QStringList backups(const QString& scriptPath) {
    QDir dir(backupDir(scriptPath));
    QStringList out;
    for (const QFileInfo& fi : dir.entryInfoList(QDir::Files, QDir::Name | QDir::Reversed))
        out << fi.absoluteFilePath();
    return out;  // os nomes têm data e hora ISO, então a ordem do nome é a ordem do tempo
}

QString backup(const QString& scriptPath, const QByteArray& data, bool throttle) {
    if (scriptPath.isEmpty()) return {};
    const QStringList existing = backups(scriptPath);
    if (!existing.isEmpty()) {
        const QFileInfo last(existing.first());
        if (readFile(last.absoluteFilePath()) == data) return {};
        if (throttle && last.lastModified().secsTo(QDateTime::currentDateTime()) < kAutoBackupMinutes * 60)
            return {};
    }
    const QString dir = backupDir(scriptPath);
    if (!QDir().mkpath(dir)) return {};
    const QFileInfo fi(scriptPath);
    const QString stamp = QDateTime::currentDateTime().toString("yyyy-MM-dd HH-mm-ss");
    QString path = dir + "/" + fi.completeBaseName() + " " + stamp + "." + fi.suffix();
    for (int k = 2; QFileInfo::exists(path); ++k)
        path = dir + "/" + fi.completeBaseName() + " " + stamp + " (" + QString::number(k) + ")." + fi.suffix();
    if (!writeFile(path, data)) return {};
    const QStringList all = backups(scriptPath);
    for (int i = kKeepBackups; i < all.size(); ++i) QFile::remove(all[i]);
    return path;
}

// ------------------------------------------------------------ recuperação

Session::Session() : id_(QUuid::createUuid().toString(QUuid::WithoutBraces)) {
    QDir().mkpath(recoveryDir());
    lock_ = std::make_unique<QLockFile>(recoveryDir() + "/" + id_ + ".lock");
    lock_->tryLock(0);
}

Session::~Session() = default;

void Session::write(const QByteArray& fountain, const QString& originalPath) {
    QDir().mkpath(recoveryDir());
    const QString base = recoveryDir() + "/" + id_;
    writeFile(base + ".path", originalPath.toUtf8());
    writeFile(base + ".fountain", fountain);
}

void Session::clear() {
    const QString base = recoveryDir() + "/" + id_;
    QFile::remove(base + ".fountain");
    QFile::remove(base + ".path");
}

QList<Recovery> orphans() {
    QList<Recovery> out;
    QDir dir(recoveryDir());
    for (const QFileInfo& fi : dir.entryInfoList({"*.fountain"}, QDir::Files)) {
        const QString id = fi.completeBaseName();
        // Se conseguimos a trava, ninguém mais está usando esta cópia.
        QLockFile probe(dir.filePath(id + ".lock"));
        if (!probe.tryLock(0)) continue;
        probe.unlock();
        Recovery r;
        r.id = id;
        r.file = fi.absoluteFilePath();
        r.originalPath = QString::fromUtf8(readFile(dir.filePath(id + ".path")));
        r.when = fi.lastModified();
        out << r;
    }
    std::sort(out.begin(), out.end(), [](const Recovery& a, const Recovery& b) { return a.when > b.when; });
    return out;
}

void discard(const Recovery& r) {
    QDir dir(recoveryDir());
    QFile::remove(dir.filePath(r.id + ".fountain"));
    QFile::remove(dir.filePath(r.id + ".path"));
    QFile::remove(dir.filePath(r.id + ".lock"));
}

}  // namespace autosave
