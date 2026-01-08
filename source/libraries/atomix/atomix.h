/*
atomix.h - Portable, single-file, wait-free atomic sound mixing library utilizing SIMD-accelerated mixing

To the extent possible under law, the author(s) have dedicated all copyright and related and neighboring
rights to this software to the public domain worldwide. This software is distributed without any warranty.
You should have received a copy of the CC0 Public Domain Dedication along with this software.
If not, see <http://creativecommons.org/publicdomain/zero/1.0/>.
*/

/*
atomix supports the following three configurations:
#define ATOMIX_EXTERN
    Default, should be used when using atomix in multiple compilation units within the same project.
#define ATOMIX_IMPLEMENTATION
    Must be defined in exactly one source file within a project for atomix to be found by the linker.
#define ATOMIX_STATIC
    Defines all atomix functions as static, useful if atomix is only used in a single compilation unit.

atomix supports the following additional options:
#define ATOMIX_NO_CLIP
    Disables internal clipping, useful if you are using a backend that already does clipping of its own.
#define ATOMIX_NO_SIMD
    Disables all SIMD optimizations, which makes atomix mix about 4 times slower but also use less memory.
#define ATOMIX_LBITS
    Determines the number of layers as a power of 2. For example the default value of 8 means 256 layers.
#define ATOMIX_ZALLOC(S)
    Overrides the zalloc function used by atomix with your own. This is calloc but with just 1 argument.

atomix threads:
    Atomix is built around having one thread occasionally calling atomixMixerMix (usually in a callback)
    and one other thread (usually the main thread) calling the other functions to play/stop/etc sounds.
    Anything more than this will likely break thread safety and result in various problems and/or bugs.

atomix frames:
    A frame refers to a number of samples equal to the number of channels, so usually two floats as one
    sample is a single float and atomix works primarily in stereo. Calling atomixSoundNew with a channel
    count of one is the one exception where a frame is a single sample as there is only one channel.

atomix fading:
    Fading out happens automatically when a playing sound is stopped (changed to ATOMIX_STOP or ATOMIX_HALT).
    Fading in happens when a sound is resumed (changed to ATOMIX_PLAY or ATOMIX_LOOP) after having been halted.
    Fading out will not happen if the sound is close to its end, in which case it will simply play out instead.
    A sound started in a halted state will start fully faded out, resulting in a fade in when it is unhalted.

atomix limits (SIMD):
    The atomixMixerMix function uses a buffer on the stack which scales with the number of frames requested,
    therefore requesting an excessively high number of frames at once will likely result in a stack overflow.
    All frame numbers passed to atomix functions such as cursor positions, start and end frames, fade timers,
    and sound lengths are rounded to multiples of 4 in some way for reasons relating to internal alignment.

atomix limits (non-SIMD):
    Without SIMD alignment requirements the atomixMixerMix no longer has to use a buffer on the stack, which
    removes the risk of stack overflow when mixing lots of frames at once. Memory usage is lower in general.
    Frame numbers passed to atomix functions are still rounded to multiples of 4 to keep the API consistent.
    Internally things are slightly different, as frames are now processed one-by-one instead of 4 at a time.
*/

//header section
#ifndef ATOMIX_H
#define ATOMIX_H

//process configuration
#ifdef ATOMIX_STATIC
    #define ATOMIX_IMPLEMENTATION
    #define ATMXDEF static
#else //ATOMIX_EXTERN
    #define ATMXDEF extern
#endif

//enums
enum atomix_state_e {
    ATOMIX_FREE=0,
    ATOMIX_STOP=1,
    ATOMIX_HALT=2,
    ATOMIX_PLAY=3,
    ATOMIX_LOOP=4,
    ATOMIX_VOL_CHANGE=8 // or'ed with flag for streams
};

enum atomix_format_e {
    ATOMIX_U8,
    ATOMIX_F32
};

//includes
#include <stdint.h> //integer types

//structs
struct atomix_mixer; //forward declaration
struct atomix_sound; //forward declaration
struct atomix_stream;
struct atomix_stream_callbacks {
    void     (*render)    (void* userdata, float* buffer, uint32_t samples);
    void     (*free)      (void* userdata);
    void     (*volume)    (void* userdata, float vol);
};

//function declarations
    //creates a new atomix sound with given number of channels and data
    //length of data is in frames and rounded to multiple of 4 for alignment
    //given data is copied, so the buffer can safely be freed after return
    //assumes that incoming data is comprised of floating point
    //samples at the correct mixing rate and channel count
    //if this is not the case, use atomixSoundNewResampled instead
    //returns a pointer to the new atomix sound or NULL on failure
ATMXDEF struct atomix_sound* atomixSoundNew(uint8_t, const float*, int32_t);
    //creates a new resampled atomix sound with given mixer, number of channels,
    //data, sample rate targets, and format
    //length of data is in frames and rounded to multiple of 4 for alignment
    //given data is copied, so the buffer can safely be freed after return
    //returns a pointer to the new atomix sound or NULL on failure
ATMXDEF struct atomix_sound* atomixSoundNewResampled(struct atomix_mixer*, uint8_t, const void*, int32_t, int32_t, enum atomix_format_e);
    //creates a new atomix stream with the given userdata, callbacks, and sample rate
    //returns a pointer to the new atomix stream or NULL on failure
ATMXDEF struct atomix_stream* atomixStreamNew(void*, struct atomix_stream_callbacks*, int32_t);
    //frees the given atomix_sound
    //the pointer will not be valid afterwards
ATMXDEF void atomixSoundFree(struct atomix_sound*);
    //frees the given atomix_stream
    //the pointer will not be valid afterwards
ATMXDEF void atomixStreamFree(struct atomix_stream*);
    //returns the length of given sound in frames, always multiple of 4
ATMXDEF int32_t atomixSoundLength(struct atomix_sound*);
    //returns the reference count of the given sound, or 0 if passed a NULL pointer
ATMXDEF int atomixSoundRefCount(struct atomix_sound*);
    //returns a new atomix mixer with given volume, fade, and sample rate or NULL on failure to allocate
ATMXDEF struct atomix_mixer* atomixMixerNew(float, int32_t, int32_t);
    //frees the given atomix_mixer
    //the pointer will not be valid afterwards
ATMXDEF void atomixMixerFree (struct atomix_mixer*);
    //uses given atomix mixer to output exactly the requested number of frames to given buffer
    //returns the number of frames actually written to the buffer, buffer must not be NULL
ATMXDEF uint32_t atomixMixerMix(struct atomix_mixer*, float*, uint32_t);
    //uses given atomix mixer to play given atomix sound with given initial state, gain, and pan
    //returns a sound handle used to reference the sound at a later point, or 0 on failure
ATMXDEF uint32_t atomixMixerPlaySound(struct atomix_mixer*, struct atomix_sound*, uint8_t, float, float);
    //variant of atomixPlay that sets the start and end frames in the sound, positions are truncated to multiple of 4
    //a negative start value can be used to play a sound with a delay, a high end value can be used to loop a few times
    //if in the ATOMIX_LOOP state, looping will include these start/end positions, allowing for partial sounds to loop
    //returns a sound handle used to reference the sound at a later point, or 0 on failure
ATMXDEF uint32_t atomixMixerPlaySoundAdv(struct atomix_mixer*, struct atomix_sound*, uint8_t, float, float, int32_t, int32_t, int32_t);
    //uses given atomix mixer to play given atomix stream with given initial state, and gain
    //will replace the currently playing stream, if any
    //returns a sound handle used to reference the stream at a later point, or 0 on failure
ATMXDEF uint32_t atomixMixerPlayStream(struct atomix_mixer*, struct atomix_stream*, uint8_t, float);
    //sets the gain and pan for the sound with given handle in given mixer
    //gain may be any float including negative, pan is clamped internally
    //returns 1 on success, 0 if the handle is invalid
ATMXDEF int atomixMixerSetSoundGainPan(struct atomix_mixer*, uint32_t, float, float);
    //sets the gain for the stream with given handle in given mixer
    //gain will be clamped to the 0.0-1.0f range
    //returns 1 on success, 0 if the handle is invalid
ATMXDEF int atomixMixerSetStreamGain(struct atomix_mixer*, uint32_t, float);
//sets the cursor for the sound with given handle in given mixer
    //given cursor value is clamped and truncated to multiple of 4
    //returns 0 on success, non-zero if the handle is invalid
ATMXDEF int atomixMixerSetCursor(struct atomix_mixer*, uint32_t, int32_t);
    //gets the state for the sound with given handle in given mixer
    //returns the state on success, -1 if the handle is invalid
ATMXDEF int atomixMixerGetSoundState(struct atomix_mixer*, uint32_t);
    //gets the state for the stream with given handle in given mixer
    //returns the state on success, -1 if the handle is invalid
ATMXDEF int atomixMixerGetStreamState(struct atomix_mixer*, uint32_t);
    //sets the state for the sound with given handle in given mixer
    //given state must be one of the ATOMIX_XXX define constants other than
    //ATOMIX_FREE. returns 0 on success, non-zero if the handle is invalid
ATMXDEF int atomixMixerSetSoundState(struct atomix_mixer*, uint32_t, uint8_t);
    //sets the state for the stream with given handle in given mixer
    //given state must be one of the ATOMIX_XXX define constants other than
    //ATOMIX_FREE. returns 0 on success, non-zero if the handle is invalid
ATMXDEF int atomixMixerSetStreamState(struct atomix_mixer*, uint32_t, uint8_t);
    //gets the number of active sounds in the given atomix_mixer
    //returns the number of channels on success, -1 if the mixer was invalid
ATMXDEF int atomixMixerGetActive(struct atomix_mixer*);
    //sets the global volume for given atomix mixer, may be any float including negative
ATMXDEF void atomixMixerVolume(struct atomix_mixer*, float);
    //sets the global default fade value applied to all new sounds added after this command
ATMXDEF void atomixMixerFade(struct atomix_mixer*, int32_t);
    //stops all sounds in given mixer, invalidating any existing sound handles in that mixer
