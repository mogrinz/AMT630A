MixedAnimatedWindows
====================

Hardware-tested mixed-mode animation example using all five OSD windows:

  Window 0: animated LargeFont text window
  Window 1: animated LargeFont text window
  Window 2: animated robot bitmap
  Window 3: animated robot bitmap
  Window 4: animated robot bitmap

The three bitmap windows share one copy of robot.h in Font RAM. All five
position changes are grouped into one short batch per animation frame.
Less-frequent text/color Index RAM changes use their own short batches.

Copy the same generated robot.h used by the AnimatedBitmaps example into this
sketch folder before compiling.

Hardware result: tested successfully with two moving text windows and three
moving robot bitmap windows simultaneously.