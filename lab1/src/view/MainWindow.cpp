#include "MainWindow.h"

#include <QWidget>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QComboBox>
#include <QColorDialog>
#include <QSignalBlocker>
#include <QRegularExpression>
#include <QRegularExpressionValidator>
#include <functional>

namespace {

QSlider* makeSlider(int minV, int maxV)
{
    auto* s = new QSlider(Qt::Horizontal);
    s->setRange(minV, maxV);
    s->setMinimumWidth(160);
    return s;
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    m_viewModel = new ColorViewModel(this);

    auto* central = new QWidget(this);
    auto* mainLayout = new QVBoxLayout(central);

    mainLayout->addWidget(buildTopRow());
    mainLayout->addWidget(buildSettingsRow());

    auto* groupsRow = new QHBoxLayout();
    groupsRow->addWidget(buildRgbGroup());
    groupsRow->addWidget(buildCmykGroup());
    groupsRow->addWidget(buildHlsGroup());
    mainLayout->addLayout(groupsRow);

    m_warningLabel = new QLabel();
    m_warningLabel->setStyleSheet("color:#b45309; font-weight:600; padding:4px;");
    m_warningLabel->setVisible(false);
    mainLayout->addWidget(m_warningLabel);

    mainLayout->addStretch();

    setCentralWidget(central);
    setWindowTitle(tr("Лабораторная работа №1 - Цветовые модели (RGB / CMYK / HLS)"));
    resize(900, 460);

    connect(m_viewModel, &ColorViewModel::colorChanged, this, &MainWindow::refreshUI);
    connect(m_viewModel, &ColorViewModel::warningRaised, this, [this](const QString& msg) {
        m_warningLabel->setText(msg);
        m_warningLabel->setVisible(!msg.isEmpty());
    });

    refreshUI();
}


// Верхняя панель: образец цвета, выбор из палитры, HEX
QWidget* MainWindow::buildTopRow()
{
    auto* box = new QWidget();
    auto* layout = new QHBoxLayout(box);

    m_swatch = new QLabel();
    m_swatch->setFixedSize(64, 64);
    m_swatch->setStyleSheet("background-color:#ff0000; border:1px solid #333; border-radius:6px;");
    layout->addWidget(m_swatch);

    m_paletteButton = new QPushButton(tr("Выбрать из палитры..."));
    layout->addWidget(m_paletteButton);
    connect(m_paletteButton, &QPushButton::clicked, this, [this]() {
        QColor chosen = QColorDialog::getColor(m_viewModel->toQColor(), this, tr("Выбор цвета"));
        if (chosen.isValid()) {
            m_viewModel->setFromQColor(chosen);
        }
    });

    layout->addWidget(new QLabel(tr("HEX:")));
    m_hexEdit = new QLineEdit();
    m_hexEdit->setMaximumWidth(110);
    m_hexEdit->setValidator(new QRegularExpressionValidator(
        QRegularExpression("^#?[0-9A-Fa-f]{6}$"), m_hexEdit));
    layout->addWidget(m_hexEdit);
    connect(m_hexEdit, &QLineEdit::editingFinished, this, [this]() {
        QString text = m_hexEdit->text();
        if (!text.startsWith('#')) text.prepend('#');
        QColor c(text);
        if (c.isValid()) {
            m_viewModel->setFromQColor(c);
        }
    });

    layout->addStretch();
    return box;
}


// Настройки: алгоритм CMYK, стратегия выхода за гамму
QWidget* MainWindow::buildSettingsRow()
{
    auto* box = new QGroupBox(tr("Настройки пересчёта"));
    auto* layout = new QHBoxLayout(box);

    layout->addWidget(new QLabel(tr("Цветоделение CMYK:")));
    m_cmykAlgoCombo = new QComboBox();
    m_cmykAlgoCombo->addItem(tr("GCR - замена серого по всему диапазону"), int(ColorMath::CmykAlgorithm::GCR));
    m_cmykAlgoCombo->addItem(tr("UCR - чёрный только в тенях"), int(ColorMath::CmykAlgorithm::UCR));
    layout->addWidget(m_cmykAlgoCombo);
    connect(m_cmykAlgoCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        auto algo = static_cast<ColorMath::CmykAlgorithm>(m_cmykAlgoCombo->itemData(idx).toInt());
        m_viewModel->setCmykAlgorithm(algo);
    });

