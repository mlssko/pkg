#include "mainwindow.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QSpinBox>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QtConcurrent/QtConcurrent>

#include <chrono>

#include "algorithms.h"
#include "imageconvert.h"
#include "parallel.h"
#include "widgets.h"

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Лабораторная работа 3 - обработка изображений");
    createUi();
    connect(&m_watcher, &QFutureWatcher<TaskResult>::finished, this, &MainWindow::onTaskFinished);
}

// ======================================================================
//                              Интерфейс
// ======================================================================

void MainWindow::createUi()
{
    auto* central = new QWidget(this);
    setCentralWidget(central);
    auto* mainLayout = new QHBoxLayout(central);

    // ----- левая панель с настройками -----
    m_controlsPanel = new QWidget;
    m_controlsPanel->setFixedWidth(320);
    auto* left = new QVBoxLayout(m_controlsPanel);

    auto* btnOpen = new QPushButton("Открыть изображение...");
    auto* btnSave = new QPushButton("Сохранить результат...");
    auto* btnUse = new QPushButton("Результат -> исходное");
    auto* btnReset = new QPushButton("Вернуть оригинал");
    connect(btnOpen, &QPushButton::clicked, this, &MainWindow::openImage);
    connect(btnSave, &QPushButton::clicked, this, &MainWindow::saveResult);
    connect(btnUse, &QPushButton::clicked, this, &MainWindow::resultToSource);
    connect(btnReset, &QPushButton::clicked, this, &MainWindow::resetToOriginal);

    m_tabs = new QTabWidget;
    m_tabs->addTab(createBlurTab(), "Сглаживание");
    m_tabs->addTab(createContrastTab(), "Контраст");

    m_threadsCheck = new QCheckBox("Многопоточная обработка");
    m_threadsCheck->setChecked(true);

    left->addWidget(btnOpen);
    left->addWidget(btnSave);
    left->addWidget(btnUse);
    left->addWidget(btnReset);
    left->addWidget(m_tabs);
    left->addWidget(m_threadsCheck);
    left->addStretch();

    // ----- правая часть: до / после и гистограммы -----
    m_viewBefore = new ImageView;
    m_viewAfter = new ImageView;
    m_histBefore = new HistogramWidget;
    m_histAfter = new HistogramWidget;

    auto* right = new QGridLayout;
    right->addWidget(new QLabel("Исходное изображение"), 0, 0);
    right->addWidget(new QLabel("Результат"), 0, 1);
    right->addWidget(m_viewBefore, 1, 0);
    right->addWidget(m_viewAfter, 1, 1);
    right->addWidget(m_histBefore, 2, 0);
    right->addWidget(m_histAfter, 2, 1);
    right->setRowStretch(1, 1);

    mainLayout->addWidget(m_controlsPanel);
    mainLayout->addLayout(right, 1);

    // ----- строка состояния -----
    m_progress = new QProgressBar;
    m_progress->setRange(0, 0); // "бегающий" индикатор, пока идёт обработка
    m_progress->setMaximumWidth(150);
    m_progress->hide();
    m_statusLabel = new QLabel("Откройте изображение");
    statusBar()->addWidget(m_statusLabel, 1);
    statusBar()->addPermanentWidget(m_progress);
}

