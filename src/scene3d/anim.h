#pragma once

#include "formats/xfile.h"

#include <vector>

namespace Scene3d {

XFile::Matrix Multiply(const XFile::Matrix& a, const XFile::Matrix& b);

XFile::Matrix FromQuaternion(float w, float x, float y, float z);

XFile::Matrix Compose(const float scale[3], const float quat[4], const float pos[3]);

XFile::Matrix SampleChannel(const XFile::AnimationChannel& channel, float time);

int LoopTicks(const XFile::Scene& scene);

void SampleLocals(const XFile::Scene& scene, float time, std::vector<XFile::Matrix>& locals);

void ComputeWorlds(const XFile::Scene& scene, const std::vector<XFile::Matrix>& locals,
                   std::vector<XFile::Matrix>& worlds);

bool InvertAffine(const XFile::Matrix& m, XFile::Matrix& out);

}
