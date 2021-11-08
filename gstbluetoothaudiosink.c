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

#include <WPEFramework/bluetoothaudiosink/bluetoothaudiosink.h>


GST_DEBUG_CATEGORY_STATIC (gst_bluetoothaudiosink_debug_category);
#define GST_CAT_DEFAULT gst_bluetoothaudiosink_debug_category


/* implementation */

#define CONNECTOR "/tmp/gstbtaudiobuffer"

static void _audio_sink_clear (GstBluetoothAudioSink* bluetoothaudiosink)
{
  bluetoothaudiosink->request_acquire = FALSE;
  bluetoothaudiosink->request_playback = FALSE;
  bluetoothaudiosink->acquired = FALSE;
  bluetoothaudiosink->playing = FALSE;
}

static gboolean _audio_sink_acquire (GstBluetoothAudioSink* bluetoothaudiosink)
{
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;
  gboolean result = TRUE;

  bluetoothaudiosink_state (&state);

  if (state == BLUETOOTHAUDIOSINK_STATE_CONNECTED) {
    bluetoothaudiosink->request_acquire = FALSE;

    /* pick this up from... somewhere... */
    bluetoothaudiosink_format_t format;
    format.sample_rate = 44100;
    format.frame_rate = 24;
    format.channels = 2;
    format.resolution = 16;

    if (bluetoothaudiosink_acquire (CONNECTOR, &format, 2) != 0) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_acquire() failed");
      result = FALSE;
    } else {
      GST_INFO_OBJECT (bluetoothaudiosink, "Starting Bluetooth playback session...");
      bluetoothaudiosink->acquired = TRUE;
    }
  } else if ((state != BLUETOOTHAUDIOSINK_STATE_READY) || (state != BLUETOOTHAUDIOSINK_STATE_STREAMING)) {
    /* Can't acquire just now, but schedule acquiring as soon as the device connects! */
    bluetoothaudiosink->request_acquire = TRUE;
  }

  return result;
}

static gboolean _audio_sink_relinquish (GstBluetoothAudioSink* bluetoothaudiosink)
{
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;
  gboolean result = TRUE;

  bluetoothaudiosink->request_acquire = FALSE;
  bluetoothaudiosink->request_playback = FALSE;

  bluetoothaudiosink_state (&state);

  if ((state == BLUETOOTHAUDIOSINK_STATE_READY) || (state == BLUETOOTHAUDIOSINK_STATE_STREAMING)) {
    if (bluetoothaudiosink_relinquish() != 0) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_relinquish() failed");
      result = FALSE;
    }

    bluetoothaudiosink->acquired = FALSE;
  }

  return result;
}

static gboolean _audio_sink_start (GstBluetoothAudioSink* bluetoothaudiosink)
{
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;
  gboolean result = TRUE;

  bluetoothaudiosink_state (&state);

  if (state == BLUETOOTHAUDIOSINK_STATE_READY) {
    if (bluetoothaudiosink_speed (100) != 0) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_speed(100) failed");
      result = FALSE;
    } else {
      GST_INFO_OBJECT (bluetoothaudiosink, "Now streaming audio over Bluetooth!");
      bluetoothaudiosink->playing = TRUE;
    }
  } else if (state != BLUETOOTHAUDIOSINK_STATE_STREAMING) {
    /* Can't start playback just now, but schedule acquiring as soon as the device connects! */
    bluetoothaudiosink->request_playback = TRUE;
  }

  return result;
}

static gboolean _audio_sink_stop (GstBluetoothAudioSink* bluetoothaudiosink)
{
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;
  gboolean result = TRUE;

  bluetoothaudiosink->request_playback = FALSE;

  bluetoothaudiosink_state (&state);

  if (state == BLUETOOTHAUDIOSINK_STATE_STREAMING) {
    if (bluetoothaudiosink_speed (0) != 0) {
      GST_ERROR_OBJECT( bluetoothaudiosink, "bluetoothaudiosink_speed(0) failed");
      result = FALSE;
    }

    bluetoothaudiosink->playing = FALSE;
  }

  return result;
}

static guint _audio_sink_frame (GstBluetoothAudioSink *bluetoothaudiosink, gpointer data, guint size)
{
  guint16 played = 0;

  if (bluetoothaudiosink->playing) {
    if (bluetoothaudiosink_frame (size, data, &played) != 0) {
      GST_ERROR_OBJECT( bluetoothaudiosink, "bluetoothaudiosink_frame() failed");
    }
  }

  return played;
}

static void _audio_sink_callback_connected (void *user_data)
{
  GstBluetoothAudioSink *bluetoothaudiosink = (GstBluetoothAudioSink*)user_data;

  if (bluetoothaudiosink->request_acquire) {
    if (!_audio_sink_acquire (bluetoothaudiosink)) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to acquire Bluetooth audio sink device!");
    }
  }

  if (bluetoothaudiosink->request_playback) {
    if (!_audio_sink_start (bluetoothaudiosink)) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to start playback on the Bluetooth audi sink device!");
    }
  }
}

static void _audio_sink_callback_disconnected (void *user_data)
{
  GstBluetoothAudioSink *bluetoothaudiosink = (GstBluetoothAudioSink*)user_data;

  _audio_sink_clear (bluetoothaudiosink);
}

