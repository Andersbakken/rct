#ifndef Point_h
#define Point_h

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
    inline const Point operator+(const Point &p1, const Point &p2) { return Point(p1.x +p2.x , p1.y +p2.y ); }
    inline const Point operator-(const Point &p1, const Point &p2) { return Point(p1.x -p2.x , p1.y -p2.y ); }
    inline const Point operator*(const Point &p, float c) { return Point(static_cast<int>(round(p.x *c)), static_cast<int>(round(p.y *c))); }
    inline const Point operator*(const Point &p, double c) { return Point(static_cast<int>(round(p.x *c)), static_cast<int>(round(p.y *c))); }
    inline const Point operator*(const Point &p, int c) { return Point(p.x *c, p.y *c); }
    inline const Point operator*(float c, const Point &p) { return Point(static_cast<int>(round(p.x *c)), static_cast<int>(round(p.y *c))); }
    inline const Point operator*(double c, const Point &p) { return Point(static_cast<int>(round(p.x *c)), static_cast<int>(round(p.y *c))); }
    inline const Point operator*(int c, const Point &p) { return Point(p.x *c, p.y *c); }
    inline const Point operator-(const Point &p) { return Point(-p.x , -p.y ); }
    inline Point &operator/=(double c) {x = round(x / c); y = round(y /c); return *this;}
    inline const Point operator/(const Point &p, double c) { return Point(static_cast<int>(round(p.x / c)), static_cast<int>(round(p.y / c))); }

    int x, y;
};

#endif
