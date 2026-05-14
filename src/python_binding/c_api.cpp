#include "flow_field_builder.h"

#include <cstddef>
#include <exception>
#include <filesystem>
#include <stdexcept>
#include <string>

#if defined(_WIN32)
#define FFB_API __declspec(dllexport)
#else
#define FFB_API __attribute__((visibility("default")))
#endif

namespace {

thread_local std::string g_lastError;

using BuilderHandle = FlowFieldBuilder*;

int failWithCurrentException()
{
    try
    {
        throw;
    }
    catch (const std::exception& error)
    {
        g_lastError = error.what();
    }
    catch (...)
    {
        g_lastError = "unknown flow-field builder error";
    }
    return -1;
}

template <typename Function>
int runChecked(Function&& function)
{
    try
    {
        function();
        g_lastError.clear();
        return 0;
    }
    catch (...)
    {
        return failWithCurrentException();
    }
}

BuilderHandle checkedHandle(void* handle)
{
    if (handle == nullptr)
    {
        throw std::invalid_argument("builder handle must not be null");
    }
    return static_cast<BuilderHandle>(handle);
}

std::filesystem::path checkedPath(const char* path, const char* name)
{
    if (path == nullptr)
    {
        throw std::invalid_argument(std::string(name) + " must not be null");
    }
    return std::filesystem::path(path);
}

std::string checkedString(const char* value, const char* name)
{
    if (value == nullptr)
    {
        throw std::invalid_argument(std::string(name) + " must not be null");
    }
    return std::string(value);
}

} // namespace

extern "C" {

FFB_API const char* ffb_last_error()
{
    return g_lastError.c_str();
}

FFB_API void* ffb_create(const char* outputDirectory)
{
    try
    {
        g_lastError.clear();
        if (outputDirectory == nullptr)
        {
            return new FlowFieldBuilder();
        }
        return new FlowFieldBuilder(std::filesystem::path(outputDirectory));
    }
    catch (...)
    {
        failWithCurrentException();
        return nullptr;
    }
}

FFB_API void ffb_destroy(void* handle)
{
    delete static_cast<BuilderHandle>(handle);
}

FFB_API int ffb_set_output_directory(void* handle, const char* outputDirectory)
{
    return runChecked([&] {
        checkedHandle(handle)->setOutputDirectory(checkedPath(outputDirectory, "output_directory"));
    });
}

FFB_API int ffb_set_texture_size(void* handle, int width, int height)
{
    return runChecked([&] {
        checkedHandle(handle)->setTextureSize(width, height);
    });
}

FFB_API int ffb_set_projection_texture_size(void* handle, int width, int height)
{
    return runChecked([&] {
        checkedHandle(handle)->setProjectionTextureSize(width, height);
    });
}

FFB_API int ffb_set_quikgrid_options(
        void* handle,
        int scanRatio,
        int densityRatio,
        int edgeFactor,
        float undefinedValue,
        long sample)
{
    return runChecked([&] {
        XpandOptions options;
        options.scanRatio = scanRatio;
        options.densityRatio = densityRatio;
        options.edgeFactor = edgeFactor;
        options.undefinedValue = undefinedValue;
        options.sample = sample;
        checkedHandle(handle)->setQuikgridOptions(options);
    });
}

FFB_API int ffb_set_domain_from_arrays(
        void* handle,
        const double* xs,
        const double* ys,
        std::size_t count,
        int sourceEpsg)
{
    return runChecked([&] {
        auto* builder = checkedHandle(handle);
        if (sourceEpsg > 0)
        {
            builder->setDomainFromArrays(xs, ys, count, sourceEpsg);
        }
        else
        {
            builder->setDomainFromArrays(xs, ys, count);
        }
    });
}

FFB_API int ffb_set_domain_from_vector(
        void* handle,
        const double* xs,
        const double* ys,
        std::size_t count,
        const char* vectorPath,
        int sourceEpsg)
{
    return runChecked([&] {
        auto* builder = checkedHandle(handle);
        const auto path = checkedPath(vectorPath, "vector_path");
        if (sourceEpsg > 0)
        {
            builder->setDomainFromVector(xs, ys, count, path, sourceEpsg);
        }
        else
        {
            builder->setDomainFromVector(xs, ys, count, path);
        }
    });
}

FFB_API int ffb_build_uv_texture(
        void* handle,
        const float* us,
        const float* vs,
        std::size_t count,
        const char* outputName)
{
    return runChecked([&] {
        checkedHandle(handle)->buildUvTexture(us, vs, count, checkedString(outputName, "output_name"));
    });
}

FFB_API int ffb_build_projection_texture(
        void* handle,
        const char* target,
        const char* outputName)
{
    return runChecked([&] {
        checkedHandle(handle)->buildProjectionTexture(
                checkedString(target, "target"),
                checkedString(outputName, "output_name"));
    });
}

FFB_API int ffb_texture_width(void* handle)
{
    try
    {
        return checkedHandle(handle)->textureWidth();
    }
    catch (...)
    {
        failWithCurrentException();
        return 0;
    }
}

FFB_API int ffb_texture_height(void* handle)
{
    try
    {
        return checkedHandle(handle)->textureHeight();
    }
    catch (...)
    {
        failWithCurrentException();
        return 0;
    }
}

FFB_API int ffb_projection_texture_width(void* handle)
{
    try
    {
        return checkedHandle(handle)->projectionTextureWidth();
    }
    catch (...)
    {
        failWithCurrentException();
        return 0;
    }
}

FFB_API int ffb_projection_texture_height(void* handle)
{
    try
    {
        return checkedHandle(handle)->projectionTextureHeight();
    }
    catch (...)
    {
        failWithCurrentException();
        return 0;
    }
}

FFB_API int ffb_domain_extent(void* handle, double* output)
{
    return runChecked([&] {
        if (output == nullptr)
        {
            throw std::invalid_argument("domain extent output must not be null");
        }
        const auto& extent = checkedHandle(handle)->domainExtent();
        output[0] = extent[0];
        output[1] = extent[1];
        output[2] = extent[2];
        output[3] = extent[3];
    });
}

FFB_API int ffb_projection_extent(void* handle, double* output)
{
    return runChecked([&] {
        if (output == nullptr)
        {
            throw std::invalid_argument("projection extent output must not be null");
        }
        const auto& extent = checkedHandle(handle)->projectionExtent();
        output[0] = extent[0];
        output[1] = extent[1];
        output[2] = extent[2];
        output[3] = extent[3];
    });
}

}
