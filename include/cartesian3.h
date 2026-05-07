#pragma once

class Cartesian3 {
public:
    double x;
    double y;
    double z;

    Cartesian3(double x = 0.0, double y = 0.0, double z = 0.0)
            : x { x }, y { y }, z { z }
    {
    }

    static double magnitudeSquared(Cartesian3* cartesian);

    static double magnitude(Cartesian3* cartesian);

    static Cartesian3* normalize(Cartesian3* cartesian, Cartesian3* result);

    static Cartesian3* multiplyComponents(Cartesian3* left, Cartesian3* right, Cartesian3* result);

    static double dot(Cartesian3* left, Cartesian3* right);

    static Cartesian3* divideByScalar(Cartesian3* cartesian, double scalar, Cartesian3* result);

    static Cartesian3* multiplyByScalar(Cartesian3* cartesian, double scalar, Cartesian3* result);

    static Cartesian3* add(Cartesian3* left, Cartesian3* right, Cartesian3* result);

    static Cartesian3* fromRadians(double longitude, double latitude, double height = 0.0, Cartesian3* result = nullptr);

    static Cartesian3* fromDegrees(double longitude, double latitude, double height = 0.0, Cartesian3* result = nullptr);
};
