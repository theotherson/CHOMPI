/** DSPEngine
 *  Core DSP for sampling engine, looping engine, and additional DSP
 */
#pragma once
#include "Sampler.h"
#include "RamBuffer.h"
using namespace daisy;

static const uint32_t kRecordClearTimeout = 2000;
static const uint32_t kButtonTimeout = 10;
namespace daisy
{
    /** PORTA: the loop is up to four layers. The first recording, on whichever layer is active, sets the
     *  loop's length; that layer is the master. The others then play in lockstep with it, each from its
     *  own buffer, and recording a layer overdubs into that layer only. The output is all layers summed. */
    static constexpr size_t kNumLayers = 4;

    /** @brief Logic for main looper engine */
    class LooperEngine
    {
    public:
        LooperEngine() {}
        ~LooperEngine() {}

        void Init(float sr, RamBufferMemory* layer_buffs, bool tape_slew)
        {
            for(size_t i = 0; i < kNumLayers; i++)
            {
                mem_[i] = &layer_buffs[i];
                layers_[i].Init(sr, mem_[i], tape_slew);
                filled_[i] = false;
                dirty_len_[i] = 0;
                muted_[i] = false;
                layer_gain_[i] = 1.f;
            }
            active_ = master_ = 0;

            record = false;
            first_record = true;
            record_target = false;
            record_arm = false;
            playing = false;

            Master().Reset();

            fx_env_ = fx_env_target_ = 1.f;
        }

        void FXEnvelope()
        {
            if(IsFirstRecording() && !IsRecording() && !Master().IsResetting())
                return;

            fx_env_target_ = 0.f;
        }

        void Process(float* out_l, float* out_r, size_t size)
        {
            if(IsFirstRecording() && !IsRecording() && !Master().IsResetting())
                return;

            if(fx_env_ < .01f)
                fx_env_target_ = 1.f;

            for(size_t i = 0; i < size; i++)
            {
                daisysp::fonepole(fx_env_, fx_env_target_, .001f);

                const int16_t in_l = f2s16(out_l[i] * fx_env_);
                const int16_t in_r = f2s16(out_r[i] * fx_env_);

                if(!first_record || Master().IsResetting())
                {
                    float sum_l = 0.f, sum_r = 0.f;
                    for(size_t l = 0; l < kNumLayers; l++)
                    {
                        const bool rec_here = record && l == active_;
                        int16_t aol = 0;
                        int16_t aor = 0;
                        layers_[l].PopStereoSamps(rec_here ? in_l : 0, rec_here ? in_r : 0, &aol, &aor,
                                                  rec_here, playing);
                        // a muted layer fades out over a few ms rather than clicking; auditioning one layer
                        // (CHOMPI + its key) plays only that one, mutes or not
                        const bool audible = audition_ >= 0 ? int(l) == audition_ : !muted_[l];
                        layer_gain_[l] += ((audible ? 1.f : 0.f) - layer_gain_[l]) * .002f;

                        // a layer waiting to be cleared stays silent, except while the reset fades everything out
                        if(!dirty_len_[l] || Master().IsResetting())
                        {
                            sum_l += s162f(aol) * layer_gain_[l];
                            sum_r += s162f(aor) * layer_gain_[l];
                        }
                    }

                    out_l[i] += sum_l * fx_env_;
                    out_r[i] += sum_r * fx_env_;
                }
                else if(!reset)
                {
                    Master().PushStereoSamps(in_l, in_r);
                }

                if(first_record && Master().WriteFullLength())
                {
                    ToggleRecord(); // overdub mode
                    ToggleRecord(); // play mode
                }
            }
        }

        /** SD timer: zero layers left over from the last loop, a chunk at a time. A dirty layer is silent
         *  and can't be recorded into, so the audio never meets a half-cleared one. */
        void ServiceClears()
        {
            static constexpr size_t kChunk = 16 * 1024; // samples, 32 KB per tick
            if(reset || Master().IsResetting())
                return; // the old loop is still fading out
            if(bouncing_)
            {
                BounceChunk(kChunk);
                return;
            }
            for(size_t l = 0; l < kNumLayers; l++)
            {
                const size_t len = dirty_len_[l];
                if(!len)
                    continue;
                const size_t n = len < kChunk ? len : kChunk;
                std::fill(&mem_[l]->mem[len - n], &mem_[l]->mem[len], 0);

                // the audio interrupt may have started a first recording here and called it clean meanwhile
                const uint32_t primask = __get_PRIMASK();
                __disable_irq();
                if(dirty_len_[l] == len)
                    dirty_len_[l] = len - n;
                __set_PRIMASK(primask);
                return; // one chunk per tick
            }
        }

