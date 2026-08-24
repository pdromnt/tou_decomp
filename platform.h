#ifndef TOU_PLATFORM_H
#define TOU_PLATFORM_H

typedef enum PlatformEventType {
    PLATFORM_EVENT_NONE = 0,
    PLATFORM_EVENT_QUIT,
    PLATFORM_EVENT_FOCUS_GAINED,
    PLATFORM_EVENT_FOCUS_LOST,
    PLATFORM_EVENT_MOUSE_MOTION
} PlatformEventType;

typedef struct PlatformEvent {
    PlatformEventType type;
    int x;
    int y;
} PlatformEvent;

int   Platform_CreateWindow(const char *title, int width, int height);
void  Platform_ShowWindow(void);
void  Platform_DestroyWindow(void);
void *Platform_GetNativeWindowHandle(void);
void *Platform_GetSdlWindow(void);
int   Platform_PollEvent(PlatformEvent *event);
int   Platform_GetMousePosition(int *x, int *y);
int   Platform_ApplyDisplaySettings(int width, int height, int fullscreen);

#endif
