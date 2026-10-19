/* On-demand final-frame capture. No OS chrome, simulation state or extra passes. */
#include "screenshots.h"
#include "platform.h"
#include <SDL3/SDL.h>
#include <string>

namespace {
bool pending = false;
unsigned int sequence = 0;
}

void Screenshot_Request(void) { pending = true; }
bool Screenshot_Pending(void) { return pending; }

void Screenshot_Failed(const char *reason)
{
    pending = false;
    SDL_Log("[SCREENSHOT] Capture failed: %s", reason ? reason : "unknown error");
}

void Screenshot_Save(SDL_Surface *surface)
{
    pending = false;
    if (!surface) { Screenshot_Failed(SDL_GetError()); return; }
    const std::string directory = Platform_UserFilePath("screenshots");
    if (directory.empty() || !SDL_CreateDirectory(directory.c_str())) {
        Screenshot_Failed(SDL_GetError()); return;
    }
    SDL_Time now;
    SDL_DateTime date{};
    if (!SDL_GetCurrentTime(&now) || !SDL_TimeToDateTime(now, &date, true)) {
        Screenshot_Failed(SDL_GetError()); return;
    }
    // Exclusive creation also protects screenshots from a second local game instance.
    for (int attempt = 0; attempt < 100; ++attempt) {
        char filename[128];
        SDL_snprintf(filename, sizeof(filename), "/TOU-%04d%02d%02d-%02d%02d%02d-%09d-%u.png",
            date.year, date.month, date.day, date.hour, date.minute, date.second,
            date.nanosecond, sequence++);
        const std::string path = directory + filename;
        SDL_IOStream *file = SDL_IOFromFile(path.c_str(), "wbx");
        if (!file) {
            const std::string error = SDL_GetError();
            SDL_PathInfo info{};
            if (SDL_GetPathInfo(path.c_str(), &info) && info.type == SDL_PATHTYPE_FILE) continue;
            Screenshot_Failed(error.c_str()); return;
        }
        if (!SDL_SavePNG_IO(surface, file, true)) {
            const std::string error = SDL_GetError();
            SDL_RemovePath(path.c_str()); // Only our exclusively created, incomplete file.
            Screenshot_Failed(error.c_str()); return;
        }
        SDL_Log("[SCREENSHOT] Saved %s", path.c_str());
        return;
    }
    Screenshot_Failed("Unable to reserve a unique filename");
}
