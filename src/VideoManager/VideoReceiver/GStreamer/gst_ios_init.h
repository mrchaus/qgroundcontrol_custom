/****************************************************************************
 *
 * (c) 2009-2024 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/

#pragma once

#include <gst/gst.h>

G_BEGIN_DECLS

void gst_ios_pre_init();

/// Lower the ranks of filesrc and giosrc so iosavassetsrc is tried first in gst_element_make_from_uri() for file://
void gst_ios_post_init();

G_END_DECLS
