// Copyright(c) 2017 POLYGONTEK
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
// http ://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

BE_NAMESPACE_BEGIN

class Point;

class Platform {
public:
    static constexpr int MaxPlatformEvents = 256;

    struct EventType {
        enum Enum {
            NoEvent,                        // time is still valid
            Key,                            // value is a key code, value2 is the down flag
            Char,                           // value is an unicode char (char32_t)
            Composition,                    // value is composition wide character code
            MouseDelta,                     // value and value2 are reletive signed x / y moves
            MouseMove,                      // value and value2 are absolute coordinates in the window's client area.
            JoyAxis,                        // value is an axis number and value2 is the current state (-127 to 127)
            TouchBegan,
            TouchMoved,
            TouchEnded,
            TouchCanceled,
            Console,                        // ptr is a wchar_t *
            Packet                          // ptr is a netadr_t followed by data bytes to ptrLength
        };
    };

    struct Event {
        int                 time;
        EventType::Enum     type;
        int64_t             value, value2;
        int                 ptrLength;      // bytes of data pointed to by ptr, for journaling
        void *              ptr;            // this must be manually freed if not nullptr
    };

    static void             Init();
    static void             Shutdown();
};

class PlatformAbstract {
public:
    virtual ~PlatformAbstract() = 0;
    
    virtual void            Init() = 0;
    virtual void            Shutdown() = 0;

    virtual void            EnableMouse(bool enable) = 0;

    virtual void            SetMainWindowHandle(void *windowHandle) = 0;

    virtual void            Quit() = 0;
    virtual void            Log(const char *msg) = 0;
    virtual void            Error(const char *msg) = 0;

    virtual void            GetEvent(Platform::Event *ev) = 0;
    virtual void            QueEvent(Platform::EventType::Enum type, int64_t value, int64_t value2, int ptrLength, void *ptr) = 0;

    virtual bool            IsActive() const = 0;
    virtual void            AppActivate(bool active, bool minimized) = 0;

    virtual bool            IsCursorLocked() const = 0;
    virtual bool            LockCursor(bool lock) = 0;

    virtual void            GetMousePos(Point &pos) const = 0;
    virtual void            GenerateMouseDeltaEvent() = 0;
};

extern PlatformAbstract *   platform;

BE_NAMESPACE_END