ATMXDEF void atomixMixerStopAll(struct atomix_mixer*);
    //halts all sounds currently playing in given mixer, allowing them to be resumed later
ATMXDEF void atomixMixerHaltAll(struct atomix_mixer*);
    //resumes all halted sounds in given mixer, no effect on looping or stopped sounds
ATMXDEF void atomixMixerPlayAll(struct atomix_mixer*);
#endif //ATOMIX_H

//implementation section
#ifdef ATOMIX_IMPLEMENTATION
#undef ATOMIX_IMPLEMENTATION

//includes
#ifndef ATOMIX_NO_SIMD
    #if defined(__SSE__) || defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 1)
        #include <xmmintrin.h>
        #define ATOMIX_VEC_TYPE __m128
        #define ATOMIX_VEC_CREATE(A,B,C,D) _mm_setr_ps(A,B,C,D)
        #define ATOMIX_VEC_LOAD_UNALIGNED _mm_loadu_ps
        #define ATOMIX_VEC_STORE_ALIGNED _mm_store_ps
        #define ATOMIX_VEC_SET_ZERO _mm_setzero_ps()
        #define ATOMIX_VEC_SET_FLOAT _mm_set_ps1
        #define ATOMIX_VEC_ADD _mm_add_ps
        #define ATOMIX_VEC_MULTIPLY _mm_mul_ps
        #define ATOMIX_VEC_MAX_FLOAT _mm_max_ps
        #define ATOMIX_VEC_MIN_FLOAT _mm_min_ps
        #define ATOMIX_VEC_UNPACK_LO _mm_unpacklo_ps
        #define ATOMIX_VEC_UNPACK_HI _mm_unpackhi_ps
        #define ATOMIX_VEC_MOVE_HI_LO(A,B) _mm_movehl_ps(A,B)
        #define ATOMIX_VEC_SHUFFLE_SUMS(A) _mm_shuffle_ps(A, A, 1)
        #define ATOMIX_VEC_SCALAR_ADD(A,B) _mm_add_ss(A,B)
        #define ATOMIX_VEC_LANE_ZERO(A) _mm_cvtss_f32(A)
    #elif defined(__ARM_NEON) || defined(__ARM_NEON__)
        #include <arm_neon.h>
        #define ATOMIX_VEC_TYPE float32x4_t
        #define ATOMIX_VEC_CREATE(A,B,C,D) (float32x4_t){A,B,C,D}
        #define ATOMIX_VEC_LOAD_UNALIGNED vld1q_f32
        //there is no specific "aligned store" variant of this instruction;
        //vst1q_f32 is used regardless of alignment, but the only
        //place this is used in atomix already assures that the destination
        //buffer is aligned
        #define ATOMIX_VEC_STORE_ALIGNED vst1q_f32
        #define ATOMIX_VEC_SET_ZERO vdupq_n_f32(0.0f)
        #define ATOMIX_VEC_SET_FLOAT vdupq_n_f32
        #define ATOMIX_VEC_ADD vaddq_f32
        #define ATOMIX_VEC_MULTIPLY vmulq_f32
        #define ATOMIX_VEC_MAX_FLOAT vmaxq_f32
        #define ATOMIX_VEC_MIN_FLOAT vminq_f32
        #if defined(__aarch64__)
            #define ATOMIX_VEC_UNPACK_LO vzip1q_f32
            #define ATOMIX_VEC_UNPACK_HI vzip2q_f32
        #else
            static inline float32x4_t atomix_unpack_lo(float32x4_t a, float32x4_t b) {
                float32x4x2_t result = vzipq_f32(a, b);
                return result.val[0];
            }
            static inline float32x4_t atomix_unpack_hi(float32x4_t a, float32x4_t b) {
                float32x4x2_t result = vzipq_f32(a, b);
                return result.val[1];
            }
            #define ATOMIX_VEC_UNPACK_LO atomix_unpack_lo
            #define ATOMIX_VEC_UNPACK_HI atomix_unpack_hi
        #endif
        #define ATOMIX_VEC_MOVE_HI_LO(A,B) vextq_f32(A, B, 2)
        static inline float32x4_t atomix_shuffle_sums(float32x4_t sums) {
            float32x2_t rev = vrev64_f32(vget_low_f32(sums));
            float32x2_t dup = vdup_lane_f32(vget_low_f32(sums), 0);
            return vcombine_f32(rev, dup);
        }
        #define ATOMIX_VEC_SHUFFLE_SUMS(A) atomix_shuffle_sums(A)
        static inline float32x4_t atomix_scalar_add(float32x4_t a, float32x4_t b) {
            float32x2_t sum_low = vadd_f32(vget_low_f32(a), vget_low_f32(b));
            // Replace lane 0 of 'a' with the sum
            float32x2_t res_low = vset_lane_f32(vget_lane_f32(sum_low, 0), vget_low_f32(a), 0);
            return vcombine_f32(res_low, vget_high_f32(a));
        }
        #define ATOMIX_VEC_SCALAR_ADD(A,B) atomix_scalar_add(A,B)
        #define ATOMIX_VEC_LANE_ZERO(A) vgetq_lane_f32(A, 0)
    #else
        #define ATOMIX_NO_SIMD
    #endif
#endif

//macros
#if !defined(ATOMIX_ZALLOC) || !defined(ATOMIX_ZFREE) || (!defined(ATOMIX_NO_SIMD) && (!defined(ATOMIX_ZALLOC_ALIGNED) || !defined(ATOMIX_ZFREE_ALIGNED)))
    #include <stdlib.h> //calloc/free
    #ifndef ATOMIX_ZALLOC
        #define ATOMIX_ZALLOC(S) calloc(1, S)
    #endif
    #ifndef ATOMIX_ZFREE
        #define ATOMIX_ZFREE(P) free(P)
    #endif
    #ifndef ATOMIX_NO_SIMD
        #ifndef ATOMIX_ZALLOC_ALIGNED
            #if defined(_MSC_VER) || defined(__MINGW32__)
                #define ATOMIX_ZALLOC_ALIGNED(P,S) *P = _aligned_malloc(S, 16)
            #else
                #define ATOMIX_ZALLOC_ALIGNED(P,S) if (posix_memalign(P, 16, S) != 0) *P = NULL
            #endif
        #endif
        #ifndef ATOMIX_ZFREE_ALIGNED
            #if defined(_MSC_VER) || defined(__MINGW32__)
                #define ATOMIX_ZFREE_ALIGNED(P) _aligned_free(P)
            #else
                #define ATOMIX_ZFREE_ALIGNED(P) free(P)
            #endif
        #endif
    #endif
#endif

//constants
#ifndef ATOMIX_LBITS
    #define ATOMIX_LBITS 8
#endif
#define ATMX_LAYERS (1 << ATOMIX_LBITS)
#define ATMX_LMASK (ATMX_LAYERS - 1)
#ifndef ATMX_RATE_MIN
#define ATMX_RATE_MIN 8000
#endif
#ifndef ATMX_RATE_MAX
#define ATMX_RATE_MAX 192000
#endif
#define ATMX_STREAM_RESAMPLE_MAX_RATIO (ATMX_RATE_MAX / ATMX_RATE_MIN)
#define ATMX_STREAM_RESAMPLE_BLOCK_SIZE 128
#define ATMX_STREAM_RESAMPLE_SRC_FRAMES (ATMX_STREAM_RESAMPLE_BLOCK_SIZE * ATMX_STREAM_RESAMPLE_MAX_RATIO + 1)
#ifndef ATMX_FIR_TAPS
    #define ATMX_FIR_TAPS 4
#endif

#ifdef _MSC_VER
#define _USE_MATH_DEFINES
#endif
#include <math.h>
#include <string.h> //memcpy
#include "thread.h" //portable atomics

//structs
struct atomix_sound {
    uint8_t cha; //channels
    int32_t len; //data length
    thread_atomic_int_t ref; //reference count
    #ifndef ATOMIX_NO_SIMD
        ATOMIX_VEC_TYPE* data; //aligned data
    #else
        float data[]; //float data
    #endif
};
struct atmx_layer {
    uint32_t id; //playing id
    thread_atomic_int_t flag; //state
    thread_atomic_int_t cursor; //cursor
    thread_atomic_int_t gain_l; //left channel gain
    thread_atomic_int_t gain_r; //right channel gain
    struct atomix_sound* snd; //sound data
    int32_t start, end; //start and end
    int32_t fade, fmax; //fading
};
struct atomix_mixer {
    int32_t samplerate; // needed for possible stream resampling
    uint32_t nid; //next id
    thread_atomic_int_t mixing; //actively mixing?
    thread_atomic_int_t volume; //global volume
    uint32_t sid; //stream id
    thread_atomic_ptr_t stream; //singular stream
    struct atmx_layer lays[ATMX_LAYERS]; //layers
    int32_t fade; //global default fade value
    float strm_firs[ATMX_FIR_TAPS];
    float strm_fir_hist_l[ATMX_FIR_TAPS];
    float strm_fir_hist_r[ATMX_FIR_TAPS];
    float strm_fir_resample_src[ATMX_STREAM_RESAMPLE_SRC_FRAMES * 2];
    float snd_fir_hist_l[ATMX_FIR_TAPS];
    float snd_fir_hist_r[ATMX_FIR_TAPS];
    #ifndef ATOMIX_NO_SIMD
        ATOMIX_VEC_TYPE *align; //aligned buffer for mixing
        uint32_t align_size; //aligned buffer size in members
        uint32_t rem; //remaining frames
        float data[6]; //old frames
    #endif
};

struct atomix_stream {
    thread_atomic_int_t flag; //state
    thread_atomic_int_t gain;
    int32_t samplerate;
    void* userdata;
    struct atomix_stream_callbacks callbacks;
};

