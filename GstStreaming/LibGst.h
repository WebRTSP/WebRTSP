#pragma once

#include <gst/gst.h>


struct LibGst
{
    LibGst() { gst_init(0, 0); }
    ~LibGst() { gst_deinit(); }
};