QWidget* MainWindow::createBlurTab()
{
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);

    m_filterCombo = new QComboBox;
    m_filterCombo->addItem("Усредняющий (box)");
    m_filterCombo->addItem("Гаусса");

    m_radiusSpin = new QSpinBox;
    m_radiusSpin->setRange(1, 50);
    m_radiusSpin->setValue(2);

    m_sigmaSpin = new QDoubleSpinBox;
    m_sigmaSpin->setRange(0.3, 30.0);
    m_sigmaSpin->setSingleStep(0.1);
    m_sigmaSpin->setValue(1.5);
    m_sigmaSpin->setEnabled(false); // нужна только для Гаусса

    m_edgeCombo = new QComboBox;
    m_edgeCombo->addItem("Clamp (дублирование границы)");
    m_edgeCombo->addItem("Reflect (зеркальное отражение)");

    m_separableCheck = new QCheckBox("Разделяемая свёртка (2 прохода)");
    m_separableCheck->setChecked(true);

    auto* btn = new QPushButton("Применить");
    connect(btn, &QPushButton::clicked, this, &MainWindow::applyBlur);
    connect(m_filterCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) { m_sigmaSpin->setEnabled(index == 1); });

    form->addRow("Фильтр:", m_filterCombo);
    form->addRow("Радиус:", m_radiusSpin);
    form->addRow("Sigma:", m_sigmaSpin);
    form->addRow("Края:", m_edgeCombo);
    form->addRow(m_separableCheck);
    form->addRow(btn);
    return page;
}

QWidget* MainWindow::createContrastTab()
{
    auto* page = new QWidget;
    auto* form = new QFormLayout(page);

    m_methodCombo = new QComboBox;
    m_methodCombo->addItem("Линейное контрастирование");
    m_methodCombo->addItem("Эквализация RGB (по каналам)");
    m_methodCombo->addItem("Эквализация HSV (канал V)");
    m_methodCombo->addItem("CLAHE (канал V)");

    m_cutSpin = new QDoubleSpinBox;
    m_cutSpin->setRange(0.0, 20.0);
    m_cutSpin->setSingleStep(0.5);
    m_cutSpin->setValue(0.0);
    m_cutSpin->setSuffix(" %");

    m_clipSpin = new QDoubleSpinBox;
    m_clipSpin->setRange(1.0, 10.0);
    m_clipSpin->setSingleStep(0.5);
    m_clipSpin->setValue(3.0);
    m_clipSpin->setEnabled(false);

    m_tilesSpin = new QSpinBox;
    m_tilesSpin->setRange(2, 32);
    m_tilesSpin->setValue(8);
    m_tilesSpin->setEnabled(false);

    auto* btn = new QPushButton("Применить");
    connect(btn, &QPushButton::clicked, this, &MainWindow::applyContrast);
    connect(m_methodCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                m_cutSpin->setEnabled(index == 0);
                m_clipSpin->setEnabled(index == 3);
                m_tilesSpin->setEnabled(index == 3);
            });

    form->addRow("Метод:", m_methodCombo);
    form->addRow("Отсечение краёв:", m_cutSpin);
    form->addRow("Clip limit:", m_clipSpin);
    form->addRow("Блоков по стороне:", m_tilesSpin);
    form->addRow(btn);
    return page;
}

// ======================================================================
//                         Файлы и отображение
// ======================================================================

void MainWindow::openImage()
{
    QString path = QFileDialog::getOpenFileName(this, "Открыть изображение", QString(),
                                                "Изображения (*.png *.jpg *.jpeg *.bmp *.tif)");
    if (path.isEmpty())
        return;

    QImage img;
    if (!img.load(path)) {
        QMessageBox::warning(this, "Ошибка", "Не удалось загрузить файл");
        return;
    }

    m_original = ImageConvert::fromQImage(img);
    m_source = m_original;
    m_result = ImageBuffer();
    showSource();
    m_viewAfter->setImage(QImage());
    m_histAfter->clear();
    m_statusLabel->setText(QString("Загружено: %1 x %2").arg(img.width()).arg(img.height()));
}

void MainWindow::saveResult()
{
    if (m_result.isEmpty()) {
        QMessageBox::information(this, "Нет результата", "Сначала примените какой-нибудь метод.");
        return;
    }
    QString path = QFileDialog::getSaveFileName(this, "Сохранить результат", "result.png",
                                                "PNG (*.png);;JPEG (*.jpg);;BMP (*.bmp)");
    if (path.isEmpty())
        return;
    if (!ImageConvert::toQImage(m_result).save(path))
        QMessageBox::warning(this, "Ошибка", "Не удалось сохранить файл");
}