static void _audio_sink_callback_state_updated (void *user_data)
{
  GstBluetoothAudioSink *bluetoothaudiosink = (GstBluetoothAudioSink*)user_data;
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;

  if (bluetoothaudiosink_state (&state) == 0) {
    switch (state) {
    case BLUETOOTHAUDIOSINK_STATE_UNASSIGNED:
      GST_WARNING_OBJECT (bluetoothaudiosink, "Bluetooth audio sink is currently unassigned!");
      break;
    case BLUETOOTHAUDIOSINK_STATE_CONNECTED:
      GST_INFO_OBJECT (bluetoothaudiosink, "Bluetooth audio sink is now connected!");
      _audio_sink_callback_connected (bluetoothaudiosink);
      break;
    case BLUETOOTHAUDIOSINK_STATE_CONNECTED_BAD_DEVICE:
      GST_ERROR_OBJECT (bluetoothaudiosink, "Invalid device connected - cant't play!");
      break;
    case BLUETOOTHAUDIOSINK_STATE_CONNECTED_RESTRICTED:
      GST_ERROR_OBJECT (bluetoothaudiosink, "Restricted Bluetooth audio device connected - won't play!");
      break;
    case BLUETOOTHAUDIOSINK_STATE_DISCONNECTED:
      GST_WARNING_OBJECT (bluetoothaudiosink, "Bluetooth Audio sink is now disconnected!");
      _audio_sink_callback_disconnected (bluetoothaudiosink);
      break;
    case BLUETOOTHAUDIOSINK_STATE_READY:
      GST_INFO_OBJECT (bluetoothaudiosink, "Bluetooth Audio sink now ready!");
      break;
    case BLUETOOTHAUDIOSINK_STATE_STREAMING:
      GST_INFO_OBJECT (bluetoothaudiosink, "Bluetooth Audio sink is now streaming!");
      break;
    default:
      break;
    }
  }
}

static void _audio_sink_callback_operational_state_updated (const bool running, void *user_data)
{
  GstBluetoothAudioSink *bluetoothaudiosink = (GstBluetoothAudioSink*)user_data;

  if (running == true) {
    bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;

    bluetoothaudiosink_state (&state);

    if (state == BLUETOOTHAUDIOSINK_STATE_UNKNOWN) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "Unknown Bluetooth Audio Sink failure!");
    } else {
      GST_INFO_OBJECT (bluetoothaudiosink, "Bluetooth Audio Sink service now available");

      /* Register for the sink updates... */
      if (bluetoothaudiosink_register_state_update_callback (&_audio_sink_callback_state_updated, bluetoothaudiosink) != 0) {
        GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_register_state_update_callback() failed");
      }
    }
  } else {
    GST_INFO_OBJECT (bluetoothaudiosink, "Bluetooth Audio Sink service is now unvailable");
  }
}

static void _audio_sink_initialize (GstBluetoothAudioSink *bluetoothaudiosink)
{
  _audio_sink_clear (bluetoothaudiosink);

  /* Register for the Bluetooth Audio Sink service updates... */
  if (bluetoothaudiosink_register_operational_state_update_callback (&_audio_sink_callback_operational_state_updated, bluetoothaudiosink) != 0) {
    GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_register_operational_state_update_callback() failed");
  } else {
    GST_INFO_OBJECT (bluetoothaudiosink, "Successfully registered to Bluetooth Audio Sink service operational callback");
  }
}

static void _audio_sink_dispose (GstBluetoothAudioSink *bluetoothaudiosink)
{
  bluetoothaudiosink->acquired = FALSE;
  bluetoothaudiosink->request_acquire = FALSE;

  bluetoothaudiosink_unregister_state_update_callback (&_audio_sink_callback_state_updated);
  bluetoothaudiosink_unregister_operational_state_update_callback (&_audio_sink_callback_operational_state_updated);
  bluetoothaudiosink_dispose ();
}


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

  _audio_sink_initialize (bluetoothaudiosink);
}

void gst_bluetoothaudiosink_set_property (GObject *object, guint property_id, const GValue *value, GParamSpec *pspec)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (object);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "set_property");

  switch (property_id) {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, property_id, pspec);
      break;
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
  }
}

void gst_bluetoothaudiosink_dispose (GObject *object)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (object);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "dispose");

  /* clean up as as fast possible.  may be called multiple times */
  _audio_sink_dispose (bluetoothaudiosink);

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
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosink, "open");

  if (!_audio_sink_acquire (bluetoothaudiosink)) {
    GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to acquire Bluetooth audio sink device!");
    result = FALSE;
  }

  return result;
}

/* prepare resources and state to operate with the given specs */
static gboolean gst_bluetoothaudiosink_prepare (GstAudioSink *sink, GstAudioRingBufferSpec *spec)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosink, "prepare");

  if (!_audio_sink_start (bluetoothaudiosink)) {
    GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to start playback over Bluetooth audio sink device!");
    result = FALSE;
  }

  return result;
}

/* undo anything that was done in prepare() */
static gboolean gst_bluetoothaudiosink_unprepare (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosink, "unprepare");

  if (!_audio_sink_stop (bluetoothaudiosink)) {
    GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to stop Bluetooth audio playback!");
    result = FALSE;
  }

  return result;
}

/* close the device */
static gboolean gst_bluetoothaudiosink_close (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosink, "close");

  if (!_audio_sink_relinquish (bluetoothaudiosink)) {
    GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to relinquish the Bluetooth audio sink device!");
    result = FALSE;
  }

  return result;
}

/* write samples to the device */
static gint gst_bluetoothaudiosink_write (GstAudioSink *sink, gpointer data, guint length)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  gint result = _audio_sink_frame (bluetoothaudiosink, data, length);

  return result;
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

