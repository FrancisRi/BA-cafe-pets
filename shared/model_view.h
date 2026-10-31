// Copyright (C) 2026 FrancisRi
// SPDX-License-Identifier: GPL-3.0-only
#pragma once
#include "raylib.h"
#include <array>
#include <cmath>

// Presentation only: keep the floor camera and movement coordinates unchanged.
// Pitch in screen/world space AFTER facing, so side views never lean sideways.
struct ModelView {
    static constexpr float PitchDegrees=-5.0f;
    static std::array<float,16> matrix(float yaw,float zoom) {
        const float cy=std::cos(yaw*DEG2RAD),sy=std::sin(yaw*DEG2RAD);
        const float cp=std::cos(PitchDegrees*DEG2RAD),sp=std::sin(PitchDegrees*DEG2RAD);
        return {cy*zoom,sp*sy*zoom,-cp*sy*zoom,0,
                0,cp*zoom,sp*zoom,0,
                sy*zoom,-sp*cy*zoom,cp*cy*zoom,0,
                0,0,0,1};
    }
    static Vector3 apply(const std::array<float,16>& m,Vector3 point) {
        return {m[0]*point.x+m[4]*point.y+m[8]*point.z,
                m[1]*point.x+m[5]*point.y+m[9]*point.z,
                m[2]*point.x+m[6]*point.y+m[10]*point.z};
    }
};
