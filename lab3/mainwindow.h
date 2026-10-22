#pragma once
// Слой интерфейса: только отображение и параметры. Вся математика - в algorithms.cpp.

#include <QFutureWatcher>
#include <QMainWindow>
#include <functional>

#include "pixelbuffer.h"

class QCheckBox;
class QComboBox;
class QDoubleSpinBox;
class QLabel;
class QProgressBar;
class QSpinBox;
class QTabWidget;
class ImageView;
class HistogramWidget;

// Что возвращает фоновая задача
struct TaskResult {
    ImageBuffer image;
    double ms = 0;
};

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void openImage();
    void saveResult();
    void resultToSource();
    void resetToOriginal();
    void applyBlur();
    void applyContrast();
    void onTaskFinished();

private:
    void createUi();
    QWidget* createBlurTab();
    QWidget* createContrastTab();

    bool checkSource();
    void runTask(const std::function<ImageBuffer()>& job, const QString& name);
    void setBusy(bool busy);
    void showSource();
    void showResult();

    // данные
    ImageBuffer m_original; // то, что загрузили из файла
    ImageBuffer m_source;   // текущее исходное (после "результат -> исходное")
    ImageBuffer m_result;
    QString m_taskName;

    QFutureWatcher<TaskResult> m_watcher;

    // виджеты
    QWidget* m_controlsPanel = nullptr;
    QTabWidget* m_tabs = nullptr;
    QCheckBox* m_threadsCheck = nullptr;

    QComboBox* m_filterCombo = nullptr;
    QSpinBox* m_radiusSpin = nullptr;
    QDoubleSpinBox* m_sigmaSpin = nullptr;
    QComboBox* m_edgeCombo = nullptr;
    QCheckBox* m_separableCheck = nullptr;

    QComboBox* m_methodCombo = nullptr;
    QDoubleSpinBox* m_cutSpin = nullptr;
    QDoubleSpinBox* m_clipSpin = nullptr;
    QSpinBox* m_tilesSpin = nullptr;

    ImageView* m_viewBefore = nullptr;
    ImageView* m_viewAfter = nullptr;
    HistogramWidget* m_histBefore = nullptr;
    HistogramWidget* m_histAfter = nullptr;

    QProgressBar* m_progress = nullptr;
    QLabel* m_statusLabel = nullptr;
};
