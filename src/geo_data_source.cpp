//
// Created by Yucheng Soku on 2023/1/29.
//
#include "geo_data_source.h"


GeoDataSource::GeoDataSource(const char* path)
    :Path(path)
{
    GDALAllRegister();
    Content = (GDALDataset*) GDALOpenEx(Path, GDAL_OF_VECTOR, NULL, NULL, NULL);
    if (Content == NULL)
    {
        std::cout << "No shp file was founded, vector mask function will not be activated." << std::endl;
    }
}

GeoDataSource::~GeoDataSource()
{
    GDALClose(Content);
}

OGRGeometry* GeoDataSource::getGeometry(int iLayer, int iFeature)
{
    auto layer = Content->GetLayer(iLayer);
    auto feature = layer->GetFeature(iFeature);
    return feature->GetGeometryRef()->clone();
}

const OGRSpatialReference* GeoDataSource::getSpatialReference(int iLayer)
{
    auto layer = Content->GetLayer(iLayer);
    return layer->GetSpatialRef();
}

char* GeoDataSource::getSpatialReferenceWkt(int iLayer)
{
    char* pszWKT = nullptr;
    Content->GetLayer(iLayer)->GetSpatialRef()->exportToWkt(&pszWKT);;
    return pszWKT;
}

OGREnvelope GeoDataSource::getEnvelope(int iLayer, int iFeature)
{
    OGREnvelope envelope;
    getGeometry(iLayer, iFeature)->getEnvelope(&envelope);
    return envelope;
}

void GeoDataSource::buildMask(const char* path, int width, int height, int iLayer, int iField)
{
    char** buildOptions = NULL;

    buildOptions = CSLAddString(buildOptions, "-i");

    buildOptions = CSLAddString(buildOptions, "-a");
    buildOptions = CSLAddString(buildOptions, Content->GetLayer(iLayer)->GetLayerDefn()->GetFieldDefn(iField)->GetNameRef());

    buildOptions = CSLAddString(buildOptions, "-l");
    buildOptions = CSLAddString(buildOptions, Content->GetLayer(iLayer)->GetName());

    buildOptions = CSLAddString(buildOptions, "-init");
    buildOptions = CSLAddString(buildOptions, "255");

    buildOptions = CSLAddString(buildOptions, "-of");
    buildOptions = CSLAddString(buildOptions, "BMP");

    buildOptions = CSLAddString(buildOptions, "-a_srs");
    buildOptions = CSLAddString(buildOptions, getSpatialReferenceWkt(iLayer));

    buildOptions = CSLAddString(buildOptions, "-ot");
    buildOptions = CSLAddString(buildOptions, "byte");

    buildOptions = CSLAddString(buildOptions, "-ts");
    buildOptions = CSLAddString(buildOptions, std::to_string(width).c_str());
    buildOptions = CSLAddString(buildOptions, std::to_string(height).c_str());

    auto options = GDALRasterizeOptionsNew(buildOptions, NULL);

    ///
    int error;
    GDALClose(GDALRasterize(path, NULL, Content, options, &error));
    GDALRasterizeOptionsFree(options);
}
