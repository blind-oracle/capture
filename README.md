# capture
My very old work on V4L2 video recording.
It was developed to run on ARM board with a USB UVC camera and GPS receiver in a car.
I don't know whether it even compiles currently or not, but it may be useful to someone.

It should have the follwing capabilities:
* Read video stream from V4L2 devices in JPEG or YUYV format
* Record and split the stream into MKV files of configurable size
* Read location data from GPSD daemon and draw them on the frame like OSD
* Serve the resulting video stream and still images using an HTTP protocol
* Save the capture file history and GPS coordinates into SQLite database