    layout->addSpacing(16);
    layout->addWidget(new QLabel(tr("При выходе за гамму:")));
    m_gamutStrategyCombo = new QComboBox();
    m_gamutStrategyCombo->addItem(tr("Clipping - обрезание"), int(ColorMath::GamutStrategy::Clipping));
    m_gamutStrategyCombo->addItem(tr("Scaling - масштабирование"), int(ColorMath::GamutStrategy::Scaling));
    layout->addWidget(m_gamutStrategyCombo);
    connect(m_gamutStrategyCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int idx) {
        auto strat = static_cast<ColorMath::GamutStrategy>(m_gamutStrategyCombo->itemData(idx).toInt());
        m_viewModel->setGamutStrategy(strat);
    });

    layout->addStretch();
    return box;
}


// RGB
QWidget* MainWindow::buildRgbGroup()
{
    auto* box = new QGroupBox(tr("RGB"));
    auto* grid = new QGridLayout(box);

    auto rgbChanged = [this]() {
        m_viewModel->setRgb255(m_spinR->value(), m_spinG->value(), m_spinB->value());
    };

    auto addRow = [&](int row, const QString& label, QSlider*& slider, QSpinBox*& spin) {
        slider = makeSlider(0, 255);
        spin = new QSpinBox();
        spin->setRange(0, 255);
        grid->addWidget(new QLabel(label), row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(spin, row, 2);

        connect(slider, &QSlider::valueChanged, this, [this, spin, rgbChanged](int v) {
            QSignalBlocker b(spin);
            spin->setValue(v);
            rgbChanged();
        });
        connect(spin, QOverload<int>::of(&QSpinBox::valueChanged), this, [this, slider, rgbChanged](int v) {
            QSignalBlocker b(slider);
            slider->setValue(v);
            rgbChanged();
        });
    };

    addRow(0, tr("R"), m_sliderR, m_spinR);
    addRow(1, tr("G"), m_sliderG, m_spinG);
    addRow(2, tr("B"), m_sliderB, m_spinB);

    return box;
}

// CMYK
QWidget* MainWindow::buildCmykGroup()
{
    auto* box = new QGroupBox(tr("CMYK (%)"));
    auto* grid = new QGridLayout(box);

    auto cmykChanged = [this]() {
        m_viewModel->setCmykPercent(m_spinC->value(), m_spinM->value(), m_spinY->value(), m_spinK->value());
    };

    auto addRow = [&](int row, const QString& label, QSlider*& slider, QDoubleSpinBox*& spin) {
        slider = makeSlider(0, 1000); // 0.1% шаг
        spin = new QDoubleSpinBox();
        spin->setRange(0.0, 100.0);
        spin->setDecimals(1);
        spin->setSuffix(" %");
        grid->addWidget(new QLabel(label), row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(spin, row, 2);

        connect(slider, &QSlider::valueChanged, this, [this, spin, cmykChanged](int v) {
            QSignalBlocker b(spin);
            spin->setValue(v / 10.0);
            cmykChanged();
        });
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, slider, cmykChanged](double v) {
            QSignalBlocker b(slider);
            slider->setValue(qRound(v * 10.0));
            cmykChanged();
        });
    };

    addRow(0, tr("C"), m_sliderC, m_spinC);
    addRow(1, tr("M"), m_sliderM, m_spinM);
    addRow(2, tr("Y"), m_sliderY, m_spinY);
    addRow(3, tr("K"), m_sliderK, m_spinK);

    return box;
}

