#pragma once
#include <QByteArray>
#include <QDateTime>
#include <QList>
#include <QString>
#include <QStringList>
#include <memory>

class QLockFile;

// Salvamento automático, recuperação depois de um fechamento inesperado e backups.
//
// - Recuperação: enquanto há alterações não salvas, cada janela grava uma cópia em
//   <dados do app>/recovery/<id>.fountain, protegida por um QLockFile. Se o Owlio fechar
//   sem passar pelo fechamento normal, a trava fica órfã e a cópia é oferecida no próximo início.
// - Backups: a cada salvamento, uma cópia com data e hora vai para
//   <dados do app>/backups/<nome do roteiro>-<hash>/, guardando as mais recentes.
namespace autosave {

constexpr int kKeepBackups = 30;
constexpr int kAutoBackupMinutes = 10;  // salvamentos automáticos geram no máximo um backup a cada 10 min

QString recoveryDir();
QString backupRoot();
QString backupDir(const QString& scriptPath);

// Grava um backup de `data`. Ignora se for igual ao último; com `throttle`, também se o último
// tiver menos de kAutoBackupMinutes. Retorna o caminho criado (vazio se não criou).
QString backup(const QString& scriptPath, const QByteArray& data, bool throttle);
QStringList backups(const QString& scriptPath);  // mais recente primeiro

struct Recovery {
    QString id;
    QString file;          // cópia em Fountain
    QString originalPath;  // arquivo que estava aberto (vazio = sem título)
    QDateTime when;
};

// Uma sessão de recuperação por janela.
class Session {
public:
    Session();
    ~Session();  // solta a trava, mas mantém a cópia (só clear() apaga)
    void write(const QByteArray& fountain, const QString& originalPath);
    void clear();
    QString id() const { return id_; }

private:
    QString id_;
    std::unique_ptr<QLockFile> lock_;
};

QList<Recovery> orphans();  // cópias de execuções que não fecharam direito, mais recente primeiro
void discard(const Recovery& r);

}  // namespace autosave
