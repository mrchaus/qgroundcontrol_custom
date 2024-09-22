/****************************************************************************
 *
 * (c) 2009-2020 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

/**
 * @file
 *   @brief GStreamer plugin for QGC's Video Receiver
 *   @author Andrew Voznyts <andrew.voznytsa@gmail.com>
 *   @author Tomaz Canabrava <tcanabrava@kde.org>
 */

#include <gst/gst.h>

extern gboolean gst_qgc_video_sink_bin_plugin_init(GstPlugin *plugin);

static gboolean plugin_init(GstPlugin *plugin)
{
    return gst_qgc_video_sink_bin_plugin_init(plugin);
}

#define GST_PACKAGE_NAME   "GStreamer plugin for QGC's Video Receiver"
#define GST_PACKAGE_ORIGIN "https://qgroundcontrol.com/"
#define GST_LICENSE        "LGPL"
#define PACKAGE            "QGC Video Receiver"
#define PACKAGE_VERSION    "current"

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    qgc,
    "QGC Video Receiver Plugin",
    plugin_init, PACKAGE_VERSION, GST_LICENSE, GST_PACKAGE_NAME,
    GST_PACKAGE_ORIGIN)
