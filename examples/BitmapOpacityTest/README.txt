BitmapOpacityTest
=================

This example intentionally does not include robot.h.

Copy a generated AMT630A bitmap header named robot.h into this directory.
The header must define an AMT630A_OSD::Bitmap object named `robot`.

The example loads the bitmap once, enables global OSD/video blending at
opacity 4, places the robot at the bottom of the configured display, and
animates it left/right with moveWindow(). Serial keys 0..7 change opacity,
+/- adjust brightness, and x toggles blending.