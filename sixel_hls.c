// sixel.c (part of mintty)
// this function is derived from a part of graphics.c
// in Xterm pl#310 originally written by Ross Combs.
//
// Copyright 2013,2014 by Ross Combs
//
//                         All Rights Reserved
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE ABOVE LISTED COPYRIGHT HOLDER(S) BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//
// Except as contained in this notice, the name(s) of the above copyright
// holders shall not be used in advertising or otherwise to promote the
// sale, use or other dealings in this Software without prior written
// authorization.

#define SIXEL_RGB(r, g, b) ((255 << 24) + ((r) << 16) + ((g) << 8) +  (b))

int
hls_to_rgb(int hue, int lum, int sat)
{
  double lv = lum / 100.0;
  double sv = sat / 100.0;
  double c, x, m, c2;
  double r1, g1, b1;
  int r, g, b, hs;

  hue = (hue + 240) % 360;
  if (sat == 0) {
    r = g = b = lum * 255 / 100;
    return SIXEL_RGB(r, g, b);
  }

  if ((c2 = ((2.0 * lv) - 1.0)) < 0.0) {
    c2 = -c2;
  }
  if ((hs = (hue % 120) - 60) < 0) {
    hs = -hs;
  }
  c = (1.0 - c2) * sv;
  x = ((60 - hs) / 60.0) * c;
  m = lv - 0.5 * c;

  switch (hue / 60) {
  case 0:
    r1 = c;
    g1 = x;
    b1 = 0.0;
    break;
  case 1:
    r1 = x;
    g1 = c;
    b1 = 0.0;
    break;
  case 2:
    r1 = 0.0;
    g1 = c;
    b1 = x;
    break;
  case 3:
    r1 = 0.0;
    g1 = x;
    b1 = c;
    break;
  case 4:
    r1 = x;
    g1 = 0.0;
    b1 = c;
    break;
  case 5:
    r1 = c;
    g1 = 0.0;
    b1 = x;
    break;
  default:
    return SIXEL_RGB(255, 255, 255);
  }

  r = (int) ((r1 + m) * 255.0 + 0.5);
  g = (int) ((g1 + m) * 255.0 + 0.5);
  b = (int) ((b1 + m) * 255.0 + 0.5);

  if (r < 0) {
    r = 0;
  } else if (r > 255) {
    r = 255;
  }
  if (g < 0) {
    g = 0;
  } else if (g > 255) {
    g = 255;
  }
  if (b < 0) {
    b = 0;
  } else if (b > 255) {
    b = 255;
  }
  return SIXEL_RGB(r, g, b);
}
