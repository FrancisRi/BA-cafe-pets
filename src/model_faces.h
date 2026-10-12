#pragma once
#include "raylib.h"
#include "rlgl.h"
#include "external/cgltf.h" // The implementation is bundled and linked by raylib.
#include <algorithm>
#include <cmath>
#include <string>
#include <stdexcept>
#include <vector>

// Supplements glTF morph weights, material alphaMode and texture samplers.
// Skeleton import and skinning remain entirely raylib's.
struct ModelFaces {
    struct MaterialAlpha { int mode = 0; float cutoff = 0.5f; };
    struct Morph {
        int mesh = 0;
        int node = 0;
        std::string name;
        std::vector<float> base, defaults, weights, transitionWeights;
        std::vector<std::vector<float>> deltas;
    };
    struct Track {
        int node = 0;
        bool step = false;
        int width = 0;
        std::vector<float> times, values;
    };
    std::vector<MaterialAlpha> materials;
    std::vector<Morph> morphs;
    std::vector<std::vector<Track>> clips;
    Shader shader{};
    int modeLocation = -1, cutoffLocation = -1;

    void load(Model model, const char* path, const char* shaderPath) {
        cgltf_options options{};
        cgltf_data* data = nullptr;
        if (cgltf_parse_file(&options, path, &data) != cgltf_result_success)
            throw std::runtime_error("Cannot read glTF facial/material data");
        struct Cleanup { cgltf_data* data; ~Cleanup() { cgltf_free(data); } } cleanup{data};
        if (cgltf_load_buffers(&options, data, path) != cgltf_result_success)
            throw std::runtime_error("Cannot read glTF facial buffers");
        materials.resize(model.materialCount); // raylib adds default material at index 0.
        for (size_t i = 0; i < data->materials_count; ++i) {
            if (i + 1 >= materials.size()) throw std::runtime_error("glTF material mapping mismatch");
            const auto& source = data->materials[i];
            materials[i + 1] = {source.alpha_mode == cgltf_alpha_mode_mask ? 1 :
                                source.alpha_mode == cgltf_alpha_mode_blend ? 2 : 0, source.alpha_cutoff};
            if (const auto* texture = source.pbr_metallic_roughness.base_color_texture.texture) {
                const auto* sampler = texture->sampler;
                auto& loaded = model.materials[i + 1].maps[MATERIAL_MAP_ALBEDO].texture;
                const int minFilter = sampler && sampler->min_filter ? sampler->min_filter : cgltf_filter_type_linear_mipmap_linear;
                const int magFilter = sampler && sampler->mag_filter ? sampler->mag_filter : cgltf_filter_type_linear;
                if (minFilter >= cgltf_filter_type_nearest_mipmap_nearest) GenTextureMipmaps(&loaded);
                // glTF uses the same numeric filter/wrap enums as OpenGL.
                rlTextureParameters(loaded.id, RL_TEXTURE_MIN_FILTER, minFilter);
                rlTextureParameters(loaded.id, RL_TEXTURE_MAG_FILTER, magFilter);
                rlTextureParameters(loaded.id, RL_TEXTURE_WRAP_S, sampler ? sampler->wrap_s : cgltf_wrap_mode_repeat);
                rlTextureParameters(loaded.id, RL_TEXTURE_WRAP_T, sampler ? sampler->wrap_t : cgltf_wrap_mode_repeat);
            }
        }
        int meshIndex = 0;
        // Same node/triangle-primitive traversal as raylib 6.0 LoadGLTF.
        for (size_t n = 0; n < data->nodes_count; ++n) {
            const auto& node = data->nodes[n];
            if (!node.mesh) continue;
            float world[16];
            cgltf_node_transform_world(&node, world);
            for (size_t p = 0; p < node.mesh->primitives_count; ++p) {
                const auto& primitive = node.mesh->primitives[p];
                if (primitive.type != cgltf_primitive_type_triangles) continue;
                if (meshIndex >= model.meshCount) throw std::runtime_error("glTF mesh mapping mismatch");
                if (primitive.targets_count) {
                    const Mesh& mesh = model.meshes[meshIndex];
                    Morph morph;
                    morph.mesh = meshIndex;
                    morph.node = static_cast<int>(n);
                    morph.name = node.name ? node.name : "morph";
                    morph.base.assign(mesh.vertices, mesh.vertices + mesh.vertexCount*3);
                    morph.defaults.resize(primitive.targets_count, 0.0f);
                    for (size_t t = 0; t < primitive.targets_count; ++t) {
                        if (t < node.weights_count) morph.defaults[t] = node.weights[t];
                        else if (t < node.mesh->weights_count) morph.defaults[t] = node.mesh->weights[t];
                        std::vector<float> delta(mesh.vertexCount*3, 0.0f);
                        const auto& target = primitive.targets[t];
                        for (size_t a = 0; a < target.attributes_count; ++a) {
                            const auto& attribute = target.attributes[a];
                            if (attribute.type != cgltf_attribute_type_position) continue;
                            if (attribute.data->count != static_cast<size_t>(mesh.vertexCount))
                                throw std::runtime_error("Morph vertex count mismatch");
                            for (int v = 0; v < mesh.vertexCount; ++v) {
                                float d[3];
                                if (!cgltf_accessor_read_float(attribute.data, v, d, 3))
                                    throw std::runtime_error("Cannot read morph position");
                                // raylib stores base positions in node/world space; transform
                                // displacement with the same linear transform (no translation).
                                for (int c = 0; c < 3; ++c)
                                    delta[v*3 + c] = world[c]*d[0] + world[4 + c]*d[1] + world[8 + c]*d[2];
                            }
                        }
                        morph.deltas.push_back(std::move(delta));
                    }
                    morph.weights = morph.defaults;
                    morphs.push_back(std::move(morph));
                }
                ++meshIndex;
            }
        }
        if (meshIndex != model.meshCount) throw std::runtime_error("glTF mesh count mismatch");
        clips.resize(data->animations_count);
        for (size_t a = 0; a < data->animations_count; ++a) {
            const auto& animation = data->animations[a];
            for (size_t c = 0; c < animation.channels_count; ++c) {
                const auto& channel = animation.channels[c];
                if (channel.target_path != cgltf_animation_path_type_weights || !channel.target_node) continue;
                Track track;
                track.node = static_cast<int>(channel.target_node - data->nodes);
                auto morph = std::find_if(morphs.begin(), morphs.end(), [&](const Morph& m) { return m.node == track.node; });
                if (morph == morphs.end()) throw std::runtime_error("Weight track has no morph mesh");
                track.width = static_cast<int>(morph->deltas.size());
                const auto& sampler = *channel.sampler;
                if (sampler.interpolation != cgltf_interpolation_type_step && sampler.interpolation != cgltf_interpolation_type_linear)
                    throw std::runtime_error("Facial animation requires Blender-exported LINEAR/STEP weights");
                track.step = sampler.interpolation == cgltf_interpolation_type_step;
                if (sampler.output->count != sampler.input->count*track.width || sampler.input->count == 0)
                    throw std::runtime_error("Invalid weight animation sizes");
                track.times.resize(sampler.input->count);
                track.values.resize(sampler.output->count);
                for (size_t k = 0; k < track.times.size(); ++k)
                    if (!cgltf_accessor_read_float(sampler.input, k, &track.times[k], 1)) throw std::runtime_error("Invalid weight time");
                for (size_t k = 0; k < track.values.size(); ++k)
                    if (!cgltf_accessor_read_float(sampler.output, k, &track.values[k], 1)) throw std::runtime_error("Invalid weight value");
                if (!std::is_sorted(track.times.begin(), track.times.end())) throw std::runtime_error("Unsorted weight times");
                clips[a].push_back(std::move(track));
            }
        }
        shader = LoadShader(nullptr, shaderPath); // raylib's default vertex shader, CPU skinning.
        modeLocation = GetShaderLocation(shader, "alphaMode");
        cutoffLocation = GetShaderLocation(shader, "alphaCutoff");
        if (modeLocation < 0 || cutoffLocation < 0) throw std::runtime_error("Alpha-cutout shader failed");
        TraceLog(LOG_INFO, "FACE: loaded %d morph meshes; alpha-mask materials preserved", static_cast<int>(morphs.size()));
    }

