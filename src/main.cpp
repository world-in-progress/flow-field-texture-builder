#include "flow_field.h"

#include <memory>

int main(int argc, char *argv[]) {
    if (argc < 2)
    {
        std::cerr << "Usage: FlowField_TextureBuilder <description.json>" << std::endl;
        return 1;
    }

    Json resultJson;

    std::unique_ptr<FlowField> ff(FlowField::create(argv[1], resultJson));
    for(auto i = 0; i < ff->getTargetSpaceCount(); ++i)
    {
        ff->buildProjectionTexture(1024, 2048, i);
    }
    ff->buildTextures();

    std::ofstream oStream(ff->resultPath.c_str());
    oStream << std::setw(4) << resultJson << std::endl;

    return 0;
}
