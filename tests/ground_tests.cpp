#include "pet_ground.h"
#include "rlgl.h"
#include "raymath.h"
#include <iostream>
#include <stdexcept>

void check(bool ok, const char* message) { if (!ok) throw std::runtime_error(message); }
int main() {
    try {
        rlSetClipPlanes(PetGround::NearPlane, PetGround::FarPlane);
        {
            PetGround ground;
            ground.width=3840; ground.height=1080;
            ground.monitors={{0,0,1920,1080},{1920,0,3840,1080}};
            ground.place({1918,700});
            check(ground.move(90,0.2f) && ground.screen(ground.position).x>1920,
                  "Walking must cross a shared monitor edge");
            ground.place({3000,700});
            check(std::abs(ground.screen(ground.position).x-3000)<0.1,
                  "Dragging must reach secondary monitor");
            ground.monitors={{0,0,1800,1080},{2100,0,3840,1080}};
            ground.place({1950,700});
            const float x=ground.screen(ground.position).x;
            check(x<=1800.1 || x>=2099.9,"Desktop gaps must not be valid pet locations");
            ground.monitors.clear(); ground.place({1800,700});
            const auto home=ground.position;
            PetJourney journey; journey.begin(home);
            check(ground.move(90,1),"First excursion step"); journey.begin(ground.position);
            check(ground.move(0,1),"Second excursion step");
            float yaw=0; bool arrived=journey.stepHome(ground,0.02f,yaw);
            check(std::abs(yaw+135.0f)<0.01f,"Return must head directly toward the original home, not the last turn");
            for(int i=0;i<1000 && !arrived;++i) arrived=journey.stepHome(ground,0.02f,yaw);
            check(arrived && journey.route.empty() && std::hypot(ground.position.x-home.x,ground.position.z-home.z)<0.001,
                  "Return must reach the original excursion start");
        }
        for (const auto size : {Vector2{1920, 1008}, Vector2{1920, 1080}, Vector2{1366, 768}, Vector2{2560, 1440}}) {
            PetGround ground;
            ground.width = static_cast<int>(size.x); ground.height = static_cast<int>(size.y);
            const Vector2 center{size.x/2, size.y*0.68f};
            ground.place(center);
            const auto origin = ground.position;
            const auto originalCamera = ground.camera.position;
            check(std::abs(std::atan2(originalCamera.y, originalCamera.z)*RAD2DEG - 12) < 0.001,
                  "Camera elevation must be 12 degrees");
            for (float zoom : {0.45f, 1.0f, 1.65f}) {
                const float eyeHeight = 1.7f*zoom;
                const auto eye = ground.screen({origin.x, eyeHeight, origin.z});
                float yaw = 0;
                check(!ground.faceCursor(eye, eyeHeight, yaw), "Cursor on eye anchor must preserve orientation");
                check(ground.faceCursor({eye.x + 100, eye.y}, eyeHeight, yaw) && std::abs(yaw - 90) < 0.1,
                      "Cursor right of eyes must face right, independent of foot height and zoom");
                check(ground.faceCursor({eye.x - 100, eye.y}, eyeHeight, yaw) && std::abs(yaw + 90) < 0.1,
                      "Cursor left of eyes must face left");
                check(ground.faceCursor({eye.x, eye.y - 100}, eyeHeight, yaw) && std::abs(std::abs(yaw) - 180) < 0.1,
                      "Cursor above eyes must face away");
                check(ground.faceCursor({eye.x, eye.y + 100}, eyeHeight, yaw) && std::abs(yaw) < 0.1,
                      "Cursor below eyes must face screen");
            }
            auto pixelHeight = [&]() {
                const auto foot = ground.screen(ground.position);
                return foot.y - ground.screen({ground.position.x, 2.3f, ground.position.z}).y;
            };
            float diagonalYaw = 0;
            const auto eyeAnchor = ground.screen({origin.x, 1.7f, origin.z});
            check(ground.faceCursor({eyeAnchor.x + 100, eyeAnchor.y + 100}, 1.7f, diagonalYaw) &&
                  diagonalYaw > 50 && diagonalYaw < 65, "Diagonal look must favor side views instead of depth amplification");
            check(std::abs(PetGround::turnToward(179, -179, 0.1f) + 179) < 0.01,
                  "Tracking must take shortest path across wrap boundary");
            check(std::abs(PetGround::turnToward(0, 90, 1.0f/60) - 4) < 0.01,
                  "Tracking must turn smoothly at a bounded rate");
            const auto baseHeight = pixelHeight();
            check(std::abs(ground.screen(ground.position).y - center.y) < 0.1, "Floor unprojection must preserve screen point");
            check(ground.move(180, 0.5), "Away step must be allowed");
            check(ground.screen(ground.position).y < center.y && pixelHeight() < baseHeight,
                  "Back facing movement must go up and become smaller");
            ground.position = origin;
            check(ground.move(0, 0.5), "Near step must be allowed");
            check(ground.screen(ground.position).y > center.y && pixelHeight() > baseHeight,
                  "Front facing movement must go down and become larger");
            ground.position = origin;
            check(ground.move(90, 0.5), "Right step must be allowed");
            check(ground.screen(ground.position).x > center.x && std::abs(pixelHeight() - baseHeight) < 0.001,
                  "Horizontal movement must preserve perspective size");
            check(ground.camera.position.x == originalCamera.x && ground.camera.position.y == originalCamera.y &&
                  ground.camera.position.z == originalCamera.z, "Camera must remain fixed");
            ground.place({size.x/2, 0});
            const auto farStart = ground.screen(ground.position);
            const float farStep = ground.screen(ground.step(90, 0.1)).x - farStart.x;
            ground.place({size.x/2, size.y});
            const auto nearStart = ground.screen(ground.position);
            const float nearStep = ground.screen(ground.step(90, 0.1)).x - nearStart.x;
            check(nearStep > farStep, "Same world speed must move a larger/nearer character faster on screen");
            const float zoomedStep = ground.screen(ground.step(90, 0.16)).x - nearStart.x;
            check(std::abs(zoomedStep/nearStep - 1.6) < 0.002, "Manual zoom must scale ground stride consistently");
            ground.place({-500, 10000});
            check(ground.screen(ground.position).x < 100, "Character must be allowed near the horizontal screen edge");
            const auto crop = ground.crop();
            double left, right, bottom, top;
            ground.frustum(crop, PetGround::NearPlane, left, right, bottom, top);
            const auto view = GetCameraMatrix(ground.camera);
            const auto projection = MatrixFrustum(left, right, bottom, top, PetGround::NearPlane, PetGround::FarPlane);
            auto point = QuaternionTransform({ground.position.x, 1, ground.position.z, 1}, view);
            point = QuaternionTransform(point, projection);
            const Vector2 local{(point.x/point.w + 1)*240, (1 - point.y/point.w)*240};
            const auto full = ground.screen({ground.position.x, 1, ground.position.z});
            check(std::abs(local.x + crop.x - full.x) < 0.01 && std::abs(local.y + crop.y - full.y) < 0.01,
                  "Cropped frustum must match the fixed full-desktop projection");
            check(crop.x >= 0 && crop.y >= 0 && crop.x + 480 <= size.x && crop.y + 480 <= size.y,
                  "Presentation crop must stay in work area");
            check(!ground.canWalk(0, 100), "Out-of-work-area walks must be rejected");
            ground.place({size.x/2, 0});
            const float farHeight = pixelHeight();
            ground.place({size.x/2, size.y});
            const float nearHeight = pixelHeight();
            check(nearHeight > farHeight && nearHeight/farHeight < 1.28f,
                  "Size change must remain moderate across supported work areas");
            if (ground.width == 1920 && ground.height == 1008)
                check(std::abs(nearHeight/farHeight - 1.25f) < 0.01f,
                      "Current work area must have approximately 25 percent size difference");
            std::cout << "Depth size change: " << (nearHeight/farHeight - 1)*100 << "%\n";
            std::cout << "PASS " << ground.width << 'x' << ground.height << " perspective + plane + bounds\n";
        }
        return 0;
    } catch (const std::exception& e) { std::cerr << e.what() << '\n'; return 1; }
}
