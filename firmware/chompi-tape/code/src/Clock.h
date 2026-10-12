#pragma once

#include "daisy.h"
#include "util/scopedirqblocker.h"

namespace chompi
{
    // PORTA's one clock, from MIXTAPE's Clock.h (nils): the beat grid is MIDI clock while it runs, else
    // the internal clock, which always runs. Added for PORTA: tap tempo, MIDI clock out, a bar count.
    // MIDI comes in the receive interrupts, at the audio callback's priority, so only lower priority code
    // needs the lock. The MIDI grid starts with Start or Song Position, joining a running clock would be
    // off the beat.
    class Clock
    {
    public:
        static constexpr uint32_t kTicksPerQuarter = 24;
        static constexpr int kMinTempo = 40;
        static constexpr int kMaxTempo = 240;
        static constexpr float kBeepLevel = .2f; // the beep's peak
        static constexpr uint8_t kSigs = 5;
        static constexpr int kMaxBars = 14; // a white key each

        void Init()
        {
            beep_on_ = had_grid_ = false;
            tempo_ = 120;
            sig_ = 0;
            bars_ = 4;
            metro_beat_ = last_pulse_ = 0;
            metro_phase_ = beep_env_ = beep_ph_ = beep_inc_ = 0.f;
            SetSampleRate(48000.f);
            source_ = nullptr;
            started_ = running_ = false;
            spp_ = spp_lsb_ = 0;
            count_ = outliers_ = 0;
            heard_ = last_ = t_ = period_ = 0;
            out_ticks_ = out_sent_ = 0;
            out_last_ = 0;
            last_tap_ = taps_ = 0;
        }

        // every received byte, before parsing
        void Byte(uint8_t byte, const void *source)
        {
            const bool data = byte < 0x80;
            if(data ? !spp_ || source != source_ : !IsTransport(byte))
            {
                if(!data && byte < 0xF8 && source == source_) // another message cuts a song position short
                    spp_ = 0;
                return;
            }

            // the first source to send clock keeps it until it goes quiet
            const uint32_t now = daisy::System::GetTick();
            const bool quiet = now - heard_ >= Lost();
            if(source != source_ && source_ && !quiet)
                return;
            source_ = source;
            heard_ = now;
            if(quiet) // the position is unknown after a gap
                started_ = running_ = false;

            if(data) // song position, in 16ths
            {
                if(spp_ == 1)
                {
                    spp_lsb_ = byte;
                    spp_ = 2;
                }
                else
                {
                    count_ = ((byte << 7) | spp_lsb_) * 6;
                    started_ = true;
                    spp_ = 0;
                }
                return;
            }

            switch(byte)
            {
                case 0xF8: Tick(now); break;
                case 0xFA: // start: the next tick is the downbeat
                    count_ = 0;
                    started_ = running_ = true;
                    break;
                case 0xFB: running_ = started_; break; // continue
                case 0xFC: running_ = false; break;    // stop
                case 0xF2: // song position, unknown until both data bytes are in
                    started_ = running_ = false;
                    spp_ = 1;
                    break;
            }
        }

        void SetSampleRate(float sr)
        {
            sr_ = sr;
            beep_decay_ = expf(-1.f / (.012f * sr));
        }

        // the metronome: a beep on the beat. The grid runs either way.
        void SetMetronome(bool on) { beep_on_ = on; }
        bool Metronome() const { return beep_on_; }
        float SampleRate() const { return sr_; }

        // /8 signatures count their eighths in threes: 6/8 is two dotted-quarter beats
        float QuartersPerBeat() const { return TicksPerBeat() / static_cast<float>(kTicksPerQuarter); }

        int BeatsPerBar() const { return Sig().compound() ? Sig().top / 3 : Sig().top; }

        // time signatures, in the order the wheel steps through them (most used first)
        uint8_t SigIndex() const { return sig_; }
        void StepSig(int steps)
        {
            int s = sig_ + steps;
            sig_ = s < 0 ? 0 : s >= kSigs ? kSigs - 1 : s;
        }

        // the length view's bar count, 1 - kMaxBars (not used by recording yet)
        int Bars() const { return bars_; }
        void SetBars(int bars) { bars_ = bars < 1 ? 1 : bars > kMaxBars ? kMaxBars : bars; }

