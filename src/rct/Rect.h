#ifndef Rect_h
#define Rect_h

struct Rect
{
    Rect(int xx = 0, int yy = 0, int ww = 00, int hh = 0) : x(xx), y(yy), w(ww), h(hh) {}

    int x, y, w, h;
};

#endif
