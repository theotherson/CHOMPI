#pragma GCC push_options
#pragma GCC optimize ("Os") // cold code, built for size so all of it fits in SRAM
#pragma once
#include "DSPEngine.h"
#include "temp_led_stuff.h"

namespace chompi
{
    /** each track's colour: pink, blue, orange, purple */
    static constexpr float kTrackColours[daisy::kNumLayers][3]
        = {{1.f, .36f, .62f}, {0.f, .3f, 1.f}, {1.f, .45f, 0.f}, {.58f, .05f, 1.f}};

    /** PORTA 4TRACK page, track picker: white keys 1-4 are the tracks, 5-15 the playhead.
     *  A track with content shows its colour (pink, blue, orange, purple); empty: dim white; being cleared:
     *  dim red; selected: green. Muted: blinks dimly in its colour. While one is auditioned it pulses and
     *  the others are nearly dark. Printing ("listening"): flashing red. Bounce picks (copy held): blinking
     *  white; the destination of a bounce under way flashes white. Picked for deleting: magenta.
     *  Keys 5-15: the tracks' playhead, left to right through their length, in the selected track's colour. */
    inline void DrawLayerKeys(daisy::Engine* fx, uint32_t now, uint8_t sources = 0, uint8_t erase = 0)
    {
        const size_t selected = fx->GetLooperLayer();
        for(uint8_t key = 1; key < 16; key++)
        {
            const size_t layer = key - 1;
            float r = 0.f, g = 0.f, b = 0.f;
            if(layer < daisy::kNumLayers)
            {
                const float* col = kTrackColours[layer];
                if(erase & (1 << layer)) // picked for deleting: magenta
                {
                    r = b = 1.f;
                }
                else if(fx->IsLooperBouncing() && layer == fx->LooperBounceDest())
                    r = g = b = (now / 100) % 2 ? 1.f : 0.f;
                else if(sources & (1 << layer)) // a bounce pick
                    r = g = b = (now / 150) % 2 ? 1.f : 0.f;
                else if(fx->IsPrinting() && layer == selected)
                {
                    if((now / 150) % 2)
                        r = 1.f;
                }
                else if(!fx->IsLooperLayerReady(layer))
                    r = .3f;
                else if(layer == selected)
                    g = 1.f;
                else if(fx->IsLooperLayerFilled(layer))
                {
                    r = .6f * col[0];
                    g = .6f * col[1];
                    b = .6f * col[2];
                }
                else
                    r = g = b = .15f;

                const int heard = fx->GetLooperAudition();
                if(heard >= 0 && int(layer) == heard) // auditioned: pulses
                {
                    const float pulse = .4f + .6f * ((now / 300) % 2);
                    r *= pulse;
                    g *= pulse;
                    b *= pulse;
                }
                else if((sources | erase) & (1 << layer))
                {
                    // a bounce pick, or picked for deleting, shows as one
                }
                else if(heard >= 0) // silenced by another's audition
                {
                    r *= .08f;
                    g *= .08f;
                    b *= .08f;
                }
                else if(fx->IsLooperLayerMuted(layer)) // muted: blinks dimly, so it's still there
                {
                    const float dim = (now / 400) % 2 ? .3f : 0.f;
                    r *= dim;
                    g *= dim;
                    b *= dim;
                }
            }
            else if(!fx->TracksEmpty() && !(fx->IsPrinting() && fx->IsFirstPrint()))
            {
                // the playhead: the keys up to it dim, it bright
                static constexpr size_t kHeadKeys = 15 - daisy::kNumLayers;
                const float pos = daisysp::fclamp(fx->TracksPosition(), 0.f, .9999f);
                const size_t head = static_cast<size_t>(pos * kHeadKeys);
                const size_t at = layer - daisy::kNumLayers;
                const float lvl = at == head ? 1.f : at < head ? .12f : 0.f;
                const float* col = kTrackColours[selected < daisy::kNumLayers ? selected : 0];
                r = col[0] * lvl;
                g = col[1] * lvl;
                b = col[2] * lvl;
            }
            SetSmtLedFloat(25 - key, r, g, b);
        }
    }
} // namespace chompi

#pragma GCC pop_options