        int Tempo() const { return tempo_; }
        void ChangeTempo(int bpm)
        {
            bpm += tempo_;
            tempo_ = bpm < kMinTempo ? kMinTempo : bpm > kMaxTempo ? kMaxTempo : bpm;
        }

        // tap tempo (from TEMPO): one tap a beat, the average of the last few, and the beat lands on the tap.
        // A gap longer than the slowest tempo starts over.
        void Tap(uint32_t now_ms)
        {
            const uint32_t d = now_ms - last_tap_;
            last_tap_ = now_ms;
            if(d > 60000 / kMinTempo)
                taps_ = 0;
            else
            {
                tap_iv_[taps_ % 3] = d; // the last three intervals
                taps_++;
                const uint32_t n = taps_ < 3 ? taps_ : 3;
                uint32_t sum = 0;
                for(uint32_t i = 0; i < n; i++)
                    sum += tap_iv_[i];
                const int bpm = static_cast<int>(60000.f * n / sum + .5f);
                tempo_ = bpm < kMinTempo ? kMinTempo : bpm > kMaxTempo ? kMaxTempo : bpm;
            }
            daisy::ScopedIrqBlocker block;
            if(metro_phase_ > .5f) // a tap just before the beat is that beat
                metro_beat_++;
            metro_phase_ = 0.f;
        }

        // false without a grid
        bool Quarter(float &samples, bool midi_only = false) const
        {
            uint32_t beat;
            float phase;
            bool midi;
            if(!Get(beat, phase, &samples, &midi) || (midi_only && !midi))
                return false;
            samples /= QuartersPerBeat();
            return true;
        }

        // Beats since the start, the phase within the current one and samples per beat.
        // midi tells the two grids apart: they count from their own starts, so their beats are not the same beats.
        bool Get(uint32_t &beat, float &phase, float *length = nullptr, bool *midi = nullptr) const
        {
            uint32_t period;
            if(MidiGrid(beat, phase, period))
            {
                if(length)
                    *length = float(period) * TicksPerBeat() * sr_ / daisy::System::GetTickFreq();
                if(midi)
                    *midi = true;
                return true;
            }
            beat = metro_beat_;
            phase = metro_phase_;
            if(length)
                *length = BeatSamples();
            if(midi)
                *midi = false;
            return true;
        }
        bool Following() const
        {
            uint32_t beat, period;
            float phase;
            return MidiGrid(beat, phase, period);
        }

        // MIDI clock out, sent from the main loop (a UART send blocks): one 0xF8 per call while it's true.
        // Only the internal clock goes out, a followed one isn't echoed.
        bool TakeOutTick()
        {
            if(out_sent_ == out_ticks_)
                return false;
            out_sent_++;
            return true;
        }

        // once per audio block: runs the internal clock and adds the metronome's beep to all outputs
        void Process(float **out, size_t size, float hp_gain, float line_gain)
        {
            metro_phase_ += size / BeatSamples();
            while(metro_phase_ >= 1.f)
            {
                metro_phase_ -= 1.f;
                metro_beat_++;
            }

            const bool midi = Following();
            const uint32_t per_beat = TicksPerBeat();
            const uint32_t ticks = metro_beat_ * per_beat + static_cast<uint32_t>(metro_phase_ * per_beat);
            // a tap can move the grid: forwards it catches up (a beat at most), backwards it waits
            const int32_t due = static_cast<int32_t>(ticks - out_last_);
            if(due > 0)
            {
                if(!midi)
                    out_ticks_ += due > int32_t(per_beat) ? per_beat : due;
                out_last_ = ticks;
            }

            // a beep on every beat, a higher one on the one; 6/8 also clicks the eighths between, quieter
            uint32_t beat;
            float phase;
            const bool grid = beep_on_ && Get(beat, phase);
            const uint32_t subs = Sig().compound() ? 3 : 1;
            const uint32_t sub = static_cast<uint32_t>(phase * subs);
            const uint32_t pulse = beat * subs + sub;
            if(grid && (!had_grid_ || pulse != last_pulse_) && phase * subs - sub < .25f)
            {
                beep_env_ = sub ? kBeepLevel * .5f : kBeepLevel;
                beep_ph_ = 0.f;
                beep_inc_ = (sub ? 800.f : beat % BeatsPerBar() ? 1000.f : 1500.f) / sr_;
            }
            had_grid_ = grid;
            if(grid)
                last_pulse_ = pulse;

            for(size_t i = 0; i < size && beep_env_ > .001f; i++)
            {
                const float b = sinf(beep_ph_ * 6.2831853f) * beep_env_;
                out[0][i] += b * hp_gain;
                out[1][i] += b * hp_gain;
                out[2][i] += b * line_gain;
                out[3][i] += b * line_gain;
                beep_ph_ += beep_inc_;
                if(beep_ph_ >= 1.f)
                    beep_ph_ -= 1.f;
                beep_env_ *= beep_decay_;
            }
        }

