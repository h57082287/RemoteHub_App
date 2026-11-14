#ifndef GLASSWINDOW_H
#define GLASSWINDOW_H

#include <QRegion>

class QWidget;

namespace GlassWindow {

enum class Backdrop {
    TranslucentOnly,
    NativeBlur
};

// Call after the top-level native window exists, and again after theme/size
// changes. The region is in logical window coordinates (empty = entire window).
// The caller owns WA_TranslucentBackground and all glass tint/highlight painting.
// NativeBlur means the compositor accepted a blur request; OS accessibility or
// power-saving preferences may still suppress the effect.
Backdrop apply(QWidget *window, bool dark, const QRegion &region = QRegion());

} // namespace GlassWindow

#endif // GLASSWINDOW_H
