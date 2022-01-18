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

static void _exit_handler (void)
{
  bluetoothaudiosink_dispose ();
}

static void _audio_sink_install_dispose_handler (void)
{
  static gboolean installed = FALSE;

  if (!installed) {
    atexit (_exit_handler);
  }
}

static void _audio_sink_clear (GstBluetoothAudioSink* bluetoothaudiosink)
{
  g_assert (bluetoothaudiosink != NULL);

  g_mutex_lock (&bluetoothaudiosink->lock);

  /* Not playing now and don't want playback. */
  bluetoothaudiosink->request_acquire = FALSE;
  bluetoothaudiosink->request_playback = FALSE;
  bluetoothaudiosink->request_reset = FALSE;
  bluetoothaudiosink->acquired = FALSE;
  bluetoothaudiosink->playing = FALSE;

  /* Let's start with some sensible format. */
  bluetoothaudiosink->sample_rate = 48000;
  bluetoothaudiosink->frame_rate = 10000; /* 100 Hz */
  bluetoothaudiosink->channels = 2;
  bluetoothaudiosink->bpf = 4; /* bits per frame */
  bluetoothaudiosink->bps = 2; /* bits per sample */

  g_mutex_unlock (&bluetoothaudiosink->lock);
}

static gboolean _audio_sink_acquire (GstBluetoothAudioSink *bluetoothaudiosink, gboolean postpone, guint32 sample_rate, guint16 frame_rate, guint8 bpf, guint8 bps)
{
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;
  gboolean result = FALSE;

  g_assert (bluetoothaudiosink != NULL);

  bluetoothaudiosink_state (&state);

  g_mutex_lock (&bluetoothaudiosink->lock);

  if ((state == BLUETOOTHAUDIOSINK_STATE_CONNECTED) || ((bluetoothaudiosink->acquired) && (state == BLUETOOTHAUDIOSINK_STATE_READY))) {
    bluetoothaudiosink->request_acquire = FALSE;

    if ((bluetoothaudiosink->acquired)
      && (sample_rate != bluetoothaudiosink->sample_rate) || ( bluetoothaudiosink->frame_rate != frame_rate) || (bluetoothaudiosink->bps != bps) || (bpf != bluetoothaudiosink->bpf)) {

      /* It is us still holding the lock, so release it. */
      if (bluetoothaudiosink_relinquish () != 0) {
        GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_relinquish() failed");
      }

      bluetoothaudiosink->acquired = FALSE;
    }


    if (!bluetoothaudiosink->acquired) {
      bluetoothaudiosink_format_t format;

      if (sample_rate != 0) {
        /* (Otherwise use previously set format.) */
        bluetoothaudiosink->sample_rate = sample_rate;
        bluetoothaudiosink->frame_rate = frame_rate;
        bluetoothaudiosink->channels = (bpf / bps);
        bluetoothaudiosink->bps = bps;
        bluetoothaudiosink->bpf = bpf;
      }

      format.sample_rate = bluetoothaudiosink->sample_rate;
      format.frame_rate = bluetoothaudiosink->frame_rate;
      format.channels = bluetoothaudiosink->channels;
      format.resolution = (bluetoothaudiosink->bps * 8);

      if (bluetoothaudiosink_acquire (&format) != 0) {
        GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_acquire() failed");
      } else {
        GST_INFO_OBJECT (bluetoothaudiosink, "Starting Bluetooth playback session...");
        bluetoothaudiosink->acquired = TRUE;
        result = TRUE;
      }
    } else {
      result = TRUE;
      GST_INFO_OBJECT (bluetoothaudiosink, "Already acquired with same parameters...");
    }
  } else if (postpone && ((state != BLUETOOTHAUDIOSINK_STATE_READY) || (state != BLUETOOTHAUDIOSINK_STATE_STREAMING))) {
    GST_INFO_OBJECT (bluetoothaudiosink, "Device not yet connected, will acquire it when it connects");
    bluetoothaudiosink->request_acquire = TRUE;
    result = TRUE;
  }

  g_mutex_unlock (&bluetoothaudiosink->lock);

  return result;
}

