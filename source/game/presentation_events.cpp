#include "tou.h"
#include "presentation_events.h"
#include "shader_effects.h"
#include <string.h>

namespace {
const size_t kHistory = 16, kEventsPerTick = 4096;
struct Event { PresentationEventKind kind; int a, b, c, d; };
struct Tick { uint32_t number = 0; std::vector<Event> events; };
Tick history[kHistory];
bool enabled = false, recording = false, deliver = false, healthy = true;
uint32_t current = 0, confirmed = 0;
unsigned int next_voice = 0;
int voices[64];
unsigned int generations[64];
uint64_t committed_count = 0;
uint64_t kind_counts[PRESENT_ORGANIC + 1] = {};

bool OwnsVoice(int voice)
{
    return voice >= 0 && voice < 64 && voices[voice] >= 0 &&
        Audio_ChannelGeneration(voices[voice]) == generations[voice];
}

void Dispatch(const Event &event)
{
    if (!deliver) return;
    const int voice = event.d;
    switch (event.kind) {
    case PRESENT_SOUND: {
        if (OwnsVoice(voice)) Audio_StopChannel(voices[voice]);
        const int channel = Audio_PlaySample(static_cast<unsigned int>(event.a), event.b, event.c);
        // The physical mixer may steal a track. Invalidate its old virtual owner.
        for (int i = 0; i < 64; ++i)
            if (voices[i] == channel) voices[i] = -1;
        voices[voice] = channel;
        generations[voice] = Audio_ChannelGeneration(channel);
        break;
    }
    case PRESENT_STOP:
        if (OwnsVoice(event.a)) {
            Audio_StopChannel(voices[event.a]); voices[event.a] = -1;
        }
        break;
    case PRESENT_VOLUME:
        if (OwnsVoice(event.a))
            Audio_SetChannelVolume(voices[event.a], event.b);
        break;
    case PRESENT_PAN:
        if (OwnsVoice(event.a))
            Audio_SetChannelPan(voices[event.a], event.b);
        break;
    case PRESENT_EXPLOSION: ShaderEffects_ShipExplosion(event.a, event.b); break;
    case PRESENT_BOOST: ShaderEffects_TurboBoost(event.a); break;
    case PRESENT_LASER: ShaderEffects_Laser(event.a, event.b, event.c, event.d); break;
    case PRESENT_SPARK: ShaderEffects_DiggerContact(event.a, event.b, event.c, event.d); break;
    case PRESENT_ORGANIC:
        ShaderEffects_CommitOrganicPixel(event.a, event.b, static_cast<uint32_t>(event.c), event.d != 0);
        break;
    }
}
}

bool PresentationCheckpoint::operator==(const PresentationCheckpoint &other) const
{
    return memcmp(audio, other.audio, sizeof(audio)) == 0 &&
        next_voice == other.next_voice && particle_tags == other.particle_tags;
}

bool PresentationEvents_CanFinalize(uint32_t tick)
{
    return !enabled || (healthy && tick <= confirmed);
}

void PresentationEvents_Start(bool should_deliver)
{
    PresentationEvents_Stop();
    enabled = true; deliver = should_deliver; healthy = true;
    current = confirmed = 0; next_voice = 0; committed_count = 0;
    memset(kind_counts, 0, sizeof(kind_counts));
    for (int i = 0; i < 64; ++i) voices[i] = -1;
}
void PresentationEvents_Stop(void)
{
    const bool owned = enabled;
    recording = enabled = false;
    if (owned && deliver)
        for (int i = 0; i < 64; ++i)
            if (OwnsVoice(i)) Audio_StopChannel(voices[i]);
    if (owned && DAT_00487810 != NULL)
        for (int p = 0; p < DAT_00489240; ++p) {
            Player_Get(p)->sound_timer = 0;
            Player_Get(p)->sound_channel = -1;
            Player_Get(p)->sound_id = -1;
        }
    for (size_t i = 0; i < kHistory; ++i) {
        history[i].number = 0; history[i].events.clear();
    }
}
bool PresentationEvents_Enabled(void) { return enabled; }
bool PresentationEvents_Recording(void) { return recording; }
bool PresentationEvents_Healthy(void) { return healthy; }
uint64_t PresentationEvents_CommittedCount(void) { return committed_count; }
uint64_t PresentationEvents_CommittedKindCount(PresentationEventKind kind)
{
    return kind >= PRESENT_SOUND && kind <= PRESENT_ORGANIC ? kind_counts[kind] : 0;
}