//function declarations
#ifndef ATOMIX_NO_SIMD
    static void atmxMixLayer(struct atmx_layer*, ATOMIX_VEC_TYPE, ATOMIX_VEC_TYPE*, uint32_t);
    static int32_t atmxMixFadeMono(struct atmx_layer*, int32_t, ATOMIX_VEC_TYPE, ATOMIX_VEC_TYPE*, uint32_t);
    static int32_t atmxMixFadeStereo(struct atmx_layer*, int32_t, ATOMIX_VEC_TYPE, ATOMIX_VEC_TYPE*, uint32_t);
    static int32_t atmxMixPlayMono(struct atmx_layer*, int, int32_t, ATOMIX_VEC_TYPE, ATOMIX_VEC_TYPE*, uint32_t);
    static int32_t atmxMixPlayStereo(struct atmx_layer*, int, int32_t, ATOMIX_VEC_TYPE, ATOMIX_VEC_TYPE*, uint32_t);
#else
    static void atmxMixLayer(struct atmx_layer*, float, float*, uint32_t);
    static int32_t atmxMixFadeMono(struct atmx_layer*, int32_t, float, float, float*, uint32_t);
    static int32_t atmxMixFadeStereo(struct atmx_layer*, int32_t, float, float, float*, uint32_t);
    static int32_t atmxMixPlayMono(struct atmx_layer*, int, int32_t, float, float, float*, uint32_t);
    static int32_t atmxMixPlayStereo(struct atmx_layer*, int, int32_t, float, float, float*, uint32_t);
#endif
//private functions
static inline void atmxGainf2 (thread_atomic_int_t* gain_l, thread_atomic_int_t* gain_r, float gain, float pan) {
    //clamp pan to its valid range of -1.0f to 1.0f inclusive
    pan = (pan < -1.0f) ? -1.0f : (pan > 1.0f) ? 1.0f : pan;
    //convert gain and pan to left and right gain and store it atomically
    thread_atomic_int_store(gain_l, (int)(gain*(0.5f - pan/2.0f) * 100));
    thread_atomic_int_store(gain_r, (int)(gain*(0.5f + pan/2.0f) * 100));
}
static inline float atmxToF32 (uint8_t u) {
    return ((float)u - 128.0f) * (1.0f / 128.0f);
}
static void atmxBuildFIRTable(float* h, int taps, float cutoff) {
    int M = taps - 1;
    float sum = 0.0f;
    for (int n = 0; n < taps; ++n) {
        float m = (float)n - 0.5f * (float)M;
        float x = m * (2.0f * cutoff);
        float sincv = (fabsf(x) < 1e-6f) ? 1.0f : sinf((float)M_PI * x) / ((float)M_PI * x);
        float w = 0.5f * (1.0f - cosf(2.0f * (float)M_PI * (float)n / (float)M));
        h[n] = 2.0f * cutoff * sincv * w;
        sum += h[n];
    }
    if (sum != 0.0f) {
        float inv = 1.0f / sum;
        for (int n = 0; n < taps; ++n) h[n] *= inv;
    }
}
#ifndef ATOMIX_NO_SIMD
static inline float atmxFIRDot(const float* x, const float* h, int n) {
    ATOMIX_VEC_TYPE acc = ATOMIX_VEC_SET_ZERO;
    int i = 0;
    for (; i + 4 <= n; i += 4) {
        ATOMIX_VEC_TYPE xv = ATOMIX_VEC_LOAD_UNALIGNED(x + i);
        ATOMIX_VEC_TYPE hv = ATOMIX_VEC_LOAD_UNALIGNED(h + i);
        acc = ATOMIX_VEC_ADD(acc, ATOMIX_VEC_MULTIPLY(xv, hv));
    }
    ATOMIX_VEC_TYPE shuf = ATOMIX_VEC_MOVE_HI_LO(acc, acc);
    ATOMIX_VEC_TYPE sums = ATOMIX_VEC_ADD(acc, shuf);
    shuf = ATOMIX_VEC_SHUFFLE_SUMS(sums);
    sums = ATOMIX_VEC_SCALAR_ADD(sums, shuf);
    float sum = ATOMIX_VEC_LANE_ZERO(sums);
    for (; i < n; ++i) sum += x[i] * h[i];
    return sum;
}
#else
static inline float atmxFIRDot(const float* x, const float* h, int n) {
    float s = 0.0f; for (int i = 0; i < n; ++i) s += x[i] * h[i]; return s;
}
#endif
#ifndef ATOMIX_NO_SIMD
static inline void atmxLerpStereo(float one_minus, float frac, float s0L, float s0R, float s1L, float s1R, float* yL, float* yR) {
    ATOMIX_VEC_TYPE val0 = ATOMIX_VEC_CREATE(s0L, s0R, 0.0f, 0.0f);
    ATOMIX_VEC_TYPE val1 = ATOMIX_VEC_CREATE(s1L, s1R, 0.0f, 0.0f);
    ATOMIX_VEC_TYPE om = ATOMIX_VEC_SET_FLOAT(one_minus);
    ATOMIX_VEC_TYPE f = ATOMIX_VEC_SET_FLOAT(frac);
    ATOMIX_VEC_TYPE result = ATOMIX_VEC_ADD(ATOMIX_VEC_MULTIPLY(om, val0), ATOMIX_VEC_MULTIPLY(f, val1));
    *yL = ATOMIX_VEC_LANE_ZERO(result);
    *yR = ATOMIX_VEC_LANE_ZERO(ATOMIX_VEC_SHUFFLE_SUMS(result));
}
static inline void atmxFIRShiftHistory(float* hist, int taps) {
    if (taps >= 5) {
        for (int i = taps - 1; i >= 4; i -= 4) {
            ATOMIX_VEC_TYPE v = ATOMIX_VEC_LOAD_UNALIGNED(hist + i - 4);
            ATOMIX_VEC_STORE_ALIGNED(hist + i, v);
        }
        for (int i = 3; i > 0; --i) {
            hist[i] = hist[i - 1];
        }
    } else {
        memmove(hist + 1, hist, (taps - 1) * sizeof(float));
    }
}
#else
static inline void atmxLerpStereo(float one_minus, float frac, float s0L, float s0R, float s1L, float s1R, float* yL, float* yR) {
    *yL = one_minus * s0L + frac * s1L;
    *yR = one_minus * s0R + frac * s1R;
}
static inline void atmxFIRShiftHistory(float* hist, int taps) {
    memmove(hist + 1, hist, (taps - 1) * sizeof(float));
}
#endif
static void atmxResampleStream(struct atomix_mixer* mix, struct atomix_stream* strm, float* buff, uint32_t frames) {
    float ratio = (float)strm->samplerate / (float)mix->samplerate;
    if (ratio > ATMX_STREAM_RESAMPLE_MAX_RATIO) ratio = ATMX_STREAM_RESAMPLE_MAX_RATIO;
    float src_pos = 0.0f;
    float last_samples[2] = { mix->strm_fir_hist_l[0], mix->strm_fir_hist_r[0] };
    float gain = (float)thread_atomic_int_load(&strm->gain) * 0.01f;
    for (uint32_t i = 0; i < frames; ) {
        uint32_t frames_in_block =
            (i + ATMX_STREAM_RESAMPLE_BLOCK_SIZE > frames)
            ? (frames - i)
            : ATMX_STREAM_RESAMPLE_BLOCK_SIZE;
        float next_src_pos = src_pos + (float)frames_in_block * ratio;
        uint32_t src_frames_needed = (uint32_t)(next_src_pos) - (uint32_t)(src_pos) + 1;
        mix->strm_fir_resample_src[0] = last_samples[0];
        mix->strm_fir_resample_src[1] = last_samples[1];
        strm->callbacks.render(strm->userdata, &mix->strm_fir_resample_src[2], (src_frames_needed > 1) ? (src_frames_needed - 1) : 0);
        float base_src_pos = (float)(uint32_t)src_pos;
        for (uint32_t j = 0; j < frames_in_block; ++j) {
            float pos_local = (src_pos + (float)j * ratio) - base_src_pos;
            uint32_t i0 = (uint32_t)pos_local;
            float frac = pos_local - (float)i0;
            uint32_t i1 = i0 + 1;
            if (i1 >= src_frames_needed) i1 = src_frames_needed - 1;
            size_t b0 = (size_t)i0 * 2;
            size_t b1 = (size_t)i1 * 2;
            float s0L = mix->strm_fir_resample_src[b0 + 0];
            float s0R = mix->strm_fir_resample_src[b0 + 1];
            float s1L = mix->strm_fir_resample_src[b1 + 0];
            float s1R = mix->strm_fir_resample_src[b1 + 1];
            float one_minus = 1.0f - frac;
            float yL, yR;
            atmxLerpStereo(one_minus, frac, s0L, s0R, s1L, s1R, &yL, &yR);
            atmxFIRShiftHistory(mix->strm_fir_hist_l, ATMX_FIR_TAPS);
            mix->strm_fir_hist_l[0] = yL;
            atmxFIRShiftHistory(mix->strm_fir_hist_r, ATMX_FIR_TAPS);
            mix->strm_fir_hist_r[0] = yR;
            float oL = atmxFIRDot(mix->strm_fir_hist_l, mix->strm_firs, ATMX_FIR_TAPS);
            float oR = atmxFIRDot(mix->strm_fir_hist_r, mix->strm_firs, ATMX_FIR_TAPS);
            if (strm->callbacks.volume) {
                buff[(i + j) * 2 + 0] = oL;
                buff[(i + j) * 2 + 1] = oR;
            } else {
                buff[(i + j) * 2 + 0] = oL * gain; 
                buff[(i + j) * 2 + 1] = oR * gain;
            }
        }
        last_samples[0] = mix->strm_fir_resample_src[(src_frames_needed - 1) * 2 + 0];
        last_samples[1] = mix->strm_fir_resample_src[(src_frames_needed - 1) * 2 + 1];
        src_pos = next_src_pos;
        i += frames_in_block;
    }
}