    void captureTransition() {
        for(auto& morph:morphs)morph.transitionWeights=morph.weights;
    }
    void apply(Model model, int clip, float seconds, float blend=1) {
        // Reset on every sample: clip changes never retain another clip's mouth.
        for (Morph& morph : morphs) {
            morph.weights = morph.defaults;
            if (clip >= 0 && clip < static_cast<int>(clips.size())) for (const Track& track : clips[clip]) {
                if (track.node != morph.node) continue;
                auto after = std::upper_bound(track.times.begin(), track.times.end(), seconds);
                size_t left = after == track.times.begin() ? 0 : static_cast<size_t>(after - track.times.begin() - 1);
                size_t right = std::min(left + 1, track.times.size() - 1);
                const float span = track.times[right] - track.times[left];
                const float amount = track.step || span <= 0 ? 0 : std::clamp((seconds - track.times[left])/span, 0.0f, 1.0f);
                for (int t = 0; t < track.width; ++t) {
                    const float first = track.values[left*track.width + t];
                    morph.weights[t] = first + (track.values[right*track.width + t] - first)*amount;
                }
            }
            if(blend<1 && morph.transitionWeights.size()==morph.weights.size())
                for(size_t t=0;t<morph.weights.size();++t)
                    morph.weights[t]=morph.transitionWeights[t]+(morph.weights[t]-morph.transitionWeights[t])*blend;
            Mesh& mesh = model.meshes[morph.mesh];
            std::copy(morph.base.begin(), morph.base.end(), mesh.vertices);
            for (size_t t = 0; t < morph.deltas.size(); ++t)
                for (size_t v = 0; v < morph.base.size(); ++v)
                    mesh.vertices[v] += morph.deltas[t][v]*morph.weights[t];
            // Skinned meshes are uploaded by UpdateModelAnimation, AFTER morphing.
            if (!mesh.boneIndices) UpdateMeshBuffer(mesh, 0, mesh.vertices, mesh.vertexCount*3*sizeof(float), 0);
        }
    }

    void report() const {
        for (const Morph& morph : morphs) for (size_t t = 0; t < morph.weights.size(); ++t)
            TraceLog(LOG_INFO, "FACE SAMPLE: %s target=%d weight=%.6f", morph.name.c_str(), static_cast<int>(t), morph.weights[t]);
    }

    void draw(Model model) const {
        for (int i = 0; i < model.meshCount; ++i) {
            int index = model.meshMaterial[i];
            Material material = model.materials[index];
            material.shader = shader;
            const auto& alpha = materials[index];
            SetShaderValue(shader, modeLocation, &alpha.mode, SHADER_UNIFORM_INT);
            SetShaderValue(shader, cutoffLocation, &alpha.cutoff, SHADER_UNIFORM_FLOAT);
            DrawMesh(model.meshes[i], material, model.transform);
        }
    }

    ~ModelFaces() { if (shader.id) UnloadShader(shader); }
};
