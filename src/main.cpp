#include "flow_field.h"

int main(int argc, char *argv[]) {
    Json resultJson;

    auto ff = FlowField::create(argv[1], resultJson);
    ff->preprocess();
    for(auto i = 0; i < ff->getTargetSpaceCount(); ++i)
    {
        ff->buildProjectionTexture(1024, 2048, i);
    }
    ff->buildTextures();

    std::ofstream oStream(ff->resultPath.c_str());
    oStream << std::setw(4) << resultJson << std::endl;

    return 0;
}