//public functions
ATMXDEF struct atomix_stream* atomixStreamNew(void* userdata, struct atomix_stream_callbacks* cb, int32_t samplerate) {
    //validate arguments first and return NULL if invalid
    if ((!userdata)||(!cb)||(!cb->render)||(!cb->free)) return NULL;
    if ((samplerate < ATMX_RATE_MIN)||(samplerate > ATMX_RATE_MAX)) return NULL;
    struct atomix_stream* strm = (struct atomix_stream*)ATOMIX_ZALLOC(sizeof(struct atomix_stream));
    if (!strm) return NULL;
    strm->userdata = userdata;
    strm->samplerate = samplerate;
    strm->callbacks = *cb;
    return strm;
}
ATMXDEF struct atomix_sound* atomixSoundNew (uint8_t cha, const float* data, int32_t len) {
    //validate arguments first and return NULL if invalid
    if ((cha < 1)||(cha > 2)||(!data)||(len < 1)) return NULL;
    //round length to next multiple of 4
    int32_t rlen = (len + 3) & ~0x03;
    //allocate sound struct and space for data
    #ifndef ATOMIX_NO_SIMD
        struct atomix_sound* snd = (struct atomix_sound*)ATOMIX_ZALLOC(sizeof(struct atomix_sound) + rlen*cha*sizeof(float) + 15);
    #else
        struct atomix_sound* snd = (struct atomix_sound*)ATOMIX_ZALLOC(sizeof(struct atomix_sound) + rlen*cha*sizeof(float));
    #endif
    //return if zalloc failed
    if (!snd) return NULL;
    //fill in channel and length
    snd->cha = cha; snd->len = rlen;
    //align data pointer in allocated space if SIMD
    #ifndef ATOMIX_NO_SIMD
        snd->data = (ATOMIX_VEC_TYPE*)(void*)(((uintptr_t)(void*)&snd[1] + 15) & ~15);
    #endif
    //copy sound data into now aligned buffer
    memcpy(snd->data, data, len*cha*sizeof(float));
    //return
    return snd;
}
ATMXDEF struct atomix_sound* atomixSoundNewResampled(struct atomix_mixer* mix, uint8_t cha, const void* data, int32_t len, int32_t src_rate, enum atomix_format_e fmt) {
    //validate arguments first and return NULL if invalid
    if ((!mix)||(cha < 1)||(cha > 2)||(!data)||(len < 1)) return NULL;
    if ((src_rate < ATMX_RATE_MIN)||(src_rate > ATMX_RATE_MAX)||(mix->samplerate < ATMX_RATE_MIN)||(mix->samplerate > ATMX_RATE_MAX)) return NULL;
    size_t out_len = (size_t)((double)len * mix->samplerate / src_rate + 0.5);
    //round length to next multiple of 4
    int32_t rlen = (out_len + 3) & ~0x03;
    //allocate sound struct and space for data
    #ifndef ATOMIX_NO_SIMD
        struct atomix_sound* snd = (struct atomix_sound*)ATOMIX_ZALLOC(sizeof(struct atomix_sound) + rlen*cha*sizeof(float) + 15);
    #else
        struct atomix_sound* snd = (struct atomix_sound*)ATOMIX_ZALLOC(sizeof(struct atomix_sound) + rlen*cha*sizeof(float));
    #endif
    //return if zalloc failed
    if (!snd) return NULL;
    //fill in channel and length
    snd->cha = cha; snd->len = rlen;
    //align data pointer in allocated space if SIMD
    #ifndef ATOMIX_NO_SIMD
        snd->data = (ATOMIX_VEC_TYPE*)(void*)(((uintptr_t)(void*)&snd[1] + 15) & ~15);
    #endif
     double ratio = (double)src_rate / (double)mix->samplerate;
     double pos = 0.0;
     memset(mix->snd_fir_hist_l, 0, ATMX_FIR_TAPS * sizeof(float));
     memset(mix->snd_fir_hist_r, 0, ATMX_FIR_TAPS * sizeof(float));

    switch (fmt) {
        case ATOMIX_U8:
        {
            const uint8_t* data_u = (const uint8_t*)data;
            float* data_out = (float*)snd->data;
            if (cha == 1) {
                for (size_t i = 0; i < out_len; i++) {
                    size_t i0 = (size_t)pos;
                    float frac = (float)(pos - (double)i0);
                    int i1 = (int)i0 + 1;
                    if (i1 >= (int)len) i1 = (int)len - 1;
                    float s0 = atmxToF32(data_u[i0]);
                    float s1 = atmxToF32(data_u[i1]);
                    float one_minus = 1.0f - frac;
                    float y = one_minus * s0 + frac * s1;
                    atmxFIRShiftHistory(mix->snd_fir_hist_l, ATMX_FIR_TAPS);
                    mix->snd_fir_hist_l[0] = y;
                    data_out[i] = atmxFIRDot(mix->snd_fir_hist_l, mix->strm_firs, ATMX_FIR_TAPS);
                    pos += ratio;
                    if (pos > (double)(len - 1)) pos = (double)(len - 1);
                }
            } else {
                for (size_t i = 0; i < out_len; i++) {
                    size_t i0 = (size_t)pos;
                    float frac = (float)(pos - (double)i0);
                    int i1 = (int)i0 + 1;
                    if (i1 >= (int)len) i1 = (int)len - 1;
                    size_t b_i0 = (size_t)i0 * 2;
                    size_t b_i1 = (size_t)i1 * 2;
                    float s0L = atmxToF32(data_u[b_i0 + 0]);
                    float s0R = atmxToF32(data_u[b_i0 + 1]);
                    float s1L = atmxToF32(data_u[b_i1 + 0]);
                    float s1R = atmxToF32(data_u[b_i1 + 1]);
                    float one_minus = 1.0f - frac;
                    float yL, yR;
                    atmxLerpStereo(one_minus, frac, s0L, s0R, s1L, s1R, &yL, &yR);
                    atmxFIRShiftHistory(mix->snd_fir_hist_l, ATMX_FIR_TAPS);
                    mix->snd_fir_hist_l[0] = yL;
                    atmxFIRShiftHistory(mix->snd_fir_hist_r, ATMX_FIR_TAPS);
                    mix->snd_fir_hist_r[0] = yR;
                    data_out[i * 2 + 0] = atmxFIRDot(mix->snd_fir_hist_l, mix->strm_firs, ATMX_FIR_TAPS);
                    data_out[i * 2 + 1] = atmxFIRDot(mix->snd_fir_hist_r, mix->strm_firs, ATMX_FIR_TAPS);
                    pos += ratio;
                    if (pos > (double)(len - 1)) pos = (double)(len - 1);
                }
            }
            return snd;
            break;
        }
        case ATOMIX_F32:
        {
            const float* data_f = (const float*)data;
            float* data_out = (float*)snd->data;
            if (cha == 1) {
                for (size_t i = 0; i < out_len; i++) {
                    size_t i0 = (size_t)pos;
                    float frac = (float)(pos - (double)i0);
                    int i1 = (int)i0 + 1;
                    if (i1 >= (int)len) i1 = (int)len - 1;
                    float s0 = data_f[i0];
                    float s1 = data_f[i1];
                    float one_minus = 1.0f - frac;
                    float y = one_minus * s0 + frac * s1;
                    /* shift history and insert newest sample */
                    atmxFIRShiftHistory(mix->snd_fir_hist_l, ATMX_FIR_TAPS);
                    mix->snd_fir_hist_l[0] = y;
                    data_out[i] = atmxFIRDot(mix->snd_fir_hist_l, mix->strm_firs, ATMX_FIR_TAPS);
                    pos += ratio;
                    if (pos > (double)(len - 1)) pos = (double)(len - 1);
                }
            } else {
                for (size_t i = 0; i < out_len; i++) {
                    size_t i0 = (size_t)pos;
                    float frac = (float)(pos - (double)i0);
                    int i1 = (int)i0 + 1;
                    if (i1 >= (int)len) i1 = (int)len - 1;
                    size_t b_i0 = (size_t)i0 * 2;
                    size_t b_i1 = (size_t)i1 * 2;
                    float s0L = data_f[b_i0 + 0];
                    float s0R = data_f[b_i0 + 1];
                    float s1L = data_f[b_i1 + 0];
                    float s1R = data_f[b_i1 + 1];
                    float one_minus = 1.0f - frac;
                    float yL, yR;
                    atmxLerpStereo(one_minus, frac, s0L, s0R, s1L, s1R, &yL, &yR);
                    atmxFIRShiftHistory(mix->snd_fir_hist_l, ATMX_FIR_TAPS);
                    mix->snd_fir_hist_l[0] = yL;
                    atmxFIRShiftHistory(mix->snd_fir_hist_r, ATMX_FIR_TAPS);
                    mix->snd_fir_hist_r[0] = yR;
                    data_out[i * 2 + 0] = atmxFIRDot(mix->snd_fir_hist_l, mix->strm_firs, ATMX_FIR_TAPS);
                    data_out[i * 2 + 1] = atmxFIRDot(mix->snd_fir_hist_r, mix->strm_firs, ATMX_FIR_TAPS);
                    pos += ratio;
                    if (pos > (double)(len - 1)) pos = (double)(len - 1);
                }
            }
            return snd;
            break;
        }
        default:
            ATOMIX_ZFREE(snd);
            return NULL;
            break;
    }
}
ATMXDEF void atomixSoundFree(struct atomix_sound* snd) {
    if (snd)
        ATOMIX_ZFREE(snd);
}
ATMXDEF void atomixStreamFree(struct atomix_stream* strm) {
    if (strm) {
        if (strm->callbacks.free && strm->userdata) {
            strm->callbacks.free(strm->userdata);
        }
        ATOMIX_ZFREE(strm);
    }
}
ATMXDEF int32_t atomixSoundLength (struct atomix_sound* snd) {
    //return length, always multiple of 4
    if (!snd) return 0;
    return snd->len;
}
ATMXDEF int atomixSoundRefCount(struct atomix_sound* snd) {
    if (!snd) return 0;
    return thread_atomic_int_load(&snd->ref);
}
ATMXDEF struct atomix_mixer* atomixMixerNew (float vol, int32_t fade, int32_t samplerate) {
    //allocate space for the mixer filled with zero
    struct atomix_mixer* mix = (struct atomix_mixer*)ATOMIX_ZALLOC(sizeof(struct atomix_mixer));
    //return if zalloc failed
    if (!mix) return NULL;
    //atomically set the volume
    thread_atomic_int_store(&mix->volume, (int)(vol * 100));
    //set fade value
    mix->fade = (fade < 0) ? 0 : fade & ~3;
    //set samplerate
    mix->samplerate = samplerate;
    //build FIR coefficient table for stream resampling
    atmxBuildFIRTable(mix->strm_firs, ATMX_FIR_TAPS, 0.45f);
    /* initialize sound-specific FIR history/windows to safe defaults */
    memset(mix->snd_fir_hist_l, 0, ATMX_FIR_TAPS * sizeof(float));
    memset(mix->snd_fir_hist_r, 0, ATMX_FIR_TAPS * sizeof(float));
    (void)0;
    /* initialize stream FIR history to safe defaults */
    memset(mix->strm_fir_hist_l, 0, ATMX_FIR_TAPS * sizeof(float));
    memset(mix->strm_fir_hist_r, 0, ATMX_FIR_TAPS * sizeof(float));
    (void)0;
    //return
    return mix;
}
ATMXDEF void atomixMixerFree (struct atomix_mixer* mix) {
    if (mix) {
        int timeout = 0;
        while (thread_atomic_int_load(&mix->mixing)) {
            thread_yield();
            if (++timeout > 2500) {
                // Mixer hanging, can't guarantee it's safe to free
                // Better to leak slightly
                return;
            }
        }
#ifndef ATOMIX_NO_SIMD
        if (mix->align)
            ATOMIX_ZFREE_ALIGNED(mix->align);
#endif
        ATOMIX_ZFREE(mix);
    }
}
ATMXDEF uint32_t atomixMixerMix (struct atomix_mixer* mix, float* buff, uint32_t fnum) {
    if ((!mix)||(!buff)) return 0;
    thread_atomic_int_store(&mix->mixing, 1);
    //the mixing function differs greatly depending on whether SIMD is enabled or not
    #ifndef ATOMIX_NO_SIMD
        //output remaining frames in buffer before mixing new ones
        uint32_t rnum = fnum;
        //only do this if there are old frames
        if (mix->rem)
        {
            if (rnum > mix->rem) {
                //rnum bigger than remaining frames (usual case)
                memcpy(buff, mix->data, mix->rem*2*sizeof(float));
                rnum -= mix->rem; buff += mix->rem*2; mix->rem = 0;
            } else {
                //rnum smaller equal remaining frames (rare case)
                memcpy(buff, mix->data, rnum*2*sizeof(float)); mix->rem -= rnum;
                //move back remaining old frames if any
                if (mix->rem) memmove(mix->data, mix->data + rnum*2, (3 - rnum)*2*sizeof(float));
                //return
                return fnum;
            }
        }
        //asize in ATOMIX_VEC_TYPE (ATOMIX_VEC_TYPE = 2 frames) and multiple of 2
        uint32_t asize = ((rnum + 3) & ~3) >> 1;        
        if (!mix->align) {
            size_t bytes = asize * sizeof(ATOMIX_VEC_TYPE);
            ATOMIX_VEC_TYPE* new_align = NULL;
            ATOMIX_ZALLOC_ALIGNED((void **)&new_align, bytes);
            if (!new_align) {
                thread_atomic_int_store(&mix->mixing, 0);
                return 0;
            }
            mix->align = new_align;
            mix->align_size = asize;
        } else if (mix->align_size < asize) {
            size_t old_bytes = mix->align_size * sizeof(ATOMIX_VEC_TYPE);
            size_t new_bytes = asize * sizeof(ATOMIX_VEC_TYPE);
            ATOMIX_VEC_TYPE* new_align = NULL;
            ATOMIX_ZALLOC_ALIGNED((void **)&new_align, new_bytes);
            if (!new_align) {
                thread_atomic_int_store(&mix->mixing, 0);
                return 0;
            }
            memcpy(new_align, mix->align, old_bytes);
            ATOMIX_ZFREE_ALIGNED(mix->align);
            mix->align = new_align;
            mix->align_size = asize;
        }
        //clear the aligned buffer
        memset(mix->align, 0, asize * sizeof(ATOMIX_VEC_TYPE));
        //process stream first, if present
        struct atomix_stream *strm = thread_atomic_ptr_load(&mix->stream);
        if (strm) {
            uint8_t flag = thread_atomic_int_load(&strm->flag);
            int vol_change = (flag & ATOMIX_VOL_CHANGE);
            flag &= ~ATOMIX_VOL_CHANGE;
            if (flag == ATOMIX_STOP) {
                // Don't fade out, just disconnect and free
                thread_atomic_ptr_store(&mix->stream, NULL);
                atomixStreamFree(strm);
            }
            else {
                if (flag > ATOMIX_STOP && vol_change) {
                    // Streams with a volume callback generate samples on demand and
                    // can alter their volume before copying into the destination buffer. For
                    // formats like Ogg, WAV, etc, the decoders simply copy over the PCM samples 
                    // at their "full" gain and thus a volume callback is useless. In those cases,
                    // atomix will apply gain after the samples are rendered.
                    if (strm->callbacks.volume) {
                        strm->callbacks.volume(strm->userdata, thread_atomic_int_load(&strm->gain) * 0.01f);
                    }
                }
                if (flag > ATOMIX_HALT) {
                    uint32_t frames_to_render = asize * 2;
                    if (strm->samplerate == 0 || strm->samplerate == mix->samplerate) {
                        if (strm->callbacks.volume) {
                            strm->callbacks.render(strm->userdata, (float *)mix->align, frames_to_render);
                        } else {
                            float* buff = (float*)mix->align;
                            strm->callbacks.render(strm->userdata, buff, frames_to_render);
                            ATOMIX_VEC_TYPE gvec = ATOMIX_VEC_SET_FLOAT((float)thread_atomic_int_load(&strm->gain) * 0.01f);
                            for (uint32_t vi = 0; vi < asize; ++vi) {
                                mix->align[vi] = ATOMIX_VEC_MULTIPLY(mix->align[vi], gvec);
                            }
                        }
                    } else {
                        atmxResampleStream(mix, strm, (float*)mix->align, frames_to_render);
                    }
                }
                //clear flag
                if (vol_change) thread_atomic_int_store(&strm->flag, flag);
            }
        }
        //begin sound layer mixing, caching the volume first
        ATOMIX_VEC_TYPE vol = ATOMIX_VEC_SET_FLOAT((float)thread_atomic_int_load(&mix->volume) * 0.01f);
        for (int i = 0; i < ATMX_LAYERS; i++) atmxMixLayer(&mix->lays[i], vol, mix->align, asize);
        //perform clipping using SIMD min and max (unless disabled)
        #ifndef ATOMIX_NO_CLIP
            ATOMIX_VEC_TYPE neg1 = ATOMIX_VEC_SET_FLOAT(-1.0f), pos1 = ATOMIX_VEC_SET_FLOAT(1.0f);
            for (uint32_t i = 0; i < asize; i++) mix->align[i] = ATOMIX_VEC_MIN_FLOAT(ATOMIX_VEC_MAX_FLOAT(mix->align[i], neg1), pos1);
        #endif
        //copy rnum frames, leaving possible remainder
        memcpy(buff, mix->align, rnum*2*sizeof(float));
        //determine remaining number of frames
        mix->rem = asize*2 - rnum;
        //copy remaining frames to buffer inside the mixer struct
        if (mix->rem) memcpy(mix->data, (float*)mix->align + rnum*2, mix->rem);
    #else
        //clear the output buffer using memset
        memset(buff, 0, fnum*2*sizeof(float));
        //process stream first, if present
        struct atomix_stream *strm = thread_atomic_ptr_load(&mix->stream);
        if (strm) {
            uint8_t flag = thread_atomic_int_load(&strm->flag);
            int vol_change = (flag & ATOMIX_VOL_CHANGE);
            flag &= ~ATOMIX_VOL_CHANGE;
            if (flag == ATOMIX_STOP) {
                thread_atomic_ptr_store(&mix->stream, NULL);
                thread_atomic_int_store(&strm->flag, ATOMIX_FREE);
            }
            else {
                if (flag > ATOMIX_STOP && vol_change) {
                    if (strm->callbacks.volume) {
                        strm->callbacks.volume(strm->userdata, thread_atomic_int_load(&strm->gain) * 0.01f);
                    }
                }
                if (flag > ATOMIX_HALT) {
                    if (strm->samplerate == 0 || strm->samplerate == mix->samplerate) {
                        if (strm->callbacks.volume) {
                            strm->callbacks.render(strm->userdata, buff, fnum);
                        } else { // need to apply gain while rendering
                            float gain = (float)thread_atomic_int_load(&strm->gain) * 0.01f;
                            strm->callbacks.render(strm->userdata, buff, fnum);
                            for (uint32_t i = 0; i < fnum * 2; ++i) {
                                buff[i] *= gain;
                            }
                        }
                    } else {
                        atmxResampleStream(mix, strm, buff, fnum);
                    }
                }
            }
            //clear flag
            if (vol_change) thread_atomic_int_store(&strm->flag, flag);
        }
        //begin sound layer mixing, caching the volume first
        float vol = (float)thread_atomic_int_load(&mix->volume) * 0.01f;
        for (int i = 0; i < ATMX_LAYERS; i++) atmxMixLayer(&mix->lays[i], vol, buff, fnum);
        //perform clipping using simple ternary operators (unless disabled)
        #ifndef ATOMIX_NO_CLIP
            for (uint32_t i = 0; i < fnum*2; i++) buff[i] = (buff[i] < -1.0f) ? -1.0f : (buff[i] > 1.0f) ? 1.0f : buff[i];
        #endif
    #endif
    thread_atomic_int_store(&mix->mixing, 0);
    //return
    return fnum;
}
ATMXDEF uint32_t atomixMixerPlaySound (struct atomix_mixer* mix, struct atomix_sound* snd, uint8_t flag, float gain, float pan) {
    //play with start and end equal to start and end of the sound itself
    return atomixMixerPlaySoundAdv(mix, snd, flag, gain, pan, 0, snd->len, mix->fade);
}
ATMXDEF uint32_t atomixMixerPlaySoundAdv (struct atomix_mixer* mix, struct atomix_sound* snd, uint8_t flag, float gain, float pan, int32_t start, int32_t end, int32_t fade) {
    //return failure if pointers invalid
    if ((!mix)||(!snd)) return 0;
    //return failure if given flag invalid
    if ((flag < ATOMIX_STOP)||(flag > ATOMIX_LOOP)) return 0;
    //return failure if start or end invalid
    if ((end - start < 4)||(end < 4)) return 0;
    //make ATMX_LAYERS attempts to find layer
    for (int i = 0; i < ATMX_LAYERS; i++) {
        //get layer for next sound handle id
        uint32_t id; struct atmx_layer* lay = &mix->lays[(id = mix->nid++) & ATMX_LMASK];
        //check if corresponding layer is free
        if (thread_atomic_int_load(&lay->flag) == ATOMIX_FREE) {
            //skip 0 as it is special
            if (!id) id = ATMX_LAYERS;
            //fill in non-atomic layer data along with truncating start and end
            lay->id = id; lay->snd = snd;
            lay->start = start & ~3; lay->end = end & ~3;
            lay->fmax = (fade < 0) ? 0 : fade & ~3;
            //set initial fade state based on flag
            lay->fade = (flag < ATOMIX_PLAY) ? 0 : lay->fmax;
            //convert gain and pan to left and right gain and store it atomically
            atmxGainf2(&lay->gain_l, &lay->gain_r, gain, pan);
            //atomically set cursor to start position based on given argument
            thread_atomic_int_store(&lay->cursor, lay->start);
            //increment sound reference count
            thread_atomic_int_inc(&snd->ref);
            //store flag last, releasing the layer to the mixer thread
            thread_atomic_int_store(&lay->flag, flag);
            //return success
            return id;
        }
    }
    //return failure
    return 0;
}
ATMXDEF uint32_t atomixMixerPlayStream(struct atomix_mixer* mix, struct atomix_stream* stream, uint8_t flag, float gain) {
    if (!mix || !stream || (flag > ATOMIX_LOOP) || (flag < ATOMIX_STOP)) return 0;
    gain = (gain < 0.0f) ? 0.0f : (gain > 1.0f) ? 1.0f : gain;
    if (mix->samplerate != stream->samplerate) {
        //clear FIR params if necessary
        memset(mix->strm_fir_hist_l, 0, ATMX_FIR_TAPS * sizeof(float));
        memset(mix->strm_fir_hist_r, 0, ATMX_FIR_TAPS * sizeof(float));
    }
    thread_atomic_int_store(&stream->flag, (flag | ATOMIX_VOL_CHANGE)); //trigger initial volume callback
    thread_atomic_int_store(&stream->gain, (int)(gain * 100));
    thread_atomic_ptr_swap(&mix->stream, stream);
    ++mix->sid;
    return mix->sid;
}
ATMXDEF int atomixMixerSetSoundGainPan (struct atomix_mixer* mix, uint32_t id, float gain, float pan) {
    if (!mix) return 0;
    //get layer based on the lowest bits of id
    struct atmx_layer* lay = &mix->lays[id & ATMX_LMASK];
    //check id and state flag to make sure the id is valid
    if ((id == lay->id)&&(thread_atomic_int_load(&lay->flag) > ATOMIX_STOP)) {
        //convert gain and pan to left and right gain and store it atomically
        atmxGainf2(&lay->gain_l, &lay->gain_r, gain, pan);
        //return success
        return 1;
    }
    //return failure
    return 0;
}
ATMXDEF int atomixMixerSetStreamGain (struct atomix_mixer* mix, uint32_t id, float gain) {
    if (!mix || mix->sid != id) return 0;
    struct atomix_stream *strm = thread_atomic_ptr_load(&mix->stream);
    if (!strm) return 0;
    uint8_t flag = thread_atomic_int_load(&strm->flag);
    if (flag > ATOMIX_STOP) {
        //convert gain and store it atomically
        thread_atomic_int_store(&strm->gain, (int)(gain * 100));
        thread_atomic_int_store(&strm->flag, (flag | ATOMIX_VOL_CHANGE));
        //return success
        return 1;
    }
    //return failure
    return 0;
}
ATMXDEF int atomixMixerSetCursor (struct atomix_mixer* mix, uint32_t id, int32_t cursor) {
    if (!mix) return 0;
    //get layer based on the lowest bits of id
    struct atmx_layer* lay = &mix->lays[id & ATMX_LMASK];
    //check id and state flag to make sure the id is valid
    if ((id == lay->id)&&(thread_atomic_int_load(&lay->flag) > ATOMIX_STOP)) {
        //clamp cursor and truncate to multiple of 4 before storing
        thread_atomic_int_store(&lay->cursor, (cursor < lay->start) ? lay->start : (cursor > lay->end) ? lay->end : cursor & ~3);
        //return success
        return 1;
    }
    //return failure
    return 0;
}
ATMXDEF int atomixMixerGetSoundState (struct atomix_mixer* mix, uint32_t id) {
    if (!mix) return -1;
    //get layer based on the lowest bits of id
    struct atmx_layer* lay = &mix->lays[id & ATMX_LMASK]; uint8_t state;
    //check id and state flag to make sure the id is valid
    if (id == lay->id) {
        //return state if successfully retrieved
        state = thread_atomic_int_load(&lay->flag);
        return state;
    }
    //return failure
    return -1;
}
ATMXDEF int atomixMixerGetStreamState (struct atomix_mixer* mix, uint32_t id) {
    if (!mix || !id || mix->sid != id) return -1;
    struct atomix_stream *strm = thread_atomic_ptr_load(&mix->stream);
    if (!strm) return -1;
    int flag = thread_atomic_int_load(&strm->flag);
    flag &= ~ATOMIX_VOL_CHANGE; // this is internal, don't report it    
    return flag;
}
ATMXDEF int atomixMixerSetSoundState (struct atomix_mixer* mix, uint32_t id, uint8_t flag) {
    if (!mix) return 0;
    //return failure if given flag invalid
    if ((flag < ATOMIX_STOP)||(flag > ATOMIX_HALT)) return 0;
    //get layer based on the lowest bits of id
    struct atmx_layer* lay = &mix->lays[id & ATMX_LMASK]; uint8_t prev;
    //check id and state flag to make sure the id is valid
    if ((id == lay->id)&&((prev = thread_atomic_int_load(&lay->flag)) > ATOMIX_FREE)) {
        //return success if already in desired state
        if (prev == flag) return 1;
        //swap if flag has not changed and return if successful
        if (thread_atomic_int_compare_and_swap(&lay->flag, prev, flag) == prev) return 1;
    }
    //return failure
    return 0;
}
ATMXDEF int atomixMixerSetStreamState (struct atomix_mixer* mix, uint32_t id, uint8_t flag) {
    //return failure if params invalid
    if (!mix || !id || mix->sid != id) return 0;
    //return failure if given flag invalid
    if ((flag < ATOMIX_STOP)||(flag > ATOMIX_LOOP)) return 0;
    struct atomix_stream *strm = thread_atomic_ptr_load(&mix->stream);
    if (!strm) return 0;
    int old_flag = thread_atomic_int_load(&strm->flag);
    if (old_flag & ATOMIX_VOL_CHANGE) flag |= ATOMIX_VOL_CHANGE;
    if ((old_flag & ~ATOMIX_VOL_CHANGE) > ATOMIX_FREE) {
        //return success if already in desired state
        if (old_flag == flag) return 1;
        //swap if flag has not changed and return if successful
        if (thread_atomic_int_compare_and_swap(&strm->flag, old_flag, flag) == old_flag) return 1;
    }
    //return failure
    return 0;
}
ATMXDEF int atomixMixerGetActive (struct atomix_mixer* mix) {
    if (!mix) return -1;
    int active = 0;
    //go through all active layers and increment for any valid state
    //this does include ATOMIX_STOP, as sounds that are fading out
    //are set to this
    for (int i = 0; i < ATMX_LAYERS; i++) {
        //pointer to this layer for cleaner code
        struct atmx_layer* lay = &mix->lays[i];
        //check if active and increment counter if so
        if (thread_atomic_int_load(&lay->flag) > ATOMIX_FREE) active++;
    }
    return active;
}
ATMXDEF void atomixMixerVolume (struct atomix_mixer* mix, float vol) {
    //simple atomic store of the volume
    if (mix)
        thread_atomic_int_store(&mix->volume, (int32_t)vol);
}
ATMXDEF void atomixMixerFade (struct atomix_mixer* mix, int32_t fade) {
    //simple assignment of the fade value
    if (mix)
        mix->fade = (fade < 0) ? 0 : fade & ~3;
}
ATMXDEF void atomixMixerStopAll (struct atomix_mixer* mix) {
    if (mix) {
        //go through all active layers and set their states to the stop state
        for (int i = 0; i < ATMX_LAYERS; i++) {
            //pointer to this layer for cleaner code
            struct atmx_layer* lay = &mix->lays[i];
            //check if active and set to stop if true
            if (thread_atomic_int_load(&lay->flag) > ATOMIX_STOP) thread_atomic_int_store(&lay->flag, (uint8_t)ATOMIX_STOP);
        }
    }
}
ATMXDEF void atomixMixerHaltAll (struct atomix_mixer* mix) {
    //go through all playing layers and set their states to halt
    if (mix) {
        for (int i = 0; i < ATMX_LAYERS; i++) {
            //pointer to this layer for cleaner code
            struct atmx_layer* lay = &mix->lays[i]; uint8_t flag;
            //check if playing or looping and halt
            if ((flag = thread_atomic_int_load(&lay->flag)) > ATOMIX_HALT) thread_atomic_int_store(&lay->flag, (uint8_t)ATOMIX_HALT);
        }
    }
}
ATMXDEF void atomixMixerPlayAll (struct atomix_mixer* mix) {
    //go through all halted layers and set their states to play
    if (mix) {
        for (int i = 0; i < ATMX_LAYERS; i++) {
            //pointer to this layer for cleaner code
            struct atmx_layer* lay = &mix->lays[i]; uint8_t flag;
            //swap the flag to play if it is on halt
            if ((flag = thread_atomic_int_load(&lay->flag)) == ATOMIX_HALT) thread_atomic_int_store(&lay->flag, (uint8_t)ATOMIX_PLAY);
        }
    }
}

