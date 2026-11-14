#include "glasswindow.h"

#include <QGuiApplication>
#include <QWidget>
#include <QWindow>

#if defined(Q_OS_WIN)
#  include <qt_windows.h>
#  include <dwmapi.h>
#endif

#if defined(REMOTEHUB_HAS_KWINDOWSYSTEM)
#  include <KWindowEffects>
#endif

GlassWindow::Backdrop GlassWindow::apply(QWidget *window, bool dark,
                                        const QRegion &region)
{
    if (!window || !window->isWindow() || !window->windowHandle())
        return Backdrop::TranslucentOnly;

#if defined(Q_OS_WIN)
    if (QGuiApplication::platformName() != QStringLiteral("windows"))
        return Backdrop::TranslucentOnly;

    const HWND handle = reinterpret_cast<HWND>(window->winId());
    // Documented DWM attributes. Numeric constants also allow compilation with
    // an older Windows SDK; unsupported OS versions simply reject the request.
    constexpr DWORD useImmersiveDarkMode = 20;
    constexpr DWORD windowCornerPreference = 33;
    constexpr DWORD systemBackdropType = 38;
    constexpr int roundedCorners = 2;   // DWMWCP_ROUND
    constexpr int acrylicBackdrop = 3; // DWMSBT_TRANSIENTWINDOW, Windows 11 22H2+
    const BOOL darkMode = dark ? TRUE : FALSE;
    DwmSetWindowAttribute(handle, useImmersiveDarkMode, &darkMode, sizeof(darkMode));
    DwmSetWindowAttribute(handle, windowCornerPreference,
                         &roundedCorners, sizeof(roundedCorners));
    const HRESULT backdropResult = DwmSetWindowAttribute(
        handle, systemBackdropType, &acrylicBackdrop, sizeof(acrylicBackdrop));
    if (SUCCEEDED(backdropResult)) {
        const MARGINS margins = {-1, -1, -1, -1};
        if (SUCCEEDED(DwmExtendFrameIntoClientArea(handle, &margins)))
            return Backdrop::NativeBlur;
    }
#elif defined(REMOTEHUB_HAS_KWINDOWSYSTEM)
    if (KWindowEffects::isEffectAvailable(KWindowEffects::BlurBehind)) {
        // KWindowSystem handles X11 and the compositor's Wayland protocol.
        // Keep the shadow/transparent rounded corners outside the blur region.
        KWindowEffects::enableBlurBehind(window->windowHandle(), true, region);
        return Backdrop::NativeBlur;
    }
#endif

    // GNOME, macOS without a native material view, older Windows, and headless
    // platforms retain the real alpha transparency painted by the application.
    // QGraphicsBlurEffect is intentionally not used: it blurs the controls,
    // rather than the desktop behind the window.
    Q_UNUSED(dark)
    Q_UNUSED(region)
    return Backdrop::TranslucentOnly;
}
