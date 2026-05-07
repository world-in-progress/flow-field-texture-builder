#pragma once

#include <iostream>

#include <ogr_core.h>

#include "gdal_utils.h"
#include "ogrsf_frmts.h"
#include "gdal_priv.h"
#include "ogr_geometry.h"

class GeoDataSource
{
private:
    const char* Path;
    GDALDataset* Content;
public:
    GeoDataSource(const char* path);
    ~GeoDataSource();

    OGRGeometry* getGeometry(int iLayer=0, int iFeature=0);
    const OGRSpatialReference* getSpatialReference(int iLayer=0);
    char* getSpatialReferenceWkt(int iLayer=0);
    OGREnvelope getEnvelope(int iLayer=0, int iFeature=0);
    void buildMask(const char* path, int width, int height, int iLayer=0, int iField=0);
};
