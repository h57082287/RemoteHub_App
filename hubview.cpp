#include "hubview.h"

#include <QDebug>
#include <QHideEvent>
#include <QKeyEvent>
#include <QMatrix4x4>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLShaderProgram>
#include <QPainter>
#include <QShowEvent>
#include <QSurfaceFormat>
#include <QWheelEvent>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>

namespace {
constexpr float Pi = 3.14159265358979323846f;
constexpr int CornerSteps = 16;
constexpr int ResetDuration = 420;
using Loop = std::vector<QVector3D>;

Loop roundedLoop(float halfWidth, float halfHeight, float radius, float z)
{
    Loop points;
    points.reserve(4 * (CornerSteps + 1));
    for (int corner = 0; corner < 4; ++corner) {
        const float cx = (corner == 0 || corner == 3 ? 1.0f : -1.0f)
                       * (halfWidth - radius);
        const float cy = (corner < 2 ? 1.0f : -1.0f) * (halfHeight - radius);
        for (int step = 0; step <= CornerSteps; ++step) {
            const float angle = (corner * .5f + float(step) / CornerSteps * .5f) * Pi;
            points.emplace_back(cx + std::cos(angle) * radius,
                                cy + std::sin(angle) * radius, z);
        }
    }
    return points;
}

QPointF mousePosition(const QMouseEvent *event)
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 0, 0)
    return event->position();
#else
    return event->localPos();
#endif
}
}

HubView::HubView(QWidget *parent)
    : QOpenGLWidget(parent),
      m_orientation(QQuaternion::fromEulerAngles(17.0f, -13.0f, -2.0f))
{
    QSurfaceFormat surfaceFormat;
    surfaceFormat.setRenderableType(QSurfaceFormat::OpenGL);
    surfaceFormat.setVersion(3, 3);
    surfaceFormat.setProfile(QSurfaceFormat::CoreProfile);
    surfaceFormat.setAlphaBufferSize(8);
    surfaceFormat.setDepthBufferSize(24);
    surfaceFormat.setSamples(4);
    setFormat(surfaceFormat);
    setAttribute(Qt::WA_AlwaysStackOnTop);
    setAttribute(Qt::WA_TranslucentBackground);
    setAttribute(Qt::WA_NoSystemBackground, false);
    setAutoFillBackground(false);
    setFocusPolicy(Qt::StrongFocus);
    setCursor(Qt::OpenHandCursor);
    setMinimumSize(360, 150);
    setAccessibleName(tr("可旋轉的四埠 USB Hub"));
    setAccessibleDescription(tr("滑鼠拖曳或方向鍵旋轉。Home 鍵或連按兩下回到正視圖。"));
    setToolTip(tr("拖曳旋轉 · 滾輪縮放 · 連按兩下回正"));
    m_timer.setInterval(16);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &HubView::updateAnimation);
    m_clock.start();
}

HubView::~HubView()
{
    if (context()) {
        makeCurrent();
        releaseGL();
        disconnect(context(), nullptr, this, nullptr);
        doneCurrent();
    }
}

QSize HubView::sizeHint() const
{
    return QSize(700, 230);
}

void HubView::initializeGL()
{
    initializeOpenGLFunctions();
    connect(context(), &QOpenGLContext::aboutToBeDestroyed, this, [this] {
        makeCurrent();
        releaseGL();
        doneCurrent();
    }, Qt::DirectConnection);
    m_renderError.clear();
    m_errorReported = false;
    m_program = std::make_unique<QOpenGLShaderProgram>();
    const QByteArray prefix = context()->isOpenGLES()
        ? QByteArray("#version 300 es\nprecision highp float;\n")
        : QByteArray("#version 330 core\n");
    const QByteArray vertexShader = prefix + R"GLSL(
in vec3 aPosition;
in vec3 aNormal;
in vec3 aColor;
in float aMaterial;
in float aPhase;
in float aAlpha;
uniform mat4 uMvp;
uniform mat4 uModel;
out vec3 vPosition;
out vec3 vLocal;
out vec3 vNormal;
out vec3 vColor;
out float vMaterial;
out float vPhase;
out float vAlpha;
void main() {
    vLocal = aPosition;
    vPosition = (uModel * vec4(aPosition, 1.0)).xyz;
    vNormal = mat3(uModel) * aNormal;
    vColor = aColor;
    vMaterial = aMaterial;
    vPhase = aPhase;
    vAlpha = aAlpha;
    gl_Position = uMvp * vec4(aPosition, 1.0);
}
)GLSL";
    const QByteArray fragmentShader = prefix + R"GLSL(