        /** Portastudio-style bounce: every other recorded, unmuted layer is mixed into dest and left empty,
         *  freeing it to record into; muted layers stay as they are, so muting picks what's bounced. Done on
         *  the SD timer a chunk at a time, moving each sample (added into dest, zeroed in its source) so what
         *  plays never changes. Not while recording, nor into a muted layer. */
        bool BounceInto(size_t dest)
        {
            if(dest >= kNumLayers || first_record || record || record_target || bouncing_ || dirty_len_[dest]
               || muted_[dest])
                return false;
            uint8_t sources = 0;
            for(size_t l = 0; l < kNumLayers; l++)
                if(l != dest && filled_[l] && !dirty_len_[l] && !muted_[l])
                    sources |= 1 << l;
            if(!sources)
                return false;

            bounce_dest_    = dest;
            bounce_sources_ = sources;
            bounce_pos_     = 0;
            bounce_end_     = mem_[master_]->length;
            bouncing_       = true;
            return true;
        }
        inline bool IsBouncing() const { return bouncing_; }

        /** Mute, as on x0x: a muted layer keeps playing silently, in step */
        void ToggleMute(size_t l)
        {
            if(l < kNumLayers)
                muted_[l] = !muted_[l];
        }
        inline bool Muted(size_t l) const { return l < kNumLayers && muted_[l]; }

        // hear only layer l while it's auditioned, -1 for none
        inline void SetAudition(int l) { audition_ = l < int(kNumLayers) ? l : -1; }
        inline int Audition() const { return audition_; }

        /** Pick layer l and arm it: recording into it starts with the next note played, as TAPE's record
         *  arm. An overdub under way (TAPE goes straight into one after the first take) stops first, so
         *  it doesn't follow the pick into the new layer. Not during the first take, which sets the length. */
        void ArmLayer(size_t l)
        {
            if(l >= kNumLayers || bouncing_ || (first_record && record))
                return;
            if(record)
                ToggleRecord(true); // stop the overdub, keep playing
            SetActiveLayer(l);
            record_arm = true;
        }
        inline size_t BounceDest() const { return bounce_dest_; }

        inline bool IsPlaying() { return playing && !first_record; }
        inline bool IsFirstRecording() { return first_record; }

        void IncrementDubGain(float gain)
        {
            for(auto& layer : layers_)
                layer.IncrementDubGain(gain, first_record && !record);
        }
        inline float GetDubGain() { return Master().GetDubGain(); }

        bool CheckReset()
        {
            if(!Master().IsResetting() && !record_btn && !play_btn)
                reset = false;

            uint32_t elapsed = System::GetNow() - last_button_press;
            if(elapsed >= kButtonTimeout)
            {
                if(play_btn && !record_btn && !reset && !toggle_play && (record || IsPlaying())) // rising edge and recording or playing
                    TogglePlaying();
                if(record_btn && !play_btn && !reset && !toggle_record)
                {
                    // PORTA: while stopped, the loop key arms the selected layer; the next note or loop press
                    // starts the take. Armed, playing or recording, it's TAPE's: start, overdub or stop.
                    if(!record && !record_arm && !IsPlaying() && !bouncing_)
                    {
                        record_arm = true;
                        toggle_record = true;
                    }
                    else
                        ToggleRecord();
                }
            }
            if(elapsed >= kRecordClearTimeout)
            {
                if(!record_btn && play_btn && !jump_to_start && !IsPlaying())
                {
                    for(auto& layer : layers_)
                        layer.JumpToStart();
                    jump_to_start = true;
                    return false;
                }
                else if(record_btn && play_btn && !reset)
                {
                    Reset();
                    return true;
                }
            }

            return false;
        }

        void CheckRecordReady()
        {
            if(record_target)
            {
                record = true;
                record_target = false;
                layers_[active_].ReadyToRecord();

                if(IsFirstRecording())
                {
                    ClearWindow();
                    master_ = active_;
                    dirty_len_[master_] = 0; // the new recording overwrites it, no need to clear
                    Master().ResetRamBuff();
                }
            }

            if(play_target)
            {
                playing = true;
                play_target = false;
            }
        }

        bool IsRecording()
        {
            return record;
        }

