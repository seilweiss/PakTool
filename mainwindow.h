#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTableWidget>

#include "pakfile.h"

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

    bool savePakFile(QString& path);

public slots:
    bool newPakFile();
    bool openPakFile();
    bool savePakFile();
    bool savePakFileAs();

    void importResource();
    void exportResource();
    void replaceResource();
    void moveResourceUp();
    void moveResourceDown();
    void renameResource();
    void deleteResource();

private:
    PakFile* pakFile;
    QTableWidget* resourceTableWidget;

    bool maybeSave();
    void updateWindowTitle();
    void moveResource(int from, int to);
    void setResourceTableItem(int row, PakFile::Resource& resource);
    void exportResource(PakFile::Resource& resource, QString& path);
    bool loadResource(PakFile::Resource& resource, QString& path);

    void closeEvent(QCloseEvent* event) override;
};

#endif // MAINWINDOW_H
