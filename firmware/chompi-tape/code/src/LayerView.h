#pragma GCC push_options
#pragma GCC optimize ("Os") // cold code, built for size so all of it fits in SRAM
#pragma once
#include "DSPEngine.h"
#include "temp_led_stuff.h"

namespace chompi
{
    /** PORTA loop page, layer picker: white keys 1-4 are the layers, the rest are dark.
     *  Flashing white: being bounced into. Blinking red: armed (held 2 s). Green: the one recorded into.
     *  White: recorded. Dim: empty. Dim red: being cleared. An off (muted) layer is nearly dark; while one is
     *  auditioned it pulses and the others are nearly dark. */
    inline void DrawLayerKeys(daisy::Engine* fx, uint32_t now, int armed = -1)
    {
        for(uint8_t key = 1; key < 16; key++)
        {
            const size_t layer = key - 1;
            float r = 0.f, g = 0.f, b = 0.f;
            if(layer < daisy::kNumLayers)
            {
                if(fx->IsLooperBouncing() && layer == fx->LooperBounceDest())
                    r = g = b = (now / 100) % 2 ? 1.f : 0.f;
                else if(int(layer) == armed)
                    r = (now / 150) % 2 ? 1.f : 0.f;
                else if(!fx->IsLooperLayerReady(layer))
                    r = .3f;
                else if(layer == fx->GetLooperLayer())
                    g = 1.f;
                else if(fx->IsLooperLayerFilled(layer))
                    r = g = b = 1.f;
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
                else if((heard >= 0 || fx->IsLooperLayerMuted(layer)) && int(layer) != armed) // not heard
                {
                    r *= .08f;
                    g *= .08f;
                    b *= .08f;
                }
            }
            SetSmtLedFloat(25 - key, r, g, b);
        }
    }
} // namespace chompi

#pragma GCC pop_options