void MainWindow::resultToSource()
{
    if (m_result.isEmpty())
        return;
    m_source = m_result;
    showSource();
}

void MainWindow::resetToOriginal()
{
    if (m_original.isEmpty())
        return;
    m_source = m_original;
    m_result = ImageBuffer();
    showSource();
    m_viewAfter->setImage(QImage());
    m_histAfter->clear();
}

void MainWindow::showSource()
{
    m_viewBefore->setImage(ImageConvert::toQImage(m_source));
    m_histBefore->setHistogram(Algo::computeHistogram(m_source));
}

void MainWindow::showResult()
{
    m_viewAfter->setImage(ImageConvert::toQImage(m_result));
    m_histAfter->setHistogram(Algo::computeHistogram(m_result));
}

// ======================================================================
//                        Запуск обработки
// ======================================================================

bool MainWindow::checkSource()
{
    if (m_source.isEmpty()) {
        QMessageBox::information(this, "Нет изображения", "Сначала откройте изображение.");
        return false;
    }
    return true;
}

void MainWindow::applyBlur()
{
    if (!checkSource())
        return;

    bool gauss = m_filterCombo->currentIndex() == 1;
    int radius = m_radiusSpin->value();
    double sigma = m_sigmaSpin->value();
    EdgeMode mode = (m_edgeCombo->currentIndex() == 0) ? EdgeMode::Clamp : EdgeMode::Reflect;
    bool separable = m_separableCheck->isChecked();
    ImageBuffer src = m_source;

    QString name = QString("%1, r=%2, %3, %4")
                       .arg(gauss ? "Гаусс" : "Box")
                       .arg(radius)
                       .arg(m_edgeCombo->currentIndex() == 0 ? "Clamp" : "Reflect")
                       .arg(separable ? "2 прохода" : "полная 2D свёртка");

    runTask(
        [=]() {
            std::vector<float> kernel = gauss ? Algo::makeGaussKernel(radius, sigma)
                                              : Algo::makeBoxKernel(radius);
            return separable ? Algo::blurSeparable(src, kernel, mode)
                             : Algo::blurFull2D(src, kernel, mode);
        },
        name);
}

void MainWindow::applyContrast()
{
    if (!checkSource())
        return;

    int method = m_methodCombo->currentIndex();
    double cut = m_cutSpin->value();
    double clip = m_clipSpin->value();
    int tiles = m_tilesSpin->value();
    ImageBuffer src = m_source;

    runTask(
        [=]() {
            switch (method) {
            case 0: return Algo::linearContrast(src, cut);
            case 1: return Algo::equalizeRGB(src);
            case 2: return Algo::equalizeHSV(src);
            default: return Algo::clahe(src, tiles, clip);
            }
        },
        m_methodCombo->currentText());
}

void MainWindow::runTask(const std::function<ImageBuffer()>& job, const QString& name)
{
    m_taskName = name;
    Parallel::setThreadCount(m_threadsCheck->isChecked() ? Parallel::maxThreads() : 1);
    setBusy(true);

    // обработка идёт в отдельном потоке, чтобы окно не зависало
    m_watcher.setFuture(QtConcurrent::run([job]() {
        auto t0 = std::chrono::steady_clock::now();
        TaskResult r;
        r.image = job();
        auto t1 = std::chrono::steady_clock::now();
        r.ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        return r;
    }));
}

void MainWindow::onTaskFinished()
{
    TaskResult r = m_watcher.result();
    m_result = std::move(r.image);
    setBusy(false);
    showResult();

    m_statusLabel->setText(QString("%1: %2 мс, потоков: %3, размер %4 x %5")
                               .arg(m_taskName)
                               .arg(r.ms, 0, 'f', 1)
                               .arg(Parallel::threadCount())
                               .arg(m_result.width())
                               .arg(m_result.height()));
}

void MainWindow::setBusy(bool busy)
{
    m_controlsPanel->setEnabled(!busy);
    m_progress->setVisible(busy);
    if (busy)
        m_statusLabel->setText("Обработка...");
}
