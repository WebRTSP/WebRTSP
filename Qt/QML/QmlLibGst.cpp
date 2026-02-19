#include "QmlLibGst.h"

#include <QtResource>

#include <gst/gst.h>


#ifdef __ANDROID__
G_BEGIN_DECLS
    GST_PLUGIN_STATIC_DECLARE(opengl);
    GST_PLUGIN_STATIC_DECLARE(qml6);
G_END_DECLS
#else
#include <QDebug>

static void LogToQt(
    GstDebugCategory* category,
    GstDebugLevel level,
    const gchar* /*file*/,
    const gchar* /*function*/,
    gint /*line*/,
    GObject* /*object*/,
    GstDebugMessage* message,
    gpointer /*userData*/)
{
    switch(level) {
        case GST_LEVEL_ERROR:
            qCritical().nospace()
                << "[" << gst_debug_category_get_name(category) << "] "
                << gst_debug_message_get(message);
            break;
        case GST_LEVEL_WARNING:
            qWarning().nospace()
                << "[" << gst_debug_category_get_name(category) << "] "
                << gst_debug_message_get(message);
            break;
        case GST_LEVEL_FIXME:
        case GST_LEVEL_INFO:
            qInfo().nospace()
                << "[" << gst_debug_category_get_name(category) << "] "
                << gst_debug_message_get(message);
            break;
        case GST_LEVEL_NONE:
        case GST_LEVEL_DEBUG:
        case GST_LEVEL_LOG:
        case GST_LEVEL_TRACE:
        case GST_LEVEL_MEMDUMP:
        default:
            // qDebug().nospace()
            //    << "[" << gst_debug_category_get_name(category) << "] "
            //    << gst_debug_message_get(message);
            break;
    }
}
#endif

QmlLibGst::QmlLibGst()
{
#ifdef __ANDROID__
    GST_PLUGIN_STATIC_REGISTER(opengl);
    GST_PLUGIN_STATIC_REGISTER(qml6);
    Q_INIT_RESOURCE(qml6);
#else
    gst_debug_remove_log_function(gst_debug_log_default);
    gst_debug_add_log_function(LogToQt, nullptr, nullptr);
    gst_debug_set_default_threshold(GST_LEVEL_WARNING);
    gst_debug_set_active(TRUE);
#endif
}
