#include "mainwindow.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QFileDialog>
#include <QMessageBox>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QPushButton>
#include <QHeaderView>
#include <QInputDialog>
#include <QCloseEvent>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    pakFile = nullptr;

    QMenu* fileMenu = menuBar()->addMenu(tr("&File"));
    fileMenu->addAction(tr("New PAK File"), this, &MainWindow::newPakFile);
    fileMenu->addAction(tr("Open PAK File..."), this, &MainWindow::openPakFile);
    QAction* saveAct = fileMenu->addAction(tr("Save PAK File"), this, QOverload<>::of(&MainWindow::savePakFile));
    QAction* saveAsAct = fileMenu->addAction(tr("Save PAK File as..."), this, &MainWindow::savePakFileAs);
    fileMenu->addSeparator();
    fileMenu->addAction(tr("Exit"), this, &MainWindow::close);

    connect(fileMenu, &QMenu::aboutToShow, this, [=]()
    {
        if (pakFile)
        {
            saveAct->setEnabled(true);
            saveAsAct->setEnabled(true);
        }
        else
        {
            saveAct->setEnabled(false);
            saveAsAct->setEnabled(false);
        }
    });

    resourceTableWidget = new QTableWidget;
    resourceTableWidget->setColumnCount(2);
    resourceTableWidget->setHorizontalHeaderLabels({"Name", "Size"});
    resourceTableWidget->verticalHeader()->hide();
    resourceTableWidget->setSelectionBehavior(QAbstractItemView::SelectRows);
    resourceTableWidget->setEditTriggers(QAbstractItemView::NoEditTriggers);
    resourceTableWidget->horizontalHeader()->setSectionResizeMode(0, QHeaderView::Stretch);
    resourceTableWidget->horizontalHeader()->setSectionsClickable(false);

    QPushButton* importButton = new QPushButton(tr("Import"));
    QPushButton* exportButton = new QPushButton(tr("Export"));
    QPushButton* replaceButton = new QPushButton(tr("Replace"));
    QPushButton* moveUpButton = new QPushButton(tr("Move Up"));
    QPushButton* moveDownButton = new QPushButton(tr("Move Down"));
    QPushButton* renameButton = new QPushButton(tr("Rename"));
    QPushButton* deleteButton = new QPushButton(tr("Delete"));

    connect(importButton, &QPushButton::clicked, this, &MainWindow::importResource);
    connect(exportButton, &QPushButton::clicked, this, QOverload<>::of(&MainWindow::exportResource));
    connect(replaceButton, &QPushButton::clicked, this, &MainWindow::replaceResource);
    connect(moveUpButton, &QPushButton::clicked, this, &MainWindow::moveResourceUp);
    connect(moveDownButton, &QPushButton::clicked, this, &MainWindow::moveResourceDown);
    connect(renameButton, &QPushButton::clicked, this, &MainWindow::renameResource);
    connect(deleteButton, &QPushButton::clicked, this, &MainWindow::deleteResource);

    QVBoxLayout* toolbarLayout = new QVBoxLayout;
    toolbarLayout->addWidget(importButton);
    toolbarLayout->addWidget(exportButton);
    toolbarLayout->addWidget(replaceButton);
    //toolbarLayout->addWidget(moveUpButton);
    //toolbarLayout->addWidget(moveDownButton);
    toolbarLayout->addWidget(renameButton);
    toolbarLayout->addWidget(deleteButton);
    toolbarLayout->addStretch(1);

    QHBoxLayout *mainLayout = new QHBoxLayout;
    mainLayout->addWidget(resourceTableWidget, 1);
    mainLayout->addLayout(toolbarLayout);

    QWidget* mainWidget = new QWidget;
    mainWidget->setLayout(mainLayout);

    setCentralWidget(mainWidget);
    resize(640, 480);
}

MainWindow::~MainWindow()
{
}

void MainWindow::updateWindowTitle()
{
    QString title = "";

    if (pakFile)
    {
        if (pakFile->unsaved)
        {
            title += "*";
        }

        if (!pakFile->path.isEmpty())
        {
            QFileInfo fileInfo(pakFile->path);

            title += fileInfo.fileName();
        }
        else
        {
            title += "Untitled";
        }

        title += " - ";
    }

    title += "PakTool";

    setWindowTitle(title);
}

bool MainWindow::newPakFile()
{
    if (!maybeSave())
    {
        return false;
    }

    delete pakFile;
    pakFile = new PakFile;

    resourceTableWidget->clearContents();

    updateWindowTitle();

    return true;
}

bool MainWindow::openPakFile()
{
    QString path = QFileDialog::getOpenFileName(this, tr("Open PAK File"), QString(), "PAK File (*.pak)");

    if (path.isEmpty())
    {
        return false;
    }

    if (!maybeSave())
    {
        return false;
    }

    delete pakFile;
    pakFile = PakFile::open(path);

    resourceTableWidget->setRowCount(pakFile->resources.count());

    for (int i = 0; i < pakFile->resources.count(); i++)
    {
        setResourceTableItem(i, pakFile->resources[i]);
    }

    updateWindowTitle();

    return pakFile != nullptr;
}