static gboolean _audio_sink_relinquish (GstBluetoothAudioSink *bluetoothaudiosink)
{
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;
  gboolean result = TRUE;

  g_assert (bluetoothaudiosink != NULL);

  bluetoothaudiosink_state (&state);

  g_mutex_lock (&bluetoothaudiosink->lock);

  bluetoothaudiosink->request_acquire = FALSE;
  bluetoothaudiosink->request_playback = FALSE;

  if ((state == BLUETOOTHAUDIOSINK_STATE_READY) || (state == BLUETOOTHAUDIOSINK_STATE_STREAMING)) {
    if (bluetoothaudiosink_relinquish () != 0) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_relinquish() failed");
      result = FALSE;
    }

    bluetoothaudiosink->acquired = FALSE;
  }

  g_mutex_unlock (&bluetoothaudiosink->lock);

  return result;
}

static gboolean _audio_sink_start (GstBluetoothAudioSink *bluetoothaudiosink)
{
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;
  gboolean result = TRUE;

  g_assert (bluetoothaudiosink != NULL);

  bluetoothaudiosink_state (&state);

  g_mutex_lock (&bluetoothaudiosink->lock);

  if (state == BLUETOOTHAUDIOSINK_STATE_READY) {
    if (bluetoothaudiosink_speed (100) != 0) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_speed(100) failed");
      result = FALSE;
    } else {
      GST_INFO_OBJECT (bluetoothaudiosink, "Now streaming audio over Bluetooth!");
      bluetoothaudiosink->playing = TRUE;
    }
  } else if (state != BLUETOOTHAUDIOSINK_STATE_STREAMING) {
    GST_INFO_OBJECT (bluetoothaudiosink, "Device not yet connected, will start playback once connected");
  }

  g_mutex_unlock (&bluetoothaudiosink->lock);

  return result;
}

static gboolean _audio_sink_stop (GstBluetoothAudioSink *bluetoothaudiosink)
{
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;
  gboolean result = TRUE;

  g_assert (bluetoothaudiosink != NULL);

  bluetoothaudiosink_state (&state);

  g_mutex_lock (&bluetoothaudiosink->lock);

  bluetoothaudiosink->request_playback = FALSE;

  if (state == BLUETOOTHAUDIOSINK_STATE_STREAMING) {
    if (bluetoothaudiosink_speed (0) != 0) {
      GST_ERROR_OBJECT( bluetoothaudiosink, "bluetoothaudiosink_speed(0) failed");
      result = FALSE;
    }

    bluetoothaudiosink->playing = FALSE;
  }

  g_mutex_unlock (&bluetoothaudiosink->lock);

  return result;
}

static gint _audio_sink_frame (GstBluetoothAudioSink *bluetoothaudiosink, const gpointer data, const guint size)
{
  gint result = 0;

  g_assert (bluetoothaudiosink != NULL);

  g_mutex_lock (&bluetoothaudiosink->lock);

  if (bluetoothaudiosink->playing) {
    uint16_t played = 0;

    g_mutex_unlock (&bluetoothaudiosink->lock);

    /* This is a blocking call. */
    if (bluetoothaudiosink_frame (size, data, &played) != 0) {
      GST_ERROR_OBJECT( bluetoothaudiosink, "bluetoothaudiosink_frame() failed");
    } else {
      result = played;
    }
  } else {
    if (bluetoothaudiosink->request_reset) {
      /* A rather silly trick to ensure the write loop in audiosink is broken. */
      result = size;
      bluetoothaudiosink->request_reset = FALSE;
    }

    g_mutex_unlock (&bluetoothaudiosink->lock);

  }

  return result;
}

static guint _audio_sink_delay (GstBluetoothAudioSink *bluetoothaudiosink)
{
  guint result = 0;

  g_assert (bluetoothaudiosink != NULL);

  g_mutex_lock (&bluetoothaudiosink->lock);

  if (bluetoothaudiosink->playing) {
    uint32_t delay = 0;
    if (bluetoothaudiosink_delay (&delay) != 0) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_delay() failed");
    } else {
      /* In fact the requested value is measured in frames, not samples. */
      result = (delay / bluetoothaudiosink->channels);
    }
  }

  g_mutex_unlock (&bluetoothaudiosink->lock);

  return result;
}

