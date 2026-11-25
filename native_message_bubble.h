#pragma once
#include "native_messages.h"
#include <d2d1.h>
#include <dwrite.h>
#include <wrl/client.h>
#include <cstddef>
#include <cstring>
#include <numeric>

struct NativeMessageBubble {
    template<class T> using ComPtr=Microsoft::WRL::ComPtr<T>;
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
    std::optional<Surface> surface,capture,textLayer;
    std::vector<std::byte> cleanBackdrop,horizontal,blurred;
    RECT cleanBounds{},blurredBounds{};
    ULONGLONG nextCapture=0;
    int cachedRadius=-1;
    NativeFluent theme;
    ULONG_PTR gdiplus{};
    ComPtr<IDWriteFactory> textFactory;
    ComPtr<ID2D1Factory> drawingFactory;
    ComPtr<ID2D1DCRenderTarget> textTarget;
    struct TextGeometry {
        std::wstring title,body;
        UINT dpi=0;
        int width=0,heightLimit=0,height=0,padding=0,bodyTop=0;
        int titleWidth=0,titleHeight=0,bodyWidth=0,bodyHeight=0;
        COLORREF titleInk=CLR_INVALID,bodyInk=CLR_INVALID;
    } text;
    std::optional<NativeMessages::Message> current;
    std::string modelName;
    std::wstring modelTitle;
    ULONGLONG expires=0,earliestAdvance=0,nextPaint=0;
    int opacity=-1,blur=-1;
    unsigned revision=0,paintedRevision=0;
    POINT origin{};

