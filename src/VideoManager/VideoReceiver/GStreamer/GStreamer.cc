/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


/**
 * @file
 *   @brief QGC Video Streaming Initialization
 *   @author Gus Grubba <gus@auterion.com>
 */

#include "GStreamer.h"
#include "GstVideoReceiver.h"
#include "QGCLoggingCategory.h"

#include <QtQuick/QQuickItem>

#ifdef Q_OS_IOS
#include "gst_ios_init.h"
#endif

QGC_LOGGING_CATEGORY(GStreamerLog, "qgc.videomanager.videoreceiver.gstreamer")
QGC_LOGGING_CATEGORY(GStreamerAPILog, "qgc.videomanager.videoreceiver.gstreamer.api")

namespace {

G_BEGIN_DECLS
#ifdef QGC_GST_STATIC_BUILD
GST_PLUGIN_STATIC_DECLARE(coreelements);
GST_PLUGIN_STATIC_DECLARE(playback);
GST_PLUGIN_STATIC_DECLARE(libav);
GST_PLUGIN_STATIC_DECLARE(rtp);
GST_PLUGIN_STATIC_DECLARE(rtsp);
GST_PLUGIN_STATIC_DECLARE(udp);
GST_PLUGIN_STATIC_DECLARE(videoparsersbad);
GST_PLUGIN_STATIC_DECLARE(x264);
GST_PLUGIN_STATIC_DECLARE(rtpmanager);
GST_PLUGIN_STATIC_DECLARE(isomp4);
GST_PLUGIN_STATIC_DECLARE(matroska);
GST_PLUGIN_STATIC_DECLARE(mpegtsdemux);
GST_PLUGIN_STATIC_DECLARE(opengl);
GST_PLUGIN_STATIC_DECLARE(tcp);
GST_PLUGIN_STATIC_DECLARE(asf);
GST_PLUGIN_STATIC_DECLARE(va);
#ifdef Q_OS_ANDROID
GST_PLUGIN_STATIC_DECLARE(androidmedia);
#elif defined(Q_OS_IOS)
GST_PLUGIN_STATIC_DECLARE(applemedia);
#endif // Q_OS_ANDROID
#endif // QGC_GST_STATIC_BUILD
GST_PLUGIN_STATIC_DECLARE(qml6);
GST_PLUGIN_STATIC_DECLARE(qgc);
G_END_DECLS

void _QtGstLog(GstDebugCategory *category,
               GstDebugLevel level,
               const gchar *file,
               const gchar *function,
               gint line,
               GObject *object,
               GstDebugMessage *message,
               gpointer data)
{
    Q_UNUSED(data);

    if (level > gst_debug_category_get_threshold(category)) {
        return;
    }

    QMessageLogger log(file, line, function);

    char *object_info = gst_info_strdup_printf("%" GST_PTR_FORMAT, static_cast<void*>(object));

    switch (level) {
    default:
    case GST_LEVEL_ERROR:
        log.critical(GStreamerAPILog, "%s %s", object_info, gst_debug_message_get(message));
        break;
    case GST_LEVEL_WARNING:
        log.warning(GStreamerAPILog, "%s %s", object_info, gst_debug_message_get(message));
        break;
    case GST_LEVEL_FIXME:
    case GST_LEVEL_INFO:
        log.info(GStreamerAPILog, "%s %s", object_info, gst_debug_message_get(message));
        break;
    case GST_LEVEL_DEBUG:
    case GST_LEVEL_LOG:
    case GST_LEVEL_TRACE:
    case GST_LEVEL_MEMDUMP:
        log.debug(GStreamerAPILog, "%s %s", object_info, gst_debug_message_get(message));
        break;
    }

    g_free(object_info);
    object_info = nullptr;
}

void qgcputenv(const QString &key, const QString &root, const QString &path = "")
{
    const QString value = root + path;
    (void) qputenv(key.toStdString().c_str(), QByteArray(value.toStdString().c_str()));
}

} // namespace

