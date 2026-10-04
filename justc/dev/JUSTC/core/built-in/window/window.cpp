/*

MIT License

Copyright (c) 2025-2026 JustStudio. <https://juststudio.is-a.dev/>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/

#include "window.hpp"
#include "../../utility.h"
#include <stdexcept>
#include <sstream>
#include <cstring>

namespace JUSTCWindow {

static std::unordered_map<uint64_t, WindowCreateOptions> managedWindows;

#ifdef _WIN32

static std::unique_ptr<WindowLib::IWindow> g_window;
static uint64_t g_windowHandle = 0;

Value Create(const std::vector<Value>& args, Parser* parser) {
    try {
        WindowCreateOptions options;
        
        if (!args.empty() && args[0].type == DataType::JSON_OBJECT) {
            auto props = args[0].properties;
            
            auto it = props.find("title");
            if (it != props.end()) options.title = parser->v(it->second).toString();
            
            it = props.find("x");
            if (it != props.end()) options.x = static_cast<int>(parser->v(it->second).toNumber());
            
            it = props.find("y");
            if (it != props.end()) options.y = static_cast<int>(parser->v(it->second).toNumber());
            
            it = props.find("width");
            if (it != props.end()) options.width = static_cast<int>(parser->v(it->second).toNumber());
            
            it = props.find("height");
            if (it != props.end()) options.height = static_cast<int>(parser->v(it->second).toNumber());
            
            it = props.find("resizable");
            if (it != props.end()) options.resizable = parser->v(it->second).toBoolean();
            
            it = props.find("visible");
            if (it != props.end()) options.visible = parser->v(it->second).toBoolean();
        }
        
        g_window = WindowLib::CreateClassicWindow();
        if (!g_window) {
            throw std::runtime_error("Failed to create window instance");
        }
        
        WindowLib::WindowConfig config;
        config.title = options.title.empty() ? L"JUSTC Window" : std::wstring(options.title.begin(), options.title.end());
        config.width = options.width > 0 ? options.width : 800;
        config.height = options.height > 0 ? options.height : 600;
        config.x = options.x;
        config.y = options.y;
        config.resizable = options.resizable;
        config.maximized = false;
        config.hInstance = GetModuleHandle(nullptr);
        
        if (!g_window->Create(config)) {
            throw std::runtime_error("Failed to create window");
        }
        
        g_windowHandle = reinterpret_cast<uint64_t>(g_window.get());
        managedWindows[g_windowHandle] = options;
        
        if (options.visible) {
            g_window->Show();
        }
        
        return Value::createNumberWithType(g_windowHandle, NumericType::UINT64);
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.Create: " + std::string(e.what()));
    }
}

WindowInfo getWindowInfo(uint64_t handle) {
    if (managedWindows.find(handle) == managedWindows.end()) {
        throw std::runtime_error("Window not found");
    }
    
    auto it = managedWindows.find(handle);
    WindowInfo info;
    info.handle = handle;
    info.title = it->second.title;
    info.x = it->second.x;
    info.y = it->second.y;
    info.width = it->second.width;
    info.height = it->second.height;
    info.isVisible = it->second.visible;
    
    return info;
}

std::vector<WindowInfo> getAllWindows() {
    std::vector<WindowInfo> windows;
    
    for (const auto& [handle, options] : managedWindows) {
        try {
            windows.push_back(getWindowInfo(handle));
        } catch (...) {}
    }
    
    return windows;
}

bool setWindowTitle(uint64_t handle, const std::string& title) {
    auto it = managedWindows.find(handle);
    if (it == managedWindows.end() || !g_window) {
        return false;
    }
    
    std::wstring wTitle(title.begin(), title.end());
    g_window->SetTitle(wTitle);
    it->second.title = title;
    return true;
}

bool setWindowPosition(uint64_t handle, int x, int y) {
    auto it = managedWindows.find(handle);
    if (it == managedWindows.end() || !g_window) {
        return false;
    }

    it->second.x = x;
    it->second.y = y;
    
    return true;
}

bool setWindowSize(uint64_t handle, int width, int height) {
    auto it = managedWindows.find(handle);
    if (it == managedWindows.end() || !g_window) {
        return false;
    }
    
    g_window->SetSize(width, height);
    it->second.width = width;
    it->second.height = height;
    return true;
}

bool showWindow(uint64_t handle) {
    auto it = managedWindows.find(handle);
    if (it == managedWindows.end() || !g_window) {
        return false;
    }
    
    g_window->Show();
    it->second.visible = true;
    return true;
}

bool hideWindow(uint64_t handle) {
    auto it = managedWindows.find(handle);
    if (it == managedWindows.end() || !g_window) {
        return false;
    }
    
    g_window->Hide();
    it->second.visible = false;
    return true;
}

bool closeWindow(uint64_t handle) {
    auto it = managedWindows.find(handle);
    if (it == managedWindows.end() || !g_window) {
        return false;
    }
    
    g_window->Close();
    managedWindows.erase(handle);
    g_window.reset();
    g_windowHandle = 0;
    return true;
}

Value RunMessageLoop(const std::vector<Value>& args) {
    if (!g_window) {
        throw std::runtime_error("No window to run message loop");
    }
    
    g_window->RunMessageLoop();
    return Value::createNull();
}

#elif defined(__linux__)

static Display* display = nullptr;
static Window rootWindow = 0;

void initX11() {
    if (display) return;
    
    display = XOpenDisplay(nullptr);
    if (!display) {
        throw std::runtime_error("Cannot open X11 display");
    }
    
    rootWindow = DefaultRootWindow(display);
}

Value Create(const std::vector<Value>& args, Parser* parser) {
    try {
        initX11();
        
        WindowCreateOptions options;
        
        if (!args.empty() && args[0].type == DataType::JSON_OBJECT) {
            auto props = args[0].properties;
            
            auto it = props.find("title");
            if (it != props.end()) options.title = parser->v(it->second).toString();
            
            it = props.find("x");
            if (it != props.end()) options.x = static_cast<int>(parser->v(it->second).toNumber());
            
            it = props.find("y");
            if (it != props.end()) options.y = static_cast<int>(parser->v(it->second).toNumber());
            
            it = props.find("width");
            if (it != props.end()) options.width = static_cast<int>(parser->v(it->second).toNumber());
            
            it = props.find("height");
            if (it != props.end()) options.height = static_cast<int>(parser->v(it->second).toNumber());
            
            it = props.find("visible");
            if (it != props.end()) options.visible = parser->v(it->second).toBoolean();
        }
        
        int screen = DefaultScreen(display);
        
        XSetWindowAttributes attrs;
        attrs.background_pixel = WhitePixel(display, screen);
        attrs.border_pixel = BlackPixel(display, screen);
        attrs.event_mask = ExposureMask | KeyPressMask | ButtonPressMask | StructureNotifyMask;
        
        Window window = XCreateWindow(
            display, rootWindow,
            options.x, options.y,
            options.width, options.height,
            1,
            CopyFromParent,
            InputOutput,
            CopyFromParent,
            CWBackPixel | CWBorderPixel | CWEventMask,
            &attrs
        );
        
        XStoreName(display, window, options.title.c_str());
        
        Atom netWmName = XInternAtom(display, "_NET_WM_NAME", False);
        XChangeProperty(display, window, netWmName, XA_STRING, 8, PropModeReplace,
                       reinterpret_cast<const unsigned char*>(options.title.c_str()),
                       options.title.length());
        
        Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
        XSetWMProtocols(display, window, &wmDeleteWindow, 1);
        
        uint64_t handle = static_cast<uint64_t>(window);
        managedWindows[handle] = options;
        
        if (options.visible) {
            XMapWindow(display, window);
        }
        
        XFlush(display);
        
        return Value::createNumberWithType(handle, NumericType::UINT64);
        
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.Create: " + std::string(e.what()));
    }
}

WindowInfo getWindowInfo(uint64_t handle) {
    if (!display) {
        throw std::runtime_error("X11 display not available");
    }
    
    Window window = static_cast<Window>(handle);
    WindowInfo info;
    info.handle = handle;
    
    Atom type;
    int format;
    unsigned long nitems, bytesafter;
    unsigned char* data;
    
    Atom netWmName = XInternAtom(display, "_NET_WM_NAME", False);
    if (XGetWindowProperty(display, window, netWmName, 0, 1024, False,
                           XA_STRING, &type, &format, &nitems, &bytesafter, &data) == Success) {
        if (data) {
            info.title = reinterpret_cast<char*>(data);
            XFree(data);
        }
    }
    
    if (info.title.empty()) {
        Atom wmName = XInternAtom(display, "WM_NAME", False);
        if (XGetWindowProperty(display, window, wmName, 0, 1024, False,
                               XA_STRING, &type, &format, &nitems, &bytesafter, &data) == Success) {
            if (data) {
                info.title = reinterpret_cast<char*>(data);
                XFree(data);
            }
        }
    }
    
    Window root;
    int x, y;
    unsigned int width, height, borderWidth, depth;
    if (XGetGeometry(display, window, &root, &x, &y, &width, &height, &borderWidth, &depth)) {
        Window child;
        XTranslateCoordinates(display, window, root, 0, 0, &info.x, &info.y, &child);
        info.width = width;
        info.height = height;
    }
    
    XWindowAttributes attrs;
    if (XGetWindowAttributes(display, window, &attrs)) {
        info.isVisible = (attrs.map_state == IsViewable);
    }
    
    return info;
}

std::vector<WindowInfo> getAllWindows() {
    std::vector<WindowInfo> windows;
    
    for (const auto& [handle, options] : managedWindows) {
        try {
            windows.push_back(getWindowInfo(handle));
        } catch (...) {}
    }
    
    return windows;
}

bool setWindowTitle(uint64_t handle, const std::string& title) {
    if (!display) return false;
    Window window = static_cast<Window>(handle);
    
    XStoreName(display, window, title.c_str());
    
    Atom netWmName = XInternAtom(display, "_NET_WM_NAME", False);
    XChangeProperty(display, window, netWmName, XA_STRING, 8, PropModeReplace,
                   reinterpret_cast<const unsigned char*>(title.c_str()),
                   title.length());
    XFlush(display);
    return true;
}

bool setWindowPosition(uint64_t handle, int x, int y) {
    if (!display) return false;
    Window window = static_cast<Window>(handle);
    XMoveWindow(display, window, x, y);
    XFlush(display);
    return true;
}

bool setWindowSize(uint64_t handle, int width, int height) {
    if (!display) return false;
    Window window = static_cast<Window>(handle);
    XResizeWindow(display, window, width, height);
    XFlush(display);
    return true;
}

bool showWindow(uint64_t handle) {
    if (!display) return false;
    Window window = static_cast<Window>(handle);
    XMapWindow(display, window);
    XFlush(display);
    return true;
}

bool hideWindow(uint64_t handle) {
    if (!display) return false;
    Window window = static_cast<Window>(handle);
    XUnmapWindow(display, window);
    XFlush(display);
    return true;
}

bool closeWindow(uint64_t handle) {
    if (!display) return false;
    Window window = static_cast<Window>(handle);
    
    Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
    XEvent event;
    event.type = ClientMessage;
    event.xclient.window = window;
    event.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
    event.xclient.format = 32;
    event.xclient.data.l[0] = wmDeleteWindow;
    event.xclient.data.l[1] = CurrentTime;
    
    XSendEvent(display, window, False, NoEventMask, &event);
    XFlush(display);
    return true;
}

Value RunMessageLoop(const std::vector<Value>& args) {
    if (!display) {
        throw std::runtime_error("X11 display not available");
    }
    
    XEvent event;
    while (true) {
        XNextEvent(display, &event);
        
        if (event.type == ClientMessage) {
            Atom wmDeleteWindow = XInternAtom(display, "WM_DELETE_WINDOW", False);
            if (event.xclient.data.l[0] == static_cast<long>(wmDeleteWindow)) {
                Window window = event.xclient.window;
                uint64_t handle = static_cast<uint64_t>(window);
                managedWindows.erase(handle);
                XDestroyWindow(display, window);
                
                if (managedWindows.empty()) {
                    break;
                }
            }
        }
    }
    
    return Value::createNull();
}

#elif defined(__APPLE__) && defined(__MACH__)

Value Create(const std::vector<Value>& args, Parser* parser) {
    throw std::runtime_error("Window.Create: Not fully supported on macOS yet. Use Windows or Linux.");
}

WindowInfo getWindowInfo(uint64_t handle) {
    throw std::runtime_error("Window operations not supported on macOS yet");
}

std::vector<WindowInfo> getAllWindows() {
    return {};
}

bool setWindowTitle(uint64_t handle, const std::string& title) {
    return false;
}

bool setWindowPosition(uint64_t handle, int x, int y) {
    return false;
}

bool setWindowSize(uint64_t handle, int width, int height) {
    return false;
}

bool showWindow(uint64_t handle) {
    return false;
}

bool hideWindow(uint64_t handle) {
    return false;
}

bool closeWindow(uint64_t handle) {
    return false;
}

Value RunMessageLoop(const std::vector<Value>& args) {
    throw std::runtime_error("Message loop not supported on macOS yet");
}

#else

Value Create(const std::vector<Value>& args, Parser* parser) {
    throw std::runtime_error("Window.Create: Not supported on this platform");
}

WindowInfo getWindowInfo(uint64_t handle) {
    throw std::runtime_error("Window operations not supported on this platform");
}

std::vector<WindowInfo> getAllWindows() {
    return {};
}

bool setWindowTitle(uint64_t handle, const std::string& title) {
    return false;
}

bool setWindowPosition(uint64_t handle, int x, int y) {
    return false;
}

bool setWindowSize(uint64_t handle, int width, int height) {
    return false;
}

bool showWindow(uint64_t handle) {
    return false;
}

bool hideWindow(uint64_t handle) {
    return false;
}

bool closeWindow(uint64_t handle) {
    return false;
}

Value RunMessageLoop(const std::vector<Value>& args) {
    throw std::runtime_error("Message loop not supported on this platform");
}

#endif

Value windowInfoToValue(const WindowInfo& info) {
    std::unordered_map<std::string, Value> obj;
    obj["title"] = Value::createString(info.title);
    obj["handle"] = Value::createNumberWithType(info.handle, NumericType::UINT64);
    obj["x"] = Value::createNumber(info.x);
    obj["y"] = Value::createNumber(info.y);
    obj["width"] = Value::createNumber(info.width);
    obj["height"] = Value::createNumber(info.height);
    obj["isVisible"] = Value::createBoolean(info.isVisible);
    return Value::createJsonObject(obj);
}

Value windowInfoListToValue(const std::vector<WindowInfo>& windows) {
    std::vector<Value> arr;
    for (const auto& win : windows) {
        arr.push_back(windowInfoToValue(win));
    }
    return Value::createJsonArray(arr);
}

Value List(const std::vector<Value>& args) {
    try {
        return windowInfoListToValue(getAllWindows());
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.List: " + std::string(e.what()));
    }
}

Value Close(const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Window.Close: Expected window handle");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        bool success = closeWindow(handle);
        return Value::createBoolean(success);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.Close: " + std::string(e.what()));
    }
}

Value SetTitle(const std::vector<Value>& args) {
    if (args.size() < 2) {
        throw std::runtime_error("Window.SetTitle: Expected window handle and title");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        std::string title = args[1].toString();
        bool success = setWindowTitle(handle, title);
        return Value::createBoolean(success);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.SetTitle: " + std::string(e.what()));
    }
}

Value GetTitle(const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Window.GetTitle: Expected window handle");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        WindowInfo info = getWindowInfo(handle);
        return Value::createString(info.title);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.GetTitle: " + std::string(e.what()));
    }
}

Value SetPosition(const std::vector<Value>& args) {
    if (args.size() < 3) {
        throw std::runtime_error("Window.SetPosition: Expected window handle, x, y");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        int x = static_cast<int>(args[1].toNumber());
        int y = static_cast<int>(args[2].toNumber());
        bool success = setWindowPosition(handle, x, y);
        return Value::createBoolean(success);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.SetPosition: " + std::string(e.what()));
    }
}

Value GetPosition(const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Window.GetPosition: Expected window handle");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        WindowInfo info = getWindowInfo(handle);
        std::unordered_map<std::string, Value> result;
        result["x"] = Value::createNumber(info.x);
        result["y"] = Value::createNumber(info.y);
        return Value::createJsonObject(result);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.GetPosition: " + std::string(e.what()));
    }
}

Value SetSize(const std::vector<Value>& args) {
    if (args.size() < 3) {
        throw std::runtime_error("Window.SetSize: Expected window handle, width, height");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        int width = static_cast<int>(args[1].toNumber());
        int height = static_cast<int>(args[2].toNumber());
        bool success = setWindowSize(handle, width, height);
        return Value::createBoolean(success);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.SetSize: " + std::string(e.what()));
    }
}

Value GetSize(const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Window.GetSize: Expected window handle");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        WindowInfo info = getWindowInfo(handle);
        std::unordered_map<std::string, Value> result;
        result["width"] = Value::createNumber(info.width);
        result["height"] = Value::createNumber(info.height);
        return Value::createJsonObject(result);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.GetSize: " + std::string(e.what()));
    }
}

Value Show(const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Window.Show: Expected window handle");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        bool success = showWindow(handle);
        return Value::createBoolean(success);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.Show: " + std::string(e.what()));
    }
}

Value Hide(const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Window.Hide: Expected window handle");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        bool success = hideWindow(handle);
        return Value::createBoolean(success);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.Hide: " + std::string(e.what()));
    }
}

Value IsVisible(const std::vector<Value>& args) {
    if (args.empty()) {
        throw std::runtime_error("Window.IsVisible: Expected window handle");
    }
    
    try {
        uint64_t handle = static_cast<uint64_t>(args[0].toNumber());
        WindowInfo info = getWindowInfo(handle);
        return Value::createBoolean(info.isVisible);
    } catch (const std::exception& e) {
        throw std::runtime_error("Window.IsVisible: " + std::string(e.what()));
    }
}

}