bool MainWindow::savePakFile(QString& path)
{
    if (!pakFile)
    {
        return false;
    }

    pakFile->path = path;

    bool success = pakFile->save();

    updateWindowTitle();

    return success;
}

bool MainWindow::savePakFile()
{
    if (!pakFile)
    {
        return false;
    }

    if (pakFile->path.isEmpty())
    {
        return savePakFileAs();
    }

    return savePakFile(pakFile->path);
}

bool MainWindow::savePakFileAs()
{
    if (!pakFile)
    {
        return false;
    }

    QString path = QFileDialog::getSaveFileName(this, tr("Save PAK File"), QString(), "PAK File (*.pak)");

    if (path.isEmpty())
    {
        return false;
    }

    pakFile->path = path;

    return savePakFile(pakFile->path);
}

bool MainWindow::maybeSave()
{
    if (pakFile && pakFile->unsaved)
    {
        QMessageBox::StandardButton answer =
                QMessageBox::question(this, tr("Unsaved changes"), tr("You have unsaved changes. Save first?"),
                                      QMessageBox::Yes | QMessageBox::No | QMessageBox::Cancel);

        if (answer == QMessageBox::Yes)
        {
            return savePakFile();
        }
        else if (answer == QMessageBox::No)
        {
            return true;
        }
        else
        {
            return false;
        }
    }

    return true;
}

bool MainWindow::loadResource(PakFile::Resource& resource, QString& path)
{
    QFile file(path);
    QFileInfo fileInfo(path);
    QString fileName = fileInfo.fileName();

    if (!file.open(QFile::ReadOnly))
    {
        QMessageBox::warning(this, tr("Error importing resource"),
                             QString(tr("Could not open file %1 for reading.")).arg(fileName));
        return false;
    }

    qint64 size = file.size();
    char* data = new char[size];

    if (file.read(data, size) != size)
    {
        QMessageBox::warning(this, tr("Error importing resource"),
                             QString(tr("Could not read file %1.").arg(fileName)));
        delete[] data;
        file.close();
        return false;
    }

    file.close();

    resource.name = fileInfo.fileName();
    resource.data = data;
    resource.size = size;
    resource.ownsData = true;

    return true;
}

void MainWindow::importResource()
{
    if (!pakFile)
    {
        return;
    }

    QStringList paths = QFileDialog::getOpenFileNames(this, tr("Import Resource(s)"));

    if (paths.empty())
    {
        return;
    }

    for (QString& path : paths)
    {
        PakFile::Resource resource;

        if (!loadResource(resource, path))
        {
            continue;
        }

        pakFile->resources.append(resource);

        resourceTableWidget->setRowCount(pakFile->resources.count());
        setResourceTableItem(pakFile->resources.count() - 1, resource);
    }

    resourceTableWidget->setFocus();

    pakFile->unsaved = true;
    updateWindowTitle();
}

void MainWindow::exportResource(PakFile::Resource& resource, QString& path)
{
    QFileInfo fileInfo(path);
    QDir dir;
    dir.mkpath(fileInfo.absolutePath());

    QFile file(path);

    if (!file.open(QFile::WriteOnly))
    {
        QMessageBox::warning(this, tr("Error exporting resource"),
                             QString(tr("Could not open file %1 for writing.")).arg(resource.name));
        return;
    }

    if (file.write(resource.data, resource.size) != resource.size)
    {
        QMessageBox::warning(this, tr("Error exporting resource"),
                             QString(tr("Could not write file %1.")).arg(resource.name));
        file.close();
        return;
    }

    file.close();
}

void MainWindow::exportResource()
{
    if (!pakFile)
    {
        return;
    }

    QList<QTableWidgetItem*> selectedItems = resourceTableWidget->selectedItems();

    if (selectedItems.empty())
    {
        return;
    }

    QString folderPath = QFileDialog::getExistingDirectory(this, tr("Export Resource(s)"));

    if (folderPath.isEmpty())
    {
        return;
    }

    for (QTableWidgetItem* item : selectedItems)
    {
        if (item->column() == 0)
        {
            PakFile::Resource& resource = pakFile->resources[item->row()];
            QString path = QDir(folderPath).filePath(resource.name);

            exportResource(resource, path);
        }
    }

    resourceTableWidget->setFocus();
}

