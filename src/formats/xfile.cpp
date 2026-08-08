#include "formats/xfile.h"

#include "formats/xfile_lexer.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace XFile {

namespace {

constexpr int kKeyRotation = 0;
constexpr int kKeyScale = 1;
constexpr int kKeyPosition = 2;
constexpr int kKeyMatrix = 4;

void ReadMeshGeometry(Lexer& lex, Mesh& mesh) {
    const int nverts = lex.Integer();
    mesh.positions.reserve((size_t)(nverts > 0 ? nverts : 0));
    for (int i = 0; i < nverts; i++) {
        Vec3 p;
        p.x = lex.Number();
        p.y = lex.Number();
        p.z = lex.Number();
        mesh.positions.push_back(p);
    }
    const int nfaces = lex.Integer();
    for (int i = 0; i < nfaces; i++) {
        const int n = lex.Integer();
        std::vector<uint32_t> poly;
        poly.reserve((size_t)(n > 0 ? n : 0));
        for (int k = 0; k < n; k++)
            poly.push_back((uint32_t)lex.Integer());
        for (int k = 2; k < n; k++) {
            mesh.indices.push_back(poly[0]);
            mesh.indices.push_back(poly[(size_t)k - 1]);
            mesh.indices.push_back(poly[(size_t)k]);
            mesh.face_material.push_back(0);
            mesh.triangle_face.push_back((uint32_t)i);
        }
    }
}

void ReadTextureCoords(Lexer& lex, Mesh& mesh) {
    const int n = lex.Integer();
    mesh.uvs.clear();
    mesh.uvs.reserve((size_t)(n > 0 ? n : 0));
    for (int i = 0; i < n; i++) {
        Vec2 t;
        t.u = lex.Number();
        t.v = lex.Number();
        mesh.uvs.push_back(t);
    }
}

Material ReadMaterial(Lexer& lex);

void ReadMaterialList(Lexer& lex, Mesh& mesh) {
    const int nmat = lex.Integer();
    const int nfaceidx = lex.Integer();
    std::vector<uint32_t> per_face;
    per_face.reserve((size_t)(nfaceidx > 0 ? nfaceidx : 0));
    for (int i = 0; i < nfaceidx; i++)
        per_face.push_back((uint32_t)lex.Integer());

    mesh.materials.clear();
    mesh.materials.reserve((size_t)(nmat > 0 ? nmat : 0));
    while (lex.Peek() != '}' && !lex.Eof()) {
        if (lex.Peek() == '{') {
            lex.Expect('{');
            Material referenced;
            referenced.ref = lex.Token();
            lex.Expect('}');
            mesh.materials.push_back(std::move(referenced));
            continue;
        }
        const std::string tok = lex.Token();
        if (tok == "Material") {
            mesh.materials.push_back(ReadMaterial(lex));
        } else if (!tok.empty() && lex.Peek() == '{') {
            lex.SkipBlock();
        } else if (tok.empty()) {
            break;
        }
    }
    lex.Expect('}');

    for (size_t tri = 0; tri < mesh.face_material.size(); tri++) {
        const size_t face =
            (tri < mesh.triangle_face.size()) ? (size_t)mesh.triangle_face[tri] : tri;
        if (per_face.size() == 1) {
            mesh.face_material[tri] = per_face[0];
        } else if (face < per_face.size()) {
            mesh.face_material[tri] = per_face[face];
        }
    }
}

Material ReadMaterial(Lexer& lex) {
    Material mat;
    if (lex.Peek() != '{') lex.Token();
    lex.Expect('{');
    for (float& c : mat.diffuse)
        c = lex.Number();
    lex.Number();
    for (int i = 0; i < 3; i++)
        lex.Number();
    for (int i = 0; i < 3; i++)
        lex.Number();
    while (lex.Peek() != '}' && !lex.Eof()) {
        const std::string tok = lex.Token();
        if (tok == "TextureFilename") {
            if (lex.Peek() != '{') lex.Token();
            lex.Expect('{');
            mat.texture = lex.QuotedString();
            lex.SkipSeparators();
            lex.Expect('}');
        } else if (!tok.empty() && lex.Peek() == '{') {
            lex.SkipBlock();
        } else if (tok.empty()) {
            break;
        }
    }
    lex.Expect('}');
    return mat;
}

void ReadMesh(Lexer& lex, Frame& frame) {
    Mesh mesh;
    if (lex.Peek() != '{') lex.Token();
    lex.Expect('{');
    ReadMeshGeometry(lex, mesh);
    while (lex.Peek() != '}' && !lex.Eof()) {
        const std::string tok = lex.Token();
        if (tok == "MeshTextureCoords") {
            if (lex.Peek() != '{') lex.Token();
            lex.Expect('{');
            ReadTextureCoords(lex, mesh);
            lex.Expect('}');
        } else if (tok == "MeshMaterialList") {
            if (lex.Peek() != '{') lex.Token();
            lex.Expect('{');
            ReadMaterialList(lex, mesh);
        } else if (!tok.empty() && lex.Peek() == '{') {
            lex.SkipBlock();
        } else if (tok.empty()) {
            break;
        }
    }
    lex.Expect('}');
    frame.meshes.push_back(std::move(mesh));
}

void ReadTransform(Lexer& lex, Frame& frame) {
    if (lex.Peek() != '{') lex.Token();
    lex.Expect('{');
    for (float& v : frame.transform)
        v = lex.Number();
    lex.Expect('}');
}

void ReadAnimationKeys(Lexer& lex, AnimationChannel& ch, Scene& scene) {
    if (lex.Peek() != '{') lex.Token();
    lex.Expect('{');
    const int type = lex.Integer();
    const int nkeys = lex.Integer();
    for (int i = 0; i < nkeys; i++) {
        AnimationKey key;
        key.time = lex.Integer();
        const int nvals = lex.Integer();
        for (int k = 0; k < nvals && (size_t)k < key.value.size(); k++)
            key.value.at((size_t)k) = lex.Number();
        for (int k = (int)key.value.size(); k < nvals; k++)
            lex.Number();
        scene.max_key_time = (key.time > scene.max_key_time) ? key.time : scene.max_key_time;
        if (type == kKeyRotation) ch.rotation.push_back(key);
        if (type == kKeyScale) ch.scale.push_back(key);
        if (type == kKeyPosition) ch.position.push_back(key);
        if (type == kKeyMatrix) ch.matrix.push_back(key);
    }
    lex.Expect('}');
}

void ReadAnimation(Lexer& lex, Scene& scene) {
    AnimationChannel ch;
    if (lex.Peek() != '{') lex.Token();
    lex.Expect('{');
    while (lex.Peek() != '}' && !lex.Eof()) {
        if (lex.Peek() == '{') {
            lex.Expect('{');
            ch.frame_name = lex.Token();
            lex.Expect('}');
            continue;
        }
        const std::string tok = lex.Token();
        if (tok == "AnimationKey") {
            ReadAnimationKeys(lex, ch, scene);
        } else if (!tok.empty() && lex.Peek() == '{') {
            lex.SkipBlock();
        } else if (tok.empty()) {
            break;
        }
    }
    lex.Expect('}');
    if (!ch.frame_name.empty()) scene.channels.push_back(std::move(ch));
}

void ReadAnimationSet(Lexer& lex, Scene& scene) {
    if (lex.Peek() != '{') lex.Token();
    lex.Expect('{');
    while (lex.Peek() != '}' && !lex.Eof()) {
        const std::string tok = lex.Token();
        if (tok == "Animation") {
            ReadAnimation(lex, scene);
        } else if (!tok.empty() && lex.Peek() == '{') {
            lex.SkipBlock();
        } else if (tok.empty()) {
            break;
        }
    }
    lex.Expect('}');
}

void ReadFrame(Lexer& lex, Scene& scene, int parent);
void ResolveMaterialRefs(Scene& scene);

void ReadFrameBody(Lexer& lex, Scene& scene, int self) {
    while (lex.Peek() != '}' && !lex.Eof()) {
        const std::string tok = lex.Token();
        if (tok == "Frame") {
            ReadFrame(lex, scene, self);
        } else if (tok == "FrameTransformMatrix") {
            ReadTransform(lex, scene.frames[(size_t)self]);
        } else if (tok == "Mesh") {
            ReadMesh(lex, scene.frames[(size_t)self]);
        } else if (!tok.empty() && lex.Peek() == '{') {
            lex.SkipBlock();
        } else if (tok.empty()) {
            break;
        }
    }
    lex.Expect('}');
}

void ReadFrame(Lexer& lex, Scene& scene, int parent) {
    Frame frame;
    frame.transform = Identity();
    frame.parent = parent;
    if (lex.Peek() != '{') frame.name = lex.Token();
    lex.Expect('{');

    const auto self = (int)scene.frames.size();
    scene.frames.push_back(std::move(frame));
    if (parent >= 0) scene.frames[(size_t)parent].children.push_back(self);

    ReadFrameBody(lex, scene, self);
}

void ResolveMaterialRefs(Scene& scene) {
    for (auto& frame : scene.frames) {
        for (auto& mesh : frame.meshes) {
            for (auto& mat : mesh.materials) {
                if (mat.ref.empty()) continue;
                for (const auto& named : scene.named_materials) {
                    if (named.name != mat.ref) continue;
                    const std::string keep = mat.ref;
                    mat = named.material;
                    mat.ref = keep;
                    break;
                }
            }
        }
    }
}

}

