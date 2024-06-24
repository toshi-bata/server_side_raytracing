#ifndef AXIS_TRIAD_H
#define AXIS_TRIAD_H

#ifndef IS_WIN
    #include <cstddef>
using std::size_t;
#endif

#include <REDObject.h>

namespace HC_luminate_bridge {

    /**
     * Structure storing data about an AxisTriad.
     */
    struct AxisTriad {
        // Viewpoint rendering the axis triad.
        RED::Object* axisCamera;

        // Materials used by the axis triad.
        RED::Object* materialWhite;
        RED::Object* materialRed;
        RED::Object* materialGreen;
        RED::Object* materialBlue;
    };

    /**
     * Axis triad orientation synchronisation with a camera.
     * @param[in] a_axisTriad Axis triad to synchronize.
     * @param[in] a_camera Camera to synchronize with.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC synchronizeAxisTriadWithCamera(AxisTriad const& a_axisTriad, RED::Object* a_camera);

    /**
     * Create and attach a new axis triad to a window.
     * @param[in] a_window Window that will receive the axis triad.
     * @param[in] a_outAxisTriad Output axis triad informations.
     * @return RED_OK if success, otherwise error code.
     */
    RED_RC createAxisTriad(RED::Object* a_window, const int a_num_vrl, AxisTriad& a_outAxisTriad);

} // namespace hoops_luminate_bridge

#endif
