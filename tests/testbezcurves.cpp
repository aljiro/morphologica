#include "BezCurve.h"
#include "BezCurvePath.h"
#include <utility>
#include <iostream>
#include <fstream>
#include <math.h>

using namespace std;
using morph::BezCoord;
using morph::BezCurve;
using morph::BezCurvePath;

int main()
{
    int rtn = -1;

    // Make some control points
    pair<float,float> i, f, c1, c2;
    i.first = 1;
    i.second = 1;
    c1.first = 5;
    c1.second = 5;
    c2.first = 2;
    c2.second = -4;
    f.first = 10;
    f.second = 1;
    // Make a cubic curve
    BezCurve cc3(i, f, c1, c2);

    // Make a second quartic curve.
    vector<pair<float, float>> quart;
    quart.push_back (f);
    quart.push_back (make_pair(10,10));
    quart.push_back (make_pair(10,0));
    quart.push_back (make_pair(12,-5));
    quart.push_back (make_pair(14,0));
    BezCurve cc4(quart);

    // Put em in a BezCurvePath
    BezCurvePath bcp;
    bcp.name = "testbezcurves";
    bcp.addCurve (cc3);
    bcp.addCurve (cc4);

    unsigned int nPoints = 201;
    vector<BezCoord> points = bcp.getPoints (nPoints);

    for (auto p : points) {
        cout << p.x() << "," << p.y() << endl;
    }

    if (points.size() == nPoints) {
        rtn = 0;
    }

    return rtn;
}
