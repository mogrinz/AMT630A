BlinkDemo
=========

Creates three visible LargeFont text windows. Each is exactly 7 characters
wide by 3 rows high and all 21 cells are populated with alphabet letters.

Blink cell coordinates are hardware-verified as 1-based: the first character
cell is (1,1), not (0,0).

The three stored blink regions are:
  Window 0: x=1, y=1, width=3, height=3
  Window 1: x=3, y=1, width=3, height=3
  Window 2: x=5, y=1, width=3, height=3

The demo starts with blinking ACTIVE on Window 0.

Controls at 115200 baud:
  0 = select Window 0 and its blink region
  1 = select Window 1 and its blink region
  2 = select Window 2 and its blink region
  b = toggle blink on/off

Hardware-verified behavior:
- The AMT630A has one primary blink engine that targets one OSD window at a time.
- Multiple OSD windows may remain visible while the selected window blinks.
- FB35 bits 2:0 select the blink window.
- Blink cell coordinates are 1-based.
- Smaller raw blink-rate values blink faster.
- Text window size registers use direct character counts.