//internal functions
#ifndef ATOMIX_NO_SIMD
static void atmxMixLayer (struct atmx_layer* lay, ATOMIX_VEC_TYPE vol, ATOMIX_VEC_TYPE* align, uint32_t asize) {
    if ((!lay)||(!align)) return;
    //load flag value atomically first
    uint8_t flag = thread_atomic_int_load(&lay->flag);
    //return if flag cleared
    if (flag == ATOMIX_FREE) return;
    //atomically load cursor
    int32_t cur = thread_atomic_int_load(&lay->cursor);
    //atomically load left and right gain
    float gain_l = (float)thread_atomic_int_load(&lay->gain_l) * 0.01f;
    float gain_r = (float)thread_atomic_int_load(&lay->gain_r) * 0.01f;
    ATOMIX_VEC_TYPE gmul = ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_CREATE(gain_l, gain_r, gain_l, gain_r), vol);
    //action based on flag
    if (flag < ATOMIX_PLAY) {
        //ATOMIX_STOP or ATOMIX_HALT, fade out if not faded or at end
        if ((lay->fade > 0)&&(cur < lay->end))
        {
            if (lay->snd->cha == 1)
                cur = atmxMixFadeMono(lay, cur, gmul, align, asize);
            else
                cur = atmxMixFadeStereo(lay, cur, gmul, align, asize);
        }
        //clear flag if ATOMIX_STOP and fully faded or at end
        if ((flag == ATOMIX_STOP)&&((lay->fade == 0)||(cur == lay->end)))
        {
            thread_atomic_int_store(&lay->flag, (uint8_t)ATOMIX_FREE);
            thread_atomic_int_dec(&lay->snd->ref);
        }
    } else {
        //ATOMIX_PLAY or ATOMIX_LOOP, play including fade in
        if (lay->snd->cha == 1)
            cur = atmxMixPlayMono(lay, (flag == ATOMIX_LOOP), cur, gmul, align, asize);
        else
            cur = atmxMixPlayStereo(lay, (flag == ATOMIX_LOOP), cur, gmul, align, asize);
        //clear flag if ATOMIX_PLAY and the cursor has reached the end
        if ((flag == ATOMIX_PLAY)&&(cur == lay->end))
        {
            thread_atomic_int_store(&lay->flag, (uint8_t)ATOMIX_FREE);
            thread_atomic_int_dec(&lay->snd->ref);
        }
    }
}
static int32_t atmxMixFadeMono (struct atmx_layer* lay, int32_t cur, ATOMIX_VEC_TYPE gmul, ATOMIX_VEC_TYPE* align, uint32_t asize) {
    //cache cursor
    int32_t old = cur;
    if ((!lay)||(!align)) return old;
    //check if enough samples left for fade out
    if (lay->fade < lay->end - cur) {
        //perform fade out
        for (uint32_t i = 0; i < asize; i += 2) {
            //quit if fully faded out
            if (lay->fade == 0) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //get faded volume multiplier
                ATOMIX_VEC_TYPE fmul = ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_SET_FLOAT((float)lay->fade/(float)lay->fmax), gmul);
                //load 4 samples from data (this is 4 frames)
                ATOMIX_VEC_TYPE sam = lay->snd->data[(cur % lay->snd->len) >> 2];
                //mix low samples obtained with unpacklo
                align[i] = ATOMIX_VEC_ADD(align[i], ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_UNPACK_LO(sam, sam), fmul));
                //mix high samples obtained with unpackhi
                align[i+1] = ATOMIX_VEC_ADD(align[i+1], ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_UNPACK_HI(sam, sam), fmul));
            }
            //advance cursor and fade
            lay->fade -= 4; cur += 4;
        }
    } else {
        //continue playback to end without fade out
        for (uint32_t i = 0; i < asize; i += 2) {
            //quit if cursor at end
            if (cur == lay->end) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //load 4 samples from data (this is 4 frames)
                ATOMIX_VEC_TYPE sam = lay->snd->data[(cur % lay->snd->len) >> 2];
                //mix low samples obtained with unpacklo
                align[i] = ATOMIX_VEC_ADD(align[i], ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_UNPACK_LO(sam, sam), gmul));
                //mix high samples obtained with unpackhi
                align[i+1] = ATOMIX_VEC_ADD(align[i+1], ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_UNPACK_HI(sam, sam), gmul));
            }
            //advance cursor
            cur += 4;
        }
    }
    //swap back cursor if unchanged
    if (thread_atomic_int_compare_and_swap(&lay->cursor, old, cur) != old) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixFadeStereo (struct atmx_layer* lay, int32_t cur, ATOMIX_VEC_TYPE gmul, ATOMIX_VEC_TYPE* align, uint32_t asize) {
    //cache cursor
    int32_t old = cur;
    if ((!lay)||(!align)) return old;
    //check if enough samples left for fade out
    if (lay->fade < lay->end - cur) {
        //perform fade out
        for (uint32_t i = 0; i < asize; i += 2) {
            //quit if fully faded out
            if (lay->fade == 0) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //get faded volume multiplier
                ATOMIX_VEC_TYPE fmul = ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_SET_FLOAT((float)lay->fade/(float)lay->fmax), gmul);
                //mod for repeating and convert to ATOMIX_VEC_TYPE offset
                int32_t off = (cur % lay->snd->len) >> 1;
                //mix in first two frames
                align[i] = ATOMIX_VEC_ADD(align[i], ATOMIX_VEC_MULTIPLY(lay->snd->data[off], fmul));
                //mix in second two frames
                align[i+1] = ATOMIX_VEC_ADD(align[i+1], ATOMIX_VEC_MULTIPLY(lay->snd->data[off+1], fmul));
            }
            //advance cursor and fade
            lay->fade -= 4; cur += 4;
        }
    } else {
        //continue playback to end without fade out
        for (uint32_t i = 0; i < asize; i += 2) {
            //quit if cursor at end
            if (cur == lay->end) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //mod for repeating and convert to ATOMIX_VEC_TYPE offset
                int32_t off = (cur % lay->snd->len) >> 1;
                //mix in first two frames
                align[i] = ATOMIX_VEC_ADD(align[i], ATOMIX_VEC_MULTIPLY(lay->snd->data[off], gmul));
                //mix in second two frames
                align[i+1] = ATOMIX_VEC_ADD(align[i+1], ATOMIX_VEC_MULTIPLY(lay->snd->data[off+1], gmul));
            }
            //advance cursor
            cur += 4;
        }
    }
    //swap back cursor if unchanged
    if (thread_atomic_int_compare_and_swap(&lay->cursor, old, cur) != old) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixPlayMono (struct atmx_layer* lay, int loop, int32_t cur, ATOMIX_VEC_TYPE gmul, ATOMIX_VEC_TYPE* align, uint32_t asize) {
    //cache cursor
    int32_t old = cur;
    if ((!lay)||(!align)) return old;
    //check if fully faded in yet
    if (lay->fade < lay->fmax) {
        //perform fade in
        for (uint32_t i = 0; i < asize; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //get faded volume multiplier
                ATOMIX_VEC_TYPE fmul = ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_SET_FLOAT((float)lay->fade/(float)lay->fmax), gmul);
                //load 4 samples from data (this is 4 frames)
                ATOMIX_VEC_TYPE sam = lay->snd->data[(cur % lay->snd->len) >> 2];
                //mix low samples obtained with unpacklo
                align[i] = ATOMIX_VEC_ADD(align[i], ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_UNPACK_LO(sam, sam), fmul));
                //mix high samples obtained with unpackhi
                align[i+1] = ATOMIX_VEC_ADD(align[i+1], ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_UNPACK_HI(sam, sam), fmul));
            }
            //advance fade unless fully faded in
            if (lay->fade < lay->fmax) lay->fade += 4;
            //advance cursor
            cur += 4;
        }
    } else {
        //regular playback
        for (uint32_t i = 0; i < asize; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //load 4 samples from data (this is 4 frames)
                ATOMIX_VEC_TYPE sam = lay->snd->data[(cur % lay->snd->len) >> 2];
                //mix low samples obtained with unpacklo
                align[i] = ATOMIX_VEC_ADD(align[i], ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_UNPACK_LO(sam, sam), gmul));
                //mix high samples obtained with unpackhi
                align[i+1] = ATOMIX_VEC_ADD(align[i+1], ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_UNPACK_HI(sam, sam), gmul));
            }
            //advance cursor
            cur += 4;
        }
    }
    //swap back cursor if unchanged
    if (thread_atomic_int_compare_and_swap(&lay->cursor, old, cur) != old) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixPlayStereo (struct atmx_layer* lay, int loop, int32_t cur, ATOMIX_VEC_TYPE gmul, ATOMIX_VEC_TYPE* align, uint32_t asize) {
    //cache cursor
    int32_t old = cur;
    if ((!lay)||(!align)) return old;
    //check if fully faded in yet
    if (lay->fade < lay->fmax) {
        //perform fade in
        for (uint32_t i = 0; i < asize; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //get faded volume multiplier
                ATOMIX_VEC_TYPE fmul = ATOMIX_VEC_MULTIPLY(ATOMIX_VEC_SET_FLOAT((float)lay->fade/(float)lay->fmax), gmul);
                //mod for repeating and convert to ATOMIX_VEC_TYPE offset
                int32_t off = (cur % lay->snd->len) >> 1;
                //mix in first two frames
                align[i] = ATOMIX_VEC_ADD(align[i], ATOMIX_VEC_MULTIPLY(lay->snd->data[off], fmul));
                //mix in second two frames
                align[i+1] = ATOMIX_VEC_ADD(align[i+1], ATOMIX_VEC_MULTIPLY(lay->snd->data[off+1], fmul));
            }
            //advance fade unless fully faded in
            if (lay->fade < lay->fmax) lay->fade += 4;
            //advance cursor
            cur += 4;
        }
    } else {
        //regular playback
        for (uint32_t i = 0; i < asize; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //mod for repeating and convert to ATOMIX_VEC_TYPE offset
                int32_t off = (cur % lay->snd->len) >> 1;
                //mix in first two frames
                align[i] = ATOMIX_VEC_ADD(align[i], ATOMIX_VEC_MULTIPLY(lay->snd->data[off], gmul));
                //mix in second two frames
                align[i+1] = ATOMIX_VEC_ADD(align[i+1], ATOMIX_VEC_MULTIPLY(lay->snd->data[off+1], gmul));
            }
            //advance cursor
            cur += 4;
        }
    }
    //swap back cursor if unchanged
    if (thread_atomic_int_compare_and_swap(&lay->cursor, old, cur) != old) cur = old;
    //return new cursor
    return cur;
}
#else
static void atmxMixLayer (struct atmx_layer* lay, float vol, float* buff, uint32_t fnum) {
    if ((!lay)||(!buff)) return;
    //load flag value atomically first
    uint8_t flag = thread_atomic_int_load(&lay->flag);
    //return if flag cleared
    if (flag == ATOMIX_FREE) return;
    //atomically load cursor
    int32_t cur = thread_atomic_int_load(&lay->cursor);
    //atomically load left and right gain
    float gain_l = (float)thread_atomic_int_load(&lay->gain_l) * 0.01f;
    float gain_r = (float)thread_atomic_int_load(&lay->gain_r) * 0.01f;
    //multiply volume into gain
    gain_l *= vol; gain_r *= vol;
    //action based on flag
    if (flag < ATOMIX_PLAY) {
        //ATOMIX_STOP or ATOMIX_HALT, fade out if not faded or at end
        if ((lay->fade > 0)&&(cur < lay->end)) 
            if (lay->snd->cha == 1)
                cur = atmxMixFadeMono(lay, cur, gain_l, gain_r, buff, fnum);
            else
                cur = atmxMixFadeStereo(lay, cur, gain_l, gain_r, buff, fnum);
        //clear flag if ATOMIX_STOP and fully faded or at end
        if ((flag == ATOMIX_STOP)&&((lay->fade == 0)||(cur == lay->end))) {
            thread_atomic_int_store(&lay->flag, (uint8_t)ATOMIX_FREE);
            thread_atomic_int_dec(&lay->snd->ref);
        }
    } else {
        //ATOMIX_PLAY or ATOMIX_LOOP, play including fade in
        if (lay->snd->cha == 1)
            cur = atmxMixPlayMono(lay, (flag == ATOMIX_LOOP), cur, gain_l, gain_r, buff, fnum);
        else
            cur = atmxMixPlayStereo(lay, (flag == ATOMIX_LOOP), cur, gain_l, gain_r, buff, fnum);
        //clear flag if ATOMIX_PLAY and the cursor has reached the end
        if ((flag == ATOMIX_PLAY)&&(cur == lay->end)) {
            thread_atomic_int_store(&lay->flag, (uint8_t)ATOMIX_FREE);
            thread_atomic_int_dec(&lay->snd->ref);
        }
    }
}
static int32_t atmxMixFadeMono (struct atmx_layer* lay, int32_t cur, float gain_l, float gain_r, float* buff, uint32_t fnum) {
    //cache cursor
    int32_t old = cur;
    if ((!lay)||(!buff)) return old;
    //check if enough samples left for fade out
    if (lay->fade < lay->end - cur) {
        //perform fade out
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //quit if fully faded out
            if (lay->fade == 0) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //get fade multiplier
                float fade = (float)lay->fade/(float)lay->fmax;
                //load 1 sample from data (this is 1 frame)
                float sam = lay->snd->data[cur % lay->snd->len];
                //mix left sample of frame
                buff[i] += sam*fade*gain_l;
                //mix right sample of frame
                buff[i+1] += sam*fade*gain_r;
            }
            //advance cursor and fade
            lay->fade--; cur++;
        }
    } else {
        //continue playback to end without fade out
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //quit if cursor at end
            if (cur == lay->end) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //load 1 sample from data (this is 1 frame)
                float sam = lay->snd->data[cur % lay->snd->len];
                //mix left sample of frame
                buff[i] += sam*gain_l;
                //mix right sample of frame
                buff[i+1] += sam*gain_r;
            }
            //advance cursor
            cur++;
        }
    }
    //swap back cursor if unchanged
    if (thread_atomic_int_compare_and_swap(&lay->cursor, old, cur) != old) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixFadeStereo (struct atmx_layer* lay, int32_t cur, float gain_l, float gain_r, float* buff, uint32_t fnum) {
    //cache cursor
    int32_t old = cur;
    if ((!lay)||(!buff)) return old;
    //check if enough samples left for fade out
    if (lay->fade < lay->end - cur) {
        //perform fade out
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //quit if fully faded out
            if (lay->fade == 0) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //get fade multiplier
                float fade = (float)lay->fade/(float)lay->fmax;
                //mod for repeating and convert to float offset
                int32_t off = (cur % lay->snd->len) << 1;
                //mix left sample of frame
                buff[i] += lay->snd->data[off]*fade*gain_l;
                //mix right sample of frame
                buff[i+1] += lay->snd->data[off+1]*fade*gain_r;
            }
            //advance cursor and fade
            lay->fade--; cur++;
        }
    } else {
        //continue playback to end without fade out
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //quit if cursor at end
            if (cur == lay->end) break;
            //mix if cursor within sound
            if (cur >= 0) {
                //mod for repeating and convert to float offset
                int32_t off = (cur % lay->snd->len) << 1;
                //mix left sample of frame
                buff[i] += lay->snd->data[off]*gain_l;
                //mix right sample of frame
                buff[i+1] += lay->snd->data[off+1]*gain_r;
            }
            //advance cursor
            cur++;
        }
    }
    //swap back cursor if unchanged
    if (thread_atomic_int_compare_and_swap(&lay->cursor, old, cur) != old) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixPlayMono (struct atmx_layer* lay, int loop, int32_t cur, float gain_l, float gain_r, float* buff, uint32_t fnum) {
    //cache cursor
    int32_t old = cur;
    if ((!lay)||(!buff)) return old;
    //check if fully faded in yet
    if (lay->fade < lay->fmax) {
        //perform fade in
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //get fade multiplier
                float fade = (float)lay->fade/(float)lay->fmax;
                //load 1 sample from data (this is 1 frame)
                float sam = lay->snd->data[cur % lay->snd->len];
                //mix left sample of frame
                buff[i] += sam*fade*gain_l;
                //mix right sample of frame
                buff[i+1] += sam*fade*gain_r;
            }
            //advance fade unless fully faded in
            if (lay->fade < lay->fmax) lay->fade++;
            //advance cursor
            cur++;
        }
    } else {
        //regular playback
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //load 1 sample from data (this is 1 frame)
                float sam = lay->snd->data[cur % lay->snd->len];
                //mix left sample of frame
                buff[i] += sam*gain_l;
                //mix right sample of frame
                buff[i+1] += sam*gain_r;
            }
            //advance cursor
            cur++;
        }
    }
    //swap back cursor if unchanged
    if (thread_atomic_int_compare_and_swap(&lay->cursor, old, cur) != old) cur = old;
    //return new cursor
    return cur;
}
static int32_t atmxMixPlayStereo (struct atmx_layer* lay, int loop, int32_t cur, float gain_l, float gain_r, float* buff, uint32_t fnum) {
    //cache cursor
    int32_t old = cur;
    if ((!lay)||(!buff)) return old;
    //check if fully faded in yet
    if (lay->fade < lay->fmax) {
        //perform fade in
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //get fade multiplier
                float fade = (float)lay->fade/(float)lay->fmax;
                //mod for repeating and convert to float offset
                int32_t off = (cur % lay->snd->len) << 1;
                //mix left sample of frame
                buff[i] += lay->snd->data[off]*fade*gain_l;
                //mix right sample of frame
                buff[i+1] += lay->snd->data[off+1]*fade*gain_r;
            }
            //advance fade unless fully faded in
            if (lay->fade < lay->fmax) lay->fade++;
            //advance cursor
            cur++;
        }
    } else {
        //regular playback
        for (uint32_t i = 0; i < fnum*2; i += 2) {
            //check if cursor at end
            if (cur == lay->end) {
                //quit unless looping
                if (!loop) break;
                //wrap around if looping
                cur = lay->start;
            }
            //mix if cursor within sound
            if (cur >= 0) {
                //mod for repeating and convert to float offset
                int32_t off = (cur % lay->snd->len) << 1;
                //mix left sample of frame
                buff[i] += lay->snd->data[off]*gain_l;
                //mix right sample of frame
                buff[i+1] += lay->snd->data[off+1]*gain_r;
            }
            //advance cursor
            cur++;
        }
    }
    //swap back cursor if unchanged
    if (thread_atomic_int_compare_and_swap(&lay->cursor, old, cur) != old) cur = old;
    //return new cursor
    return cur;
}
#endif

#endif //ATOMIX_IMPLEMENTATION