static void _audio_sink_reset (GstBluetoothAudioSink *bluetoothaudiosink)
{
  g_mutex_lock (&bluetoothaudiosink->lock);

  bluetoothaudiosink->request_reset = TRUE;

  g_mutex_unlock (&bluetoothaudiosink->lock);
}

static void _audio_sink_callback_connected (void *user_data)
{
  GstBluetoothAudioSink *bluetoothaudiosink = (GstBluetoothAudioSink*)user_data;

  g_assert (bluetoothaudiosink != NULL);

  if (bluetoothaudiosink->request_acquire) {
    if (!_audio_sink_acquire (bluetoothaudiosink, TRUE, 0, 0, 0, 0)) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to acquire Bluetooth audio sink device!");
    }
  } else {
    GST_DEBUG_OBJECT (bluetoothaudiosink, "Sink connected, but acquiring not requested");
  }

  if (bluetoothaudiosink->request_playback) {
    if (!_audio_sink_start (bluetoothaudiosink)) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to start playback on the Bluetooth audio sink device!");
    }
  }
}

static void _audio_sink_callback_disconnected (void *user_data)
{
  GstBluetoothAudioSink *bluetoothaudiosink = (GstBluetoothAudioSink*)user_data;

  g_assert (bluetoothaudiosink != NULL);

  g_mutex_lock (&bluetoothaudiosink->lock);

  if (bluetoothaudiosink->playing) {
    bluetoothaudiosink->playing = FALSE;
    bluetoothaudiosink->request_playback = TRUE;
  }

  if (bluetoothaudiosink->acquired) {
    bluetoothaudiosink->acquired = FALSE;
    bluetoothaudiosink->request_acquire = TRUE;
  }

  g_mutex_unlock (&bluetoothaudiosink->lock);
}

static void _audio_sink_callback_state_updated (void *user_data)
{
  GstBluetoothAudioSink *bluetoothaudiosink = (GstBluetoothAudioSink*)user_data;
  bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;

  g_assert (bluetoothaudiosink != NULL);

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

static void _audio_sink_callback_operational_state_updated (const uint8_t running, void *user_data)
{
  GstBluetoothAudioSink *bluetoothaudiosink = (GstBluetoothAudioSink*)user_data;

  g_assert (bluetoothaudiosink != NULL);

  if (running) {
    bluetoothaudiosink_state_t state = BLUETOOTHAUDIOSINK_STATE_UNKNOWN;

    bluetoothaudiosink_state (&state);

    if (state == BLUETOOTHAUDIOSINK_STATE_UNKNOWN) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "Unknown Bluetooth Audio Sink failure!");
    } else {
      GST_INFO_OBJECT (bluetoothaudiosink, "Bluetooth Audio Sink service now available");

      /* Register for the sink updates... */
      if (bluetoothaudiosink_register_state_update_callback (&_audio_sink_callback_state_updated, bluetoothaudiosink) != 0) {
        GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_register_state_update_callback() failed");
      } else {
        GST_INFO_OBJECT (bluetoothaudiosink, "Successfully registered to Bluetooth Audio Sink status update callback");
      }
    }
  } else {
    GST_INFO_OBJECT (bluetoothaudiosink, "Bluetooth Audio Sink service is now unvailable");
  }
}

static void _audio_sink_initialize (GstBluetoothAudioSink *bluetoothaudiosink)
{
  g_mutex_init (&bluetoothaudiosink->lock);

  _audio_sink_clear (bluetoothaudiosink);

  /* Register for the Bluetooth Audio Sink service updates... */
  if (bluetoothaudiosink_register_operational_state_update_callback (&_audio_sink_callback_operational_state_updated, bluetoothaudiosink) != 0) {
    GST_ERROR_OBJECT (bluetoothaudiosink, "bluetoothaudiosink_register_operational_state_update_callback() failed");
  } else {
    GST_INFO_OBJECT (bluetoothaudiosink, "Successfully registered to Bluetooth Audio Sink service operational callback");
  }

  _audio_sink_install_dispose_handler ();
}