Matrix Identity() {
    Matrix m{};
    m[0] = 1.0F;
    m[5] = 1.0F;
    m[10] = 1.0F;
    m[15] = 1.0F;
    return m;
}

bool Parse(const std::string& text, Scene& out, std::string& err) {
    out.frames.clear();
    out.channels.clear();
    out.max_key_time = 0;

    if (!text.starts_with("xof ")) {
        err = "not a DirectX .x file (missing 'xof ' magic)";
        return false;
    }
    if (text.size() < 11 || text.compare(8, 3, "txt") != 0) {
        err = "only the text .x form is supported, got '" + text.substr(8, 3) + "'";
        return false;
    }

    Lexer lex(text);
    lex.Token();

    while (!lex.Eof()) {
        const std::string tok = lex.Token();
        if (tok.empty()) break;
        if (tok == "template") {
            lex.Token();
            lex.SkipBlock();
        } else if (tok == "Frame") {
            ReadFrame(lex, out, -1);
        } else if (tok == "AnimationSet") {
            ReadAnimationSet(lex, out);
        } else if (tok == "Material") {
            NamedMaterial named;
            named.name = (lex.Peek() != '{') ? lex.Token() : std::string();
            named.material = ReadMaterial(lex);
            out.named_materials.push_back(std::move(named));
        } else if (tok == "Mesh") {
            Frame frame;
            frame.transform = Identity();
            frame.name = "_rootmesh";
            out.frames.push_back(std::move(frame));
            ReadMesh(lex, out.frames.back());
        } else if (lex.Peek() == '{') {
            lex.SkipBlock();
        }
    }

    ResolveMaterialRefs(out);

    if (out.frames.empty()) {
        err = "no Frame or Mesh objects found";
        return false;
    }
    return true;
}

}
