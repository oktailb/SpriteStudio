#ifndef __EXPORT_H__
#define __EXPORT_H__

#include <QString>

class Extractor;

enum Format {
    FORMAT_TEXTUREPACKER_JSON,
    FORMAT_SPARROW_XML,
    FORMAT_PHASER_JSON,
    FORMAT_CSS_SPRITES,
    FORMAT_UNITY,
    FORMAT_GODOT,
    FORMAT_ASEPRITE_JSON
};

enum AtlasStrategy {
    ATLASSTRATEGY_ORIGINAL_ATLAS,
    ATLASSTRATEGY_ONE_ATLAS_FOR_ALL_ANIMATIONS,
    ATLASSTRATEGY_ONE_ATLAS_PER_ANIMATION
};

struct ExportOptions {
    Format format;
    bool trimSprites;
    bool rotateSprites;
    int padding;
    bool compressJson;
    bool embedAnimations;
    QString namingConvention;
};

class ExportManager {
public:

    virtual bool exportFrames(const QString &basePath, const QString &projectName, Extractor* in) = 0;
};

#endif