bool PresentationEvents_BeginTick(uint32_t tick)
{
    if (!enabled || recording || tick <= confirmed || tick - confirmed > kHistory) {
        healthy = false; return false;
    }
    Tick &slot = history[tick % kHistory];
    if (slot.number > confirmed && slot.number != tick) { healthy = false; return false; }
    slot.number = current = tick;
    slot.events.clear(); // Re-simulation replaces this tick's events, never appends.
    recording = true;
    return true;
}
void PresentationEvents_EndTick(void) { recording = false; }
void PresentationEvents_Record(PresentationEventKind kind, int a, int b, int c, int d)
{
    if (!recording) return;
    std::vector<Event> &events = history[current % kHistory].events;
    if (events.size() >= kEventsPerTick) { healthy = false; return; }
    events.push_back(Event{kind, a, b, c, d});
}
int PresentationEvents_Play(unsigned int sample, int volume, int pan)
{
    const int voice = static_cast<int>(next_voice);
    next_voice = (next_voice + 1) % 64;
    PresentationEvents_Record(PRESENT_SOUND, static_cast<int>(sample), volume, pan, voice);
    return voice;
}
uint64_t PresentationEvents_Digest(uint32_t tick)
{
    const Tick &slot = history[tick % kHistory];
    if (!enabled || tick == 0 || slot.number != tick) return 0;
    uint64_t hash = 14695981039346656037ull;
    for (const Event &event : slot.events) {
        const uint32_t values[] = {static_cast<uint32_t>(event.kind),
            static_cast<uint32_t>(event.a), static_cast<uint32_t>(event.b),
            static_cast<uint32_t>(event.c), static_cast<uint32_t>(event.d)};
        for (uint32_t value : values)
            for (int shift = 0; shift < 32; shift += 8) {
                hash ^= (value >> shift) & 255u; hash *= 1099511628211ull;
            }
    }
    return hash;
}
bool PresentationEvents_CommitThrough(uint32_t tick)
{
    if (!enabled || recording || !healthy)
        return false;
    if (tick <= confirmed) return true; // Duplicate or stale confirmation is idempotent.
    if (tick - confirmed > kHistory) return false;
    // Preflight the entire range before emitting anything irreversible.
    for (uint32_t n = confirmed; n != tick;) {
        ++n;
        if (history[n % kHistory].number != n) return false;
    }
    while (confirmed != tick) {
        const Tick &slot = history[(confirmed + 1) % kHistory];
        for (const Event &event : slot.events) {
            Dispatch(event); ++committed_count; ++kind_counts[event.kind];
        }
        ++confirmed;
    }
    return true;
}
void PresentationEvents_Capture(PresentationCheckpoint *checkpoint)
{
    static_assert(GAMEPLAY_PLAYER_CAPACITY == 64, "Update local audio checkpoint capacity");
    for (int p = 0; p < DAT_00489240; ++p) {
        const PlayerData &player = *Player_Get(p);
        checkpoint->audio[p][0] = player.sound_timer;
        checkpoint->audio[p][1] = player.sound_channel;
        checkpoint->audio[p][2] = player.sound_id;
    }
    checkpoint->next_voice = next_voice;
    ShaderEffects_SaveParticleTags(&checkpoint->particle_tags);
}
void PresentationEvents_Restore(const PresentationCheckpoint &checkpoint)
{
    if (checkpoint.next_voice >= 64 ||
        checkpoint.particle_tags.size() != PARTICLE_CAPACITY + ENTITY_ACTIVE_CAPACITY) {
        healthy = false; return;
    }
    for (int p = 0; p < DAT_00489240; ++p) {
        PlayerData &player = *Player_Get(p);
        player.sound_timer = checkpoint.audio[p][0];
        player.sound_channel = checkpoint.audio[p][1];
        player.sound_id = checkpoint.audio[p][2];
    }
    next_voice = checkpoint.next_voice;
    ShaderEffects_RestoreParticleTags(checkpoint.particle_tags);
}