in vec3 vPosition;
in vec3 vLocal;
in vec3 vNormal;
in vec3 vColor;
in float vMaterial;
in float vPhase;
in float vAlpha;
uniform float uTime;
uniform float uCameraDistance;
uniform float uDark;
uniform float uRgb;
out vec4 fragColor;
vec3 rainbow(float h) {
    vec3 p = abs(fract(vec3(h) + vec3(0.0, 0.666667, 0.333333)) * 6.0 - 3.0);
    return mix(vec3(1.0), clamp(p - 1.0, 0.0, 1.0), 0.82);
}
void main() {
    vec3 n = normalize(vNormal);
    vec3 viewDir = normalize(vec3(0.0, 0.0, uCameraDistance) - vPosition);
    if (vMaterial > 1.5 && vMaterial < 3.5) {
        // The diffuser is part of the model, so it obeys depth and orientation.
        if (vMaterial > 2.5 && uRgb < 0.5) discard;
        vec3 rgb = mix(vec3(0.16, 0.18, 0.20), rainbow(vPhase + uTime * 0.065), uRgb);
        float alpha = vAlpha;
        fragColor = vec4(rgb, alpha);
        return;
    }
    vec3 base = vColor;
    bool body = vMaterial > 3.5;
    bool plastic = vMaterial > 0.5 && vMaterial < 1.5;
    if (body) base *= mix(1.0, 0.38, uDark);
    vec3 key = normalize(vec3(-0.42, 0.82, 1.0));
    vec3 fill = normalize(vec3(0.85, 0.15, 0.30));
    float diffuse = max(dot(n, key), 0.0);
    float secondary = max(dot(n, fill), 0.0);
    float specular = pow(max(dot(n, normalize(key + viewDir)), 0.0), plastic ? 38.0 : 95.0);
    vec3 reflection = reflect(-viewDir, n);
    float strip = pow(max(0.0, 1.0 - abs(reflection.y - 0.55) * 1.6), 10.0);
    float rim = pow(1.0 - max(dot(n, viewDir), 0.0), 3.0);
    float brushing = sin(vLocal.y * 1900.0 + sin(vLocal.x * 11.0) * 1.2) * 0.008;
    vec3 color = base * (0.39 + diffuse * 0.63 + secondary * 0.13);
    color += vec3(specular * (plastic ? 0.16 : 0.68));
    if (!plastic) {
        color += vec3(strip * 0.22 + rim * 0.10 + brushing);
        color += vec3(0.035, 0.05, 0.08) * secondary;
    }
    // A little RGB bounce on the front chamfer joins the light to the housing.
    if (body && vLocal.z > 0.65)
        color += rainbow(vLocal.x * 0.09 + uTime * 0.065) * uRgb * 0.055;
    fragColor = vec4(clamp(color, 0.0, 1.0), 1.0);
}
)GLSL";

    if (!m_program->addShaderFromSourceCode(QOpenGLShader::Vertex, vertexShader)
        || !m_program->addShaderFromSourceCode(QOpenGLShader::Fragment, fragmentShader)
        || !m_program->link()) {
        m_renderError = tr("無法建立 3D 預覽：OpenGL 著色器初始化失敗。");
        qWarning().noquote() << m_renderError << m_program->log();
        m_program.reset();
        return;
    }
    buildModel();
    if (!m_vao.create() || !m_buffer.create()) {
        m_renderError = tr("無法建立 3D 預覽：顯示卡不支援所需的 OpenGL 緩衝區。");
        m_program.reset();
        return;
    }
    QOpenGLVertexArrayObject::Binder binder(&m_vao);
    m_buffer.bind();
    m_buffer.setUsagePattern(QOpenGLBuffer::StaticDraw);
    m_buffer.allocate(m_vertices.data(), int(m_vertices.size() * sizeof(Vertex)));
    m_program->bind();
    const auto attribute = [this](const char *name, int count, std::size_t offset) {
        const int location = m_program->attributeLocation(name);
        m_program->enableAttributeArray(location);
        m_program->setAttributeBuffer(location, GL_FLOAT, int(offset), count, sizeof(Vertex));
    };
    attribute("aPosition", 3, offsetof(Vertex, position));
    attribute("aNormal", 3, offsetof(Vertex, normal));
    attribute("aColor", 3, offsetof(Vertex, color));
    attribute("aMaterial", 1, offsetof(Vertex, material));
    attribute("aPhase", 1, offsetof(Vertex, phase));
    attribute("aAlpha", 1, offsetof(Vertex, alpha));
    m_program->release();
    m_buffer.release();
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE); // Socket interiors and the light diffuser have two sides.
    glClearColor(0, 0, 0, 0);
    updateTimer();
}

