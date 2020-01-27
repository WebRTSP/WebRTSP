#include "LibGst.h"

#include <gst/gst.h>


LibGst::LibGst()
{
    gst_init(0, 0);
}
