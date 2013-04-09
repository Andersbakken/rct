#ifndef Size_h
#define Size_h

struct Size
{
    Size(int ww = 00, int hh = 0) : w(ww), h(hh) {}

    bool operator==(const Size &other) const { return (w == other.w && h == other.h); }
    bool operator!=(const Size &other) const { return !operator==(other); }

    bool isEmpty() const { return !w && !h; }
    bool isNull() const { return isEmpty(); }

    int w, h;
};

#endif
