AnimatedBitmaps example
=======================

This example requires a 64x64 bitmap header named robot.h in the same sketch
folder. Generate/copy robot.h using the AMT630A bitmap converter. The bitmap is
loaded once and shared by all five bitmap windows.

Hardware observation during development: five moving bitmap windows showed no
visible tearing in testing, although overlap involving transparent regions can
show some visual glitching. This observation is empirical and does not prove
the AMT630A internally synchronizes bitmap updates to VSYNC.