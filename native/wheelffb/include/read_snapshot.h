/* Optional copied-input bridge ABI. No device ownership, callbacks or force API.
 * Existing exports retain their behavior. Size/version must be initialized by
 * the caller; all reserved fields are zero. Raw buttons use DirectInput bit 7.
 * A failed snapshot is an observation, never neutral pedal data. */
#ifndef DBCE_WHEEL_READ_SNAPSHOT_H
#define DBCE_WHEEL_READ_SNAPSHOT_H
#include <stdint.h>
#pragma pack(push, 8)
typedef struct WheelReadDeviceInfoV1 {
    uint32_t struct_size, abi;
    uint64_t generation;
    uint8_t instance[16], product[16];
    uint32_t axis_mask, range_mask, pov_mask, flags; /* bit 0: shared FFB handle */
    uint8_t button_mask[16];
    int32_t minimum[8], maximum[8];
} WheelReadDeviceInfoV1;
typedef struct WheelReadSnapshotV1 {
    uint32_t struct_size, abi;
    uint64_t generation, sequence;
    int64_t read_start_us, read_end_us; /* QPC since system start; not module start */
    int32_t poll_hresult, state_hresult;
    uint8_t poll_call, state_call, valid, reserved;
    int32_t axes[8];
    uint8_t buttons[128];
    uint32_t pov[4], reserved2;
} WheelReadSnapshotV1;
#pragma pack(pop)
enum { WHEEL_READ_OK=1, WHEEL_READ_BAD_ARGUMENT=-1, WHEEL_READ_UNQUALIFIED=-2, WHEEL_READ_INJECTION_ACTIVE=-3 };
enum { WHEEL_READ_CALL_OK=0, WHEEL_READ_CALL_LOST=1, WHEEL_READ_CALL_GONE=2, WHEEL_READ_CALL_NOT_CALLED=3, WHEEL_READ_CALL_PENDING=4 };
/* GetReadDeviceInfoV1 qualifies/caches actual-open identity and object/range
 * readback, without Acquire/SetProperty. Call in the owner's idle preparation.
 * ReadDeviceSnapshotV1 requires this qualified generation, Polls/reads once,
 * never acquires/retries/injects, and returns OK for a completed observation
 * including invalid reads. A failed read ends the generation: refresh metadata
 * and perform normal owner-controlled recovery before accepting new samples.
 * An active legacy test-injection workload refuses this physical-only path. */
/* Optional GetReadDevicePathV1(slot, expectedGeneration, uint16_t* path, capacity)
 * queries DIPROP_GUIDANDPATH on the SAME already-qualified handle. Capacity is
 * UTF-16 code units including terminator; use 260. Returns the existing OK/BAD_
 * ARGUMENT/UNQUALIFIED codes; on failure a supplied nonempty buffer starts NUL.
 * No truncation, new device, Poll, Acquire, state read, or force API. Identity,
 * source generation and path must stay stable across the driver callback.
 * The V1 metadata and snapshot layouts remain 144/232 bytes. */
#endif
