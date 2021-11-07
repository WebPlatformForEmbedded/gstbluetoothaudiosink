/* GStreamer
 * Copyright (C) 2021 Metrological
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 51 Franklin Street, Suite 500,
 * Boston, MA 02110-1335, USA.
 */

#include <gst/gst.h>
#include <gst/audio/gstaudiosink.h>
#include "gstbluetoothaudiosink.h"

GST_DEBUG_CATEGORY_STATIC (gst_bluetoothaudiosink_debug_category);
#define GST_CAT_DEFAULT gst_bluetoothaudiosink_debug_category

/* prototypes */

static void gst_bluetoothaudiosink_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec);
static void gst_bluetoothaudiosink_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec);
static void gst_bluetoothaudiosink_dispose (GObject *object);
static void gst_bluetoothaudiosink_finalize (GObject *object);

static gboolean gst_bluetoothaudiosink_open (GstAudioSink *sink);
static gboolean gst_bluetoothaudiosink_prepare (GstAudioSink *sink, GstAudioRingBufferSpec *spec);
static gboolean gst_bluetoothaudiosink_unprepare (GstAudioSink *sink);
static gboolean gst_bluetoothaudiosink_close (GstAudioSink *sink);
static gint gst_bluetoothaudiosink_write (GstAudioSink *sink, gpointer data, guint length);
static guint gst_bluetoothaudiosink_delay (GstAudioSink *sink);
static void gst_bluetoothaudiosink_reset (GstAudioSink *sink);

enum
{
  PROP_0
  // ...
};

/* pad templates */

static GstStaticPadTemplate gst_bluetoothaudiosink_sink_template =
GST_STATIC_PAD_TEMPLATE ("sink",
    GST_PAD_SINK,
    GST_PAD_ALWAYS,
    GST_STATIC_CAPS ("audio/x-raw,"
      "format=S16LE,"
      "rate=[44100,48000],"
      "channels=[1,2],"
      "layout=interleaved")
    );

/* class initialization */

G_DEFINE_TYPE_WITH_CODE (GstBluetoothAudioSink, gst_bluetoothaudiosink, GST_TYPE_AUDIO_SINK,
  GST_DEBUG_CATEGORY_INIT (gst_bluetoothaudiosink_debug_category, "bluetoothaudiosink", 0, "debug category for bluetoothaudiosink element"));

static void gst_bluetoothaudiosink_class_init (GstBluetoothAudioSinkClass *klass)
{
  GObjectClass *gobject_class = G_OBJECT_CLASS (klass);
  GstAudioSinkClass *audio_sink_class = GST_AUDIO_SINK_CLASS (klass);

  gst_element_class_add_static_pad_template (GST_ELEMENT_CLASS(klass),
      &gst_bluetoothaudiosink_sink_template);

  gst_element_class_set_static_metadata (GST_ELEMENT_CLASS(klass),
      "Audio Sink (Bluetooth)", "Sink/Audio", "Output to Bluetooth audio device", "Metrological");

  gobject_class->set_property = gst_bluetoothaudiosink_set_property;
  gobject_class->get_property = gst_bluetoothaudiosink_get_property;
  gobject_class->dispose = gst_bluetoothaudiosink_dispose;
  gobject_class->finalize = gst_bluetoothaudiosink_finalize;

  audio_sink_class->open = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosink_open);
  audio_sink_class->prepare = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosink_prepare);
  audio_sink_class->unprepare = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosink_unprepare);
  audio_sink_class->close = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosink_close);
  audio_sink_class->write = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosink_write);
  audio_sink_class->delay = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosink_delay);
  audio_sink_class->reset = GST_DEBUG_FUNCPTR (gst_bluetoothaudiosink_reset);
}

/* implementation */

static void gst_bluetoothaudiosink_init (GstBluetoothAudioSink *bluetoothaudiosink)
{
  GST_DEBUG_OBJECT (bluetoothaudiosink, "init");
}

void gst_bluetoothaudiosink_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (object);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    // ...
  }
}

void gst_bluetoothaudiosink_get_property (GObject *object, guint property_id, GValue *value, GParamSpec *pspec)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (object);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "get_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
    // ...
  }
}

void gst_bluetoothaudiosink_dispose (GObject *object)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (object);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "dispose");

  /* clean up as as fast possible.  may be called multiple times */

  G_OBJECT_CLASS (gst_bluetoothaudiosink_parent_class)->dispose (object);
}

void gst_bluetoothaudiosink_finalize (GObject *object)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (object);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "finalize");

  /* clean up object here */

  G_OBJECT_CLASS (gst_bluetoothaudiosink_parent_class)->finalize (object);
}

/* open the device with given specs */
static gboolean gst_bluetoothaudiosink_open (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "open");

  return TRUE;
}

/* prepare resources and state to operate with the given specs */
static gboolean gst_bluetoothaudiosink_prepare (GstAudioSink *sink, GstAudioRingBufferSpec *spec)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "prepare");

  return TRUE;
}

/* undo anything that was done in prepare() */
static gboolean gst_bluetoothaudiosink_unprepare (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "unprepare");

  return TRUE;
}

/* close the device */
static gboolean gst_bluetoothaudiosink_close (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "close");

  return TRUE;
}

/* write samples to the device */
static gint gst_bluetoothaudiosink_write (GstAudioSink *sink, gpointer data, guint length)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "write");

  return 0;
}

/* get number of samples queued in the device */
static guint gst_bluetoothaudiosink_delay (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "delay");

  return 0;
}

/* reset the audio device, unblock from a write */
static void gst_bluetoothaudiosink_reset (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "reset");
}

static gboolean plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, "bluetoothaudiosink", GST_RANK_NONE, GST_TYPE_BLUETOOTHAUDIOSINK);
}

#ifndef VERSION
#define VERSION "0.0.1"
#endif
#ifndef PACKAGE
#define PACKAGE "gstbluetoothaudiosink"
#endif
#ifndef PACKAGE_NAME
#define PACKAGE_NAME "gstbluetoothaudiosink"
#endif
#ifndef GST_PACKAGE_ORIGIN
#define GST_PACKAGE_ORIGIN "https://github.com/WebPlatformForEmbedded/gstbluetoothaudiosink"
#endif

GST_PLUGIN_DEFINE (GST_VERSION_MAJOR,
    GST_VERSION_MINOR,
    bluetoothaudiosink,
    "bluetoothaudisink plugin",
    plugin_init, VERSION, "LGPL", PACKAGE_NAME, GST_PACKAGE_ORIGIN)