// HLS
QWidget* MainWindow::buildHlsGroup()
{
    auto* box = new QGroupBox(tr("HLS"));
    auto* grid = new QGridLayout(box);

    auto hlsChanged = [this]() {
        m_viewModel->setHls(m_spinH->value(), m_spinL->value(), m_spinS->value());
    };

    m_sliderH = makeSlider(0, 3600);
    m_spinH = new QDoubleSpinBox();
    m_spinH->setRange(0.0, 360.0);
    m_spinH->setDecimals(1);
    m_spinH->setSuffix(tr(" °"));
    grid->addWidget(new QLabel(tr("H")), 0, 0);
    grid->addWidget(m_sliderH, 0, 1);
    grid->addWidget(m_spinH, 0, 2);
    connect(m_sliderH, &QSlider::valueChanged, this, [this, hlsChanged](int v) {
        QSignalBlocker b(m_spinH);
        m_spinH->setValue(v / 10.0);
        hlsChanged();
    });
    connect(m_spinH, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, hlsChanged](double v) {
        QSignalBlocker b(m_sliderH);
        m_sliderH->setValue(qRound(v * 10.0));
        hlsChanged();
    });

    auto addPercentRow = [&](int row, const QString& label, QSlider*& slider, QDoubleSpinBox*& spin) {
        slider = makeSlider(0, 1000);
        spin = new QDoubleSpinBox();
        spin->setRange(0.0, 100.0);
        spin->setDecimals(1);
        spin->setSuffix(" %");
        grid->addWidget(new QLabel(label), row, 0);
        grid->addWidget(slider, row, 1);
        grid->addWidget(spin, row, 2);

        connect(slider, &QSlider::valueChanged, this, [this, spin, hlsChanged](int v) {
            QSignalBlocker b(spin);
            spin->setValue(v / 10.0);
            hlsChanged();
        });
        connect(spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this, slider, hlsChanged](double v) {
            QSignalBlocker b(slider);
            slider->setValue(qRound(v * 10.0));
            hlsChanged();
        });
    };

    addPercentRow(1, tr("L"), m_sliderL, m_spinL);
    addPercentRow(2, tr("S"), m_sliderS, m_spinS);

    return box;
}


// Синхронизация всех виджетов с текущим состоянием ViewModel
void MainWindow::refreshUI()
{
    const auto rgb = m_viewModel->rgb();
    const auto cmyk = m_viewModel->cmyk();
    const auto hls = m_viewModel->hls();

    {
        const QSignalBlocker b1(m_sliderR), b2(m_spinR), b3(m_sliderG), b4(m_spinG), b5(m_sliderB), b6(m_spinB);
        int r = qRound(rgb.r * 255.0), g = qRound(rgb.g * 255.0), bch = qRound(rgb.b * 255.0);
        m_sliderR->setValue(r); m_spinR->setValue(r);
        m_sliderG->setValue(g); m_spinG->setValue(g);
        m_sliderB->setValue(bch); m_spinB->setValue(bch);
    }
    {
        const QSignalBlocker b1(m_sliderC), b2(m_spinC), b3(m_sliderM), b4(m_spinM),
            b5(m_sliderY), b6(m_spinY), b7(m_sliderK), b8(m_spinK);
        m_sliderC->setValue(qRound(cmyk.c * 1000.0)); m_spinC->setValue(cmyk.c * 100.0);
        m_sliderM->setValue(qRound(cmyk.m * 1000.0)); m_spinM->setValue(cmyk.m * 100.0);
        m_sliderY->setValue(qRound(cmyk.y * 1000.0)); m_spinY->setValue(cmyk.y * 100.0);
        m_sliderK->setValue(qRound(cmyk.k * 1000.0)); m_spinK->setValue(cmyk.k * 100.0);
    }
    {
        const QSignalBlocker b1(m_sliderH), b2(m_spinH), b3(m_sliderL), b4(m_spinL), b5(m_sliderS), b6(m_spinS);
        m_sliderH->setValue(qRound(hls.h * 10.0)); m_spinH->setValue(hls.h);
        m_sliderL->setValue(qRound(hls.l * 1000.0)); m_spinL->setValue(hls.l * 100.0);
        m_sliderS->setValue(qRound(hls.s * 1000.0)); m_spinS->setValue(hls.s * 100.0);
    }
    {
        const QSignalBlocker b1(m_hexEdit);
        m_hexEdit->setText(m_viewModel->toQColor().name().toUpper());
    }

    const QColor qc = m_viewModel->toQColor();
    m_swatch->setStyleSheet(QString("background-color:%1; border:1px solid #333; border-radius:6px;").arg(qc.name()));

    updateGradients();
}


