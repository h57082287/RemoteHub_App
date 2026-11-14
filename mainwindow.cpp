#include "mainwindow.h"
#include "glasswindow.h"
#include "ui_mainwindow.h"

#include <QApplication>
#include <QConicalGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QShowEvent>
#include <QStyleHints>
#include <QTimer>
#include <QWindow>

namespace {
QString cssColor(QColor color, int alpha = 255)
{
    return QStringLiteral("rgba(%1,%2,%3,%4)")
        .arg(color.red()).arg(color.green()).arg(color.blue()).arg(alpha);
}

QPixmap controlIcon(const QString &name, const QColor &color)
{
    QPixmap icon(40, 40);
    icon.setDevicePixelRatio(2);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(QPen(color, 1.5, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    if (name == QStringLiteral("close")) {
        painter.drawLine(QPointF(6, 6), QPointF(14, 14));
        painter.drawLine(QPointF(14, 6), QPointF(6, 14));
    } else if (name == QStringLiteral("minimize")) {
        painter.drawLine(QPointF(5, 10), QPointF(15, 10));
    } else {
        painter.drawArc(QRectF(3.5, 3.5, 13, 13), 35 * 16, 285 * 16);
        painter.drawPolyline(QPolygonF{QPointF(13, 2.5), QPointF(16, 5), QPointF(12.5, 6)});
    }
    return icon;
}

QPixmap brandIcon(const QColor &accent)
{
    QPixmap icon(72, 72);
    icon.setDevicePixelRatio(2);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    QColor tint = accent;
    tint.setAlpha(24);
    painter.setPen(Qt::NoPen);
    painter.setBrush(tint);
    painter.drawRoundedRect(QRectF(0, 0, 36, 36), 11, 11);
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(accent, 1.5));
    painter.drawRoundedRect(QRectF(5, 11, 26, 14), 4, 4);
    for (int i = 0; i < 4; ++i)
        painter.drawRoundedRect(QRectF(8 + i * 5.5, 16, 3.5, 4), 0.6, 0.6);
    return icon;
}

QPoint globalMousePosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->globalPosition().toPoint();
#else
    return event->globalPos();
#endif
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);
    setAutoFillBackground(false);
    ui->setupUi(this);
    m_uiReady = true;
    ui->centralwidget->setAutoFillBackground(false);

    // Keep errors outside QOpenGLWidget: it cannot paint explanatory text if
    // its context itself is unavailable. A queued connection avoids hiding a
    // widget while its paintEvent is still on the stack.
    auto *renderError = new QLabel(ui->centralwidget);
    renderError->setObjectName(QStringLiteral("renderErrorLabel"));
    renderError->setAlignment(Qt::AlignCenter);
    renderError->setWordWrap(true);
    renderError->setMinimumHeight(208);
    renderError->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    ui->mainLayout->insertWidget(2, renderError, 1);
    renderError->hide();
    connect(ui->hubView, &HubView::renderingFailed, this, [this, renderError](const QString &message) {
        renderError->setText(message);
        ui->hubView->hide();
        renderError->show();
        ui->rgbButton->setEnabled(false);
        ui->resetViewButton->setEnabled(false);
        ui->gestureLabel->setText(tr("3D 預覽無法啟動"));
    }, Qt::QueuedConnection);

    // Keep the whole title area draggable without intercepting the controls.
    for (QWidget *widget : {static_cast<QWidget *>(this), ui->centralwidget,
                           ui->titleBar, static_cast<QWidget *>(ui->titleLabel),
                           static_cast<QWidget *>(ui->subtitleLabel),
                           static_cast<QWidget *>(ui->appIcon),
                           static_cast<QWidget *>(ui->previewLabel)}) {
        widget->setMouseTracking(true);
        widget->installEventFilter(this);
    }
    for (auto *button : {ui->rgbButton, ui->resetViewButton,
                         ui->minimizeButton, ui->closeButton})
        button->setCursor(Qt::PointingHandCursor);