        // speed, direction and scrub move every layer together, so they stay in step
        inline void SetPitch(float val)
        {
            for(auto& layer : layers_)
                layer.SetVarispeed(val);
        }
        inline void SetReverse(bool rev)
        {
            for(auto& layer : layers_)
                layer.SetReverse(rev);
        }
        inline void SetScrub(float scrub)
        {
            for(auto& layer : layers_)
                layer.SetScrub(scrub);
        }
        inline float GetScrub() { return Master().GetScrub(); }
        inline bool GetReverse() { return Master().GetReverse(); }

        void RecordButton(bool rising)
        {

            record_btn = rising;
            toggle_record = false;
            last_button_press = System::GetNow();

            if(Master().IsResetting() || reset || record_arm)
                return; // short circuit situations
            else if(record_btn && play_btn && first_record && !record)
                record_arm = true;
        }

        void PlayButton(bool rising)
        {
            // falling edge and not recording
            if(play_btn && !rising && !jump_to_start && !record && !toggle_play)
                TogglePlaying();

            toggle_play = false;

            jump_to_start = false;

            play_btn = rising;
            last_button_press = System::GetNow();

            if(Master().IsResetting() || reset || record_arm)
                return; // short circuit situations
            else if(record_btn && play_btn && first_record && !record)
                record_arm = true;
        }

        void ToggleRecord(bool play_pressed = false)
        {
            toggle_record = true;
            record_arm = false;
            if(record)
            {
                // don't go into overdub from new record if play is pressed
                if(play_pressed || !first_record)
                {
                    record_target = record = false;
                }

                if(first_record) // reload, then overdub
                {
                    Master().JumpToStart();
                    FollowMaster();
                }

                filled_[active_] = true;
                first_record = false;
                play_target = true;
                playing = false;
            }
            else
            {
                if(bouncing_)
                    return; // a bounce is moving audio between layers, a moment
                if(!first_record && dirty_len_[active_])
                    return; // the layer is still being cleared, a moment

                if(first_record)
                {
                    active_ = ActiveForFirstRecording();
                    layers_[active_].WriteJump(sizeof(WAV_FormatTypeDef));
                }
                if (!first_record)
                {
                    playing = true;
                }
                record_target = true;
            }
        }

        void TogglePlaying()
        {
            toggle_play = true;

            if(first_record && !record)
                return; // do nothing on empty looper and not recording

            if(record)
            {
                ToggleRecord(true);
            }
            else
            {
                playing = !playing;
            }
        }

        inline bool GetRecordArm() { return record_arm; }
        inline bool GetIsEmpty() { return !record && first_record; }

        // where the loop is within its window, 0 to 1
        float GetPosition() {
            const float start = static_cast<float>(Master().GetLoopStart());
            const float end   = static_cast<float>(Master().GetLoopEnd());
            const float tell  = static_cast<float>(Master().GetReadTell());
            return end > start ? (tell - start) / (end - start) : 0.f;
        }

        /** PORTA: the part of the recorded loop that plays, as fractions of its length, the same on every
         *  layer. Kept at least 50 ms long. Nothing to set until the first recording is done. */
        void SetWindow(float start, float end)
        {
            if(first_record)
                return;
            const size_t len = mem_[master_]->length;
            static constexpr size_t kMinWindow = 4800; // samples, 50 ms of stereo
            if(len < kMinWindow)
                return;
            size_t s = static_cast<size_t>(daisysp::fclamp(start, 0.f, 1.f) * len) & ~size_t(1);
            size_t e = static_cast<size_t>(daisysp::fclamp(end, 0.f, 1.f) * len) & ~size_t(1);
            if(e > len)
                e = len;
            if(e < s + kMinWindow)
            {
                if(s + kMinWindow <= len)
                    e = s + kMinWindow;
                else
                    s = e - kMinWindow;
            }
            for(size_t l = 0; l < kNumLayers; l++)
            {
                mem_[l]->win_start = s;
                mem_[l]->win_end   = e;
            }
        }

        float GetPitch()
        {
            return Master().GetVarispeed();
        }

        inline bool GetReset() { return reset; }

        void Reset()
        {
            bouncing_ = false; // the layers are being cleared anyway
            for(auto& m : muted_)
                m = false;
            ClearWindow();
            for(size_t l = 0; l < kNumLayers; l++)
            {
                layers_[l].Reset();
                // what's in the layers now is the old loop: it gets cleared in the background
                dirty_len_[l] = mem_[l]->length;
                filled_[l] = false;
            }
            reset = true;

            record = false;
            first_record = true;
            record_target = false;
            record_arm = false;
            playing = false;
        }

