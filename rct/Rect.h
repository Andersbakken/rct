#ifndef Rect_h
#define Rect_h

#include <rct/Point.h>
#include <rct/Size.h>

struct Rect
{
    Rect(int xx = 0, int yy = 0, int ww = 00, int hh = 0) : x(xx), y(yy), w(ww), h(hh) {}
    Rect(const Point &pos, const Size &size) : x(pos.x), y(pos.y), w(size.w), h(size.h) {}

    bool operator==(const Rect &other) const { return (x == other.x && y == other.y && w == other.w && h == other.h); }
    bool operator!=(const Rect &other) const { return !operator==(other); }

    bool isEmpty() const { return !w && !h; }
    bool isNull() const { return !x && !y && !w && !h; }

    int x, y, w, h;
};

#endif
