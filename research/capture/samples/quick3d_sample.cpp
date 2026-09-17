#include "samples/quick3d_sample.h"

namespace hyremote {
namespace spike {

Quick3DSample::Quick3DSample()
    : QuickSample(QuickSceneSpec{
          QStringLiteral("quick3d"),
          QStringLiteral("Quick3D scene (View3D, lit materials, MSAA)"),
          QStringLiteral("Quick3D"),
          QStringLiteral("Quick3DScene"),
          {},
          {QStringLiteral(
              "Quick3D renders through its own renderer inside the scene graph; the baseline "
              "capture must not assume a plain 2D scene graph."),
           QStringLiteral(
               "This case needs the QtQuick3D module; it is reported as unverified when the "
               "module is not present in the Qt installation.")},
      })
{
}

}  // namespace spike
}  // namespace hyremote
