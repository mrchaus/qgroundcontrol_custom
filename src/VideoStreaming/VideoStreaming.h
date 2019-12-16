/****************************************************************************
 *
 *   (c) 2009-2018 QGROUNDCONTROL PROJECT <http://www.qgroundcontrol.org>
 *
 * QGroundControl is licensed according to the terms in the file
 * COPYING.md in the root of the source code directory.
 *
 ****************************************************************************/


/**
 * @file
 *   @brief QGC Video Streaming Initialization
 *   @author Gus Grubba <mavlink@grubba.com>
 */

#pragma once

extern void initializeVideoStreaming    (int &argc, char *argv[], char* filename, char* debuglevel);
extern void shutdownVideoStreaming      ();
