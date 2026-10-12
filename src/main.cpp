// raylib and Win32 both declare these identifiers.
#define CloseWindow Win32CloseWindow
#define ShowCursor Win32ShowCursor
#define Rectangle Win32Rectangle
#include <windows.h>
#include <windowsx.h>
#include <commdlg.h>
#undef CloseWindow
#undef ShowCursor
#undef Rectangle
#undef DrawText
#undef DrawTextEx
#undef LoadImage
#undef PlaySound

#include "raylib.h"
#include "rlgl.h"
#include "model_faces.h"
#include "model_transition.h"
#include "pet_behavior.h"
#include "pet_battle.h"
#include "pet_ground.h"
#include "pet_battle_input.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdarg>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>
#include <cwchar>
#include "pet_settings.h"

namespace {
constexpr int Size = 480;
constexpr int RenderScale = 2; // 2x SSAA also smooths alpha-cutout edges.
constexpr char ModelPath[] = "assets/Hibiki.blender.glb";
constexpr char IdleName[] = "Cafe_Idle";

// raylib 6.0 resamples glTF at 60 Hz, independent of render FPS.
constexpr float AnimationFrameSeconds = 1.0f/60.0f;

void logMessage(int level, const char* format, va_list args) {
    if (std::strstr(format, "Pixel data retrieved successfully")) return;
    std::printf("[%d] ", level);
    std::vprintf(format, args);
    std::putchar('\n');
    std::fflush(stdout);
}

struct Input {
    bool left = false, right = false, dragged = false, click = false;
    bool ground = false, dragStarted = false;
    bool previewLook = false;
    bool settingsRequested = false;
    bool displaysChanged = false;
    bool previewChordHeld = false;
    float dragDx = 0, dragDy = 0;
    POINT down{}, windowStart{}, previous{};
    float yaw = 0.0f, zoom = 1.0f;
};

LRESULT CALLBACK PetWindowProc(HWND window, UINT message, WPARAM w, LPARAM l) {
    auto* input = reinterpret_cast<Input*>(GetWindowLongPtrW(window, GWLP_USERDATA));
    if (message == WM_NCCREATE) {
        input = static_cast<Input*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);
        SetWindowLongPtrW(window, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(input));
    }
    if (!input) return DefWindowProcW(window, message, w, l);
    POINT cursor{};
    GetCursorPos(&cursor); // Screen coordinates: stable while the pet window moves.
    switch (message) {
    case WM_DISPLAYCHANGE:
        input->displaysChanged = true;
        return 0;
    case WM_MOUSEACTIVATE: return MA_NOACTIVATE;
    case WM_ERASEBKGND: return 1;
    case WM_LBUTTONDOWN: {
        if (input->right) return 0;
        RECT rect{};
        GetWindowRect(window, &rect);
        input->left = true;
        input->dragged = false;
        input->down = cursor;
        input->previous = cursor;
        input->windowStart = {rect.left, rect.top};
        SetCapture(window);
        return 0;
    }
    case WM_RBUTTONDOWN:
        if (input->left) return 0;
        input->right = true;
        input->previous = cursor;
        SetCapture(window);
        return 0;
    case WM_MOUSEMOVE:
        if (input->left) {
            const LONG dx = cursor.x - input->down.x, dy = cursor.y - input->down.y;
            const bool alreadyDragging = input->dragged;
            input->dragged |= std::abs(dx) > 4 || std::abs(dy) > 4;
            if (input->dragged) {
                input->dragStarted |= !alreadyDragging;
                if (input->ground) {
                    input->dragDx += alreadyDragging ? cursor.x - input->previous.x : dx;
                    input->dragDy += alreadyDragging ? cursor.y - input->previous.y : dy;
                    input->previous = cursor;
                    return 0;
                }
                MONITORINFO monitor{sizeof(MONITORINFO), {}, {}, 0};
                GetMonitorInfoW(MonitorFromPoint(cursor, MONITOR_DEFAULTTONEAREST), &monitor);
                const LONG x = std::clamp(input->windowStart.x + dx, monitor.rcWork.left - Size/3,
                                          std::max(monitor.rcWork.left - Size/3, monitor.rcWork.right - Size*2/3));
                const LONG y = std::clamp(input->windowStart.y + dy, monitor.rcWork.top,
                                          std::max(monitor.rcWork.top, monitor.rcWork.bottom - Size));
                SetWindowPos(window, HWND_TOPMOST, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            }
            input->previous = cursor;
        }
        if (input->right) {
            input->yaw = std::remainder(input->yaw + (cursor.x - input->previous.x)*0.7f, 360.0f);
            input->previous = cursor;
        }
        return 0;
    case WM_LBUTTONUP:
        if (input->left) {
            input->click = !input->dragged;
            input->left = false;
            ReleaseCapture();
        }
        return 0;
    case WM_RBUTTONUP:
        if (input->right) { input->right = false; ReleaseCapture(); }
        return 0;
    case WM_CAPTURECHANGED:
    case WM_CANCELMODE:
        input->left = input->right = false;
        return 0;
    case WM_MOUSEWHEEL:
        input->zoom = std::clamp(input->zoom*std::pow(1.1f, GET_WHEEL_DELTA_WPARAM(w)/120.0f), 0.45f, 1.65f);
        return 0;
    case WM_MBUTTONUP:
        input->settingsRequested = true;
        return 0;
    case WM_CLOSE:
        DestroyWindow(window);
        return 0;
    case WM_HOTKEY:
        if (w == 1) DestroyWindow(window);
        else if (w == 2) {
            input->previewLook = true;
            TraceLog(LOG_INFO, "INPUT Ctrl+Alt+L: WM_HOTKEY received");
        }
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    default: return DefWindowProcW(window, message, w, l);
    }
}

// The only Win32 presentation code: one reusable 32-bit DIB + layered popup.
// No WS_EX_TRANSPARENT: that would make the character itself click-through too.
struct DesktopSurface {
    HWND window = nullptr;
    HDC memory = nullptr;
    HBITMAP bitmap = nullptr;
    HGDIOBJ oldBitmap = nullptr;
    unsigned char* pixels = nullptr;
    bool quitHotkey = false, lookHotkey = false;
    ULONGLONG nextHotkeyAttempt = 0;

