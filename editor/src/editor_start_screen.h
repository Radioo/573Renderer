#pragma once

#include <QString>
#include <QWidget>

#include <vector>

class QLabel;
class QListWidget;

namespace Editor {

class Commands;

struct RecentFile {
    QString path;
    QString folder;
    int animations = -1;
    bool project = false;
};

struct HostCard {
    QString install;
    QString build;
    bool running = false;
};

class StartScreen : public QWidget {
    Q_OBJECT

public:
    StartScreen(Commands& commands, QWidget* parent = nullptr);

    void ShowRecent(const std::vector<RecentFile>& recent);
    void ShowHost(const HostCard& host);

signals:
    void FileAsked(const QString& path);

private:
    Commands& commands_;
    QListWidget* recent_;
    QLabel* host_;
};

}
