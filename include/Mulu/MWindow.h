#pragma once

#include <functional>

#include "Mulu/MObject.h"
#include "Mulu/MString.h"
#include "Mulu/MSize.h"

namespace mulu {

class MWidget;
class MPlatformWindow;

// A top-level native window (QWindow / QMainWindow equivalent).
class MWindow : public MObject {
public:
    explicit MWindow(MWindow* parent = nullptr);
    ~MWindow() override;

    // Window lifecycle ------------------------------------------------------
    void show();
    void hide();
    void close();

    // Title & geometry ------------------------------------------------------
    void setTitle(const MString& title);
    MString title() const { return m_title; }

    void setSize(const MSize& size);
    MSize size() const { return m_size; }

    void setMinimumSize(const MSize& size) { m_minimumSize = size; }
    MSize minimumSize() const { return m_minimumSize; }

    void setMaximumSize(const MSize& size) { m_maximumSize = size; }
    MSize maximumSize() const { return m_maximumSize; }

    // Content ---------------------------------------------------------------
    void setCentralWidget(MWidget* widget);
    MWidget* centralWidget() const { return m_centralWidget; }

    // Events ----------------------------------------------------------------
    using CloseCallback = std::function<void()>;
    void setOnClose(CloseCallback callback) { m_onClose = std::move(callback); }

    // Native access ---------------------------------------------------------
    void* nativeHandle() const;

    // Internal API used by the platform backend -----------------------------
    void handleClose();
    void requestWidgetSync(MWidget* widget);

protected:
    MPlatformWindow* platformWindow() const { return m_platformWindow; }
    void ensurePlatformWindow();

private:
    MString m_title;
    MSize m_size{800, 600};
    MSize m_minimumSize{0, 0};
    MSize m_maximumSize{16384, 16384};
    MWidget* m_centralWidget = nullptr;
    CloseCallback m_onClose;
    MPlatformWindow* m_platformWindow = nullptr;
};

} // namespace mulu
