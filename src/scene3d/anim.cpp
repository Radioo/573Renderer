#include "scene3d/anim.h"

#include "formats/xfile.h"

#include <cmath>
#include <cstddef>
#include <vector>

namespace Scene3d {

namespace {

struct KeyPair {
    size_t a = 0;
    size_t b = 0;
    float t = 0.0F;
};

KeyPair Locate(const std::vector<XFile::AnimationKey>& keys, float time) {
    KeyPair kp;
    if (keys.size() < 2) return kp;
    if (time <= (float)keys.front().time) return kp;
    if (time >= (float)keys.back().time) {
        kp.a = keys.size() - 1;
        kp.b = kp.a;
        return kp;
    }
    size_t i = 0;
    while (i + 1 < keys.size() && (float)keys[i + 1].time <= time)
        i++;
    kp.a = i;
    kp.b = i + 1;
    const auto ta = (float)keys[kp.a].time;
    const auto tb = (float)keys[kp.b].time;
    kp.t = (tb > ta) ? (time - ta) / (tb - ta) : 0.0F;
    return kp;
}

void Slerp(const float a[4], const float b[4], float t, float out[4]) {
    float dot = (a[0] * b[0]) + (a[1] * b[1]) + (a[2] * b[2]) + (a[3] * b[3]);
    float sign = 1.0F;
    if (dot < 0.0F) {
        dot = -dot;
        sign = -1.0F;
    }
    float wa = 1.0F - t;
    float wb = t * sign;
    if (dot < 0.9995F) {
        const float theta = std::acos(dot);
        const float st = std::sin(theta);
        if (st > 1e-6F) {
            wa = std::sin((1.0F - t) * theta) / st;
            wb = sign * std::sin(t * theta) / st;
        }
    }
    float len = 0.0F;
    for (int i = 0; i < 4; i++) {
        out[i] = (a[i] * wa) + (b[i] * wb);
        len += out[i] * out[i];
    }
    len = std::sqrt(len);
    if (len > 1e-8F) {
        for (int i = 0; i < 4; i++)
            out[i] /= len;
    }
}

void LerpN(const float* a, const float* b, float t, int n, float* out) {
    for (int i = 0; i < n; i++)
        out[i] = a[i] + ((b[i] - a[i]) * t);
}

}

XFile::Matrix Multiply(const XFile::Matrix& a, const XFile::Matrix& b) {
    XFile::Matrix m{};
    for (int r = 0; r < 4; r++) {
        for (int c = 0; c < 4; c++) {
            float sum = 0.0F;
            for (int k = 0; k < 4; k++)
                sum += a[(size_t)(r * 4) + (size_t)k] * b[(size_t)(k * 4) + (size_t)c];
            m[(size_t)(r * 4) + (size_t)c] = sum;
        }
    }
    return m;
}

XFile::Matrix FromQuaternion(float w, float x, float y, float z) {
    XFile::Matrix m = XFile::Identity();
    const float xx = x * x;
    const float yy = y * y;
    const float zz = z * z;
    m[0] = 1.0F - (2.0F * (yy + zz));
    m[1] = 2.0F * ((x * y) + (z * w));
    m[2] = 2.0F * ((x * z) - (y * w));
    m[4] = 2.0F * ((x * y) - (z * w));
    m[5] = 1.0F - (2.0F * (xx + zz));
    m[6] = 2.0F * ((y * z) + (x * w));
    m[8] = 2.0F * ((x * z) + (y * w));
    m[9] = 2.0F * ((y * z) - (x * w));
    m[10] = 1.0F - (2.0F * (xx + yy));
    return m;
}

XFile::Matrix Compose(const float scale[3], const float quat[4], const float pos[3]) {
    XFile::Matrix s = XFile::Identity();
    s[0] = scale[0];
    s[5] = scale[1];
    s[10] = scale[2];
    XFile::Matrix m = Multiply(s, FromQuaternion(quat[0], quat[1], quat[2], quat[3]));
    m[12] = pos[0];
    m[13] = pos[1];
    m[14] = pos[2];
    return m;
}

XFile::Matrix SampleChannel(const XFile::AnimationChannel& channel, float time) {
    if (!channel.matrix.empty()) {
        const KeyPair kp = Locate(channel.matrix, time);
        XFile::Matrix m{};
        LerpN(channel.matrix[kp.a].value.data(), channel.matrix[kp.b].value.data(), kp.t, 16,
              m.data());
        return m;
    }

    float scale[3] = {1.0F, 1.0F, 1.0F};
    float quat[4] = {1.0F, 0.0F, 0.0F, 0.0F};
    float pos[3] = {0.0F, 0.0F, 0.0F};

    if (!channel.scale.empty()) {
        const KeyPair kp = Locate(channel.scale, time);
        LerpN(channel.scale[kp.a].value.data(), channel.scale[kp.b].value.data(), kp.t, 3, scale);
    }
    if (!channel.position.empty()) {
        const KeyPair kp = Locate(channel.position, time);
        LerpN(channel.position[kp.a].value.data(), channel.position[kp.b].value.data(), kp.t, 3,
              pos);
    }
    if (!channel.rotation.empty()) {
        const KeyPair kp = Locate(channel.rotation, time);
        Slerp(channel.rotation[kp.a].value.data(), channel.rotation[kp.b].value.data(), kp.t, quat);
        quat[1] = -quat[1];
        quat[2] = -quat[2];
        quat[3] = -quat[3];
    }
    return Compose(scale, quat, pos);
}

void SampleLocals(const XFile::Scene& scene, float time, std::vector<XFile::Matrix>& locals) {
    locals.resize(scene.frames.size());
    for (size_t i = 0; i < scene.frames.size(); i++)
        locals[i] = scene.frames[i].transform;

    for (const auto& ch : scene.channels) {
        for (size_t i = 0; i < scene.frames.size(); i++) {
            if (scene.frames[i].name != ch.frame_name) continue;
            locals[i] = SampleChannel(ch, time);
            break;
        }
    }
}

void ComputeWorlds(const XFile::Scene& scene, const std::vector<XFile::Matrix>& locals,
                   std::vector<XFile::Matrix>& worlds) {
    worlds.assign(scene.frames.size(), XFile::Identity());
    for (size_t i = 0; i < scene.frames.size(); i++) {
        const int parent = scene.frames[i].parent;
        if (parent < 0) {
            worlds[i] = locals[i];
        } else {
            worlds[i] = Multiply(locals[i], worlds[(size_t)parent]);
        }
    }
}

bool InvertAffine(const XFile::Matrix& m, XFile::Matrix& out) {
    const float a = m[0];
    const float b = m[1];
    const float c = m[2];
    const float d = m[4];
    const float e = m[5];
    const float f = m[6];
    const float g = m[8];
    const float h = m[9];
    const float i = m[10];
    const float det =
        (a * ((e * i) - (f * h))) - (b * ((d * i) - (f * g))) + (c * ((d * h) - (e * g)));
    if (std::fabs(det) < 1e-12F) return false;
    const float inv = 1.0F / det;

    out = XFile::Identity();
    out[0] = ((e * i) - (f * h)) * inv;
    out[1] = ((c * h) - (b * i)) * inv;
    out[2] = ((b * f) - (c * e)) * inv;
    out[4] = ((f * g) - (d * i)) * inv;
    out[5] = ((a * i) - (c * g)) * inv;
    out[6] = ((c * d) - (a * f)) * inv;
    out[8] = ((d * h) - (e * g)) * inv;
    out[9] = ((b * g) - (a * h)) * inv;
    out[10] = ((a * e) - (b * d)) * inv;

    out[12] = -((m[12] * out[0]) + (m[13] * out[4]) + (m[14] * out[8]));
    out[13] = -((m[12] * out[1]) + (m[13] * out[5]) + (m[14] * out[9]));
    out[14] = -((m[12] * out[2]) + (m[13] * out[6]) + (m[14] * out[10]));
    return true;
}

}
