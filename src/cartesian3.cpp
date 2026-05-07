//
// Created by Yucheng Soku on 2023/3/22.
//
#include <cmath>

#include "cartesian3.h"

namespace {
constexpr double kRadiansPerDegree = M_PI / 180.0;

double toRadians(double degrees)
{
    return degrees * kRadiansPerDegree;
}
}


double Cartesian3::magnitudeSquared(Cartesian3* cartesian)
{

    return cartesian->x * cartesian->x + cartesian->y * cartesian->y + cartesian->z * cartesian->z;
}

double Cartesian3::magnitude(Cartesian3* cartesian)
{

    return sqrt(Cartesian3::magnitudeSquared(cartesian));
}

Cartesian3* Cartesian3::normalize(Cartesian3* cartesian, Cartesian3* result)
{

    double magnitude = Cartesian3::magnitude(cartesian);
    result->x = cartesian->x / magnitude;
    result->y = cartesian->y / magnitude;
    result->z = cartesian->z / magnitude;

    return result;
}

Cartesian3* Cartesian3::multiplyComponents(Cartesian3* left, Cartesian3* right, Cartesian3* result)
{

    result->x = left->x * right->x;
    result->y = left->y * right->y;
    result->z = left->z * right->z;
    return result;
}

double Cartesian3::dot(Cartesian3* left, Cartesian3* right)
{

    return left->x * right->x + left->y * right->y + left->z * right->z;
}

Cartesian3* Cartesian3::divideByScalar(Cartesian3* cartesian, double scalar, Cartesian3* result)
{

    result->x = cartesian->x / scalar;
    result->y = cartesian->y / scalar;
    result->z = cartesian->z / scalar;

    return result;
}

Cartesian3* Cartesian3::multiplyByScalar(Cartesian3* cartesian, double scalar, Cartesian3* result)
{

    result->x = cartesian->x * scalar;
    result->y = cartesian->y * scalar;
    result->z = cartesian->z * scalar;

    return result;
}

Cartesian3* Cartesian3::add(Cartesian3* left, Cartesian3* right, Cartesian3* result)
{
    result->x = left->x + right->x;
    result->y = left->y + right->y;
    result->z = left->z + right->z;

    return result;
}

Cartesian3* wgs84RadiiSquared = new Cartesian3{
        6378137.0 * 6378137.0,
        6378137.0 * 6378137.0,
        6356752.3142451793 * 6356752.3142451793
};

Cartesian3* Cartesian3::fromRadians(double longitude, double latitude, double height, Cartesian3* result)
{
    Cartesian3* scratchN = new Cartesian3();
    Cartesian3* scratchK = new Cartesian3();
    Cartesian3* radiiSquared = wgs84RadiiSquared;
    double cosLatitude = cos(latitude);
    scratchN->x = cosLatitude * cos(longitude);
    scratchN->y = cosLatitude * sin(longitude);
    scratchN->z = sin(latitude);
    scratchN = Cartesian3::normalize(scratchN, scratchN);

    Cartesian3::multiplyComponents(radiiSquared, scratchN, scratchK);
    double gamma = sqrt(Cartesian3::dot(scratchN, scratchK));
    scratchK = Cartesian3::divideByScalar(scratchK, gamma, scratchK);
    scratchN = Cartesian3::multiplyByScalar(scratchN, height, scratchN);

    if (result == nullptr)
        result = new Cartesian3();

    return Cartesian3::add(scratchK, scratchN, result);

}

Cartesian3* Cartesian3::fromDegrees(double longitude, double latitude, double height, Cartesian3* result)
{
    longitude = toRadians(longitude);
    latitude = toRadians(latitude);

    return Cartesian3::fromRadians(longitude, latitude, height, result);
}