    connect(ui->closeButton, &QPushButton::clicked, this, &QWidget::close);
    connect(ui->minimizeButton, &QPushButton::clicked, this, &QWidget::showMinimized);
    connect(ui->resetViewButton, &QPushButton::clicked, ui->hubView, &HubView::resetView);
    connect(ui->rgbButton, &QPushButton::toggled, this, [this](bool enabled) {
        ui->hubView->setRgbEnabled(enabled);
        updateRgbButton();
    });
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged, this, [this] {
        // Qt updates the palette after announcing a scheme change.
        QTimer::singleShot(0, this, &MainWindow::applySystemTheme);
    });
#endif
    applySystemTheme();
}

MainWindow::~MainWindow()
{
    m_uiReady = false;
    delete ui;
}

void MainWindow::applySystemTheme()
{
    if (m_applyingTheme)
        return;
    m_applyingTheme = true;
    const QPalette system = QApplication::palette();
    m_dark = system.color(QPalette::Window).lightnessF() < 0.5;
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    const auto scheme = qApp->styleHints()->colorScheme();
    if (scheme != Qt::ColorScheme::Unknown)
        m_dark = scheme == Qt::ColorScheme::Dark;
#endif
    QColor text = system.color(QPalette::WindowText);
    if ((text.lightnessF() > 0.5) != m_dark)
        text = m_dark ? QColor(238, 240, 244) : QColor(28, 31, 37);
    const QColor accent = system.color(QPalette::Highlight);
    const QColor rim = m_dark ? QColor(255, 255, 255) : QColor(30, 40, 55);
    setStyleSheet(QStringLiteral(
        "QWidget#centralwidget, QWidget#titleBar, QWidget#controlBar { background: transparent; }"
        "QLabel { background: transparent; color: %1; }"
        "QLabel#titleLabel { font-size: 18px; font-weight: 600; }"
        "QLabel#subtitleLabel { font-size: 9px; letter-spacing: 2px; color: %2; }"
        "QLabel#gestureLabel { font-size: 11px; color: %2; }"
        "QLabel#previewLabel { font-size: 11px; color: %2; padding: 5px 9px;"
        " border: 1px solid %3; border-radius: 9px; }"
        "QPushButton { color: %1; font-size: 12px; font-weight: 500; padding: 0px 12px;"
        " background: %4; border: 1px solid %3; border-radius: 11px; }"
        "QPushButton:hover { background: %5; border-color: %6; }"
        "QPushButton:pressed { background: %7; }"
        "QPushButton:focus { border: 1px solid %8; }"
        "QPushButton#rgbButton:checked { background: %9; border-color: %6; }"
        "QPushButton#resetViewButton { background: %10; border-color: %6; }"
        "QPushButton#resetViewButton:hover { background: %7; }"
        "QPushButton#minimizeButton, QPushButton#closeButton { padding: 0px;"
        " border: 1px solid transparent; background: transparent; border-radius: 10px; }"
        "QPushButton#minimizeButton:hover { background: %5; }"
        "QPushButton#closeButton:hover { background: rgba(230,75,82,45); }"
        "QPushButton#minimizeButton:focus, QPushButton#closeButton:focus { border-color: %8; }"
        "QToolTip { color: %1; background: %11; border: 1px solid %3; padding: 6px; }")
        .arg(cssColor(text), cssColor(text, m_dark ? 155 : 160), cssColor(rim, 24),
             cssColor(rim, m_dark ? 12 : 6), cssColor(rim, 22), cssColor(accent, 90),
             cssColor(accent, 60), cssColor(accent), cssColor(accent, 23))
        .arg(cssColor(accent, m_dark ? 40 : 26))
        .arg(cssColor(m_dark ? QColor(35, 38, 44) : QColor(249, 250, 252))));
    ui->appIcon->setPixmap(brandIcon(accent));
    setWindowIcon(QIcon(brandIcon(accent)));
    ui->minimizeButton->setIcon(QIcon(controlIcon(QStringLiteral("minimize"), text)));
    ui->closeButton->setIcon(QIcon(controlIcon(QStringLiteral("close"), text)));
    ui->resetViewButton->setIcon(QIcon(controlIcon(QStringLiteral("reset"), text)));
    ui->resetViewButton->setIconSize(QSize(18, 18));
    ui->hubView->setDarkTheme(m_dark);
    updateRgbButton();
    m_applyingTheme = false;
    update();
    if (isVisible())
        applyBackdrop();
}

