#pragma once

#include <cstdint>

// IPC pipe name (overlay <-> driver). The path namespace is required.
#define FINGERSMOOTHING_PIPE_NAME "\\\\.\\pipe\\FingerSmoothing"

// Shared memory name (driver -> overlay). The driver writes recent
// curl/splay samples (raw + smoothed) into this segment so the overlay
// can render a live visualization without round-tripping through the pipe
// every frame. Reserved for M4 visualization work.
#define FINGERSMOOTHING_SHMEM_NAME "FingerSmoothing_LiveBuf_v1"

namespace protocol
{
    // Bumped on any wire-format-affecting change. The overlay handshakes against
    // this on connect and refuses to attach to a mismatched driver.
    constexpr uint32_t Version = 1;

    // Per-finger enable mask. Bit layout (LSB first):
    //   0  left thumb
    //   1  left index
    //   2  left middle
    //   3  left ring
    //   4  left pinky
    //   5  right thumb
    //   6  right index
    //   7  right middle
    //   8  right ring
    //   9  right pinky
    constexpr uint16_t kAllFingersMask = 0x03FF;

    // POD config payload sent overlay -> driver. Plain values + flags so the
    // struct is trivially memcpy-safe across the pipe with no marshalling.
    struct SmoothingConfig
    {
        // Master kill switch. When false the driver passes incoming bone arrays
        // through untouched — no derive/smooth/reconstruct work runs.
        bool     master_enabled;

        // One-Euro filter parameters (Casiez et al., 2012). Tunable via UI.
        //   mincutoff: low-frequency cutoff applied when the signal is stationary
        //              (heavier smoothing at lower values).
        //   beta:      slope by which cutoff rises with measured velocity (higher
        //              values let rapid gestures pass through more freely).
        //   dcutoff:   cutoff applied to the velocity estimate itself.
        float    mincutoff;
        float    beta;
        float    dcutoff;

        // Per-finger enable bits. Disabled fingers pass through unsmoothed —
        // useful when one finger's smoothing produces an artifact and the user
        // wants to isolate it without disabling everything.
        uint16_t finger_mask;

        // When true, the driver auto-tunes mincutoff per-scalar based on the
        // observed noise floor (variance during stationary periods). The UI's
        // mincutoff slider becomes a "baseline" the auto-tune is bounded by.
        bool     adaptive_enabled;
    };

    enum RequestType : uint32_t
    {
        RequestInvalid = 0,
        RequestHandshake = 1,
        RequestSetConfig = 2,
    };

    enum ResponseType : uint32_t
    {
        ResponseInvalid = 0,
        ResponseHandshake = 1,
        ResponseSuccess = 2,
    };

    struct Request
    {
        RequestType type;
        union
        {
            SmoothingConfig setConfig;
        };

        Request() : type(RequestInvalid) { }
        Request(RequestType type) : type(type) { }
    };

    struct Response
    {
        ResponseType type;
        union
        {
            struct { uint32_t version; } protocol;
        };

        Response() : type(ResponseInvalid) { }
        Response(ResponseType type) : type(type) { }
    };
}
