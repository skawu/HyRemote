#include "samples/custom_fbo_item.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>

#include <atomic>

namespace hyremote {
namespace spike {

namespace {

std::atomic<int> g_renderCount{0};
std::atomic<int> g_lastEncoded{0};

// A custom renderer that draws without the scene graph's own material system.
//
// In Qt 6 QQuickFramebufferObject::Renderer is no longer a QOpenGLFunctions
// subclass, so the spike adds it explicitly and initializes it once a context
// is guaranteed to be current (inside render()).
class CustomFboRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
protected:
    void render() override
    {
        if (!m_functionsReady) {
            initializeOpenGLFunctions();
            m_functionsReady = true;
        }

        const int value = (g_renderCount.fetch_add(1, std::memory_order_relaxed) + 1) & 0xffff;
        g_lastEncoded.store(value, std::memory_order_relaxed);

        glClearColor(float(value & 0xff) / 255.0f,
                     float((value >> 8) & 0xff) / 255.0f,
                     1.0f,
                     1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Keep the item animating so every capture has a chance to observe a
        // freshly rendered frame.
        update();
    }

    QOpenGLFramebufferObject *createFramebufferObject(const QSize &size) override
    {
        QOpenGLFramebufferObjectFormat format;
        format.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        return new QOpenGLFramebufferObject(size, format);
    }

private:
    bool m_functionsReady = false;
};

}  // namespace

CustomFboItem::CustomFboItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setTextureFollowsItemSize(true);
}

QQuickFramebufferObject::Renderer *CustomFboItem::createRenderer() const
{
    return new CustomFboRenderer;
}

int CustomFboItem::renderedFrameCount()
{
    return g_renderCount.load(std::memory_order_relaxed);
}

int CustomFboItem::lastEncodedValue()
{
    return g_lastEncoded.load(std::memory_order_relaxed);
}

void CustomFboItem::resetCounters()
{
    g_renderCount.store(0, std::memory_order_relaxed);
    g_lastEncoded.store(0, std::memory_order_relaxed);
}

}  // namespace spike
}  // namespace hyremote
