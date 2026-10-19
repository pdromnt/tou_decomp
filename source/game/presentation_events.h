#ifndef TOU_PRESENTATION_EVENTS_H
#define TOU_PRESENTATION_EVENTS_H
#include <stdint.h>
#include <vector>

enum PresentationEventKind {
    PRESENT_SOUND, PRESENT_STOP, PRESENT_VOLUME, PRESENT_PAN,
    PRESENT_EXPLOSION, PRESENT_BOOST, PRESENT_LASER, PRESENT_SPARK,
    PRESENT_ORGANIC
};
struct PresentationCheckpoint {
    int audio[64][3] = {}; // timer, virtual channel, sound identity; local only.
    unsigned int next_voice = 0;
    std::vector<uint8_t> particle_tags;
    bool operator==(const PresentationCheckpoint &other) const;
};
// Experimental, game-thread-only timeline. Ordinary play never enables it.
void PresentationEvents_Start(bool deliver);
void PresentationEvents_Stop(void);
bool PresentationEvents_Enabled(void);
bool PresentationEvents_Recording(void);
bool PresentationEvents_BeginTick(uint32_t tick);
void PresentationEvents_EndTick(void);
void PresentationEvents_Record(PresentationEventKind kind, int a = 0, int b = 0,
                               int c = 0, int d = 0);
int PresentationEvents_Play(unsigned int sample, int volume, int pan);
uint64_t PresentationEvents_Digest(uint32_t tick);
bool PresentationEvents_CommitThrough(uint32_t tick);
bool PresentationEvents_Healthy(void);
bool PresentationEvents_CanFinalize(uint32_t tick);
uint32_t PresentationEvents_ConfirmedTick(void);
bool PresentationEvents_DiscardAfter(uint32_t tick);
uint64_t PresentationEvents_CommittedCount(void);
uint64_t PresentationEvents_CommittedKindCount(PresentationEventKind kind);
void PresentationEvents_Capture(PresentationCheckpoint *checkpoint);
void PresentationEvents_Restore(const PresentationCheckpoint &checkpoint);
#endif
