#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPoint>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void changeEvent(QEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void applySystemTheme();
    void applyBackdrop();
    void updateRgbButton();
    Qt::Edges resizeEdges(const QPoint &position) const;

    Ui::MainWindow *ui;
    bool m_uiReady = false;
    bool m_dark = false;
    bool m_applyingTheme = false;
    bool m_draggingWindow = false;
    QPoint m_dragOffset;
};
#endif // MAINWINDOW_H