    void retryHotkeys() {
        const auto now=GetTickCount64();
        if (now<nextHotkeyAttempt || (quitHotkey && lookHotkey)) return;
        nextHotkeyAttempt=now+1000;
        if (!quitHotkey) quitHotkey=RegisterHotKey(window,1,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'Q')!=FALSE;
        if (!lookHotkey) lookHotkey=RegisterHotKey(window,2,MOD_CONTROL|MOD_ALT|MOD_NOREPEAT,'L')!=FALSE;
        if (quitHotkey && lookHotkey) TraceLog(LOG_INFO,"INPUT global hotkeys registered");
    }

    void create(Input& input) {
        WNDCLASSW wc{};
        wc.lpfnWndProc = PetWindowProc;
        wc.hInstance = GetModuleHandleW(nullptr);
        wc.lpszClassName = L"RaylibDesktopPet";
        wc.hCursor = LoadCursorW(nullptr, MAKEINTRESOURCEW(32512));
        if (!RegisterClassW(&wc)) throw std::runtime_error("RegisterClass failed");
        RECT work{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        window = CreateWindowExW(WS_EX_LAYERED | WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE | WS_EX_TOPMOST,
            wc.lpszClassName, L"3D Desktop Pet - Hibiki", WS_POPUP,
            work.right - Size - 40, std::max(work.top, work.bottom - Size), Size, Size,
            nullptr, nullptr, wc.hInstance, &input);
        if (!window) throw std::runtime_error("CreateWindowEx failed");
        memory = CreateCompatibleDC(nullptr);
        BITMAPINFO info{};
        info.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth = Size;
        // Positive height is bottom-up, matching the OpenGL texture readback.
        info.bmiHeader.biHeight = Size;
        info.bmiHeader.biPlanes = 1;
        info.bmiHeader.biBitCount = 32;
        info.bmiHeader.biCompression = BI_RGB;
        bitmap = CreateDIBSection(memory, &info, DIB_RGB_COLORS,
                                  reinterpret_cast<void**>(&pixels), nullptr, 0);
        if (!memory || !bitmap || !pixels) throw std::runtime_error("CreateDIBSection failed");
        oldBitmap = SelectObject(memory, bitmap);
        retryHotkeys(); // The previous pet may still own the keys during import handoff.
    }

    void present(const Image& image, bool outline = false) {
        if (!image.data || image.format != PIXELFORMAT_UNCOMPRESSED_R8G8B8A8)
            throw std::runtime_error("RGBA texture readback failed");
        const auto* rgba = static_cast<const unsigned char*>(image.data);
        for (int i = 0; i < Size*Size*4; i += 4) {
            // Rendering already produced premultiplied RGB. Only swizzle RGBA -> BGRA.
            pixels[i] = rgba[i + 2]; pixels[i + 1] = rgba[i + 1];
            pixels[i + 2] = rgba[i]; pixels[i + 3] = rgba[i + 3];
        }
        if (outline) {
            // Two-pixel alpha silhouette, composited behind the original premultiplied image.
            // Transparent pixels beyond the outline stay transparent and click-through.
            for (int y=0; y<Size; ++y) for (int x=0; x<Size; ++x) {
                const int i=(y*Size+x)*4;
                if (rgba[i+3]==255) continue;
                int alpha=0;
                for (int dy=-2; dy<=2; ++dy) for (int dx=-2; dx<=2; ++dx) {
                    if (dx*dx+dy*dy>4 || x+dx<0 || x+dx>=Size || y+dy<0 || y+dy>=Size) continue;
                    alpha=std::max(alpha,static_cast<int>(rgba[((y+dy)*Size+x+dx)*4+3]));
                }
                const int behind=alpha*(255-rgba[i+3])/255;
                pixels[i] = static_cast<unsigned char>(pixels[i]+75*behind/255);
                pixels[i+1] = static_cast<unsigned char>(pixels[i+1]+55*behind/255);
                pixels[i+2] = static_cast<unsigned char>(pixels[i+2]+35*behind/255);
                pixels[i+3] = static_cast<unsigned char>(pixels[i+3]+behind);
            }
        }
        POINT source{};
        SIZE size{Size, Size};
        BLENDFUNCTION blend{AC_SRC_OVER, 0, 255, AC_SRC_ALPHA};
        if (!UpdateLayeredWindow(window, nullptr, nullptr, &size, memory, &source, 0, &blend, ULW_ALPHA))
            throw std::runtime_error("UpdateLayeredWindow failed");
        if (!IsWindowVisible(window)) ShowWindow(window, SW_SHOWNOACTIVATE);
    }

    void verifyHitTesting() const {
        RECT rect{};
        GetWindowRect(window, &rect);
        bool transparent = false, opaque = false;
        for (int y = 0; y < Size && !(transparent && opaque); ++y) {
            for (int x = 0; x < Size && !(transparent && opaque); ++x) {
                const int alpha = pixels[((Size - 1 - y)*Size + x)*4 + 3];
                if (alpha == 0 && !transparent)
                    transparent = WindowFromPoint({rect.left + x, rect.top + y}) != window;
                if (alpha == 255 && !opaque)
                    opaque = WindowFromPoint({rect.left + x, rect.top + y}) == window;
            }
        }
        const bool topmost = (GetWindowLongPtrW(window, GWL_EXSTYLE) & WS_EX_TOPMOST) != 0;
        TraceLog(LOG_INFO, "WINDOW CHECK: transparent pass-through=%d opaque hit=%d topmost=%d", transparent, opaque, topmost);
        if (!transparent || !opaque || !topmost) throw std::runtime_error("Window hit-test verification failed");
    }

    ~DesktopSurface() {
        if (window) { UnregisterHotKey(window, 1); UnregisterHotKey(window, 2); if (IsWindow(window)) DestroyWindow(window); }
        if (oldBitmap) SelectObject(memory, oldBitmap);
        if (bitmap) DeleteObject(bitmap);
        if (memory) DeleteDC(memory);
    }
};

int findAnimation(ModelAnimation* animations, const std::vector<int>& valid, const char* name) {
    for (int i : valid) if (_stricmp(animations[i].name, name) == 0) return i;
    return -1;
}

void drawPlaceholder(float seconds, float reaction) {
    const float hop = reaction > 0 ? std::abs(std::sin(reaction*10.0f))*0.22f : std::sin(seconds*2.0f)*0.035f;
    rlTranslatef(0, hop, 0);
    DrawSphere({0, 0.82f, 0}, 0.53f, Color{101, 151, 203, 255});
    DrawSphere({0, 1.52f, 0}, 0.48f, Color{226, 232, 245, 255});
    DrawCube({-0.30f, 1.99f, 0}, 0.18f, 0.42f, 0.24f, Color{101, 151, 203, 255});
    DrawCube({0.30f, 1.99f, 0}, 0.18f, 0.42f, 0.24f, Color{101, 151, 203, 255});
    DrawSphere({-0.17f, 1.58f, 0.43f}, 0.065f, reaction > 0 ? GOLD : DARKBLUE);
    DrawSphere({0.17f, 1.58f, 0.43f}, 0.065f, reaction > 0 ? GOLD : DARKBLUE);
    DrawSphere({-0.27f, 0.25f, 0.10f}, 0.23f, DARKBLUE);
    DrawSphere({0.27f, 0.25f, 0.10f}, 0.23f, DARKBLUE);
}
} // namespace

int runDemo(int argc, char** argv) {
    SetProcessDPIAware();
    // Default assets resolve beside the executable, regardless of working directory.
    wchar_t executable[32768]{};
    GetModuleFileNameW(nullptr, executable, static_cast<DWORD>(std::size(executable)));
    std::string path = (std::filesystem::path(executable).parent_path()/ModelPath).string();
    int frameLimit = 0;
    bool geometry = false, checkWindow = false;
    std::string snapshot, initialClip, initialMode, poseDump, exportModels, btLog;
    std::string behaviorPath = (std::filesystem::path(executable).parent_path()/"behavior/hibiki.bt.xml").string();
    wchar_t savedModel[32768]{}, savedTree[32768]{};
    const auto activeFile=std::filesystem::path(executable).parent_path()/"active-model.ini";
    GetPrivateProfileStringW(L"Model",L"Path",L"",savedModel,32768,activeFile.c_str());
    GetPrivateProfileStringW(L"Model",L"Behavior",L"",savedTree,32768,activeFile.c_str());
    auto resolveSaved=[&](const wchar_t* value) {
        std::filesystem::path saved(value);
        return saved.is_absolute() ? saved : std::filesystem::path(executable).parent_path()/saved;
    };
    if (*savedModel && *savedTree && std::filesystem::exists(resolveSaved(savedModel)) && std::filesystem::exists(resolveSaved(savedTree))) {
        path=resolveSaved(savedModel).string(); behaviorPath=resolveSaved(savedTree).string();
    }
    float sampleSeconds = 0;
    for (int i = 1; i < argc; ++i) {
        if (!std::strcmp(argv[i], "--geometry")) geometry = true;
        else if (!std::strcmp(argv[i], "--check-window")) checkWindow = true;
        else if (!std::strcmp(argv[i], "--frames") && i + 1 < argc) frameLimit = std::max(1, std::atoi(argv[++i]));
        else if (!std::strcmp(argv[i], "--snapshot") && i + 1 < argc) snapshot = argv[++i];
        else if (!std::strcmp(argv[i], "--pose-dump") && i + 1 < argc) poseDump = argv[++i];
        else if (!std::strcmp(argv[i], "--behavior") && i + 1 < argc) behaviorPath = argv[++i];
        else if (!std::strcmp(argv[i], "--bt-log") && i + 1 < argc) btLog = argv[++i];
        else if (!std::strcmp(argv[i], "--export-node-models") && i + 1 < argc) exportModels = argv[++i];
        else if (!std::strcmp(argv[i], "--model") && i + 1 < argc) path = argv[++i];
        else if (!std::strcmp(argv[i], "--clip") && i + 1 < argc) initialClip = argv[++i];
        else if (!std::strcmp(argv[i], "--mode") && i + 1 < argc) initialMode = argv[++i];
        else if (!std::strcmp(argv[i], "--sample") && i + 1 < argc) sampleSeconds = std::max(0.0f, static_cast<float>(std::atof(argv[++i])));
        else { std::puts("desktop_pet [--geometry] [--model path.glb] [--mode cafe|battle] [--frames N] [--snapshot path.png] [--clip name] [--sample seconds] [--behavior tree.xml] [--bt-log trace.btlog] [--export-node-models models.xml]"); return 1; }
    }
    if(!initialMode.empty() && initialMode!="cafe" && initialMode!="battle")throw std::runtime_error("Mode must be cafe or battle");
    if (!exportModels.empty()) {
        pet::Playback state;
        pet::Behavior behavior(state);
        std::ofstream file(exportModels);
        file << behavior.models();
        return file ? 0 : 1;
    }
    SetConfigFlags(FLAG_WINDOW_HIDDEN);
    SetTraceLogCallback(logMessage);
    InitWindow(Size, Size, "Desktop Pet render context");
    if (!IsWindowReady()) return 1;
    SetTargetFPS(60);
    SetExitKey(KEY_NULL);
    SetTraceLogLevel(LOG_INFO);
    RenderTexture2D target = LoadRenderTexture(Size*RenderScale, Size*RenderScale);
    if (!IsRenderTextureValid(target)) { CloseWindow(); return 1; }
    RenderTexture2D resolved = LoadRenderTexture(Size, Size);
    if (!IsRenderTextureValid(resolved)) { UnloadRenderTexture(target); CloseWindow(); return 1; }
    SetTextureFilter(target.texture, TEXTURE_FILTER_BILINEAR);
    Model model{};
    ModelAnimation* animations = nullptr;
    int animationCount = 0;
    int result = 0;
    try {
        Input input;
        DesktopSurface desktop;
        desktop.create(input);
        ModelFaces faces;
        ModelTransition transition;
        bool loaded = !geometry && FileExists(path.c_str());
        if (loaded) {
            model = LoadModel(path.c_str());
            // CPU skinning does not have the GPU bone VBOs checked by IsModelValid in 6.0.
            loaded = model.meshCount > 0 && model.meshes && model.materials;
            if (loaded) {
                animations = LoadModelAnimations(path.c_str(), &animationCount);
                const std::string shaderPath = (std::filesystem::path(executable).parent_path()/"assets/alpha_cutout.fs").string();
                faces.load(model, path.c_str(), shaderPath.c_str());
            }
        }
        if (!loaded) TraceLog(LOG_WARNING, "Using geometry fallback (model absent, disabled or invalid).");
        std::vector<int> valid;
        for (int i = 0; animations && i < animationCount; ++i) {
            if (animations[i].keyframeCount > 0 && IsModelAnimationValid(model, animations[i])) {
                valid.push_back(i);
                TraceLog(LOG_INFO, "CLIP [%d] %s (%d frames)", i, animations[i].name, animations[i].keyframeCount);
            }
        }
        int idle = findAnimation(animations, valid, IdleName);
        if (idle < 0) idle = findAnimation(animations, valid, "Idle");
        if (idle < 0 && !valid.empty()) idle = valid.front();

        int current = idle;
        if (!initialClip.empty()) {
            current = findAnimation(animations, valid, initialClip.c_str());
            if (current < 0) throw std::runtime_error("Requested animation was not found");
        }
        if (loaded) faces.apply(model, idle, 0);
        if (idle >= 0) UpdateModelAnimation(model, animations[idle], 0);
        // Bound the initial animated pose, since its orientation can differ from the bind pose.
        BoundingBox bounds{{-1, 0, -1}, {1, 2.3f, 1}};
        if (loaded) {
            bounds = {{1e10f, 1e10f, 1e10f}, {-1e10f, -1e10f, -1e10f}};
            for (int m = 0; m < model.meshCount; ++m) {
                const Mesh& mesh = model.meshes[m];
                const float* vertices = mesh.animVertices ? mesh.animVertices : mesh.vertices;
                for (int v = 0; v < mesh.vertexCount; ++v) {
                    Vector3 point{vertices[v*3], vertices[v*3 + 1], vertices[v*3 + 2]};
                    bounds.min = {std::min(bounds.min.x, point.x), std::min(bounds.min.y, point.y), std::min(bounds.min.z, point.z)};
                    bounds.max = {std::max(bounds.max.x, point.x), std::max(bounds.max.y, point.y), std::max(bounds.max.z, point.z)};
                }
            }
        }
        const float height = bounds.max.y - bounds.min.y;
        const float fit = loaded && std::isfinite(height) && height > 0.0001f ? 2.3f/height : 1.0f;
        const Vector3 origin{-(bounds.min.x + bounds.max.x)*0.5f, -bounds.min.y,
                             -(bounds.min.z + bounds.max.z)*0.5f};
        // Use the model's eye anchors in the stable Idle pose, not eyelid bones.
        float eyeHeight = 1.7f;
        float eyeSum = 0;
        int eyeCount = 0;
        for (int b = 0; loaded && b < model.skeleton.boneCount; ++b) {
            if (std::strcmp(model.skeleton.bones[b].name, "Bip001 eye_L") == 0 ||
                std::strcmp(model.skeleton.bones[b].name, "Bip001 eye_R") == 0) {
                const auto& pose = idle >= 0 ? animations[idle].keyframePoses[0][b] : model.skeleton.bindPose[b];
                eyeSum += (pose.translation.y + origin.y)*fit;
                ++eyeCount;
            }
        }
        if (eyeCount) eyeHeight = eyeSum/eyeCount;
        TraceLog(LOG_INFO, "ATTENTION eye height=%.3f, eye anchors=%d", eyeHeight, eyeCount);
        TraceLog(LOG_INFO, "Bounds height %.3f, scale %.3f; idle=%d", height, fit, idle);
        TraceLog(LOG_INFO, "Left drag: move | Right drag: rotate | Wheel: zoom | Click: react | Middle click: settings | Ctrl+Alt+Q: quit");
        Camera3D camera{{0, 1.7f, 6}, {0, 1.15f, 0}, {0, 1, 0}, 36, CAMERA_PERSPECTIVE};
        const bool groundMode = initialClip.empty();
        input.ground = groundMode;
        // The fixed camera is about 143 units away. A sensible near plane also avoids
        // float precision loss in raylib's screen-to-floor unprojection.
        if (groundMode) rlSetClipPlanes(PetGround::NearPlane, PetGround::FarPlane);
        RECT workArea{};
        SystemParametersInfoW(SPI_GETWORKAREA, 0, &workArea, 0);
        PetGround ground;
        const float primaryWorkHeight=static_cast<float>(workArea.bottom-workArea.top);
        auto refreshDisplays = [&]() {
            std::vector<RECT> areas;
            EnumDisplayMonitors(nullptr,nullptr,[](HMONITOR monitor,HDC,LPRECT,LPARAM data)->BOOL {
                MONITORINFO info{sizeof(MONITORINFO),{},{},0};
                if (GetMonitorInfoW(monitor,&info)) reinterpret_cast<std::vector<RECT>*>(data)->push_back(info.rcWork);
                return TRUE;
            },reinterpret_cast<LPARAM>(&areas));
            if (areas.empty()) areas.push_back(workArea);
            workArea=areas.front();
            for (const auto& area:areas) {
                workArea.left=std::min(workArea.left,area.left); workArea.top=std::min(workArea.top,area.top);
                workArea.right=std::max(workArea.right,area.right); workArea.bottom=std::max(workArea.bottom,area.bottom);
            }
            ground.width=workArea.right-workArea.left; ground.height=workArea.bottom-workArea.top;
            // Keep apparent size stable when a staggered/vertical monitor extends desktop height.
            const float desktopScale=ground.height/primaryWorkHeight;
            ground.camera.position={0,29.789228f*desktopScale,140.147298f*desktopScale};
            if (groundMode) { camera=ground.camera; rlSetClipPlanes(PetGround::NearPlane,PetGround::FarPlane*std::max(1.0f,desktopScale)); }
            ground.monitors.clear();
            for (const auto& area:areas) ground.monitors.push_back({static_cast<float>(area.left-workArea.left),
                static_cast<float>(area.top-workArea.top),static_cast<float>(area.right-workArea.left),static_cast<float>(area.bottom-workArea.top)});
            TraceLog(LOG_INFO,"DESKTOP monitors=%d origin=%ld,%ld size=%dx%d",static_cast<int>(areas.size()),workArea.left,workArea.top,ground.width,ground.height);
        };
        refreshDisplays();
        RECT initialWindow{};
        GetWindowRect(desktop.window, &initialWindow);
        ground.place({static_cast<float>(initialWindow.left - workArea.left + Size/2),
                      static_cast<float>(initialWindow.top - workArea.top + Size - 40)});
        Vector2 cropOrigin = ground.crop();
        if (groundMode) {
            camera = ground.camera;
            SetWindowPos(desktop.window, HWND_TOPMOST, workArea.left + static_cast<int>(cropOrigin.x),
                         workArea.top + static_cast<int>(cropOrigin.y), 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            TraceLog(LOG_INFO, "GROUND fixed camera; work area %dx%d; perspective FOV %.1f", ground.width, ground.height, camera.fovy);
        }
        pet::Playback playback;
        playback.canWalk = [&](double yaw, double distance) { return ground.canWalk(static_cast<float>(yaw), static_cast<float>(distance)); };
        playback.log = [](const std::string& message) { TraceLog(LOG_INFO, "%s", message.c_str()); };
        for (int index : valid) playback.clips.push_back({animations[index].name,
            animations[index].keyframeCount*AnimationFrameSeconds, index});
        if (playback.clips.empty()) {
            for (const char* name : {"Cafe_Idle", "Cafe_Reaction", "Cafe_Walk", "Formation_Idle", "Formation_Pickup", "Exs", "Tactical_Start"})
                playback.clips.push_back({name, 1.0, -1});
        }
        auto characterName = std::filesystem::path(path).stem();
        if (characterName.extension() == ".blender") characterName = characterName.stem();
        PetSettings settings(playback, characterName.wstring(),
            (std::filesystem::path(executable).parent_path()/"settings.ini").wstring());
        pet::Behavior behavior(playback, behaviorPath, btLog);
        if (initialClip.empty()) behavior.start(idle >= 0 ? animations[idle].name : IdleName);
        pet::Battle battle(playback);
        BattleDesktopInput battleInput;
        PetJourney journey;
        settings.modesEnabled=true;settings.battleAvailable=battle.clips.supported();
        bool battleMode=false;
        auto selectMode=[&](bool enabled,bool persist) {
            if(enabled && !battle.clips.supported()) {
                TraceLog(LOG_WARNING,"MODE battle unavailable: %s",battle.clips.missing.c_str());return false;
            }
            behavior.halt();journey.route.clear();
            playback.pendingClick=false;playback.dragging=false;playback.trackingCursor=false;
            playback.returnRequested=playback.returningHome=playback.returnArrived=false;
            playback.activeInputSeconds=0;playback.resetCooldown();
            input.yaw=0;
            battleMode=enabled;settings.battleMode=enabled;battleInput.enable(enabled);
            if(enabled)battle.start();else if(initialClip.empty())behavior.tick(0);
            if(persist)settings.save();
            if(settings.window) {InvalidateRect(settings.window,nullptr,TRUE);InvalidateRect(GetDlgItem(settings.window,8),nullptr,TRUE);InvalidateRect(GetDlgItem(settings.window,9),nullptr,TRUE);}
            TraceLog(LOG_INFO,"MODE %s; battle supported=%d",enabled?"battle":"cafe",battle.clips.supported());return true;
        };
        wchar_t savedMode[24]{};
        GetPrivateProfileStringW(L"Behavior",L"Mode",L"Cafe",savedMode,24,settings.ini.c_str());
        if(initialClip.empty()) {
            const bool requested=initialMode=="battle" || (initialMode.empty() && std::wcscmp(savedMode,L"Battle")==0);
            if(!selectMode(requested && settings.battleAvailable,false) && initialMode=="battle")throw std::runtime_error("Model has no battle animations");
            if(initialMode=="battle" && !settings.battleAvailable)throw std::runtime_error("Model has no battle animations: "+battle.clips.missing);
        }
        // Explicit --clip is a deterministic model-review mode, independent of the tree.
        float animationTime = sampleSeconds, reaction = 0;
        bool running = true;
        int frames = 0;
        const double started = GetTime();
        uint64_t walkGeneration = 0;
        bool wasWalking = false;
        bool wasDragging = false;
        bool userWasInactive = false;
        Vector3 walkStart{};
        std::unique_ptr<void, decltype(&CloseHandle)> importProcess(nullptr, CloseHandle);
        while (running && (frameLimit == 0 || frames < frameLimit)) {
            MSG message{};
            while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE)) {
                if (message.message == WM_QUIT) { running = false; break; }
                if (settings.window && IsWindowVisible(settings.window) &&
                    (message.hwnd == settings.window || IsChild(settings.window, message.hwnd))) {
                    if (message.message == WM_KEYDOWN && (message.wParam == VK_ESCAPE || message.wParam == VK_RETURN)) {
                        settings.hide(); continue;
                    }
                    if (IsDialogMessageW(settings.window, &message)) continue;
                }
                TranslateMessage(&message);
                DispatchMessageW(&message);
            }
            if (!running || !IsWindow(desktop.window)) break;
            desktop.retryHotkeys();
            if (input.settingsRequested) { settings.show(desktop.window); input.settingsRequested = false; }
            if(settings.modeRequest) {selectMode(*settings.modeRequest,true);settings.modeRequest.reset();}
            if (!settings.importSource.empty() && !importProcess) {
                const auto executablePath=std::filesystem::path(executable);
                auto project=executablePath.parent_path();
                if (!std::filesystem::exists(project/"tools/import-model.ps1")) project=project.parent_path();
                const auto script=project/"tools/import-model.ps1";
                const auto output=project/"artifacts";
                std::filesystem::create_directories(output);
                const auto stem=L"import-ui-"+std::to_wstring(GetCurrentProcessId());
                const auto result=output/(stem+L".json"), log=output/(stem+L".log");
                settings.beginProgress(output/(stem+L".progress"));
                wchar_t system[32768]{}; GetSystemDirectoryW(system,32768);
                const auto shell=std::filesystem::path(system)/"WindowsPowerShell/v1.0/powershell.exe";
                auto quote=[](const std::wstring& value){return L"\""+value+L"\"";};
                std::wstring command=quote(shell.wstring())+L" -NoProfile -ExecutionPolicy Bypass -File "+quote(script.wstring())+
                    L" -Source "+quote(settings.importSource)+L" -Exe "+quote(executablePath.wstring())+
                    L" -Result "+quote(result.wstring())+L" -Progress "+quote(settings.progressFile.wstring())+L" -Launch";
                SECURITY_ATTRIBUTES security{sizeof(SECURITY_ATTRIBUTES),nullptr,TRUE};
                HANDLE logFile=CreateFileW(log.c_str(),GENERIC_WRITE,FILE_SHARE_READ,&security,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
                STARTUPINFOW startup{}; startup.cb=sizeof(STARTUPINFOW);
                startup.dwFlags=STARTF_USESHOWWINDOW|STARTF_USESTDHANDLES; startup.wShowWindow=SW_HIDE;
                startup.hStdOutput=startup.hStdError=logFile; startup.hStdInput=GetStdHandle(STD_INPUT_HANDLE);
                PROCESS_INFORMATION process{};
                if (std::filesystem::exists(script) && logFile!=INVALID_HANDLE_VALUE &&
                    CreateProcessW(shell.c_str(),command.data(),nullptr,nullptr,TRUE,CREATE_NO_WINDOW,nullptr,project.c_str(),&startup,&process)) {
                    importProcess.reset(process.hProcess); CloseHandle(process.hThread);
                    TraceLog(LOG_INFO,"IMPORT started; log=%s",log.string().c_str());
                } else { settings.importBusy=false; settings.importMessage=L"导入启动失败，请查看日志或工具路径"; }
                if(logFile!=INVALID_HANDLE_VALUE) CloseHandle(logFile);
                settings.importSource.clear();
            }
            if (importProcess) {
                settings.pollProgress();
                DWORD exitCode=STILL_ACTIVE; GetExitCodeProcess(importProcess.get(),&exitCode);
                if (exitCode!=STILL_ACTIVE) {
                    settings.nextProgressPoll=0; settings.pollProgress();
                    importProcess.reset(); settings.importBusy=false;
                    if (exitCode==0) { PostMessageW(desktop.window,WM_CLOSE,0,0); continue; }
                    settings.importMessage=L"导入失败，原角色保留；详见导入日志";
                    if(settings.window) { InvalidateRect(settings.window,nullptr,TRUE); InvalidateRect(GetDlgItem(settings.window,7),nullptr,TRUE); }
                }
            }
            // This non-activating window must also work when another app has focus.
            // Poll only this chord, using the high bit (current state), once per press.
            const bool previewChord = (GetAsyncKeyState(VK_CONTROL) & 0x8000) &&
                (GetAsyncKeyState(VK_MENU) & 0x8000) && (GetAsyncKeyState('L') & 0x8000);
            if (previewChord && !input.previewChordHeld && !input.previewLook) {
                input.previewLook = true;
                TraceLog(LOG_INFO, "INPUT Ctrl+Alt+L: key-state fallback");
            }
            input.previewChordHeld = previewChord;
            const float dt = frames == 0 ? 0.0f : std::min(GetFrameTime(), 0.1f);
            if (input.displaysChanged) {
                auto foot=ground.screen(ground.position);
                foot.x+=workArea.left; foot.y+=workArea.top;
                refreshDisplays();
                ground.place({foot.x-workArea.left,foot.y-workArea.top});
                journey.route.clear(); playback.returnRequested=false;
                input.displaysChanged=false;
            }
            ground.modelZoom = input.zoom;
            if (groundMode && (input.dragDx != 0 || input.dragDy != 0)) {
                auto foot = ground.screen(ground.position);
                foot.x += input.dragDx; foot.y += input.dragDy;
                ground.place(foot);
            }
            input.dragDx = input.dragDy = 0;
            if (initialClip.empty()) {
                LASTINPUTINFO lastInput{sizeof(LASTINPUTINFO), 0};
                const double secondsSinceInput = GetLastInputInfo(&lastInput) ? static_cast<DWORD>(GetTickCount() - lastInput.dwTime)/1000.0 : 0;
                if (playback.observeInput(secondsSinceInput, dt) && !battleMode) {
                    playback.trackingCursor=false;
                    playback.returnRequested=playback.walkEnabled && !journey.route.empty();
                    if (!playback.returnRequested) input.yaw=0;
                    TraceLog(LOG_INFO,"USER input resumed: %s",playback.returnRequested ? "return to excursion start" : "face screen");
                }
                const bool userInactive = playback.inactiveSeconds >= playback.inactivityThreshold();
                if (frames == 0 || userInactive != userWasInactive)
                    TraceLog(LOG_INFO, "USER inactive=%d seconds=%.1f", userInactive, playback.inactiveSeconds);
                userWasInactive = userInactive;
                playback.movementScale = input.zoom*pet::MovementSpeedMultiplier;
                POINT cursor{};
                if (GetCursorPos(&cursor)) {
                    const auto goal = ground.floorAt(ground.clampFoot({static_cast<float>(cursor.x - workArea.left), static_cast<float>(cursor.y - workArea.top)}));
                    const float dx = goal.x - ground.position.x, dz = goal.z - ground.position.z;
                    playback.cursorTarget = dx*dx + dz*dz > 0.04f;
                    playback.cursorYaw = std::atan2(dx, dz)*RAD2DEG;
                    float lookYaw = 0;
                    playback.lookTarget = ground.faceCursor(
                        {static_cast<float>(cursor.x - workArea.left), static_cast<float>(cursor.y - workArea.top)},
                        eyeHeight*input.zoom*(1.0f + static_cast<float>(playback.reaction)*0.04f), lookYaw);
                    playback.lookYaw = lookYaw;
                } else playback.cursorTarget = playback.lookTarget = false;
                const bool dragging = input.left && input.dragged;
                const bool faceFront = input.click || input.dragStarted || wasDragging != dragging;
                if (faceFront) input.yaw = 0;
                if (faceFront || dragging || input.right || !playback.walkEnabled) {
                    journey.route.clear(); playback.returnRequested=false;
                }
                // Do not lose a quick start+release drained in the same message batch.
                if(battleMode) {
                    auto events=battleInput.take(input.click,dragging,false);
                    if(battle.phase==pet::Battle::Phase::Moving)playback.walkYaw=input.yaw;
                    battleInput.planJump(events,battle,playback,ground,static_cast<float>(playback.walkYaw),workArea.left,workArea.top);
                    if(input.dragStarted && !dragging) {auto drag=events;drag.dragging=true;drag.petClick=drag.mouseClick=drag.keyPressed=drag.windowClosed=false;battle.tick(0,drag);}
                    battle.tick(dt,events);
                    if(playback.walking && walkGeneration!=playback.generation) {
                        input.yaw=static_cast<float>(playback.walkYaw);walkGeneration=playback.generation;walkStart=ground.position;
                    }
                    if(battle.returnToCafe)selectMode(false,frameLimit==0 && initialMode.empty());
                    else if(playback.facingRequest && !dragging && !input.right)
                        input.yaw=static_cast<float>(*playback.facingRequest);
                } else {
                    if(input.dragStarted && !dragging)behavior.tick(0,false,true);
                    behavior.tick(dt,input.click,dragging);
                }
                if (input.previewLook && playback.trackingEnabled && !playback.settingsOpen && !playback.returningHome) {
                    playback.trackingCursor = !playback.trackingCursor;
                    TraceLog(LOG_INFO, "ATTENTION manual tracking: %s", playback.trackingCursor ? "on" : "off");
                }
                input.previewLook = false;
                // User manipulation wins over both automatic and manual tracking.
                if (faceFront || dragging || input.right) playback.trackingCursor = false;
                if (!playback.trackingEnabled) playback.trackingCursor = false;
                if (playback.trackingCursor && playback.lookTarget && !playback.settingsOpen && !playback.returningHome)
                    input.yaw = PetGround::turnToward(input.yaw, static_cast<float>(playback.lookYaw), dt);
                playback.facingRequest.reset();
                wasDragging = dragging;
                input.dragStarted = false;
                input.click = false;
                if (playback.current >= 0) current = playback.clips[playback.current].modelIndex;
                animationTime = static_cast<float>(playback.time);
                reaction = static_cast<float>(playback.reaction);
                if (playback.returningHome) {
                    if (!playback.settingsOpen && !playback.returnArrived) {
                        playback.returnArrived=journey.stepHome(ground,static_cast<float>(playback.walkSpeed*playback.movementScale*dt),input.yaw);
                        if (playback.returnArrived) input.yaw=0;
                    }
                } else if(battleMode && battle.movementSeconds>0) {
                    const float distance=static_cast<float>(battle.movementSpeed*playback.movementScale*battle.movementSeconds);
                    if(battle.jumpMotion) {
                        input.yaw=static_cast<float>(battle.movementYaw);
                        ground.position=ground.step(input.yaw,distance);
                    } else if(!ground.move(static_cast<float>(battle.movementYaw),distance))playback.walkRemaining=0;
                } else if (!battleMode && playback.walking && !playback.settingsOpen) {
                    if (walkGeneration != playback.generation) {
                        input.yaw = static_cast<float>(playback.walkYaw);
                        walkGeneration = playback.generation;
                        walkStart = ground.position;
                        if(!battleMode)journey.begin(ground.position);
                        playback.returnArrived=false;
                        TraceLog(LOG_INFO, "GROUND walk start: heading=%.0f x=%.3f z=%.3f", input.yaw, walkStart.x, walkStart.z);
                    }
                    playback.walkYaw = input.yaw; // Right-drag rotation also steers an ongoing walk.
                    if (!ground.move(input.yaw, static_cast<float>(playback.walkSpeed*playback.movementScale*std::min<double>(dt, playback.walkRemaining))))
                        playback.walkRemaining=0;
                }

                if (wasWalking && !playback.walking)
                    TraceLog(LOG_INFO, "GROUND walk stop: dx=%.3f dz=%.3f", ground.position.x - walkStart.x, ground.position.z - walkStart.z);
                wasWalking = playback.walking;
                cropOrigin = ground.crop();
                const int retreatOffset=battleMode && battle.phase==pet::Battle::Phase::Retreat
                    ? static_cast<int>(battle.retreatProgress*(cropOrigin.y+Size+20)):0;
                SetWindowPos(desktop.window, HWND_TOPMOST, workArea.left + static_cast<int>(cropOrigin.x),
                             workArea.top + static_cast<int>(cropOrigin.y)-retreatOffset, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
            } else animationTime += dt;
            if (current >= 0) {
                const float duration = animations[current].keyframeCount*AnimationFrameSeconds;
                if (!initialClip.empty() && animationTime >= duration) animationTime = std::fmod(animationTime, duration);
                const float frame = std::min(animationTime/AnimationFrameSeconds, static_cast<float>(animations[current].keyframeCount - 1));
                if(initialClip.empty()) {
                    const bool changed=transition.changed(model,current,playback.generation);
                    if(changed) {
                        faces.captureTransition();
                        TraceLog(LOG_INFO,"ANIMATION blend=%.3f to=%s",transition.duration,animations[current].name);
                    }
                    const float blend=transition.advance(playback.settingsOpen && !battle.jumpMotion ? 0 : dt,changed);
                    faces.apply(model,current,animationTime,blend);
                    transition.update(model,animations[current],frame);
                } else {
                    faces.apply(model,current,animationTime);
                    UpdateModelAnimation(model,animations[current],frame);
                }
            }
            if (frames == 0 && loaded) faces.report();
            if (frames == 0 && !poseDump.empty()) {
                std::ofstream bones(poseDump + ".bones.csv");
                bones << "name,px,py,pz,qx,qy,qz,qw,sx,sy,sz\n";
                for(int i=0;i<model.skeleton.boneCount;++i) {
                    const auto& p=model.currentPose[i];
                    bones << model.skeleton.bones[i].name << ',' << p.translation.x << ',' << p.translation.y << ',' << p.translation.z
                          << ',' << p.rotation.x << ',' << p.rotation.y << ',' << p.rotation.z << ',' << p.rotation.w
                          << ',' << p.scale.x << ',' << p.scale.y << ',' << p.scale.z << '\n';
                }
                FILE* file = std::fopen(poseDump.c_str(), "wb");
                if (!file) throw std::runtime_error("Cannot create pose dump");
                std::fwrite(&model.meshCount, sizeof(int), 1, file);
                for (int m = 0; m < model.meshCount; ++m) {
                    const Mesh& mesh = model.meshes[m];
                    std::fwrite(&mesh.vertexCount, sizeof(int), 1, file);
                    std::fwrite(&model.meshMaterial[m], sizeof(int), 1, file);
                    std::fwrite(mesh.animVertices ? mesh.animVertices : mesh.vertices, sizeof(float), mesh.vertexCount*3, file);
                }
                const bool failed = std::ferror(file) != 0;
                std::fclose(file);
                if (failed) throw std::runtime_error("Cannot write pose dump");
            }
            BeginTextureMode(target);
            ClearBackground(BLANK);
            // Separate alpha blending gives premultiplied RGBA for the layered window.
            rlSetBlendFactorsSeparate(RL_SRC_ALPHA, RL_ONE_MINUS_SRC_ALPHA, RL_ONE,
                                      RL_ONE_MINUS_SRC_ALPHA, RL_FUNC_ADD, RL_FUNC_ADD);
            BeginBlendMode(BLEND_CUSTOM_SEPARATE);
            BeginMode3D(camera);
            if (groundMode) {
                // Crop the fixed full-work-area camera to the pet's 480px native window.
                // This changes only the viewport's sub-frustum, not camera pose or model scale.
                double left, right, bottom, top;
                ground.frustum(cropOrigin, rlGetCullDistanceNear(), left, right, bottom, top);
                rlMatrixMode(RL_PROJECTION);
                rlLoadIdentity();
                rlFrustum(left, right, bottom, top, rlGetCullDistanceNear(), rlGetCullDistanceFar());
                rlMatrixMode(RL_MODELVIEW);
            }
            rlDisableBackfaceCulling(); // Hibiki materials are double-sided.
            rlPushMatrix();
            if (groundMode) rlTranslatef(ground.position.x, 0, ground.position.z);
            rlRotatef(input.yaw, 0, 1, 0);
            const float zoom = input.zoom*(1.0f + reaction*0.04f);
            rlScalef(zoom, zoom, zoom);
            if (loaded) {
                rlScalef(fit, fit, fit);
                rlTranslatef(origin.x, origin.y, origin.z);
                faces.draw(model);
            } else drawPlaceholder(static_cast<float>(GetTime()), reaction);
            rlPopMatrix();
            rlEnableBackfaceCulling();
            EndMode3D();
            EndBlendMode();
            EndTextureMode();
            BeginTextureMode(resolved);
            ClearBackground(BLANK);
            // Average four high-resolution samples on the GPU. Copy premultiplied
            // RGBA without blending a second time; read back only 480x480 pixels.
            rlDisableColorBlend();
            DrawTexturePro(target.texture, {0, 0, static_cast<float>(Size*RenderScale), -static_cast<float>(Size*RenderScale)},
                           {0, 0, static_cast<float>(Size), static_cast<float>(Size)}, {0, 0}, 0, WHITE);
            rlDrawRenderBatchActive();
            rlEnableColorBlend();
            EndTextureMode();
            Image image = LoadImageFromTexture(resolved.texture);
            desktop.present(image, playback.outlineEnabled);
            if (checkWindow && frames == 3) desktop.verifyHitTesting();
            if (!snapshot.empty() && (frames == 30 || (frameLimit > 0 && frames == frameLimit - 1))) {
                ImageFlipVertical(&image);
                ExportImage(image, snapshot.c_str());
            }
            UnloadImage(image);
            // Keeps raylib's hidden GL context/event pump and 60 FPS timer alive.
            BeginDrawing();
            EndDrawing();
            ++frames;
        }
        TraceLog(LOG_INFO, "Rendered %d frames, average %.2f FPS", frames, frames/std::max(0.001, GetTime() - started));
        if (checkWindow && frames < 4) throw std::runtime_error("Window check needs at least 4 frames");
    } catch (const std::exception& error) {
        TraceLog(LOG_ERROR, "%s (Win32 error %lu)", error.what(), GetLastError());
        result = 1;
    }
    if (animations) UnloadModelAnimations(animations, animationCount);
    if (model.meshes) UnloadModel(model);
    UnloadRenderTexture(target);
    UnloadRenderTexture(resolved);
    CloseWindow();
    return result;
}

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    return runDemo(__argc, __argv);
}