    private:
        bool MidiGrid(uint32_t &beat, float &phase, uint32_t &period) const
        {
            daisy::ScopedIrqBlocker block;
            const int32_t since = daisy::System::GetTick() - t_;
            period = period_;
            if(!running_ || !period_ || !count_ || since > int32_t(2 * period_))
                return false;

            // runs on past a late tick instead of waiting for it
            const uint32_t tick = count_ - 1;
            const uint32_t per_beat = TicksPerBeat();
            float into = tick % per_beat + (since > 0 ? float(since) / period_ : 0.f);
            beat = tick / per_beat;
            if(into >= per_beat)
            {
                into -= per_beat;
                beat++;
            }
            phase = into / per_beat;
            return true;
        }

        struct Signature
        {
            uint8_t top, bottom;
            bool compound() const { return bottom == 8; }
        };
        const Signature& Sig() const
        {
            static const Signature sigs[kSigs] = {{4, 4}, {3, 4}, {6, 8}, {5, 4}, {7, 4}};
            return sigs[sig_];
        }
        uint32_t TicksPerBeat() const { return Sig().compound() ? kTicksPerQuarter * 3 / 2 : kTicksPerQuarter; }

        // the tempo counts quarters, a 6/8 beat lasts one and a half
        float BeatSamples() const { return 60.f * sr_ / tempo_ * TicksPerBeat() / kTicksPerQuarter; }

        static bool IsTransport(uint8_t byte)
        {
            return byte == 0xF8 || byte == 0xFA || byte == 0xFB || byte == 0xFC || byte == 0xF2;
        }

        static uint32_t Lost() { return daisy::System::GetTickFreq() / 4; } // slower than 10 BPM

        // times are TIM2 ticks, which wrap at 2^32 (GetUs doesn't); a small PLL keeps USB jitter off the grid
        void Tick(uint32_t now)
        {
            const uint32_t d = now - last_;
            last_ = now;
            if(running_)
                count_++;

            if(d >= Lost())
            {
                period_ = 0;
                t_ = now;
                return;
            }
            if(!period_)
            {
                period_ = d;
                t_ = now;
                outliers_ = 0;
                return;
            }

            const int32_t err = now - (t_ + period_);
            t_ += period_;
            if(err > int32_t(period_ / 2) || -err > int32_t(period_ / 2))
            {
                // bunched USB packets or a lost byte keep the grid, a real tempo jump resyncs
                if(++outliers_ >= 3)
                {
                    period_ = d;
                    t_ = now;
                    outliers_ = 0;
                }
                return;
            }

            outliers_ = 0;
            t_ += err / 4;
            period_ += err / 16;
        }

        const void *source_; // the handler the clock comes from
        bool started_;       // the count is the song position
        bool running_;       // between start / continue and stop
        uint8_t spp_;        // song position byte to come: 1 = LSB, 2 = MSB, 0 = none
        uint8_t spp_lsb_;
        uint32_t count_;     // ticks since start
        uint32_t outliers_;  // ticks in a row far off the grid
        uint32_t heard_;     // last byte from source_
        uint32_t last_;      // last tick, raw
        uint32_t t_;         // last tick, smoothed
        uint32_t period_;    // smoothed tick length, 0 while unknown

        bool beep_on_;       // the metronome
        bool had_grid_;      // a grid that just appeared beeps at once
        int tempo_;          // BPM in quarters
        uint8_t sig_;        // time signature, see Sig()
        uint8_t bars_;       // the length view's bar count
        uint32_t metro_beat_;
        uint32_t last_pulse_;
        float metro_phase_;
        float sr_;
        float beep_env_, beep_ph_, beep_inc_, beep_decay_;
        volatile uint32_t out_ticks_; // clock out: ticks due (audio) and sent (main loop)
        uint32_t out_sent_;
        uint32_t out_last_;
        uint32_t last_tap_, taps_, tap_iv_[3];
    };

    extern Clock beat_clock;
}
