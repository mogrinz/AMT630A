RobotBitmapTest
===============

This example intentionally does not include robot.h.

Copy a generated AMT630A bitmap header named robot.h into this directory.
The header must define an AMT630A_OSD::Bitmap object named `robot`.

The example loads the bitmap once, places it at the bottom of the configured
display, then animates it left/right with moveWindow().