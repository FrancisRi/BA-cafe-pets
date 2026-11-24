#pragma once
#include "native_messages.h"
#include <cstddef>
#include <numeric>

struct NativeMessageBubble {
    struct Surface {
        HDC dc=CreateCompatibleDC(nullptr);
        HBITMAP bitmap{};
        HGDIOBJ previous{};
        std::byte* bits{};
        int width=0,height=0;
        ~Surface(){clear();if(dc)DeleteDC(dc);}
        void clear(){
            if(previous)SelectObject(dc,previous);
            if(bitmap)DeleteObject(bitmap);
            previous=nullptr;bitmap=nullptr;bits=nullptr;
        }
        bool resize(int w,int h){
            if(bitmap&&width==w&&height==h)return true;
            clear();width=w;height=h;
            BITMAPINFO info{};auto& header=info.bmiHeader;
            header.biSize=sizeof(header);header.biWidth=w;header.biHeight=-h;
            header.biPlanes=1;header.biBitCount=32;header.biCompression=BI_RGB;
            void* pixels{};
            bitmap=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&pixels,nullptr,0);
            if(!bitmap)return false;
            bits=static_cast<std::byte*>(pixels);previous=SelectObject(dc,bitmap);
            return true;
        }
    };
    HWND window{};
    std::optional<Surface> surface,capture;
    std::vector<std::byte> horizontal,blurred;
    NativeFluent theme;
    ULONG_PTR gdiplus{};
    std::optional<NativeMessages::Message> current;
    ULONGLONG expires=0,earliestAdvance=0,nextPaint=0;
    int opacity=-1,blur=-1;
    unsigned revision=0,paintedRevision=0;
    POINT origin{};

    ~NativeMessageBubble(){
        if(window)DestroyWindow(window);
        if(gdiplus)Gdiplus::GdiplusShutdown(gdiplus);
    }
    HWND visible() const {return window&&IsWindowVisible(window)?window:nullptr;}
    void hide(){if(visible()){ShowWindow(window,SW_HIDE);SetWindowTextW(window,L"");}}
    bool open(){
        if(window)return true;
        if(!gdiplus){
            Gdiplus::GdiplusStartupInput input;
            if(Gdiplus::GdiplusStartup(&gdiplus,&input,nullptr)!=Gdiplus::Ok)return false;
        }
        WNDCLASSW wc{};
        wc.hInstance=GetModuleHandleW(nullptr);wc.lpfnWndProc=DefWindowProcW;wc.lpszClassName=L"BaCafeMessageBubble";
        RegisterClassW(&wc);
        window=CreateWindowExW(WS_EX_LAYERED|WS_EX_TRANSPARENT|WS_EX_TOOLWINDOW|WS_EX_NOACTIVATE|WS_EX_TOPMOST,
            wc.lpszClassName,L"",WS_POPUP,0,0,0,0,nullptr,nullptr,wc.hInstance,nullptr);
        return window!=nullptr;
    }
    static POINT position(RECT model,RECT work,SIZE box,int gap){
        const LONG middle=std::midpoint(model.left,model.right);
        const LONG left=std::clamp(middle-box.cx/2,work.left,std::max(work.left,work.right-box.cx));
        LONG top=model.top-gap-box.cy;
        if(top<work.top&&model.bottom+gap+box.cy<=work.bottom)top=model.bottom+gap;
        return {left,std::clamp(top,work.top,std::max(work.top,work.bottom-box.cy))};
    }
    // Separable box filter using a running sum in each axis.
    static void box(const std::byte* from,std::byte* to,int length,int lines,int advance,int stride,int radius){
        const int count=radius*2+1;
        for(int line=0;line<lines;++line){
            const auto* source=from+size_t(line)*stride;
            auto* dest=to+size_t(line)*stride;
            for(int channel=0;channel<3;++channel){
                const auto sample=[&](int i){return std::to_integer<int>(source[size_t(std::clamp(i,0,length-1))*advance+channel]);};
                int total=0;
                for(int i=-radius;i<=radius;++i)total+=sample(i);
                for(int i=0;i<length;++i){
                    dest[size_t(i)*advance+channel]=std::byte(total/count);
                    total+=sample(i+radius+1)-sample(i-radius);
                }
            }
        }
    }
    bool backdrop(POINT at,int width,int height,int radius,RECT work){
        const int x=std::max<LONG>(work.left,at.x-radius),y=std::max<LONG>(work.top,at.y-radius);
        const int w=std::min<LONG>(work.right,at.x+width+radius)-x;
        const int h=std::min<LONG>(work.bottom,at.y+height+radius)-y;
        if(!capture)capture.emplace();
        if(!capture->resize(w,h))return false;
        HDC desktop=GetDC(nullptr);
        if(!desktop)return false;
        // SRCCOPY omits layered windows, including the bubble itself.
        const bool copied=BitBlt(capture->dc,0,0,w,h,desktop,x,y,SRCCOPY)!=0;
        ReleaseDC(nullptr,desktop);
        if(!copied)return false;
        GdiFlush();
        horizontal.resize(size_t(w)*h*4);blurred.resize(size_t(width)*height*4);
        box(capture->bits,horizontal.data(),w,h,4,w*4,radius);
        box(horizontal.data(),capture->bits,h,w,w*4,4,radius);
        for(int row=0;row<height;++row){
            const auto* source=capture->bits+(size_t(row+at.y-y)*w+at.x-x)*4;
            auto* dest=blurred.data()+size_t(row)*width*4;
            for(int col=0;col<width;++col){
                dest[col*4]=source[col*4];dest[col*4+1]=source[col*4+1];
                dest[col*4+2]=source[col*4+2];dest[col*4+3]=std::byte{255};
            }
        }
        return true;
    }
    static Gdiplus::Color color(COLORREF rgb,BYTE alpha=255){
        return {alpha,GetRValue(rgb),GetGValue(rgb),GetBValue(rgb)};
    }
    bool paint(RECT model,RECT work,UINT dpi,int tint,int radius){
        using namespace Gdiplus;
        if(!open())return false;
        if(!surface)surface.emplace();
        if(dpi!=theme.dpi||!theme.font||paintedRevision!=revision)theme.load(dpi);
        const auto px=[&](int value){return theme.px(value);};
        const int width=std::max(1,std::min<int>(px(328),work.right-work.left));
        const int padding=std::min(px(14),width/4),textWidth=width-2*padding;
        Gdiplus::Font heading(L"Segoe UI",REAL(px(14)),FontStyleBold,UnitPixel);
        Gdiplus::Font body(L"Segoe UI",REAL(px(14)),FontStyleRegular,UnitPixel);
        StringFormat format;
        format.SetTrimming(StringTrimmingEllipsisCharacter);
        format.SetFormatFlags(StringFormatFlagsLineLimit);
        const auto heightFor=[&](const std::wstring& text,Gdiplus::Font& font,int maxHeight){
            if(text.empty())return 0;
            Graphics measure(surface->dc);
            RectF layout(0,0,REAL(textWidth),REAL(maxHeight)),bounds;
            if(measure.MeasureString(text.c_str(),int(text.size()),&font,layout,&format,&bounds)!=Ok)return maxHeight;
            return std::clamp(int(std::ceil(bounds.Height)),px(19),maxHeight);
        };
        const int titleHeight=heightFor(current->title,heading,px(22));
        const int bodyHeight=heightFor(current->body,body,px(100));
        const int separation=titleHeight&&bodyHeight?px(3):0;
        const int height=std::min<int>(titleHeight+bodyHeight+2*padding+separation,work.bottom-work.top);
        POINT at=position(model,work,{width,height},px(8));
        if(height<=0||!surface->resize(width,height))return false;
        const bool frosted=radius>0&&tint<100&&!theme.highContrast&&backdrop(at,width,height,px(radius),work);
        Bitmap output(width,height,width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(surface->bits));
        {
            Graphics g(&output);
            g.Clear(Gdiplus::Color(0,0,0,0));
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            g.SetTextRenderingHint(TextRenderingHintAntiAliasGridFit);
            GraphicsPath shape;
            const REAL d=REAL(std::min({px(22),width-2,height-2}));
            if(d<=0)return false;
            shape.AddArc(0.5f,0.5f,d,d,180,90);shape.AddArc(width-d-0.5f,0.5f,d,d,270,90);
            shape.AddArc(width-d-0.5f,height-d-0.5f,d,d,0,90);shape.AddArc(0.5f,height-d-0.5f,d,d,90,90);
            shape.CloseFigure();
            const COLORREF panel=theme.highContrast?theme.surface:theme.dark?RGB(31,47,59):RGB(244,251,255);
            if(frosted){
                Bitmap image(width,height,width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(blurred.data()));
                TextureBrush texture(&image,WrapModeClamp);
                g.FillPath(&texture,&shape);
            }
            SolidBrush background(color(panel,theme.highContrast?255:BYTE(MulDiv(tint,255,100))));
            g.FillPath(&background,&shape);
            Pen border(color(theme.accent,theme.highContrast?255:108),1.0f);
            g.DrawPath(&border,&shape);
            SolidBrush titleInk(color(theme.accent)),bodyInk(color(theme.text));
            if(titleHeight)g.DrawString(current->title.c_str(),int(current->title.size()),&heading,
                RectF(REAL(padding),REAL(padding),REAL(textWidth),REAL(titleHeight)),&format,&titleInk);
            if(bodyHeight)g.DrawString(current->body.c_str(),int(current->body.size()),&body,
                RectF(REAL(padding),REAL(padding+titleHeight+separation),REAL(textWidth),REAL(bodyHeight)),&format,&bodyInk);
        }
        SIZE size{width,height};POINT source{};
        BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        if(!UpdateLayeredWindow(window,nullptr,&at,&size,surface->dc,&source,0,&blend,ULW_ALPHA))return false;
        origin=at;
        if(paintedRevision!=revision||!visible())SetWindowTextW(window,(current->title+L"\n"+current->body).c_str());
        ShowWindow(window,SW_SHOWNOACTIVATE);
        return true;
    }
    void update(NativeMessages& messages,HWND actor,std::optional<RECT> model,bool paused,int tint,int radius){
        const auto now=GetTickCount64();
        if(paused){hide();current.reset();messages.pending.clear();return;}
        if(current){
            using Phase=NativeMessages::Phase;
            const bool advance=!messages.pending.empty()&&now>=earliestAdvance&&
                (current->phase==Phase::Start||current->phase==Phase::Progress||
                 messages.pending.front().phase==Phase::Start);
            if(now>=expires||advance)current.reset();
        }
        if(!model){hide();return;}
        if(!current&&!messages.pending.empty()){
            current=std::move(messages.pending.front());messages.pending.pop_front();
            earliestAdvance=now+1300;
            expires=now+std::clamp<ULONGLONG>(4500+current->body.size()*24,5000,11000);++revision;
        }
        if(!current){hide();return;}
        const POINT top{std::midpoint(model->left,model->right),model->top};
        const HMONITOR monitor=MonitorFromPoint(top,MONITOR_DEFAULTTONEAREST);
        MONITORINFO info{sizeof(info)};if(!GetMonitorInfoW(monitor,&info))return;
        // The actor receives WM_DPICHANGED as it moves between monitors.
        const auto dpi=GetDpiForWindow(actor);
        const bool redraw=!visible()||paintedRevision!=revision||opacity!=tint||blur!=radius||theme.dpi!=dpi;
        if(redraw||(radius>0&&tint<100&&now>=nextPaint)){
            if(!paint(*model,info.rcWork,dpi,tint,radius)){hide();return;}
            opacity=tint;blur=radius;paintedRevision=revision;nextPaint=now+120;
        }else{
            const auto at=position(*model,info.rcWork,{surface->width,surface->height},theme.px(8));
            if(at.x!=origin.x||at.y!=origin.y){
                SetWindowPos(window,nullptr,at.x,at.y,0,0,SWP_NOSIZE|SWP_NOACTIVATE|SWP_NOZORDER);
                origin=at;
            }
        }
    }
};
