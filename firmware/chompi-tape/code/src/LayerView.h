#pragma GCC push_options
#pragma GCC optimize ("Os") // cold code, built for size so all of it fits in SRAM
#pragma once
#include "DSPEngine.h"
#include "temp_led_stuff.h"

namespace chompi
{
    /** PORTA loop page, track picker: white keys 1-4 are the tracks, the rest are dark.
     *  Flashing white: being bounced into. Blinking orange: held as a bounce source. Green: selected.
     *  Pink: has content. Dim white: empty. Dim red: being cleared. Muted: blinks dimly in its colour.
     *  While one is auditioned it pulses and the others are nearly dark. */
    inline void DrawLayerKeys(daisy::Engine* fx, uint32_t now, uint8_t sources = 0)
    {
        for(uint8_t key = 1; key < 16; key++)
        {
            const size_t layer = key - 1;
            float r = 0.f, g = 0.f, b = 0.f;
            if(layer < daisy::kNumLayers)
            {
                if(fx->IsLooperBouncing() && layer == fx->LooperBounceDest())
                    r = g = b = (now / 100) % 2 ? 1.f : 0.f;
                else if(sources & (1 << layer))
                {
                    const float on = (now / 150) % 2 ? 1.f : 0.f;
                    r = on;
                    g = .6f * on;
                    b = .24f * on;
                }
                else if(!fx->IsLooperLayerReady(layer))
                    r = .3f;
                else if(layer == fx->GetLooperLayer())
                    g = 1.f;
                else if(fx->IsLooperLayerFilled(layer))
                {
                    r = .6f;
                    g = .6f * .36f;
                    b = .6f * .62f;
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
                else if(sources & (1 << layer))
                {
                    // a bounce source shows as one
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
            SetSmtLedFloat(25 - key, r, g, b);
        }
    }
} // namespace chompi

#pragma GCC pop_options
