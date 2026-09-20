BitmapScalingTest
=================

This example intentionally does not include robot.h.

Copy a generated AMT630A bitmap header named robot.h into this directory.
The header must define an AMT630A_OSD::Bitmap object named `robot`.

The example loads the robot bitmap once and shares it between Window 0 and
Window 1 so both AMT630A scaler implementations can be tested through the
same setWindowScale() API.