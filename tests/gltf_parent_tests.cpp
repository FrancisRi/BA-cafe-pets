#include "raylib.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

static void check(Vector3 value, Vector3 expected) {
    if (std::abs(value.x - expected.x) > 0.00001f ||
        std::abs(value.y - expected.y) > 0.00001f ||
        std::abs(value.z - expected.z) > 0.00001f)
        throw std::runtime_error("Animated non-joint parent pose mismatch");
}

int main(int argc, char** argv) {
    if (argc != 2) return 1;
    int count = 0;
    ModelAnimation* clips = LoadModelAnimations(argv[1], &count);
    if (!clips || count != 2) return 1;
    int result = 0;
    try {
        if (clips[0].boneCount != 2 || clips[0].keyframeCount != 61)
            throw std::runtime_error("Unexpected animation dimensions");
        check(clips[0].keyframePoses[0][0].translation, {1, 0, 3});
        // Parent translation + rotation + scale, including a static grandparent.
        check(clips[0].keyframePoses[60][0].translation, {2, 2, 3});
        const float diagonal = 1.5f*std::sqrt(0.5f);
        check(clips[0].keyframePoses[30][0].translation, {1 + diagonal, diagonal, 3});
        // Another skin root uses its own parent; absent tracks retain defaults.
        check(clips[0].keyframePoses[60][1].translation, {0, 5, 0});
        check(clips[1].keyframePoses[60][0].translation, {1, 0, 3});
        std::cout << "PASS: animated non-joint ancestors, multiple roots, clip defaults\n";
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n'; result = 1;
    }
    UnloadModelAnimations(clips, count);
    return result;
}
