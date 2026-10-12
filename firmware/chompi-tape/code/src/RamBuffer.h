#pragma once
#include "daisy_seed.h"
#include "daisysp.h"
#include <algorithm>

namespace daisy
{

    // PORTA: 55 s of 48 kHz stereo (plus the WAV header), rounded up to a whole 8K for SDRAM page alignment.
    // The sample buffer and the four looper layers are each this size.
    static const size_t kMaxRamBuffSize = 10567680 / 2;

    // PORTA: the sample buffer holds 10 s (as SK-1 / SP-303-ish limits go, and it frees ~8.6 MB), same rounding
    static const size_t kMaxSampleSize = 1925120 / 2;

    // a recording waiting for its threshold keeps ~6ms before it, and ignores ~100ms of key noise (from MIXTAPE)
    static const size_t kRecPreRoll = 576;
    static const uint8_t kRecIgnoreWraps = 17;

    // just a buffer with its length
    struct RamBufferMemory
    {
        // m: an array of capacity samples (kMaxRamBuffSize unless given), plus room for the threshold's
        // pre-roll behind it where a recording waits for one
        void Init(int16_t* m, size_t cap = kMaxRamBuffSize)
        {
            mem      = m;
            capacity = cap;
        }

        int16_t* mem;
        size_t length;
        size_t capacity = kMaxRamBuffSize;

        // PORTA: the part of the recording that loops; win_end 0 is all of it
        size_t win_start = 0, win_end = 0;

        void Clear()
        {
            std::fill(&mem[0], &mem[capacity], 0);
        }
    };

    // keeps track of its own read and write heads
    class RamBuffer
    {
    public:
        void Init(RamBufferMemory* b)
        {
            buff = b;
            buff->Clear();
        }

        void Reset()
        {
            buff->length = read_head = write_head = 0;
        }

        void BlockRead(int16_t* copy_buff, size_t size)
        {
            if(read_head + size > buff->capacity)
                size = buff->capacity - read_head;
            if(read_head + size > buff->length)
                size = buff->length - read_head;

            std::copy(&buff->mem[read_head], &buff->mem[read_head + size], copy_buff);
            read_head += size;
        }

        void StereoRead(int16_t* l, int16_t* r, bool rev)
        {
            const size_t new_read_head = read_head + 2;
            if(!rev && new_read_head <= LoopEnd() && new_read_head <= buff->capacity)
            {
                *l = buff->mem[read_head];
                *r = buff->mem[read_head + 1];
                read_head = new_read_head;
            }
            else if(rev && read_head >= LoopStart() + 2)
            {
                *l = buff->mem[read_head - 2];
                *r = buff->mem[read_head - 1];
                read_head -= 2;
            }
            else
            {
                *l = *r = 0;
            }
        }

        void BlockWrite(int16_t* copy_buff, size_t size)
        {
            if(write_head + size > buff->capacity)
                size = buff->capacity - write_head;

            std::copy(copy_buff, &copy_buff[size], &buff->mem[write_head]);
            write_head += size;
            buff->length = write_head;
            buff->length -= buff->length % 2;
        }

        void StereoWrite(int16_t l, int16_t r, bool rev, bool set_len)
        {
            if(!rev)
            {
                const size_t new_write_head = write_head + 2;
                if(new_write_head <= buff->capacity)
                {
                    buff->mem[write_head] = l;
                    buff->mem[write_head + 1] = r;
                    write_head = new_write_head;
                    if(set_len)
                        buff->length = write_head;
                }
            }
            else if(write_head >= 2)
            {
                buff->mem[write_head - 2] = l;
                buff->mem[write_head - 1] = r;
                write_head -= 2;
            }
        }

        inline size_t GetSize() { return buff->length; }

        // the loop window, the whole recording unless one is set
        inline size_t LoopEnd() const
        {
            return buff->win_end && buff->win_end < buff->length ? buff->win_end : buff->length;
        }
        inline size_t LoopStart() const { return buff->win_start < LoopEnd() ? buff->win_start : 0; }

        // PORTA: play in step with another buffer: same heads, own memory
        void CopyHeads(const RamBuffer& other)
        {
            read_head  = other.read_head;
            write_head = other.write_head;
        }

        inline size_t GetReadHead() { return read_head; }
        inline bool ReadEOF() { return read_head >= LoopEnd() || read_head >= buff->capacity; }
        inline bool ReadLoop(bool rev)
        {
            return (rev && (read_head <= LoopStart() || read_head > LoopEnd())) || (!rev && ReadEOF());
        }

        void SetReadHead(size_t pos) 
        { 
            if(pos <= buff->capacity && pos <= buff->length)
                read_head = pos;
        }
        
        size_t GetRemainingRead(bool rev)
        {
            if(!rev && !ReadEOF())
                return LoopEnd() - read_head;
            else if(rev)
                return read_head > LoopStart() ? read_head - LoopStart() : 0;

            return 0;
        }

        inline size_t GetWriteHead() { return write_head; }
        inline bool WriteEOF() { return write_head >= LoopEnd() || write_head >= buff->capacity; }
        inline bool WriteFullLength() { return write_head >= buff->capacity; }
        inline bool WriteLoop(bool rev) { return (rev && write_head <= LoopStart()) || (!rev && WriteEOF()); }

        void SetWriteHead(size_t pos) 
        {
            if(pos <= buff->capacity && pos <= buff->length)
                write_head = pos;
        }

        size_t GetRemainingWrite(bool rev)
        {
            if(!rev && !WriteEOF())
                return LoopEnd() - write_head;
            else if(rev)
                return write_head > LoopStart() ? write_head - LoopStart() : 0;

            return 0;            
        }

        void AdvanceWrite(bool rev)
        {
            if(rev && write_head >= LoopStart() + 2)
                write_head -= 2;
            else if(!rev && write_head + 2 <= LoopEnd() && write_head + 2 < buff->capacity)
                write_head += 2;
        }

        // the pre-roll loops behind the buffer, so the old sample stays until the threshold (from MIXTAPE)
        bool PreRollWrite(int16_t l, int16_t r)
        {
            buff->mem[buff->capacity + write_head]     = l;
            buff->mem[buff->capacity + write_head + 1] = r;
            write_head = (write_head + 2) % kRecPreRoll;
            return !write_head;
        }

        void EndPreRoll()
        {
            const int16_t* const ring = &buff->mem[buff->capacity];
            const size_t head = write_head;
            Reset();
            std::rotate_copy(ring, ring + head, ring + kRecPreRoll, &buff->mem[0]);
            write_head = buff->length = kRecPreRoll;
        }

    private:
        RamBufferMemory* buff;
        size_t read_head, write_head;
    };
} // namespace daisy