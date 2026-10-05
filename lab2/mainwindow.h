#pragma once
#include <QAbstractTableModel>
#include <QElapsedTimer>
#include <QFutureWatcher>
#include <QList>
#include <QMainWindow>
#include <QStringList>

#include "ImageParser.h"

class QLabel;
class QProgressBar;
class QPushButton;
class QTableView;

// Простая модель таблицы: держит список результатов
class ImageModel : public QAbstractTableModel {
public:
    explicit ImageModel(QObject *parent = nullptr) : QAbstractTableModel(parent) {}

    void setItems(const QList<ImageInfo> &list)
    {
        beginResetModel();
        items = list;
        endResetModel();
    }
    const ImageInfo &at(int row) const { return items.at(row); }

    int rowCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : items.size();
    }
    int columnCount(const QModelIndex &parent = QModelIndex()) const override
    {
        return parent.isValid() ? 0 : 8;
    }
    QVariant data(const QModelIndex &index, int role) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role) const override;

private:
    QList<ImageInfo> items;
};

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    MainWindow();

private slots:
    void chooseFolder();
    void scanFinished();
    void readFinished();
    void openImage(const QModelIndex &index);

private:
    ImageModel *model;
    QTableView *table;
    QProgressBar *progress;
    QLabel *statusLabel;
    QPushButton *button;

    QStringList paths;
    QElapsedTimer timer;
    QFutureWatcher<QStringList> scanWatcher;
    QFutureWatcher<ImageInfo> readWatcher;
};
