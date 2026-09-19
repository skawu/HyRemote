#include "custom_fbo_item.h"

#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>

#include <atomic>

namespace hyremote {
namespace asyncspike {

namespace {

std::atomic<int> g_renderCount{0};
std::atomic<int> g_lastEncoded{0};

class CustomFboRenderer : public QQuickFramebufferObject::Renderer, protected QOpenGLFunctions
{
public:
    void synchronize(QQuickFramebufferObject *item) override
    {
        // Runs on the render thread while the GUI thread is blocked, which is the
        // documented way to pass state into a renderer.
        m_frozen = static_cast<CustomFboItem *>(item)->frozen();
    }

protected:
    void render() override
    {
        if (!m_functionsReady) {
            initializeOpenGLFunctions();
            m_functionsReady = true;
        }

        int value = g_lastEncoded.load(std::memory_order_relaxed);
        if (!m_frozen) {
            value = (g_renderCount.fetch_add(1, std::memory_order_relaxed) + 1) & 0xffff;
            g_lastEncoded.store(value, std::memory_order_relaxed);
        }

        glClearColor(float(value & 0xff) / 255.0f,
                     float((value >> 8) & 0xff) / 255.0f,
                     1.0f,
                     1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        if (!m_frozen)
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
    bool m_frozen = false;
};

}  // namespace

CustomFboItem::CustomFboItem(QQuickItem *parent)
    : QQuickFramebufferObject(parent)
{
    setTextureFollowsItemSize(true);
}

void CustomFboItem::setFrozen(bool frozen)
{
    if (m_frozen == frozen)
        return;
    m_frozen = frozen;
    Q_EMIT frozenChanged();
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

}  // namespace asyncspike
}  // namespace hyremote
