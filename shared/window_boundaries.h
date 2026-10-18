#pragma once
#include <dwmapi.h>
#include <vector>
#include <unordered_set>
#include "screen_boundary.h"

// Cache desktop z-order briefly. Only original frame edges count, not the new
// edges created by occlusion. No screenshots or user-window modifications.
struct VisibleWindowEdges {
    struct Entry { HWND window{};RECT rect{};HRGN region{};bool application=false; };
    std::vector<Entry> entries;
    ULONGLONG refreshed=0;
    ~VisibleWindowEdges(){clear();}
    void clear(){for(auto& e:entries)if(e.region)DeleteObject(e.region);entries.clear();}
    void refresh(HWND petWindow,bool force=false) {
        const auto now=GetTickCount64();if(!force&&now-refreshed<100)return;
        refreshed=now;clear();std::unordered_set<HWND> visited;
        for(HWND w=GetTopWindow(nullptr);w&&visited.insert(w).second;w=GetWindow(w,GW_HWNDNEXT)) {
            if(w==petWindow||!IsWindowVisible(w)||IsIconic(w))continue;
            // Every actor has an invisible, non-layered input HWND. Neither
            // it nor another pet surface may occlude desktop window edges.
            DWORD process=0;GetWindowThreadProcessId(w,&process);
            if(process==GetCurrentProcessId())continue;
            DWORD cloaked=0;DwmGetWindowAttribute(w,DWMWA_CLOAKED,&cloaked,sizeof(cloaked));if(cloaked)continue;
            const auto style=GetWindowLongPtrW(w,GWL_EXSTYLE);
            if(style&WS_EX_TRANSPARENT)continue;
            if(style&WS_EX_LAYERED){BYTE alpha=255;DWORD flags=0;COLORREF key=0;
                if(GetLayeredWindowAttributes(w,&key,&alpha,&flags)&&(flags&LWA_ALPHA)&&alpha==0)continue;}
            RECT rect{};if(FAILED(DwmGetWindowAttribute(w,DWMWA_EXTENDED_FRAME_BOUNDS,&rect,sizeof(rect))))GetWindowRect(w,&rect);
            if(rect.right<=rect.left||rect.bottom<=rect.top)continue;
            HRGN region=CreateRectRgn(0,0,0,0);
            if(GetWindowRgn(w,region)==ERROR){DeleteObject(region);region=nullptr;}
            else {RECT outer{};GetWindowRect(w,&outer);OffsetRgn(region,outer.left,outer.top);}
            entries.push_back({w,rect,region,BattleDesktopInput::application(w)});
        }
    }
    bool exposed(size_t index,POINT point) const {
        const auto& own=entries[index];if(!PtInRect(&own.rect,point)||(own.region&&!PtInRegion(own.region,point.x,point.y)))return false;
        for(size_t i=0;i<index;++i){const auto& e=entries[i];
            if(PtInRect(&e.rect,point)&&(!e.region||PtInRegion(e.region,point.x,point.y)))return false;}
        return true;
    }
    float distance(const PetGround& ground,float yaw,float maximum,LONG originX,LONG originY,HWND petWindow,bool precise=false) {
        refresh(petWindow);float nearest=INFINITY;
        for(size_t i=0;i<entries.size();++i) {
            const auto& e=entries[i];if(!e.application)continue;
            const PetGround::Area area{float(e.rect.left-originX),float(e.rect.top-originY),float(e.rect.right-originX),float(e.rect.bottom-originY)};
            const auto visible=[&](Vector2 p,const PetGround::Area&){
                // Test just inside the actual frame (right/bottom are exclusive).
                POINT point{std::clamp(LONG(std::lround(p.x))+originX,e.rect.left,e.rect.right-1),
                            std::clamp(LONG(std::lround(p.y))+originY,e.rect.top,e.rect.bottom-1)};
                return exposed(i,point);
            };
            const float d=precise?projectedBoundaryDistance(ground,yaw,maximum,{area},visible):ground.boundaryDistance(yaw,maximum,{area},visible);
            nearest=std::min(nearest,d);
        }
        return nearest;
    }
};