void HubView::releaseGL()
{
    m_vao.destroy();
    m_buffer.destroy();
    m_program.reset();
}

void HubView::resizeGL(int, int)
{
}

void HubView::paintGL()
{
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    if (!m_program)
        return;
    const float aspect = float(width()) / std::max(1, height());
    // Fit the rotated bounds, not a bounding sphere: the wide front view fills
    // the compact window, while a vertical orbit still stays entirely visible.
    float extentX = 0, extentY = 0, extentZ = 0;
    for (int x : {-1, 1}) for (int y : {-1, 1}) for (int z : {-1, 1}) {
        const QVector3D point = m_orientation.rotatedVector(QVector3D(x * 3.25f, y * .66f, z * .91f));
        extentX = std::max(extentX, std::abs(point.x()));
        extentY = std::max(extentY, std::abs(point.y()));
        extentZ = std::max(extentZ, std::abs(point.z()));
    }
    constexpr float fieldOfView = 30.0f;
    const float tangent = std::tan(fieldOfView * Pi / 360.0f);
    const float distance = extentZ + 1.08f / m_zoom
        * std::max(extentY / tangent, extentX / (tangent * aspect));
    QMatrix4x4 model;
    model.rotate(m_orientation);
    QMatrix4x4 view;
    view.translate(0, 0, -distance);
    QMatrix4x4 projection;
    projection.perspective(fieldOfView, aspect, .05f, 100.0f);
    m_program->bind();
    m_program->setUniformValue("uMvp", projection * view * model);
    m_program->setUniformValue("uModel", model);
    m_program->setUniformValue("uCameraDistance", distance);
    m_program->setUniformValue("uTime", float(m_clock.elapsed() % 600000) / 1000.0f);
    m_program->setUniformValue("uDark", m_darkTheme ? 1.0f : 0.0f);
    m_program->setUniformValue("uRgb", m_rgbEnabled ? 1.0f : 0.0f);
    QOpenGLVertexArrayObject::Binder binder(&m_vao);
    glEnable(GL_DEPTH_TEST);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    glDrawArrays(GL_TRIANGLES, 0, m_opaqueVertexCount);
    glEnable(GL_BLEND);
    glBlendFuncSeparate(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA, GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE);
    glDrawArrays(GL_TRIANGLES, m_opaqueVertexCount,
                 int(m_vertices.size()) - m_opaqueVertexCount);
    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
    m_program->release();
}

