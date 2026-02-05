#include "QmlLibGst.h"

#include <QtResource>

#include <gst/gst.h>


#ifdef __ANDROID__
G_BEGIN_DECLS
    GST_PLUGIN_STATIC_DECLARE(opengl);
    GST_PLUGIN_STATIC_DECLARE(qml6);
G_END_DECLS
#endif

QmlLibGst::QmlLibGst()
{
#ifdef __ANDROID__
    GST_PLUGIN_STATIC_REGISTER(opengl);
    GST_PLUGIN_STATIC_REGISTER(qml6);
    Q_INIT_RESOURCE(qml6);
#endif
}
