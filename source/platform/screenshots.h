#ifndef TOU_SCREENSHOTS_H
#define TOU_SCREENSHOTS_H

struct SDL_Surface;
void Screenshot_Request(void);
bool Screenshot_Pending(void);
void Screenshot_Save(SDL_Surface *surface);
void Screenshot_Failed(const char *reason);

#endif