void HubView::paintEvent(QPaintEvent *event)
{
    QOpenGLWidget::paintEvent(event);
    if (!isValid() || !m_renderError.isEmpty()) {
        m_timer.stop();
        if (m_renderError.isEmpty()) {
            m_renderError = tr("無法啟動 3D 預覽，請確認顯示卡支援 OpenGL 3.3。");
        }
        // A failed GL context cannot reliably paint its own fallback. Notify
        // the surrounding normal QWidget UI so it can show the same message.
        if (!m_errorReported) {
            m_errorReported = true;
            emit renderingFailed(m_renderError);
        }
        if (!isValid()) return;
        QPainter painter(this);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.setPen(palette().color(QPalette::WindowText));
        const QString message = m_renderError.isEmpty()
            ? tr("無法啟動 3D 預覽\n請使用支援 OpenGL 3.3 的顯示卡與驅動程式。")
            : m_renderError;
        painter.drawText(rect().adjusted(24, 12, -24, -12),
                         Qt::AlignCenter | Qt::TextWordWrap, message);
    }
}

void HubView::triangle(const QVector3D &a, const QVector3D &b, const QVector3D &c,
                       const QVector3D &normal, const QVector3D &color, float material)
{
    for (const QVector3D &point : {a, b, c}) {
        m_vertices.push_back({{point.x(), point.y(), point.z()},
                              {normal.x(), normal.y(), normal.z()},
                              {color.x(), color.y(), color.z()}, material, 0, 1});
    }
}

void HubView::quad(const QVector3D &a, const QVector3D &b, const QVector3D &c,
                   const QVector3D &d, const QVector3D &normal,
                   const QVector3D &color, float material)
{
    triangle(a, b, c, normal, color, material);
    triangle(a, c, d, normal, color, material);
}

void HubView::faceRect(float left, float bottom, float right, float top, float z,
                       const QVector3D &color, float material)
{
    quad({left, bottom, z}, {right, bottom, z}, {right, top, z}, {left, top, z},
         {0, 0, 1}, color, material);
}

void HubView::box(const QVector3D &center, const QVector3D &halfSize,
                  const QVector3D &color, float material)
{
    const float l = center.x() - halfSize.x(), r = center.x() + halfSize.x();
    const float b = center.y() - halfSize.y(), t = center.y() + halfSize.y();
    const float back = center.z() - halfSize.z(), front = center.z() + halfSize.z();
    faceRect(l, b, r, t, front, color, material);
    quad({r, b, back}, {l, b, back}, {l, t, back}, {r, t, back}, {0, 0, -1}, color, material);
    quad({l, t, front}, {r, t, front}, {r, t, back}, {l, t, back}, {0, 1, 0}, color, material);
    quad({l, b, back}, {r, b, back}, {r, b, front}, {l, b, front}, {0, -1, 0}, color, material);
    quad({r, b, front}, {r, b, back}, {r, t, back}, {r, t, front}, {1, 0, 0}, color, material);
    quad({l, b, back}, {l, b, front}, {l, t, front}, {l, t, back}, {-1, 0, 0}, color, material);
}

