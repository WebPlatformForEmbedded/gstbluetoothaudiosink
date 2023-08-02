# gstbluetoothaudiosink
Audio Sink for tapping into the audio stream to be send to audio playing devices.

# Usage
gst-launch-1.0 filesrc location=/tmp/test.wav ! decodebin ! audioconvert ! audioresample ! bluetoothaudiosink
