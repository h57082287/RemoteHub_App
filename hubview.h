#ifndef HUBVIEW_H
#define HUBVIEW_H

#include <QElapsedTimer>
#include <QOpenGLBuffer>
#include <QOpenGLFunctions>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLWidget>
#include <QQuaternion>
#include <QTimer>
#include <QVector3D>
#include <memory>
#include <vector>

class QOpenGLShaderProgram;

// A real, depth-tested model. No raster image or external model is required.
class HubView : public QOpenGLWidget, protected QOpenGLFunctions
{
    Q_OBJECT
public:
    explicit HubView(QWidget *parent = nullptr);
    ~HubView() override;

    QQuaternion orientation() const { return m_orientation; }
    bool rgbEnabled() const { return m_rgbEnabled; }
    bool darkTheme() const { return m_darkTheme; }
    QString renderError() const { return m_renderError; }
    QSize sizeHint() const override;

public slots:
    void resetView();
    void setRgbEnabled(bool enabled);
    void setDarkTheme(bool dark);

signals:
    void interactionStarted();
    void renderingFailed(const QString &message);

protected:
    void initializeGL() override;
    void resizeGL(int width, int height) override;
    void paintGL() override;
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;

private:
    struct Vertex {
        float position[3];
        float normal[3];
        float color[3];
        float material;  // 0 = metal, 1 = plastic, 2 = RGB, 3 = glow
        float phase;
        float alpha;
    };
    void buildModel();
    void releaseGL();
    void updateAnimation();
    void updateTimer();
    QVector3D arcballPoint(const QPointF &point) const;
    void triangle(const QVector3D &a, const QVector3D &b, const QVector3D &c,
                  const QVector3D &normal, const QVector3D &color, float material = 0.0f);
    void quad(const QVector3D &a, const QVector3D &b, const QVector3D &c,
              const QVector3D &d, const QVector3D &normal, const QVector3D &color,
              float material = 0.0f);
    void box(const QVector3D &center, const QVector3D &halfSize,
             const QVector3D &color, float material = 0.0f);
    void faceRect(float left, float bottom, float right, float top, float z,
                  const QVector3D &color, float material = 0.0f);

    std::unique_ptr<QOpenGLShaderProgram> m_program;
    QOpenGLBuffer m_buffer{QOpenGLBuffer::VertexBuffer};
    QOpenGLVertexArrayObject m_vao;
    std::vector<Vertex> m_vertices;
    int m_opaqueVertexCount = 0;
    QQuaternion m_orientation;
    QQuaternion m_dragOrientation;
    QQuaternion m_resetFrom;
    QVector3D m_dragStart;
    QTimer m_timer;
    QElapsedTimer m_clock;
    QElapsedTimer m_resetClock;
    bool m_dragging = false;
    bool m_resetting = false;
    bool m_rgbEnabled = true;
    bool m_darkTheme = false;
    bool m_errorReported = false;
    float m_zoom = 1.0f;
    QString m_renderError;
};

#endif // HUBVIEW_H
