#pragma once

#include "preset/doc/preset_document.h"

#include <string>
#include <vector>

namespace Preset::Doc {

struct DocumentPart {
    std::vector<Marker> markers = {};
    std::vector<Track> tracks = {};
};

namespace Build {

double Widen(float value);

Vec2 Widen(float x, float y);

Vec3 Widen(float x, float y, float z);

CameraSpec DefaultLens();

CameraSpec WideLens(Vec3 eye);

CameraSpec WideLensAt(Vec3 eye, Vec3 at);

std::vector<LightSpec> StandardLights();

Asset Scene3dAsset(std::string id, std::string dir);

Asset Package2dAsset(std::string id, std::string dir);

Track SpriteTrack(std::string id, std::string target, std::vector<Clip> clips);

Track ModelTrack(std::string id, std::string target, std::vector<Clip> clips);

Track CameraTrack(std::string id, std::vector<Clip> clips);

Track FxTrack(std::string id, std::vector<Clip> clips);

Track SceneTrack(std::string id, std::vector<Clip> clips);

void AppendPart(Document& document, DocumentPart part);

}

std::vector<Document> Iidx10Defaults();

std::vector<Document> Iidx11Defaults();

std::vector<Document> Iidx11SelectDefaults();

Document Iidx11Ending();

DocumentPart Iidx11EndingPartB();

}
