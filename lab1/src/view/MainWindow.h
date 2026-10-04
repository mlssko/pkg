#pragma once

#include <QMainWindow>

#include "../viewmodel/ColorViewModel.h"

class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QLineEdit;
class QLabel;
class QPushButton;
class QComboBox;

// MainWindow - "View" слой. Только внешний вид: слайдеры, поля ввода,
// кнопки. Вся математика находится в ColorMath (Model), вся связующая
// логика - в ColorViewModel (ViewModel), на которые MainWindow лишь
// подписывается
class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    QWidget* buildTopRow();
    QWidget* buildSettingsRow();
    QWidget* buildRgbGroup();
    QWidget* buildCmykGroup();
    QWidget* buildHlsGroup();

    void refreshUI();
    void updateGradients();

    ColorViewModel* m_viewModel;

    // Верхняя панель
    QLabel* m_swatch = nullptr;
    QPushButton* m_paletteButton = nullptr;
    QLineEdit* m_hexEdit = nullptr;

    // Настройки
    QComboBox* m_cmykAlgoCombo = nullptr;
    QComboBox* m_gamutStrategyCombo = nullptr;

    // RGB
    QSlider* m_sliderR = nullptr; QSpinBox* m_spinR = nullptr;
    QSlider* m_sliderG = nullptr; QSpinBox* m_spinG = nullptr;
    QSlider* m_sliderB = nullptr; QSpinBox* m_spinB = nullptr;

    // CMYK (слайдеры хранят промилле: 0..1000 => 0.0..100.0%)
    QSlider* m_sliderC = nullptr; QDoubleSpinBox* m_spinC = nullptr;
    QSlider* m_sliderM = nullptr; QDoubleSpinBox* m_spinM = nullptr;
    QSlider* m_sliderY = nullptr; QDoubleSpinBox* m_spinY = nullptr;
    QSlider* m_sliderK = nullptr; QDoubleSpinBox* m_spinK = nullptr;

    // HLS (H хранится *10 => 0.0..360.0, L/S хранятся *10 => 0.0..100.0%)
    QSlider* m_sliderH = nullptr; QDoubleSpinBox* m_spinH = nullptr;
    QSlider* m_sliderL = nullptr; QDoubleSpinBox* m_spinL = nullptr;
    QSlider* m_sliderS = nullptr; QDoubleSpinBox* m_spinS = nullptr;

    QLabel* m_warningLabel = nullptr;
};