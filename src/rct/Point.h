#ifndef Point_h
#define Point_h

struct Point
{
    Point(int xx = 0, int yy = 0) : x(xx), y(yy) {}

    bool operator==(const Point &other) const { return (x == other.x && y == other.y); }
    bool operator!=(const Point &other) const { return !operator==(other); }

    int x, y;
};

#endif