namespace GStreamer {

void blacklist(VideoDecoderOptions option)
{
    GstRegistry* const registry = gst_registry_get();

    if (!registry) {
        qCCritical(GStreamerLog) << "Failed to get gstreamer registry.";
        return;
    }

    auto changeRank = [registry](const char *featureName, uint16_t rank) {
        GstPluginFeature* const feature = gst_registry_lookup_feature(registry, featureName);
        if (!feature) {
            qCDebug(GStreamerLog) << "Failed to change ranking of feature. Featuer does not exist:" << featureName;
            return;
        }

        qCDebug(GStreamerLog) << "Changing feature (" << featureName << ") to use rank:" << rank;
        gst_plugin_feature_set_rank(feature, rank);
        (void) gst_registry_add_feature(registry, feature);
        gst_object_unref(feature);
    };

    changeRank("bcmdec", GST_RANK_NONE);

    switch (option) {
    case ForceVideoDecoderDefault:
        break;
    case ForceVideoDecoderSoftware:
        for (const char *name : {"avdec_h264", "avdec_h265"}) {
            changeRank(name, GST_RANK_PRIMARY + 1);
        }
        break;
    case ForceVideoDecoderVAAPI:
        for (const char *name : {"vaapimpeg2dec", "vaapimpeg4dec", "vaapih263dec", "vaapih264dec", "vaapih265dec", "vaapivc1dec"}) {
            changeRank(name, GST_RANK_PRIMARY + 1);
        }
        break;
    case ForceVideoDecoderNVIDIA:
        for (const char *name : {"nvh265dec", "nvh265sldec", "nvh264dec", "nvh264sldec"}) {
            changeRank(name, GST_RANK_PRIMARY + 1);
        }
        break;
    case ForceVideoDecoderDirectX3D:
        for (const char *name : {"d3d11vp9dec", "d3d11h265dec", "d3d11h264dec"}) {
            changeRank(name, GST_RANK_PRIMARY + 1);
        }
        break;
    case ForceVideoDecoderVideoToolbox:
        changeRank("vtdec", GST_RANK_PRIMARY + 1);
        break;
    default:
        qCWarning(GStreamerLog) << "Can't handle decode option:" << option;
        break;
    }
}

void initialize(int argc, char *argv[], int debuglevel)
{
    (void) qRegisterMetaType<VideoReceiver::STATUS>("STATUS");

    const QString currentDir = QCoreApplication::applicationDirPath();

#ifndef QGC_INSTALL_RELEASE
    qunsetenv("GST_PLUGIN_SYSTEM_PATH_1_0");
    qunsetenv("GST_PLUGIN_SYSTEM_PATH");
    qunsetenv("GST_PLUGIN_PATH_1_0");
    qunsetenv("GST_PLUGIN_PATH");
    Q_UNUSED(currentDir);
#elif defined(Q_OS_MAC)
    qgcputenv("GST_REGISTRY_REUSE_PLUGIN_SCANNER", "no");
    qgcputenv("GST_PLUGIN_SCANNER", currentDir, "/../Frameworks/GStreamer.framework/Versions/Current/libexec/gstreamer-1.0/gst-plugin-scanner");
    qgcputenv("GST_PTP_HELPER_1_0", currentDir, "/../Frameworks/GStreamer.framework/Versions/Current/libexec/gstreamer-1.0/gst-ptp-helper");
    qgcputenv("GIO_EXTRA_MODULES", currentDir, "/../Frameworks/GStreamer.framework/Versions/Current/lib/gio/modules");
    qgcputenv("GST_PLUGIN_SYSTEM_PATH_1_0", currentDir, "/../Frameworks/GStreamer.framework/Versions/Current/lib/gstreamer-1.0");
    qgcputenv("GST_PLUGIN_SYSTEM_PATH", currentDir, "/../Frameworks/GStreamer.framework/Versions/Current/lib/gstreamer-1.0");
    qgcputenv("GST_PLUGIN_PATH_1_0", currentDir, "/../Frameworks/GStreamer.framework/Versions/Current/lib/gstreamer-1.0");
    qgcputenv("GST_PLUGIN_PATH", currentDir, "/../Frameworks/GStreamer.framework/Versions/Current/lib/gstreamer-1.0");
#elif defined(Q_OS_WIN)
    qgcputenv("GST_REGISTRY_REUSE_PLUGIN_SCANNER", "no");
    qgcputenv("GST_PLUGIN_SCANNER", currentDir, "/gst-plugin-scanner");
    qgcputenv("GST_PTP_HELPER_1_0", currentDir, "/gst-ptp-helper");
    qgcputenv("GIO_EXTRA_MODULES", currentDir, "/../lib/gio/modules");
    qgcputenv("GST_PLUGIN_SYSTEM_PATH_1_0", currentDir);
    qgcputenv("GST_PLUGIN_SYSTEM_PATH", currentDir);
    qgcputenv("GST_PLUGIN_PATH_1_0", currentDir);
    qgcputenv("GST_PLUGIN_PATH", currentDir);
#endif

    if (qEnvironmentVariableIsEmpty("GST_DEBUG")) {
        gst_debug_set_default_threshold(static_cast<GstDebugLevel>(debuglevel));
        gst_debug_remove_log_function(gst_debug_log_default);
        gst_debug_add_log_function(_QtGstLog, nullptr, nullptr);
    }

#ifdef Q_OS_IOS
    gst_ios_pre_init();
#endif

    GError *error = nullptr;
    if (!gst_init_check(&argc, &argv, &error)) {
        qCCritical(GStreamerLog) << "gst_init_check() failed:" << error->message;
        g_error_free(error);
    }

#ifdef QGC_GST_STATIC_BUILD
    GST_PLUGIN_STATIC_REGISTER(coreelements);
    GST_PLUGIN_STATIC_REGISTER(playback);
    GST_PLUGIN_STATIC_REGISTER(libav);
    GST_PLUGIN_STATIC_REGISTER(rtp);
    GST_PLUGIN_STATIC_REGISTER(rtsp);
    GST_PLUGIN_STATIC_REGISTER(udp);
    GST_PLUGIN_STATIC_REGISTER(videoparsersbad);
    GST_PLUGIN_STATIC_REGISTER(x264);
    GST_PLUGIN_STATIC_REGISTER(rtpmanager);
    GST_PLUGIN_STATIC_REGISTER(isomp4);
    GST_PLUGIN_STATIC_REGISTER(matroska);
    GST_PLUGIN_STATIC_REGISTER(mpegtsdemux);
    GST_PLUGIN_STATIC_REGISTER(opengl);
    GST_PLUGIN_STATIC_REGISTER(tcp);
    GST_PLUGIN_STATIC_REGISTER(asf);
    GST_PLUGIN_STATIC_REGISTER(va);
#ifdef Q_OS_ANDROID
    GST_PLUGIN_STATIC_REGISTER(androidmedia);
#elif defined(Q_OS_IOS)
    GST_PLUGIN_STATIC_REGISTER(applemedia);
    gst_ios_post_init();
#endif // Q_OS_ANDROID
#endif // QGC_GST_STATIC_BUILD
    GST_PLUGIN_STATIC_REGISTER(qml6);
    GST_PLUGIN_STATIC_REGISTER(qgc);
}

void *createVideoSink(QObject *parent, QQuickItem *widget)
{
    Q_UNUSED(parent)

    GstElement* const sink = gst_element_factory_make("qgcvideosinkbin", nullptr);
    if (sink) {
        g_object_set(sink, "widget", widget, NULL);
    } else {
        qCCritical(GStreamerLog) << "gst_element_factory_make('qgcvideosinkbin') failed";
    }

    return sink;
}

void releaseVideoSink(void *sink)
{
    if (sink) {
        gst_object_unref(GST_ELEMENT(sink));
    }
}

VideoReceiver *createVideoReceiver(QObject *parent)
{
    return new GstVideoReceiver(parent);
}

} // namespace GStreamer
