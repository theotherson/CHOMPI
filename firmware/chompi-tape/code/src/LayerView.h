#pragma GCC push_options
#pragma GCC optimize ("Os") // cold code, built for size so all of it fits in SRAM
#pragma once
#include "DSPEngine.h"
#include "temp_led_stuff.h"

namespace chompi
{
    /** PORTA loop page, layer view: white keys 1-4 are the layers, the rest are dark.
     *  Flashing white: being bounced into. Green: recording into it. White: recorded. Dim: empty.
     *  Dim red: being cleared. A muted layer blinks slowly in its colour. */
    inline void DrawLayerKeys(daisy::Engine* fx, uint32_t now)
    {
        for(uint8_t key = 1; key < 16; key++)
        {
            const size_t layer = key - 1;
            float r = 0.f, g = 0.f, b = 0.f;
            if(layer < daisy::kNumLayers)
            {
                if(fx->IsLooperBouncing() && layer == fx->LooperBounceDest())
                    r = g = b = (now / 100) % 2 ? 1.f : 0.f;
                else if(!fx->IsLooperLayerReady(layer))
                    r = .3f;
                else if(layer == fx->GetLooperLayer())
                    g = 1.f;
                else if(fx->IsLooperLayerFilled(layer))
                    r = g = b = 1.f;
                else
                    r = g = b = .15f;

                if(fx->IsLooperLayerMuted(layer) && (now / 400) % 2)
                {
                    r *= .1f;
                    g *= .1f;
                    b *= .1f;
                }
            }
            SetSmtLedFloat(25 - key, r, g, b);
        }
    }
} // namespace chompi

#pragma GCC pop_options
