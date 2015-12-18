#ifndef Point_h
#define Point_h

#include <cmath>

struct Point
{
    Point(int xx = 0, int yy = 0) : x(xx), y(yy) {}

    bool operator==(const Point &other) const { return (x == other.x && y == other.y); }
    bool operator!=(const Point &other) const { return !operator==(other); }

    inline Point &operator+=(const Point &p) { x += p.x ; y += p.y ; return *this; }
    inline Point &operator-=(const Point &p) { x -= p.x ; y -= p.y ; return *this; }

    inline Point &operator*=(float c) { x = static_cast<int>(round(x *c)); y = static_cast<int>(round(y *c)); return *this; }
    inline Point &operator*=(double c) { x = static_cast<int>(round(x *c)); y = static_cast<int>(round(y *c)); return *this; }
    inline Point &operator*=(int c) { x = x *c; y = y *c; return *this; }
    inline const Point operator*(int c) const { return Point(x * c, y * c); }
    inline const Point operator*(float c) const { return Point(x * c, y * c); }
    inline const Point operator*(double c) const { return Point(x * c, y * c); }
    inline const Point operator+(const Point &p) const { return Point(x + p.x, y + p.y ); }
    inline const Point operator-(const Point &p) const { return Point(x - p.x, y - p.y ); }
    inline const Point operator-() const { return Point(-x , -y ); }
    inline Point &operator/=(double c) {x = round(x / c); y = round(y /c); return *this;}
    inline const Point operator/(double c) const { return Point(static_cast<int>(round(x / c)), static_cast<int>(round(y / c))); }
    inline Point &operator/=(float c) {x = round(x / c); y = round(y /c); return *this;}
    inline const Point operator/(float c) const { return Point(static_cast<int>(round(x / c)), static_cast<int>(round(y / c))); }
    inline Point &operator/=(int c) {x = round(x / c); y = round(y /c); return *this;}
    inline const Point operator/(int c) const { return Point(static_cast<int>(round(x / c)), static_cast<int>(round(y / c))); }

    int x, y;
};

#endif