void HubView::buildModel()
{
    m_vertices.clear();
    const QVector3D aluminum(.56f, .59f, .64f);
    const QVector3D panel(.105f, .12f, .145f);
    const QVector3D steel(.59f, .62f, .67f);
    const QVector3D inside(.023f, .03f, .043f);
    const QVector3D blue(.025f, .23f, .57f);
    const QVector3D gold(.78f, .56f, .22f);

    // Rounded, extruded aluminum housing, with curved front and rear chamfers.
    struct Ring { float width; float height; float radius; float z; float nz; };
    const std::array<Ring, 8> rings{{
        {3.08f, .49f, .12f, -.86f, -1.0f},
        {3.15f, .56f, .19f, -.825f, -.65f},
        {3.19f, .60f, .23f, -.76f, -.28f},
        {3.20f, .61f, .24f, -.68f, 0.0f},
        {3.20f, .61f, .24f, .68f, 0.0f},
        {3.19f, .60f, .23f, .76f, .28f},
        {3.15f, .56f, .19f, .825f, .65f},
        {3.08f, .49f, .12f, .86f, 1.0f}
    }};
    for (std::size_t ringIndex = 0; ringIndex + 1 < rings.size(); ++ringIndex) {
        const Ring &a = rings[ringIndex], &b = rings[ringIndex + 1];
        const Loop first = roundedLoop(a.width, a.height, a.radius, a.z);
        const Loop second = roundedLoop(b.width, b.height, b.radius, b.z);
        const auto vertex = [&](const QVector3D &p, int index, const Ring &ring) {
            const int corner = index / (CornerSteps + 1);
            const int step = index % (CornerSteps + 1);
            const float angle = (corner * .5f + float(step) / CornerSteps * .5f) * Pi;
            const float radial = std::sqrt(std::max(0.0f, 1.0f - ring.nz * ring.nz));
            const QVector3D normal(std::cos(angle) * radial, std::sin(angle) * radial, ring.nz);
            m_vertices.push_back({{p.x(), p.y(), p.z()}, {normal.x(), normal.y(), normal.z()},
                                  {aluminum.x(), aluminum.y(), aluminum.z()}, 4, 0, 1});
        };
        for (int i = 0; i < int(first.size()); ++i) {
            const int next = (i + 1) % int(first.size());
            vertex(first[i], i, a); vertex(second[i], i, b); vertex(second[next], next, b);
            vertex(first[i], i, a); vertex(second[next], next, b); vertex(first[next], next, a);
        }
    }

    // The front has real openings. A solid face behind the USB sockets would
    // incorrectly hide their depth when the model is viewed at an angle.
    const Loop panelOuter = roundedLoop(3.055f, .465f, .115f, .863f);
    const Loop panelInner = roundedLoop(2.90f, .205f, 0.0f, .863f);
    for (int i = 0; i < int(panelOuter.size()); ++i) {
        const int j = (i + 1) % int(panelOuter.size());
        quad(panelOuter[i], panelOuter[j], panelInner[j], panelInner[i], {0, 0, 1}, panel, 1);
    }
    const std::array<float, 4> centers{{-2.22f, -.74f, .74f, 2.22f}};
    float left = -2.90f;
    for (float center : centers) {
        faceRect(left, -.205f, center - .52f, .205f, .863f, panel, 1);
        left = center + .52f;
    }
    faceRect(left, -.205f, 2.90f, .205f, .863f, panel, 1);

    for (int port = 0; port < 4; ++port) {
        const float x = centers[port];
        // Brushed stainless shell, dark recessed cavity and four spring contacts.
        box({x, 0, .55f}, {.521f, .206f, .065f}, inside, 1);
        box({x, .181f, .748f}, {.52f, .024f, .123f}, steel);
        box({x, -.181f, .748f}, {.52f, .024f, .123f}, steel * .80f);
        box({x - .495f, 0, .748f}, {.025f, .157f, .123f}, steel * .88f);
        box({x + .495f, 0, .748f}, {.025f, .157f, .123f}, steel * .88f);
        box({x, -.068f, .724f}, {.435f, .045f, .106f}, blue, 1);
        // The front bevel of the blue insulator catches a narrow highlight.
        faceRect(x - .435f, -.027f, x + .435f, -.013f, .831f,
                 QVector3D(.06f, .37f, .72f), 1);
        for (int contact = 0; contact < 4; ++contact) {
            const float cx = x + (contact - 1.5f) * .18f;
            box({cx, -.020f, .750f}, {.031f, .006f, .078f}, gold);
            faceRect(cx - .031f, -.030f, cx + .031f, -.012f, .833f, gold);
        }
        for (float side : {-1.0f, 1.0f}) {
            box({x + side * .285f, .146f, .735f}, {.055f, .011f, .077f}, steel * .62f);
            faceRect(x + side * .285f - .052f, .168f,
                     x + side * .285f + .052f, .193f, .872f, inside, 1);
        }
        // Small engraved port numbers, formed from geometry rather than a font texture.
        const std::array<unsigned, 4> digits{{0x06, 0x5b, 0x4f, 0x66}};
        const std::array<QVector3D, 7> segments{{
            {0, .055f, 0}, {.034f, .028f, 1}, {.034f, -.028f, 1},
            {0, -.055f, 0}, {-.034f, -.028f, 1}, {-.034f, .028f, 1}, {0, 0, 0}
        }};
        for (int segment = 0; segment < 7; ++segment) {
            if (!(digits[port] & (1u << segment))) continue;
            const QVector3D s = segments[segment];
            const float halfX = s.z() > .5f ? .004f : .030f;
            const float halfY = s.z() > .5f ? .023f : .004f;
            faceRect(x + s.x() - halfX, .330f + s.y() - halfY,
                     x + s.x() + halfX, .330f + s.y() + halfY,
                     .865f, QVector3D(.48f, .51f, .56f), 1);
        }
    }

    // Rear enclosure with inset seam and mechanical fasteners.
    const Loop rear = roundedLoop(3.075f, .485f, .12f, -.861f);
    for (int i = 0; i < int(rear.size()); ++i)
        triangle({0, 0, -.861f}, rear[(i + 1) % rear.size()], rear[i], {0, 0, -1}, aluminum, 4);
    for (float x : {-2.88f, 2.88f}) for (float y : {-.30f, .30f}) {
        for (int segment = 0; segment < 24; ++segment) {
            const float a = segment * 2 * Pi / 24;
            const float b = (segment + 1) * 2 * Pi / 24;
            triangle({x, y, -.867f}, {x + .045f * std::cos(a), y + .045f * std::sin(a), -.867f},
                     {x + .045f * std::cos(b), y + .045f * std::sin(b), -.867f},
                     {0, 0, -1}, QVector3D(.16f, .18f, .20f), 1);
        }
        quad({x - .023f, y - .003f, -.869f}, {x + .023f, y - .003f, -.869f},
             {x + .023f, y + .003f, -.869f}, {x - .023f, y + .003f, -.869f},
             {0, 0, -1}, QVector3D(.04f, .05f, .06f), 1);
    }
    // Rubber feet are visible when orbiting underneath.
    for (float x : {-2.45f, 2.45f}) for (float z : {-.43f, .43f})
        box({x, -.615f, z}, {.32f, .027f, .15f}, QVector3D(.055f, .06f, .07f), 1);

    // RGB is a single continuous perimeter around all four ports.
    const auto lightBand = [this](float outerW, float outerH, float outerR,
                                  float innerW, float innerH, float innerR,
                                  float z, float material, float alpha) {
        const Loop outer = roundedLoop(outerW, outerH, outerR, z);
        const Loop inner = roundedLoop(innerW, innerH, innerR, z);
        const auto append = [this, material, alpha](const QVector3D &p, float phase) {
            m_vertices.push_back({{p.x(), p.y(), p.z()}, {0, 0, 1}, {1, 1, 1}, material, phase, alpha});
        };
        // Arc length gives smooth color travel across long edges and curved corners.
        std::vector<float> lengths(outer.size() + 1, 0);
        for (int i = 0; i < int(outer.size()); ++i)
            lengths[i + 1] = lengths[i] + (outer[(i + 1) % outer.size()] - outer[i]).length();
        for (int i = 0; i < int(outer.size()); ++i) {
            const int j = (i + 1) % int(outer.size());
            const float phaseA = lengths[i] / lengths.back();
            const float phaseB = lengths[i + 1] / lengths.back();
            append(outer[i], phaseA); append(outer[j], phaseB); append(inner[j], phaseB);
            append(outer[i], phaseA); append(inner[j], phaseB); append(inner[i], phaseA);
        }
    };
    lightBand(3.095f, .505f, .145f, 3.052f, .462f, .102f, .873f, 2, 1);
    m_opaqueVertexCount = int(m_vertices.size());
    // Thin translucent halos keep the glass viewport clear, with depth testing
    // ensuring the front light does not shine through the back of the housing.
    lightBand(3.14f, .55f, .19f, 3.015f, .425f, .065f, .874f, 3, .055f);
    lightBand(3.12f, .53f, .17f, 3.035f, .445f, .085f, .875f, 3, .10f);
}

