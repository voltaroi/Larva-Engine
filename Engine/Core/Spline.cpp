#include "Spline.h"
#include <cmath>
#include <algorithm>

Spline::Point Spline::catmullRom(Point p0, Point p1, Point p2, Point p3, float t)
{
    auto knot = [](float ti, Point a, Point b)
    { return ti + std::sqrt(std::hypot(b.x - a.x, b.z - a.z)); };
    float t0 = 0.0f;
    float t1 = knot(t0, p0, p1);
    float t2 = knot(t1, p1, p2);
    float t3 = knot(t2, p2, p3);
    float u = t1 + (t2 - t1) * t;

    auto lerp = [u](Point a, Point b, float ta, float tb)
    {
        if (tb - ta < 1e-6f)
            return a;
        float wa = (tb - u) / (tb - ta), wb = (u - ta) / (tb - ta);
        return Point{a.x * wa + b.x * wb, a.z * wa + b.z * wb};
    };
    Point a1 = lerp(p0, p1, t0, t1), a2 = lerp(p1, p2, t1, t2), a3 = lerp(p2, p3, t2, t3);
    Point b1 = lerp(a1, a2, t0, t2), b2 = lerp(a2, a3, t1, t3);
    return lerp(b1, b2, t1, t2);
}

std::vector<Spline::Point> Spline::sampleClosed(const std::vector<Point> &points, int perSegment)
{
    const int count = (int)points.size();
    std::vector<Point> dense;
    if (count < 2)
        return points;
    dense.reserve(count * perSegment);
    auto cp = [&](int k)
    { return points[(k % count + count) % count]; };
    for (int i = 0; i < count; ++i)
        for (int s = 0; s < perSegment; ++s)
            dense.push_back(catmullRom(cp(i - 1), cp(i), cp(i + 1), cp(i + 2), (float)s / perSegment));
    return dense;
}

std::vector<Spline::Point> Spline::sampleOpen(const std::vector<Point> &ctrl, int perSegment)
{
    const int count = (int)ctrl.size();
    if (count < 2)
        return ctrl;
    std::vector<Point> dense;
    dense.reserve((count - 1) * perSegment + 1);
    for (int i = 0; i < count - 1; ++i)
    {
        Point p0 = ctrl[std::max(0, i - 1)], p1 = ctrl[i], p2 = ctrl[i + 1], p3 = ctrl[std::min(count - 1, i + 2)];
        if (i == 0)
            p0 = {2 * p1.x - p2.x, 2 * p1.z - p2.z};
        if (i == count - 2)
            p3 = {2 * p2.x - p1.x, 2 * p2.z - p1.z};
        for (int s = 0; s < perSegment; ++s)
            dense.push_back(catmullRom(p0, p1, p2, p3, (float)s / perSegment));
    }
    dense.push_back(ctrl[count - 1]);
    return dense;
}

float Spline::closedLength(const std::vector<Point> &poly)
{
    float total = 0.0f;
    for (size_t i = 0; i < poly.size(); ++i)
    {
        const Point &a = poly[i], &b = poly[(i + 1) % poly.size()];
        total += std::hypot(b.x - a.x, b.z - a.z);
    }
    return total;
}

std::vector<Spline::Point> Spline::resampleClosed(const std::vector<Point> &dense, float targetStep,
                                                  float *totalLength, float *actualStep)
{
    std::vector<Point> out;
    if (dense.size() < 2)
        return out;
    std::vector<float> cumul(dense.size() + 1, 0.0f);
    for (size_t i = 0; i < dense.size(); ++i)
    {
        const Point &a = dense[i], &b = dense[(i + 1) % dense.size()];
        cumul[i + 1] = cumul[i] + std::hypot(b.x - a.x, b.z - a.z);
    }
    float length = cumul.back();
    int n = std::max(3, (int)(length / targetStep));
    float step = length / n;
    out.reserve(n);
    size_t j = 0;
    for (int s = 0; s < n; ++s)
    {
        float d = s * step;
        while (cumul[j + 1] < d)
            ++j;
        const Point &a = dense[j], &b = dense[(j + 1) % dense.size()];
        float f = (d - cumul[j]) / std::max(1e-6f, cumul[j + 1] - cumul[j]);
        out.push_back({a.x + (b.x - a.x) * f, a.z + (b.z - a.z) * f});
    }
    if (totalLength)
        *totalLength = length;
    if (actualStep)
        *actualStep = step;
    return out;
}
