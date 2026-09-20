AnimatedBitmaps example
=======================

This example includes the 64x64 bitmap header robot.h in the sketch folder.
The bitmap is loaded once and shared by all five bitmap windows.

Hardware observation during development: five moving bitmap windows showed no
visible tearing in testing, although overlap involving transparent regions can
show some visual glitching. This observation is empirical and does not prove
the AMT630A internally synchronizes bitmap updates to VSYNC.