    ~NativeMessageBubble(){
        if(window)DestroyWindow(window);
        if(gdiplus)Gdiplus::GdiplusShutdown(gdiplus);
    }
    HWND visible() const {return window&&IsWindowVisible(window)?window:nullptr;}
    void hide(){if(visible()){ShowWindow(window,SW_HIDE);SetWindowTextW(window,L"");nextCapture=0;}}
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
            std::array<int,3> total{};
            for(int i=-radius;i<=radius;++i){
                const auto* p=source+size_t(std::clamp(i,0,length-1))*advance;
                for(int ch=0;ch<3;++ch)total[ch]+=std::to_integer<int>(p[ch]);
            }
            for(int i=0;i<length;++i){
                const auto* add=source+size_t(std::min(i+radius+1,length-1))*advance;
                const auto* drop=source+size_t(std::max(i-radius,0))*advance;
                auto* pixel=dest+size_t(i)*advance;
                for(int ch=0;ch<3;++ch){
                    pixel[ch]=std::byte(total[ch]/count);
                    total[ch]+=std::to_integer<int>(add[ch])-std::to_integer<int>(drop[ch]);
                }
            }
        }
    }
    // Keep the pre-composition pixels where the old bubble covers the capture.
    // Desktop capture can include layered pixels on some DWM paths.
    void restoreBackdrop(std::byte* pixels,RECT bounds,RECT occluded,BYTE neutral) const {
        RECT covered{};
        if(!IntersectRect(&covered,&bounds,&occluded))return;
        const size_t stride=size_t(bounds.right-bounds.left)*4;
        for(LONG row=covered.top;row<covered.bottom;++row){
            auto* dest=pixels+size_t(row-bounds.top)*stride+size_t(covered.left-bounds.left)*4;
            std::fill_n(dest,size_t(covered.right-covered.left)*4,std::byte{neutral});
        }
        RECT available{};
        if(cleanBackdrop.empty()||!IntersectRect(&available,&covered,&cleanBounds))return;
        const size_t oldStride=size_t(cleanBounds.right-cleanBounds.left)*4;
        for(LONG row=available.top;row<available.bottom;++row){
            auto* dest=pixels+size_t(row-bounds.top)*stride+size_t(available.left-bounds.left)*4;
            const auto* source=cleanBackdrop.data()+size_t(row-cleanBounds.top)*oldStride+size_t(available.left-cleanBounds.left)*4;
            std::memcpy(dest,source,size_t(available.right-available.left)*4);
        }
    }
    bool sampleBackdrop(POINT at,int width,int height,int margin,RECT work,BYTE neutral){
        const RECT bounds{std::max<LONG>(work.left,at.x-margin),std::max<LONG>(work.top,at.y-margin),
            std::min<LONG>(work.right,at.x+width+margin),std::min<LONG>(work.bottom,at.y+height+margin)};
        const auto now=GetTickCount64();
        if(now<nextCapture&&EqualRect(&bounds,&cleanBounds))return true;
        const int w=bounds.right-bounds.left,h=bounds.bottom-bounds.top;
        if(!capture)capture.emplace();
        if(!capture->resize(w,h))return false;
        HDC desktop=GetDC(nullptr);
        if(!desktop)return false;
        const bool copied=BitBlt(capture->dc,0,0,w,h,desktop,bounds.left,bounds.top,SRCCOPY)!=0;
        ReleaseDC(nullptr,desktop);
        if(!copied)return false;
        GdiFlush();
        RECT occluded{};
        if(visible()&&GetWindowRect(window,&occluded))restoreBackdrop(capture->bits,bounds,occluded,neutral);
        const auto bytes=size_t(w)*h*4;
        if(!EqualRect(&bounds,&cleanBounds)||cleanBackdrop.size()!=bytes||
            std::memcmp(cleanBackdrop.data(),capture->bits,bytes)!=0){
            cleanBackdrop.assign(capture->bits,capture->bits+bytes);
            cleanBounds=bounds;cachedRadius=-1;
        }
        nextCapture=now+120;
        return true;
    }
    void blurBackdrop(POINT at,int width,int height,int radius){
        const RECT bounds{at.x,at.y,at.x+width,at.y+height};
        if(radius==cachedRadius&&EqualRect(&bounds,&blurredBounds))return;
        const int w=cleanBounds.right-cleanBounds.left,h=cleanBounds.bottom-cleanBounds.top;
        horizontal.resize(size_t(w)*h*4);blurred.resize(size_t(width)*height*4);
        box(cleanBackdrop.data(),horizontal.data(),w,h,4,w*4,radius);
        box(horizontal.data(),capture->bits,h,w,w*4,4,radius);
        for(int row=0;row<height;++row){
            const auto* source=capture->bits+(size_t(row+at.y-cleanBounds.top)*w+at.x-cleanBounds.left)*4;
            auto* dest=blurred.data()+size_t(row)*width*4;
            std::memcpy(dest,source,size_t(width)*4);
            for(int col=0;col<width;++col)dest[col*4+3]=std::byte{255};
        }
        blurredBounds=bounds;cachedRadius=radius;
    }
    static void blendText(std::byte* pixels,const std::byte* text,size_t count){
        for(size_t i=0;i<count;i+=4){
            const int remaining=255-std::to_integer<int>(text[i+3]);
            if(remaining==255)continue;
            for(size_t channel=0;channel<4;++channel)
                pixels[i+channel]=std::byte(std::to_integer<int>(text[i+channel])+
                    (std::to_integer<int>(pixels[i+channel])*remaining+127)/255);
        }
    }
    static Gdiplus::Color color(COLORREF rgb,BYTE alpha=255){
        return {alpha,GetRValue(rgb),GetGValue(rgb),GetBValue(rgb)};
    }
    static BYTE inkFor(int background){
        return background>=118?0:255;
    }
    bool openTextTarget(){
        if(!textFactory&&FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,__uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(textFactory.GetAddressOf()))))return false;
        if(!drawingFactory&&FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED,drawingFactory.GetAddressOf())))return false;
        if(!textTarget){
            const auto properties=D2D1::RenderTargetProperties(D2D1_RENDER_TARGET_TYPE_DEFAULT,
                D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM,D2D1_ALPHA_MODE_PREMULTIPLIED),96,96,
                D2D1_RENDER_TARGET_USAGE_GDI_COMPATIBLE);
            if(FAILED(drawingFactory->CreateDCRenderTarget(&properties,textTarget.GetAddressOf())))return false;
            textTarget->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
        }
        return true;
    }
    bool layoutText(const std::wstring& value,int size,DWRITE_FONT_WEIGHT weight,int width,int limit,
        ComPtr<IDWriteTextLayout>& layout,DWRITE_TEXT_METRICS& metrics){
        if(value.empty())return true;
        ComPtr<IDWriteTextFormat> format;
        if(FAILED(textFactory->CreateTextFormat(L"Segoe UI",nullptr,weight,DWRITE_FONT_STYLE_NORMAL,
            DWRITE_FONT_STRETCH_NORMAL,float(size),L"zh-CN",format.GetAddressOf())))return false;
        if(FAILED(textFactory->CreateTextLayout(value.c_str(),UINT32(value.size()),format.Get(),float(width),
            float(limit),layout.GetAddressOf())))return false;
        const DWRITE_TRIMMING trimming{DWRITE_TRIMMING_GRANULARITY_CHARACTER,0,0};
        ComPtr<IDWriteInlineObject> ellipsis;
        if(SUCCEEDED(textFactory->CreateEllipsisTrimmingSign(format.Get(),ellipsis.GetAddressOf())))
            layout->SetTrimming(&trimming,ellipsis.Get());
        return SUCCEEDED(layout->GetMetrics(&metrics));
    }
    bool drawTextMask(int padding,int bodyTop,IDWriteTextLayout* title,IDWriteTextLayout* body){
        const RECT bounds{0,0,textLayer->width,textLayer->height};
        if(FAILED(textTarget->BindDC(textLayer->dc,&bounds)))return false;
        ComPtr<ID2D1SolidColorBrush> white;
        if(FAILED(textTarget->CreateSolidColorBrush(D2D1::ColorF(D2D1::ColorF::White),white.GetAddressOf())))return false;
        textTarget->BeginDraw();
        textTarget->Clear(D2D1::ColorF(0.f,0.f,0.f,0.f));
        if(title)textTarget->DrawTextLayout(D2D1::Point2F(float(padding),float(padding)),title,white.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        if(body)textTarget->DrawTextLayout(D2D1::Point2F(float(padding),float(bodyTop)),body,white.Get(),D2D1_DRAW_TEXT_OPTIONS_CLIP);
        const auto result=textTarget->EndDraw();
        if(result==D2DERR_RECREATE_TARGET)textTarget.Reset();
        GdiFlush();
        return SUCCEEDED(result);
    }
    static void colorText(std::byte* from,std::byte* to,COLORREF ink){
        const std::array channels{GetBValue(ink),GetGValue(ink),GetRValue(ink)};
        for(auto* pixel=from;pixel<to;pixel+=4){
            const int coverage=std::to_integer<int>(pixel[3]);
            if(!coverage)continue;
            for(size_t ch=0;ch<channels.size();++ch)pixel[ch]=std::byte((channels[ch]*coverage+127)/255);
        }
    }
    bool prepareText(int width,int limit,UINT dpi){
        if(!openTextTarget())return false;
        const auto px=[&](int value){return theme.px(value);};
        const int padding=std::min(px(14),width/4),textWidth=width-2*padding;
        const int titleLimit=px(28),bodyLimit=px(120);
        ComPtr<IDWriteTextLayout> title,body;
        DWRITE_TEXT_METRICS titleMetrics{},bodyMetrics{};
        if(!layoutText(current->title,px(16),DWRITE_FONT_WEIGHT_SEMI_BOLD,textWidth,titleLimit,title,titleMetrics)||
            !layoutText(current->body,px(16),DWRITE_FONT_WEIGHT_MEDIUM,textWidth,bodyLimit,body,bodyMetrics))return false;
        const int titleHeight=std::min(int(std::ceil(titleMetrics.height)),titleLimit);
        const int bodyHeight=std::min(int(std::ceil(bodyMetrics.height)),bodyLimit);
        const int separation=titleHeight&&bodyHeight?px(3):0;
        const int bodyTop=padding+titleHeight+separation;
        const int height=std::min(titleHeight+bodyHeight+2*padding+separation,limit);
        if(height<=0||!textLayer->resize(width,height)||!drawTextMask(padding,bodyTop,title.Get(),body.Get())){
            text={};return false;
        }
        text={current->title,current->body,dpi,width,limit,height,padding,bodyTop,
            int(std::ceil(titleMetrics.width)),titleHeight,int(std::ceil(bodyMetrics.width)),bodyHeight};
        return true;
    }
    bool paint(RECT model,RECT work,UINT dpi,int tint,int radius){
        using namespace Gdiplus;
        if(!open())return false;
        if(!surface){surface.emplace();textLayer.emplace();}
        if(dpi!=theme.dpi||!theme.font||paintedRevision!=revision)theme.load(dpi);
        const auto px=[&](int value){return theme.px(value);};
        const int width=std::max(1,std::min<int>(px(352),work.right-work.left)),limit=work.bottom-work.top;
        const bool textChanged=text.dpi!=dpi||text.width!=width||text.heightLimit!=limit||
            text.title!=current->title||text.body!=current->body;
        if(textChanged&&!prepareText(width,limit,dpi))return false;
        const auto height=text.height,padding=text.padding;
        POINT at=position(model,work,{width,height},px(8));
        if(!surface->resize(width,height))return false;
        const BYTE neutral=theme.dark?30:247;
        const COLORREF panel=theme.highContrast?theme.surface:RGB(neutral,neutral,neutral);
        const bool useBlur=radius>0&&tint<100&&!theme.highContrast;
        const bool captured=!theme.highContrast&&sampleBackdrop(at,width,height,px(24),work,neutral);
        if(useBlur&&captured)blurBackdrop(at,width,height,px(radius));
        {
            Bitmap output(width,height,width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(surface->bits));
            Graphics g(&output);
            g.Clear(Gdiplus::Color(0,0,0,0));
            g.SetSmoothingMode(SmoothingModeAntiAlias);
            GraphicsPath shape;
            const REAL d=REAL(std::min({px(22),width-2,height-2}));
            if(d<=0)return false;
            shape.AddArc(0.5f,0.5f,d,d,180,90);shape.AddArc(width-d-0.5f,0.5f,d,d,270,90);
            shape.AddArc(width-d-0.5f,height-d-0.5f,d,d,0,90);shape.AddArc(0.5f,height-d-0.5f,d,d,90,90);
            shape.CloseFigure();
            if(useBlur&&captured){
                Bitmap image(width,height,width*4,PixelFormat32bppPARGB,reinterpret_cast<BYTE*>(blurred.data()));
                TextureBrush texture(&image,WrapModeClamp);
                g.FillPath(&texture,&shape);
            }
            SolidBrush background(color(panel,theme.highContrast?255:BYTE(MulDiv(tint,255,100))));
            g.FillPath(&background,&shape);
            const BYTE outline=theme.dark?198:86;
            Pen border(color(theme.highContrast?theme.text:RGB(outline,outline,outline),theme.highContrast?255:75),1.0f);
            g.DrawPath(&border,&shape);
        }
        const auto textColor=[&](int top,int lines,int columns){
            if(theme.highContrast)return theme.text;
            const int stride=useBlur?width:cleanBounds.right-cleanBounds.left;
            const auto* pixels=useBlur?blurred.data():cleanBackdrop.data();
            const int x=useBlur?0:at.x-cleanBounds.left,y=useBlur?0:at.y-cleanBounds.top;
            int samples=0,luminance=0;
            if(captured)for(int row=top;row<std::min(top+lines,height);row+=4)
                for(int col=padding;col<std::min(padding+columns,width-padding);col+=4){
                    const auto* p=pixels+(size_t(y+row)*stride+x+col)*4;
                    luminance+=(19*std::to_integer<int>(p[0])+183*std::to_integer<int>(p[1])+54*std::to_integer<int>(p[2]))/256;
                    ++samples;
                }
            const int desktop=samples?luminance/samples:neutral;
            const BYTE ink=inkFor((neutral*tint+desktop*(100-tint))/100);
            return RGB(ink,ink,ink);
        };
        auto* const bodyPixels=textLayer->bits+size_t(std::min(text.bodyTop,height))*width*4;
        const auto titleInk=textColor(padding,text.titleHeight,text.titleWidth),bodyInk=textColor(text.bodyTop,text.bodyHeight,text.bodyWidth);
        if(text.titleInk!=titleInk){colorText(textLayer->bits,bodyPixels,titleInk);text.titleInk=titleInk;}
        if(text.bodyInk!=bodyInk){colorText(bodyPixels,textLayer->bits+size_t(width)*height*4,bodyInk);text.bodyInk=bodyInk;}
        blendText(surface->bits,textLayer->bits,size_t(width)*height*4);
        SIZE size{width,height};POINT source{};
        BLENDFUNCTION blend{AC_SRC_OVER,0,255,AC_SRC_ALPHA};
        if(!UpdateLayeredWindow(window,nullptr,&at,&size,surface->dc,&source,0,&blend,ULW_ALPHA))return false;
        origin=at;
        const bool shown=visible()!=nullptr;
        if(textChanged||!shown)SetWindowTextW(window,(current->title+L"\n"+current->body).c_str());
        if(!shown)ShowWindow(window,SW_SHOWNOACTIVATE);
        return true;
    }
    void update(NativeMessages& messages,HWND actor,std::optional<RECT> model,bool paused,int tint,int radius,std::string_view name){
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
            if(current->body.empty())current->body=current->title;
            earliestAdvance=now+1300;
            expires=now+std::clamp<ULONGLONG>(4500+current->body.size()*24,5000,11000);++revision;
        }
        if(!current){hide();return;}
        if(modelTitle.empty()||name!=modelName){
            modelName=name;modelTitle=name.empty()?L"桌面角色":wide(modelName);
        }
        if(current->title!=modelTitle){current->title=modelTitle;++revision;}
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