QVector3D HubView::arcballPoint(const QPointF &point) const
{
    const float radius = std::max(1.0f, float(std::min(width(), height())) * .80f);
    QVector3D v(float(point.x() - width() * .5) / radius,
                float(height() * .5 - point.y()) / radius, 0);
    const float lengthSquared = v.lengthSquared();
    v.setZ(lengthSquared <= .5f ? std::sqrt(1.0f - lengthSquared)
                                : .5f / std::sqrt(lengthSquared));
    return v.normalized();
}

void HubView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_resetting = false;
        m_dragging = true;
        m_dragOrientation = m_orientation;
        m_dragStart = arcballPoint(mousePosition(event));
        setFocus(Qt::MouseFocusReason);
        setCursor(Qt::ClosedHandCursor);
        emit interactionStarted();
        event->accept();
        return;
    }
    QOpenGLWidget::mousePressEvent(event);
}

void HubView::mouseMoveEvent(QMouseEvent *event)
{
    if (m_dragging) {
        const QVector3D current = arcballPoint(mousePosition(event));
        const QQuaternion drag = QQuaternion::rotationTo(m_dragStart, current);
        m_orientation = (drag * m_dragOrientation).normalized();
        update();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseMoveEvent(event);
}

void HubView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QOpenGLWidget::mouseReleaseEvent(event);
}