        /** a looper file was loaded into layer 1's memory */
        void OpenFile()
        {
            first_record = false;
            playing = false;
            record = false;
            record_arm = false;
            record_target = false;

            master_ = active_ = 0;
            ClearWindow();
            Master().SetVarispeed(1.f, true);
            Master().JumpTo(0);
            Master().ForceSetScrub(0.f);
            filled_[0] = true;
            for(size_t l = 1; l < kNumLayers; l++)
            {
                dirty_len_[l] = mem_[l]->length > mem_[0]->length ? mem_[l]->length : mem_[0]->length;
                filled_[l] = false;
            }
            FollowMaster();
        }

        /** Layers, for the UI */
        inline size_t ActiveLayer() const { return active_; }
        inline bool LayerFilled(size_t l) const { return l < kNumLayers && filled_[l]; }
        inline bool LayerReady(size_t l) const { return l < kNumLayers && !dirty_len_[l]; }

        /** Pick the layer the loop key records into. An overdub under way moves with it. Not during the
         *  first recording, which is what sets the loop's length. */
        void SetActiveLayer(size_t l)
        {
            if(l >= kNumLayers || (first_record && record))
                return;
            if(!first_record && dirty_len_[l])
                return;
            if(record)
                filled_[active_] = true; // the overdub leaves this one behind, recorded
            active_ = l;
        }

    private:
        inline FileSampler& Master() { return layers_[master_]; }

        void BounceChunk(size_t chunk)
        {
            const size_t start = bounce_pos_;
            const size_t end   = start + chunk < bounce_end_ ? start + chunk : bounce_end_;
            int16_t* const dest = mem_[bounce_dest_]->mem;
            for(size_t l = 0; l < kNumLayers; l++)
            {
                if(!(bounce_sources_ & (1 << l)))
                    continue;
                int16_t* const src = mem_[l]->mem;
                for(size_t i = start; i < end; i++)
                {
                    const int32_t sum = int32_t(dest[i]) + src[i];
                    dest[i] = sum > 32767 ? 32767 : (sum < -32768 ? -32768 : sum);
                    src[i]  = 0;
                }
            }
            bounce_pos_ = end;
            if(end < bounce_end_)
                return;

            // done: unless a reset in the audio interrupt cancelled it meanwhile
            const uint32_t primask = __get_PRIMASK();
            __disable_irq();
            if(bouncing_)
            {
                for(size_t l = 0; l < kNumLayers; l++)
                    if(bounce_sources_ & (1 << l))
                        filled_[l] = false;
                filled_[bounce_dest_] = true;
                bouncing_ = false;
            }
            __set_PRIMASK(primask);
        }

        void ClearWindow()
        {
            for(size_t l = 0; l < kNumLayers; l++)
                mem_[l]->win_start = mem_[l]->win_end = 0;
        }

        // the first recording goes to the active layer, if it's clean
        size_t ActiveForFirstRecording() const
        {
            if(!dirty_len_[active_])
                return active_;
            for(size_t l = 0; l < kNumLayers; l++)
                if(!dirty_len_[l])
                    return l;
            return active_; // all still clearing: CheckRecordReady takes this one over anyway
        }

        // the loop's length is now known: every layer takes it, and the master's position
        void FollowMaster()
        {
            for(size_t l = 0; l < kNumLayers; l++)
            {
                if(l == master_)
                    continue;
                mem_[l]->length = mem_[master_]->length;
                layers_[l].SyncFrom(Master());
            }
        }

        FileSampler layers_[kNumLayers];
        RamBufferMemory* mem_[kNumLayers];
        bool filled_[kNumLayers];
        volatile size_t dirty_len_[kNumLayers]; // samples left to clear, from the end down
        size_t active_, master_;

        bool muted_[kNumLayers];
        volatile int audition_ = -1;
        float layer_gain_[kNumLayers];

        volatile bool bouncing_ = false;
        size_t bounce_dest_ = 0, bounce_pos_ = 0, bounce_end_ = 0;
        uint8_t bounce_sources_ = 0;

        bool record, first_record, record_target;
        bool playing, play_target;
        uint32_t loop_end;
        uint32_t last_button_press;
        bool record_btn, play_btn;
        bool toggle_record, toggle_play;
        bool record_arm;
        bool reset, jump_to_start;

        float fx_env_, fx_env_target_;
    };
}
