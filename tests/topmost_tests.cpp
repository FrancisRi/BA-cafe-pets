#include "native_topmost.h"
#include <iostream>
#include <stdexcept>
static void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
int main(){
    std::vector<HWND> windows;
    try{
        WNDCLASSW wc{};wc.lpfnWndProc=DefWindowProcW;wc.hInstance=GetModuleHandleW(nullptr);wc.lpszClassName=L"BaCafeTopmostTest";RegisterClassW(&wc);
        for(int i=0;i<5;++i){
            auto hwnd=CreateWindowExW(WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE,wc.lpszClassName,L"",WS_POPUP,-30000,-30000,8,8,nullptr,nullptr,wc.hInstance,nullptr);
            check(hwnd!=nullptr,"Create test window");windows.push_back(hwnd);
        }
        NativeTopmostOrder order;std::vector<HWND> chain{windows[0],windows[1],windows[2],windows[3]};
        const auto active=GetForegroundWindow();
        check(order.apply(chain,0),"Initial chain must apply");
        auto verify=[&]{
            for(size_t i=0;i<chain.size();++i){
                check(IsWindowVisible(chain[i])&&!IsIconic(chain[i]),"Restore visible non-minimized window");
                check(GetWindowLongPtrW(chain[i],GWL_EXSTYLE)&WS_EX_TOPMOST,"Every display/input window must be topmost");
                if(i)check(GetWindow(chain[i],GW_HWNDPREV)==chain[i-1],"Keep depth/input chain order");
                RECT r{};GetWindowRect(chain[i],&r);check(r.left==-30000&&r.top==-30000&&r.right-r.left==8,"Keep geometry");
            }
            check(GetForegroundWindow()==active,"Must not steal focus");
        };
        verify();check(!order.apply(chain,0.01),"Stable frame must not reorder windows");
        ShowWindow(chain[2],SW_HIDE);order.apply(chain,0.3);verify();
        ShowWindow(chain[3],SW_SHOWMINNOACTIVE);check(IsIconic(chain[3]),"Minimize setup");order.apply(chain,0.6);verify();
        SetWindowPos(chain[2],HWND_NOTOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        order.apply(chain,0.9);verify();
        SetWindowPos(chain[3],HWND_TOPMOST,0,0,0,0,SWP_NOMOVE|SWP_NOSIZE|SWP_NOACTIVATE);
        order.apply(chain,1.2);verify();
        auto closed=chain.back();ShowWindow(closed,SW_HIDE);chain.pop_back();order.apply(chain,1.3);verify();
        order.apply(chain,1.6);check(!IsWindowVisible(closed),"Intentionally hidden actor must stay hidden");
        check(!IsWindowVisible(windows[4]),"Unrelated window must stay hidden");
        for(auto hwnd:windows)DestroyWindow(hwnd);
        std::cout<<"Topmost repair, hidden/minimized recovery, depth order, geometry, no focus steal and intentional hiding passed\n";
    }catch(const std::exception& e){for(auto hwnd:windows)DestroyWindow(hwnd);std::cerr<<e.what()<<'\n';return 1;}
}