void MainWindow::replaceResource()
{
    if (!pakFile)
    {
        return;
    }

    QList<QTableWidgetItem*> selectedItems = resourceTableWidget->selectedItems();

    if (selectedItems.empty())
    {
        return;
    }

    QTableWidgetItem* item = nullptr;

    for (int i = 0; i < selectedItems.count(); i++)
    {
        if (selectedItems[i]->column() == 0)
        {
            item = selectedItems[i];
        }
    }

    if (!item)
    {
        return;
    }

    QString path = QFileDialog::getOpenFileName(this, tr("Replace Resource"));

    if (path.isEmpty())
    {
        return;
    }

    PakFile::Resource resource;

    if (!loadResource(resource, path))
    {
        return;
    }

    PakFile::Resource& oldResource = pakFile->resources[item->row()];

    if (oldResource.ownsData)
    {
        delete[] oldResource.data;
    }

    oldResource = resource;

    setResourceTableItem(item->row(), resource);

    resourceTableWidget->setFocus();

    pakFile->unsaved = true;
    updateWindowTitle();
}

void MainWindow::moveResource(int from, int to)
{
    pakFile->resources.move(from, to);

    QTableWidgetItem* nameItem = resourceTableWidget->takeItem(from, 0);
    QTableWidgetItem* sizeItem = resourceTableWidget->takeItem(from, 1);

    resourceTableWidget->removeRow(from);
    resourceTableWidget->insertRow(to);
    resourceTableWidget->setItem(to, 0, nameItem);
    resourceTableWidget->setItem(to, 1, sizeItem);

    nameItem->setSelected(true);
    sizeItem->setSelected(true);
}

void MainWindow::moveResourceUp()
{
    if (!pakFile)
    {
        return;
    }

    QList<QTableWidgetItem*> selectedItems = resourceTableWidget->selectedItems();

    if (selectedItems.empty())
    {
        return;
    }

    resourceTableWidget->setCurrentItem(nullptr);

    int lastRow = -1;

    for (int i = 0; i < selectedItems.count(); i++)
    {
        QTableWidgetItem* item = selectedItems[i];

        if (item->column() == 0)
        {
            int row = item->row();

            if (row > lastRow + 1)
            {
                lastRow = row - 1;
                moveResource(row, row - 1);
            }
            else
            {
                lastRow = row;
            }
        }
    }

    resourceTableWidget->setFocus();

    pakFile->unsaved = true;
    updateWindowTitle();
}

void MainWindow::moveResourceDown()
{
    if (!pakFile)
    {
        return;
    }

    QList<QTableWidgetItem*> selectedItems = resourceTableWidget->selectedItems();

    if (selectedItems.empty())
    {
        return;
    }

    resourceTableWidget->setCurrentItem(nullptr);

    int firstRow = pakFile->resources.count();

    for (int i = selectedItems.count() - 1; i >= 0; i--)
    {
        QTableWidgetItem* item = selectedItems[i];

        if (item->column() == 0)
        {
            int row = item->row();

            if (row < firstRow - 1)
            {
                firstRow = row + 1;
                moveResource(row, row + 1);
            }
            else
            {
                firstRow = row;
            }
        }
    }

    resourceTableWidget->setFocus();

    pakFile->unsaved = true;
    updateWindowTitle();
}

void MainWindow::renameResource()
{
    if (!pakFile)
    {
        return;
    }

    QList<QTableWidgetItem*> selectedItems = resourceTableWidget->selectedItems();

    if (selectedItems.empty())
    {
        return;
    }

    QString& currName = pakFile->resources[selectedItems[0]->row()].name;
    bool ok;

    QString newName =
            QInputDialog::getText(this, tr("Rename Resource"), tr("Enter new resource name:"),
                                  QLineEdit::Normal, currName, &ok);

    if (!ok)
    {
        return;
    }

    for (QTableWidgetItem* item : selectedItems)
    {
        if (item->column() == 0)
        {
            PakFile::Resource& resource = pakFile->resources[item->row()];
            resource.name = newName;
            item->setText(newName);
        }
    }

    resourceTableWidget->setFocus();

    pakFile->unsaved = true;
    updateWindowTitle();
}

void MainWindow::deleteResource()
{
    if (!pakFile)
    {
        return;
    }

    QList<QTableWidgetItem*> selectedItems = resourceTableWidget->selectedItems();

    if (selectedItems.empty())
    {
        return;
    }

    int firstRow = selectedItems[0]->row();

    for (QTableWidgetItem* item : selectedItems)
    {
        if (item->column() == 0)
        {
            pakFile->deleteResource(item->row());
            resourceTableWidget->removeRow(item->row());
        }
    }

    resourceTableWidget->setCurrentItem(nullptr);
    resourceTableWidget->selectRow(firstRow);
    resourceTableWidget->setFocus();

    pakFile->unsaved = true;
    updateWindowTitle();
}

void MainWindow::setResourceTableItem(int row, PakFile::Resource& resource)
{
    resourceTableWidget->setItem(row, 0, new QTableWidgetItem(resource.name));
    resourceTableWidget->setItem(row, 1, new QTableWidgetItem(QString::number(resource.size)));
}

void MainWindow::closeEvent(QCloseEvent* event)
{
    if (!maybeSave())
    {
        event->ignore();
    }
    else
    {
        event->accept();
    }
}