void MainWindow::updateRgbButton()
{
    QPixmap icon(32, 32);
    icon.setDevicePixelRatio(2);
    icon.fill(Qt::transparent);
    QPainter painter(&icon);
    painter.setRenderHint(QPainter::Antialiasing);
    QConicalGradient spectrum(QPointF(8, 8), 0);
    for (int i = 0; i <= 6; ++i)
        spectrum.setColorAt(i / 6.0, QColor::fromHsv(i * 60 % 360, 180, 245));
    painter.setBrush(Qt::NoBrush);
    if (ui->rgbButton->isChecked())
        painter.setPen(QPen(QBrush(spectrum), 2.7));
    else
        painter.setPen(QPen(QApplication::palette().color(QPalette::Disabled, QPalette::Text), 2.7));
    painter.drawEllipse(QRectF(3, 3, 10, 10));
    painter.end();
    ui->rgbButton->setIcon(QIcon(icon));
    ui->rgbButton->setAccessibleName(ui->rgbButton->isChecked()
        ? tr("RGB 光效，已開啟") : tr("RGB 光效，已關閉"));
}

void MainWindow::paintEvent(QPaintEvent *event)
{
    Q_UNUSED(event)
    QPainter painter(this);
    painter.setRenderHint(QPainter::Antialiasing);
    const QRectF panel = QRectF(rect()).adjusted(10, 10, -10, -10);
    // Soft perimeter shadow stays inside our transparent native window.
    painter.setPen(Qt::NoPen);
    for (int spread = 9; spread > 0; --spread) {
        painter.setBrush(QColor(0, 0, 0, m_dark ? 3 : 2));
        painter.drawRoundedRect(panel.adjusted(-spread, -spread + 2, spread, spread + 2),
                                24 + spread, 24 + spread);
    }
    QPainterPath glass;
    glass.addRoundedRect(panel, 24, 24);
    painter.setClipPath(glass);
    QColor base = QApplication::palette().color(QPalette::Window);
    if ((base.lightnessF() < 0.5) != m_dark)
        base = m_dark ? QColor(28, 30, 36) : QColor(243, 245, 249);
    QLinearGradient material(panel.topLeft(), panel.bottomRight());
    QColor top = base.lighter(m_dark ? 125 : 103);
    QColor bottom = base.darker(m_dark ? 108 : 102);
    top.setAlpha(m_dark ? 206 : 200);
    bottom.setAlpha(m_dark ? 180 : 174);
    material.setColorAt(0, top);
    material.setColorAt(1, bottom);
    painter.fillPath(glass, material);

    QColor accent = QApplication::palette().color(QPalette::Highlight);
    QRadialGradient ambient(QPointF(panel.left() + panel.width() * 0.18, panel.bottom()),
                            panel.width() * 0.64);
    accent.setAlpha(m_dark ? 22 : 15);
    ambient.setColorAt(0, accent);
    accent.setAlpha(0);
    ambient.setColorAt(1, accent);
    painter.fillPath(glass, ambient);

    QLinearGradient sheen(panel.topLeft(), QPointF(panel.left(), panel.bottom()));
    sheen.setColorAt(0, QColor(255, 255, 255, m_dark ? 16 : 64));
    sheen.setColorAt(0.45, QColor(255, 255, 255, 0));
    sheen.setColorAt(1, QColor(255, 255, 255, 4));
    painter.fillPath(glass, sheen);
    painter.setClipping(false);

    QLinearGradient edge(panel.topLeft(), panel.bottomRight());
    edge.setColorAt(0, QColor(255, 255, 255, m_dark ? 76 : 215));
    edge.setColorAt(0.45, QColor(255, 255, 255, m_dark ? 20 : 70));
    edge.setColorAt(1, QColor(255, 255, 255, m_dark ? 40 : 140));
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(QBrush(edge), 1));
    painter.drawRoundedRect(panel.adjusted(0.5, 0.5, -0.5, -0.5), 24, 24);
}

