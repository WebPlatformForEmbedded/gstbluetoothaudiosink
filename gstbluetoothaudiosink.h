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
 * Free Software Foundation, Inc., 51 Franklin St, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifndef _GST_BLUETOOTHAUDIOSINK_H_
#define _GST_BLUETOOTHAUDIOSINK_H_

#include <gst/audio/gstaudiosink.h>

G_BEGIN_DECLS

#define GST_TYPE_BLUETOOTHAUDIOSINK   (gst_bluetoothaudiosink_get_type())
#define GST_BLUETOOTHAUDIOSINK(obj)   (G_TYPE_CHECK_INSTANCE_CAST((obj),GST_TYPE_BLUETOOTHAUDIOSINK,GstBluetoothAudioSink))
#define GST_BLUETOOTHAUDIOSINK_CLASS(klass)   (G_TYPE_CHECK_CLASS_CAST((klass),GST_TYPE_BLUETOOTHAUDIOSINK,GstBluetoothAudioSinkClass))
#define GST_IS_BLUETOOTHAUDIOSINK(obj)   (G_TYPE_CHECK_INSTANCE_TYPE((obj),GST_TYPE_BLUETOOTHAUDIOSINK))
#define GST_IS_BLUETOOTHAUDIOSINK_CLASS(obj)   (G_TYPE_CHECK_CLASS_TYPE((klass),GST_TYPE_BLUETOOTHAUDIOSINK))

typedef struct _GstBluetoothAudioSink GstBluetoothAudioSink;
typedef struct _GstBluetoothAudioSinkClass GstBluetoothAudioSinkClass;

struct _GstBluetoothAudioSink
{
  GstAudioSink base_bluetoothaudiosink;

  // private:
  gboolean request_acquire;
  gboolean request_playback;
  gboolean acquired;
  gboolean playing;
};

struct _GstBluetoothAudioSinkClass
{
  GstAudioSinkClass base_bluetoothaudiosink_class;
};

GType gst_bluetoothaudiosink_get_type (void);

G_END_DECLS

#endif // _GST_BLUETOOTHAUDIOSINK_H_
