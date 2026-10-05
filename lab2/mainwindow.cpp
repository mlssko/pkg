#include "mainwindow.h"

#include <QDialog>
#include <QDirIterator>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QScreen>
#include <QTableView>
#include <QVBoxLayout>
#include <QtConcurrent>

namespace {

// Поиск файлов изображений в папке и подпапках (выполняется в потоке)
QStringList scanFolder(const QString &dir)
{
    QStringList result;
    QDirIterator it(dir,
                    {"*.jpg", "*.jpeg", "*.gif", "*.tif", "*.tiff", "*.bmp", "*.png", "*.pcx"},
                    QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext())
        result << it.next();
    return result;
}

} // namespace

// ---------------------------------------------------------------- модель

QVariant ImageModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() >= items.size())
        return QVariant();
    const ImageInfo &i = items.at(index.row());

    if (role == Qt::ForegroundRole) {
        if (i.status != "OK")
            return QVariant(QColor(Qt::red));
        if (index.column() == 2 && !i.dpiFromFile)
            return QVariant(QColor(Qt::gray));   // dpi — не из файла
        return QVariant();
    }
    if (role == Qt::ToolTipRole) {
        if (index.column() == 2) {
            if (i.dpiFromFile)
                return QStringLiteral("dpi прочитан из файла");
            return QStringLiteral("В файле dpi не задан — показано значение по умолчанию");
        }
        if (index.column() == 0)
            return i.path;
    }
    if (role != Qt::DisplayRole)
        return QVariant();

    switch (index.column()) {
    case 0: return i.name;
    case 1: return i.width > 0 ? QString("%1 x %2").arg(i.width).arg(i.height) : QString("—");
    case 2: {
        if (i.dpiX <= 0)
            return QString("—");
        QString s = QString("%1 x %2")
                        .arg(i.dpiX, 0, 'f', 0)
                        .arg(i.dpiY, 0, 'f', 0);
        if (!i.dpiFromFile)
            s += QStringLiteral("*");
        return s;
    }
    case 3: return i.depth > 0 ? QString("%1 бит").arg(i.depth) : QString("—");
    case 4: return i.compression.isEmpty() ? QString("—") : i.compression;
    case 5: return i.format.isEmpty() ? QString("—") : i.format;
    case 6: return i.status;
    case 7: return i.extra;
    }
    return QVariant();
}

QVariant ImageModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    static const char *names[] = {"Имя файла", "Размер (px)", "Разрешение (dpi)",
                                  "Глубина цвета", "Сжатие", "Формат", "Статус",
                                  "Дополнительно"};
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole && section < 8)
        return QString(names[section]);
    return QVariant();
}

// ---------------------------------------------------------------- окно

MainWindow::MainWindow()
{
    setWindowTitle("Информация об изображениях");
    resize(1150, 600);

    button = new QPushButton("Выбрать папку...");
    statusLabel = new QLabel("Выберите папку с изображениями");
    progress = new QProgressBar;
    progress->setValue(0);

    model = new ImageModel(this);
    table = new QTableView;
    table->setModel(model);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setAlternatingRowColors(true);
    table->verticalHeader()->setDefaultSectionSize(22);
    table->horizontalHeader()->setStretchLastSection(true);
    const int widths[] = {260, 110, 130, 100, 190, 70, 120};
    for (int c = 0; c < 7; ++c)
        table->setColumnWidth(c, widths[c]);

    auto *top = new QHBoxLayout;
    top->addWidget(button);
    top->addWidget(statusLabel, 1);

    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->addLayout(top);
    layout->addWidget(progress);
    layout->addWidget(table);
    setCentralWidget(central);

    connect(button, &QPushButton::clicked, this, &MainWindow::chooseFolder);
    connect(table, &QTableView::clicked, this, &MainWindow::openImage);
    connect(&scanWatcher, &QFutureWatcher<QStringList>::finished, this, &MainWindow::scanFinished);
    connect(&readWatcher, &QFutureWatcher<ImageInfo>::finished, this, &MainWindow::readFinished);
    connect(&readWatcher, &QFutureWatcherBase::progressRangeChanged, progress, &QProgressBar::setRange);
    connect(&readWatcher, &QFutureWatcherBase::progressValueChanged, progress, &QProgressBar::setValue);
}

void MainWindow::chooseFolder()
{
    QString dir = QFileDialog::getExistingDirectory(this, "Выберите папку с изображениями");
    if (dir.isEmpty())
        return;

    button->setEnabled(false);
    model->setItems({});
    progress->setRange(0, 0); // "бегущая" полоса на время поиска файлов
    statusLabel->setText("Поиск файлов...");
    timer.start();
    scanWatcher.setFuture(QtConcurrent::run(scanFolder, dir));
}

void MainWindow::scanFinished()
{
    paths = scanWatcher.result();
    if (paths.isEmpty()) {
        progress->setRange(0, 1);
        progress->setValue(0);
        statusLabel->setText("Изображения не найдены");
        button->setEnabled(true);
        return;
    }
    statusLabel->setText(QString("Чтение заголовков: %1 файлов...").arg(paths.size()));
    // Многопоточное чтение метаданных, интерфейс не блокируется
    readWatcher.setFuture(QtConcurrent::mapped(paths, readImageInfo));
}

void MainWindow::readFinished()
{
    QList<ImageInfo> list = readWatcher.future().results();
    model->setItems(list);

    int problems = 0;
    for (const ImageInfo &i : list)
        if (i.status != "OK")
            ++problems;

    progress->setValue(progress->maximum());
    statusLabel->setText(QString("Файлов: %1, с проблемами: %2, время: %3 с")
                             .arg(list.size())
                             .arg(problems)
                             .arg(timer.elapsed() / 1000.0, 0, 'f', 2));
    button->setEnabled(true);
}

void MainWindow::openImage(const QModelIndex &index)
{
    const ImageInfo &info = model->at(index.row());
    QPixmap pix(info.path);
    if (pix.isNull()) {
        QMessageBox::warning(this, "Ошибка",
                             "Не удалось показать изображение (файл поврежден или формат не поддерживается для показа).");
        return;
    }
    QSize maxSize = screen()->availableGeometry().size() * 0.8;
    if (pix.width() > maxSize.width() || pix.height() > maxSize.height())
        pix = pix.scaled(maxSize, Qt::KeepAspectRatio, Qt::SmoothTransformation);

    QDialog dlg(this);
    dlg.setWindowTitle(info.name);
    auto *label = new QLabel;
    label->setPixmap(pix);
    auto *layout = new QVBoxLayout(&dlg);
    layout->addWidget(label);
    dlg.exec();
}