void MainWindow::applyBackdrop()
{
    QPainterPath path;
    path.addRoundedRect(QRectF(rect()).adjusted(10, 10, -10, -10), 24, 24);
    GlassWindow::apply(this, m_dark, QRegion(path.toFillPolygon().toPolygon()));
}

void MainWindow::showEvent(QShowEvent *event)
{
    QMainWindow::showEvent(event);
    applyBackdrop();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    if (isVisible())
        applyBackdrop();
}

void MainWindow::changeEvent(QEvent *event)
{
    QMainWindow::changeEvent(event);
    if (m_uiReady && (event->type() == QEvent::ApplicationPaletteChange
               || event->type() == QEvent::PaletteChange || event->type() == QEvent::ThemeChange))
        applySystemTheme();
}

Qt::Edges MainWindow::resizeEdges(const QPoint &position) const
{
    constexpr int grip = 15;
    Qt::Edges edges;
    if (position.x() < grip) edges |= Qt::LeftEdge;
    if (position.x() >= width() - grip) edges |= Qt::RightEdge;
    if (position.y() < grip) edges |= Qt::TopEdge;
    if (position.y() >= height() - grip) edges |= Qt::BottomEdge;
    return edges;
}

bool MainWindow::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() != QEvent::MouseButtonPress && event->type() != QEvent::MouseMove
        && event->type() != QEvent::MouseButtonRelease)
        return QMainWindow::eventFilter(watched, event);
    auto *mouse = static_cast<QMouseEvent *>(event);
    const QPoint global = globalMousePosition(mouse);
    const QPoint local = mapFromGlobal(global);
    const auto edges = resizeEdges(local);
    if (event->type() == QEvent::MouseButtonPress && mouse->button() == Qt::LeftButton) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
        if (edges && windowHandle() && windowHandle()->startSystemResize(edges))
            return true;
#endif
        const bool title = ui->titleBar->geometry().contains(local);
        if (title && !edges) {
#if QT_VERSION >= QT_VERSION_CHECK(5, 15, 0)
            if (windowHandle() && windowHandle()->startSystemMove())
                return true;
#endif
            m_draggingWindow = true;
            m_dragOffset = global - frameGeometry().topLeft();
            return true;
        }
    } else if (event->type() == QEvent::MouseMove) {
        if (m_draggingWindow && (mouse->buttons() & Qt::LeftButton)) {
            move(global - m_dragOffset);
            return true;
        }
        Qt::CursorShape cursor = Qt::ArrowCursor;
        if (edges == (Qt::TopEdge | Qt::LeftEdge) || edges == (Qt::BottomEdge | Qt::RightEdge))
            cursor = Qt::SizeFDiagCursor;
        else if (edges == (Qt::TopEdge | Qt::RightEdge) || edges == (Qt::BottomEdge | Qt::LeftEdge))
            cursor = Qt::SizeBDiagCursor;
        else if (edges & (Qt::LeftEdge | Qt::RightEdge))
            cursor = Qt::SizeHorCursor;
        else if (edges & (Qt::TopEdge | Qt::BottomEdge))
            cursor = Qt::SizeVerCursor;
        if (auto *widget = qobject_cast<QWidget *>(watched))
            widget->setCursor(cursor);
    } else if (event->type() == QEvent::MouseButtonRelease) {
        m_draggingWindow = false;
    }
    return QMainWindow::eventFilter(watched, event);
}