static void _audio_sink_dispose (GstBluetoothAudioSink *bluetoothaudiosink)
{
  bluetoothaudiosink_unregister_state_update_callback (&_audio_sink_callback_state_updated);
  bluetoothaudiosink_unregister_operational_state_update_callback (&_audio_sink_callback_operational_state_updated);
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
      "rate={32000,44100,48000}," /* Standard sample rates required to be supported by all sink devices.*/
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
  _audio_sink_clear (bluetoothaudiosink);

  G_OBJECT_CLASS (gst_bluetoothaudiosink_parent_class)->dispose (object);
}

void gst_bluetoothaudiosink_finalize (GObject *object)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (object);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "finalize");

  /* clean up object here */
  _audio_sink_dispose (bluetoothaudiosink);

  g_mutex_clear (&bluetoothaudiosink->lock);

  G_OBJECT_CLASS (gst_bluetoothaudiosink_parent_class)->finalize (object);
}

/* open the device with given specs */
static gboolean gst_bluetoothaudiosink_open (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);
  gboolean result = TRUE;

  GST_DEBUG_OBJECT (bluetoothaudiosink, "open");

  /* Lock the device now. */
  if (!_audio_sink_acquire (bluetoothaudiosink, FALSE, 0, 0, 0, 0)) {
    GST_WARNING_OBJECT (bluetoothaudiosink, "Bluetooth audio device not available!");
    result = FALSE;
  }

  return result;
}

/* prepare resources and state to operate with the given specs */
static gboolean gst_bluetoothaudiosink_prepare (GstAudioSink *sink, GstAudioRingBufferSpec *spec)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);
  gboolean result = TRUE;

  const guint32 sample_rate = GST_AUDIO_INFO_RATE (&spec->info);
  const guint8 bpf = GST_AUDIO_INFO_BPF (&spec->info);
  const guint8 bps = GST_AUDIO_INFO_BPS (&spec->info);
  const guint16 frame_rate = ((sample_rate * bpf * 100) / spec->segsize);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "prepare");

  GST_INFO_OBJECT (bluetoothaudiosink, "rate=%iHz, channels=%i, bpf=%i, bps=%i, framerate=%i.%02ifps, segsize=%i, segtotal=%i, latencytime=%ius",
                   sample_rate, (bpf/bps), bpf, bps, (frame_rate/100), (frame_rate%100), spec->segsize, spec->segtotal, spec->latency_time);

  if (!_audio_sink_acquire (bluetoothaudiosink, TRUE, sample_rate, frame_rate, bpf, bps)) {
    GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to acquire Bluetooth audio sink device!");
    result = FALSE;
  }

  if (result) {
    if (!_audio_sink_start (bluetoothaudiosink)) {
      GST_ERROR_OBJECT (bluetoothaudiosink, "Failed to start playback over Bluetooth audio sink device!");
      result = FALSE;
    }
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

  // GST_DEBUG_OBJECT (bluetoothaudiosink, "write");

  const gint result = _audio_sink_frame (bluetoothaudiosink, data, length);

  return result;
}

/* get number of samples queued in the device */
static guint gst_bluetoothaudiosink_delay (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  // GST_DEBUG_OBJECT (bluetoothaudiosink, "delay");

  const guint result = _audio_sink_delay (bluetoothaudiosink);

  return result;
}

/* reset the audio device, unblock from a write */
static void gst_bluetoothaudiosink_reset (GstAudioSink *sink)
{
  GstBluetoothAudioSink *bluetoothaudiosink = GST_BLUETOOTHAUDIOSINK (sink);

  GST_DEBUG_OBJECT (bluetoothaudiosink, "reset");

  _audio_sink_reset (bluetoothaudiosink);
}

static gboolean plugin_init (GstPlugin *plugin)
{
  return gst_element_register (plugin, "bluetoothaudiosink", (GST_RANK_PRIMARY + 100 /* whoa! */), GST_TYPE_BLUETOOTHAUDIOSINK);
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