void HubView::mouseDoubleClickEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        resetView();
        event->accept();
        return;
    }
    QOpenGLWidget::mouseDoubleClickEvent(event);
}

void HubView::keyPressEvent(QKeyEvent *event)
{
    QVector3D axis;
    switch (event->key()) {
    case Qt::Key_Home: resetView(); event->accept(); return;
    case Qt::Key_Left: axis = {0, -1, 0}; break;
    case Qt::Key_Right: axis = {0, 1, 0}; break;
    case Qt::Key_Up: axis = {-1, 0, 0}; break;
    case Qt::Key_Down: axis = {1, 0, 0}; break;
    default: QOpenGLWidget::keyPressEvent(event); return;
    }
    m_resetting = false;
    m_orientation = (QQuaternion::fromAxisAndAngle(axis, 6.0f) * m_orientation).normalized();
    emit interactionStarted();
    update();
    event->accept();
}

void HubView::wheelEvent(QWheelEvent *event)
{
    m_zoom = std::clamp(m_zoom + float(event->angleDelta().y()) / 120.0f * .045f, .68f, 1.0f);
    update();
    event->accept();
}

void HubView::resetView()
{
    m_dragging = false;
    setCursor(Qt::OpenHandCursor);
    m_resetFrom = m_orientation;
    m_resetting = true;
    m_resetClock.restart();
    m_zoom = 1.0f;
    updateTimer();
    update();
}

void HubView::setRgbEnabled(bool enabled)
{
    m_rgbEnabled = enabled;
    updateTimer();
    update();
}

void HubView::setDarkTheme(bool dark)
{
    m_darkTheme = dark;
    update();
}

void HubView::updateAnimation()
{
    if (m_resetting) {
        const float t = std::min(1.0f, float(m_resetClock.elapsed()) / ResetDuration);
        const float ease = 1.0f - std::pow(1.0f - t, 3.0f);
        m_orientation = QQuaternion::slerp(m_resetFrom, QQuaternion(), ease).normalized();
        if (t >= 1.0f) {
            m_orientation = QQuaternion();
            m_resetting = false;
            updateTimer();
        }
    }
    update();
}

void HubView::updateTimer()
{
    if (isVisible() && m_renderError.isEmpty() && (m_rgbEnabled || m_resetting)) m_timer.start();
    else m_timer.stop();
}

void HubView::showEvent(QShowEvent *event)
{
    QOpenGLWidget::showEvent(event);
    updateTimer();
}

void HubView::hideEvent(QHideEvent *event)
{
    m_timer.stop();
    QOpenGLWidget::hideEvent(event);
}