// динамический градиент под слайдерами.
// Каждый слайдер показывает, какой получится цвет, если провести его
// собственную ручку по всему диапазону при ТЕКУЩИХ значениях остальных
// компонент той же модели
void MainWindow::updateGradients()
{
    const auto rgb = m_viewModel->rgb();
    const auto cmyk = m_viewModel->cmyk();
    const auto hls = m_viewModel->hls();
    const auto strategy = m_viewModel->gamutStrategy();

    auto gradientCss = [](const std::function<QColor(double)>& colorAt) {
        const int steps = 6;
        QStringList stops;
        for (int i = 0; i <= steps; ++i) {
            double t = double(i) / steps;
            QColor c = colorAt(t);
            stops << QString("stop:%1 %2").arg(t, 0, 'f', 3).arg(c.name());
        }
        return QString("qlineargradient(x1:0, y1:0, x2:1, y2:0, %1)").arg(stops.join(", "));
    };

    auto applyGradient = [](QSlider* slider, const QString& css) {
        slider->setStyleSheet(QString(
                                  "QSlider::groove:horizontal { height:10px; border-radius:5px; background:%1; }"
                                  "QSlider::handle:horizontal { width:16px; margin:-5px 0; background:#ffffff; "
                                  "border:2px solid #333333; border-radius:8px; }").arg(css));
    };

    // RGB
    applyGradient(m_sliderR, gradientCss([&](double t) {
                      return QColor(qRound(t * 255.0), qRound(rgb.g * 255.0), qRound(rgb.b * 255.0));
                  }));
    applyGradient(m_sliderG, gradientCss([&](double t) {
                      return QColor(qRound(rgb.r * 255.0), qRound(t * 255.0), qRound(rgb.b * 255.0));
                  }));
    applyGradient(m_sliderB, gradientCss([&](double t) {
                      return QColor(qRound(rgb.r * 255.0), qRound(rgb.g * 255.0), qRound(t * 255.0));
                  }));

    // CMYK
    auto cmykColorAt = [&](ColorMath::CMYK c) {
        bool outOfGamut = false;
        ColorMath::RGB r = ColorMath::cmykToRgb(c, strategy, outOfGamut);
        return QColor(qRound(ColorMath::clampValue(r.r, 0, 1) * 255.0),
                      qRound(ColorMath::clampValue(r.g, 0, 1) * 255.0),
                      qRound(ColorMath::clampValue(r.b, 0, 1) * 255.0));
    };
    applyGradient(m_sliderC, gradientCss([&](double t) { return cmykColorAt({t, cmyk.m, cmyk.y, cmyk.k}); }));
    applyGradient(m_sliderM, gradientCss([&](double t) { return cmykColorAt({cmyk.c, t, cmyk.y, cmyk.k}); }));
    applyGradient(m_sliderY, gradientCss([&](double t) { return cmykColorAt({cmyk.c, cmyk.m, t, cmyk.k}); }));
    applyGradient(m_sliderK, gradientCss([&](double t) { return cmykColorAt({cmyk.c, cmyk.m, cmyk.y, t}); }));

    // HLS
    auto hlsColorAt = [&](ColorMath::HLS h) {
        bool outOfGamut = false;
        ColorMath::RGB r = ColorMath::hlsToRgb(h, strategy, outOfGamut);
        return QColor(qRound(ColorMath::clampValue(r.r, 0, 1) * 255.0),
                      qRound(ColorMath::clampValue(r.g, 0, 1) * 255.0),
                      qRound(ColorMath::clampValue(r.b, 0, 1) * 255.0));
    };
    applyGradient(m_sliderH, gradientCss([&](double t) { return hlsColorAt({t * 360.0, hls.l, hls.s}); }));
    applyGradient(m_sliderL, gradientCss([&](double t) { return hlsColorAt({hls.h, t, hls.s}); }));
    applyGradient(m_sliderS, gradientCss([&](double t) { return hlsColorAt({hls.h, hls.l, t}); }));
}