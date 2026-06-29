# Adding a Custom HID Device to the FX20 UVC/UAC Firmware

This document explains step by step how to add a generic vendor-defined HID
device that periodically sends data from CPU SRAM over a USB HID interrupt IN
endpoint. It covers the architecture decisions, every file that was changed,
why it was changed, and the final data path.

## Table of Contents

1. [Design Decisions](#design-decisions)
2. [USB HID Fundamentals (for this device)](#usb-hid-fundamentals)
3. [File-by-File Changes](#file-by-file-changes)
4. [The Complete Data Path](#the-complete-data-path)
5. [How to Customize](#how-to-customize)
6. [Verification](#verification)

---

## Design Decisions

### USB Speed: HS (HighSpeed / USB 2.x) only

The data path for HS and SS (SuperSpeed / USB 3.x) is fundamentally different
in this architecture:

| | HS (USB 2.x) | SS (USB 3.x) |
|---|---|---|
| **DMA engine** | DataWire (DW1) | HBDMA (socket-to-socket) |
| **Send API** | `Cy_USB_AppQueueWrite()` | `Cy_HBDma_Channel_CommitBuffer()` |
| **Completion** | DataWire ISR per endpoint | HBDMA consume-event callback |
| **CPU involvement** | Program DataWire descriptor | Commit buffer metadata |

HS is simpler and well-suited for low-bandwidth HID transfers. SS would
require creating an HBDMA `MEM_TO_IP` channel like the UAC audio path uses.

### Endpoint and Interface Allocation

This device already uses:

| Endpoint | Interface | Purpose |
|---|---|---|
| 0x01 (IN, bulk) | 1 (VS) | UVC video streaming |
| 0x02 (IN, interrupt) | 0 (VC) | UVC control |
| 0x03 (IN, isochronous) | 3 (AS) | UAC audio streaming |

The HID device claims the next free numbers:

| Endpoint | Interface | Purpose |
|---|---|---|
| **0x04** (IN, interrupt) | **4** | HID input reports |

### HID Profile: Generic Vendor-Defined

A vendor-defined HID device (Usage Page `0xFF00`) was chosen over a standard
profile (keyboard, mouse, gamepad) because:

- No OS driver issues — recognized as a generic HID device automatically
- Arbitrary report size and format
- Can be read with standard HID APIs (`hidraw` on Linux, `HidDevice` on Windows)
- No boot protocol complexity

The report descriptor defines a single 64-byte input report:

```
Usage Page:  0xFF00 (Vendor-Defined)
Usage:       0x01   (Vendor-Defined)
Collection:  Application
  Report Size:   8 bits
  Report Count:  64
  Input:         Data, Variable, Absolute
End Collection
```

### Concurrency Model: Timer → Queue → Task

Following the pattern established by the FPS timer and UAC task:

```
FreeRTOS Timer (daemon task, priority 14)
    │
    │  xQueueSendFromISR(CY_USB_HID_SEND_REPORT)
    ▼
Message Queue (4 items)
    │
    │  xQueueReceive()
    ▼
HID Task (priority 5, stack 512 words)
    │
    │  Cy_USB_AppQueueWrite(HID_IN_ENDPOINT, ...)
    ▼
DataWire DMA → USBHS Endpoint FIFO → USB Host
```

**Why a timer + task instead of just a timer callback?** Timer callbacks run in
the timer daemon task at priority 14. They must not block. USB endpoint writes
involve hardware register accesses and potential DMA programming — work that
belongs in a task context.

**Why the queue?** The queue decouples the timer (which fires at a precise
interval) from the USB transfer (which may take variable time). If the
previous transfer hasn't completed, the task simply skips that tick.

### Why No HBDMA Channel

The UVC video path uses HBDMA because it moves megabytes per second (LVDS
PHY → USB PHY, zero CPU copy). HID reports are 64 bytes every 8 ms (~8 KB/s).
The DataWire DMA path (`Cy_USB_AppQueueWrite`) is more than sufficient and
avoids the complexity of HBDMA channel lifecycle management.

---

## USB HID Fundamentals

### What the Host Sees

When the device enumerates, the host sees a **composite device** with a new
HID interface. The descriptors tell the host:

1. **Interface descriptor**: "I am interface 4, class HID (0x03), with one
   interrupt IN endpoint."
2. **HID descriptor**: "Here is my HID spec version (1.11), and I have one
   report descriptor of length 14 bytes."
3. **Endpoint descriptor**: "My IN endpoint is 0x84, interrupt type, max
   packet 64 bytes, poll me every 8 ms."
4. **Report descriptor** (fetched via GET_DESCRIPTOR): "My report is 64 bytes
   of vendor-defined raw data."

### Standard HID Requests Handled

| Request | Response |
|---|---|
| `GET_DESCRIPTOR` (type=Report, 0x22) | Return the 14-byte report descriptor |
| `SET_IDLE` | Acknowledge (ignore duration) |
| `GET_IDLE` | Return 0 (indefinite) |
| `SET_PROTOCOL` | Acknowledge (no boot protocol) |
| `GET_PROTOCOL` | Return 0 (boot protocol) |

These are the minimum required for a generic HID device to function. `SET_IDLE`
controls how often the device sends reports when data hasn't changed — since
we always send periodic data, we ignore it.

---

## File-by-File Changes

### 1. `cy_usb_uvc_device.h` — Constants

**What was added:**

```c
/* HID interface and endpoint numbers */
#define HID_INTF_NUM               (0x04)
#define HID_IN_ENDPOINT            (0x04)
#define HID_REPORT_SIZE            (64)
#define HID_REPORT_DESC_SIZE       (14)
#define HID_POLLING_INTERVAL_MS    (8)

/* HID message type for the task queue */
#define CY_USB_HID_SEND_REPORT     (0x30)

/* HID descriptor type for GET_DESCRIPTOR */
#define CY_USB_HID_DESCR_TYPE_REPORT  (0x22)

/* HID class-specific request codes */
#define CY_USB_HID_GET_REPORT      (0x01)
#define CY_USB_HID_GET_IDLE        (0x02)
#define CY_USB_HID_SET_REPORT      (0x09)
#define CY_USB_HID_SET_IDLE        (0x0A)
#define CY_USB_HID_SET_PROTOCOL    (0x0B)
#define CY_USB_HID_GET_PROTOCOL    (0x03)
```

**Why:** Centralizes all HID configuration in one place. Changing report size,
polling interval, or endpoint numbers only requires editing this header.

### 2. `cy_usb_descriptors.c` — USB Descriptors

**What was changed:**

- Updated `wTotalLength` and `bNumInterfaces` in both
  `SuperSpeedConfigDescr[]` and `HighSpeedConfigDescr[]` to account for the
  new HID interface and endpoint. Nested `#if AUDIO_IF_EN` / `#if HID_EN`
  conditionals handle all four combinations.

- Inserted the HID descriptor block after the UAC section in both config
  arrays, guarded by `#if HID_EN`:

  ```
  Interface descriptor (9 bytes)
    bInterfaceClass    = 0x03 (HID)
    bInterfaceSubClass = 0x00
    bInterfaceProtocol = 0x00
    bNumEndpoints      = 1

  HID descriptor (9 bytes)
    bcdHID             = 0x0111 (spec version 1.11)
    bCountryCode       = 0x00
    bNumDescriptors    = 1
    bDescriptorType    = 0x22 (Report)
    wDescriptorLength  = HID_REPORT_DESC_SIZE (14)

  Endpoint descriptor (7 bytes)
    bEndpointAddress   = 0x84 (IN, endpoint 4)
    bmAttributes       = 0x03 (Interrupt)
    wMaxPacketSize     = 64
    bInterval          = 7 (2^(7-1) × 125 µs = 8 ms)

  SS Endpoint Companion (6 bytes, SS config only)
    bMaxBurst          = 1
    bmAttributes       = 0x00 (Mult = 1)
    wBytesPerInterval  = 64
  ```

**Why:** The USB host discovers devices through descriptors. Without the HID
descriptors in the configuration, the host wouldn't know the device has a HID
interface. Both SS and HS config arrays are updated because the device
advertises both speeds.

**Why the nested conditionals?** The `AUDIO_IF_EN` flag changes the total
descriptor length and interface count. `HID_EN` adds another variable. Rather
than hardcoding all four combinations, the preprocessor selects the correct
values. This keeps the code readable and makes it easy to enable/disable
either feature independently.

### 3. `cy_usb_app.h` — Application Context

**What was added:**

```c
#if HID_EN
    uint8_t  hidInEpNum;          /* HID IN endpoint number */
    bool     hidReportPending;    /* Guard against overlapping transfers */
    uint8_t  hidReportBuffer[HID_REPORT_SIZE];  /* Report data buffer */
    QueueHandle_t hidMsgQueue;    /* Message queue for HID task */
    TaskHandle_t hidTaskHandle;   /* HID task handle */
    TimerHandle_t hidTimer;       /* Periodic report timer */
#endif
```

Plus function declarations:

```c
void Cy_HID_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt);
void Cy_HID_InEpDma_ISR(void);
extern const uint8_t cy_hid_report_descriptor[];
```

**Why:** The application context (`cy_stc_usb_app_ctxt_t`) is the central state
structure passed to all callbacks and tasks. Every subsystem stores its handles
and buffers here. The `hidReportPending` flag prevents queueing a second
transfer while the first is still in flight — the DataWire DMA can only handle
one transfer per endpoint at a time.

### 4. `cy_usb_app.c` — Wiring (3 insertion points)

#### 4a. Set Configuration Callback — ISR Registration

After the UVC DataWire ISR is registered (HS only):

```c
#if HID_EN
    Cy_USB_AppInitDmaIntr(HID_IN_ENDPOINT, CY_USB_ENDP_DIR_IN, Cy_HID_InEpDma_ISR);
#endif
```

**Why:** `Cy_USB_AppInitDmaIntr()` maps the DataWire channel interrupt
(`cpuss_interrupts_dw1_4_IRQn`, since endpoint 4 maps to DW1 channel 4) to
our ISR function. Without this, DMA completion would not trigger our callback.

**What happens automatically:** The existing loop in `Cy_USB_AppSetCfgCallback`
already iterates all endpoints in the configuration and calls:
- `Cy_USB_AppConfigureEndp()` — configures the endpoint in the USBD layer
  (handles interrupt endpoints generically)
- `Cy_USB_AppSetupEndpDmaParams()` → `Cy_USB_AppSetupEndpDmaParamsHs()` —
  enables the DataWire DMA set for the endpoint via
  `Cy_USBHS_App_EnableEpDmaSet()`

No code changes were needed for endpoint configuration — the new HID endpoint
descriptor is picked up automatically.

#### 4b. Setup Request Callback — HID Class Requests

Added to the Standard Request section:

```c
if (bRequest == CY_USB_SC_GET_DESCRIPTOR) {
    if (target is HID interface && descriptor type is Report) {
        SendEndp0Data(cy_hid_report_descriptor, ...);
    }
}
```

Added to the Class Request section:

```c
if (target is HID interface) {
    switch (bRequest) {
        SET_IDLE / GET_IDLE / SET_PROTOCOL / GET_PROTOCOL → stub responses
    }
}
```

**Why:** The USB host issues these requests during HID driver initialization.
`GET_DESCRIPTOR` for the report type (0x22) is how the host learns the report
format. `SET_IDLE` and `SET_PROTOCOL` are standard HID requests that must be
acknowledged even if we don't implement the full semantics.

#### 4c. Application Init — HID Initialization

After UAC init:

```c
#if HID_EN
    Cy_HID_AppInit(pAppCtxt);
#endif
```

**Why:** `Cy_USB_AppInit()` is called once at boot. It's the central place
where all subsystems create their tasks, queues, and timers.

### 5. `cy_hid_app.c` — New File: HID Application Logic

This is the core of the HID implementation. It contains:

#### HID Report Descriptor

```c
const uint8_t cy_hid_report_descriptor[] = {
    0x06, 0x00, 0xFF,  // Usage Page (Vendor-Defined 0xFF00)
    0x09, 0x01,        // Usage (Vendor-Defined 0x01)
    0xA1, 0x01,        // Collection (Application)
    0x75, 0x08,        //   Report Size (8 bits)
    0x95, HID_REPORT_SIZE, // Report Count (64)
    0x81, 0x02,        //   Input (Data, Variable, Absolute)
    0xC0,              // End Collection
};
```

This is fetched by the host via `GET_DESCRIPTOR` (type 0x22). It tells the
host: "I produce 64-byte input reports containing vendor-defined data."

#### `Cy_HID_FillReport()`

Fills `hidReportBuffer` with data from CPU SRAM. The current implementation
fills an incrementing byte pattern as an example. **This is the function you
modify to send your actual data.**

#### `Cy_HID_TimerCb()` — Timer Callback

Runs every 8 ms in the timer daemon task context (priority 14):
1. Retrieves the application context from the timer ID
2. Checks that USB is in the CONFIGURED state
3. Sends `CY_USB_HID_SEND_REPORT` to the HID message queue

#### `Cy_HID_AppTaskHandler()` — Task Loop

1. Blocks on `xQueueReceive()` waiting for messages
2. On `CY_USB_HID_SEND_REPORT`:
   - Checks `hidReportPending` — skips if a transfer is already in flight
   - Calls `Cy_HID_FillReport()` to populate the buffer
   - Sets `hidReportPending = true`
   - Calls `Cy_USB_AppQueueWrite()` to queue the DataWire DMA transfer

#### `Cy_HID_InEpDma_ISR()` — DMA Completion ISR

1. Calls `Cy_USB_AppClearDmaInterrupt()` to clear the DataWire interrupt
2. Sets `hidReportPending = false` to allow the next transfer
3. Calls `portYIELD_FROM_ISR(true)` to trigger a context switch if the
   HID task was waiting

#### `Cy_HID_AppInit()` — Initialization

1. Creates a message queue (4 items deep)
2. Creates the HID task (stack 512 words, priority 5)
3. Creates an auto-reload timer (period = `HID_POLLING_INTERVAL_MS`)
4. Starts the timer

### 6. `Makefile` — Build Flag

```makefile
HID_EN=1 \
```

**Why:** The `HID_EN` preprocessor flag gates all HID code. Setting it to `1`
enables the HID device. Removing it (or setting to `0`) removes all HID code
from the build with zero overhead.

---

## The Complete Data Path

### Initialization (boot time)

```
main()
  └─ Cy_USB_AppInit()
       └─ Cy_HID_AppInit()
            ├─ xQueueCreate("HIDMsgQueue", 4 items)
            ├─ xTaskCreate(HidTask, priority 5, stack 512)
            ├─ xTimerCreate("HidTimer", 8 ms, auto-reload)
            └─ xTimerStart()

(USB enumeration — host sends Set Configuration)
  └─ Cy_USB_AppSetCfgCallback()
       ├─ [loop over all endpoints in config descriptor]
       │    ├─ Cy_USB_AppConfigureEndp(EP 0x04)
       │    │    Sets up interrupt IN endpoint in USBD layer
       │    └─ Cy_USB_AppSetupEndpDmaParams(EP 0x04)
       │         └─ Cy_USB_AppSetupEndpDmaParamsHs(EP 0x04)
       │              └─ Cy_USBHS_App_EnableEpDmaSet(EP 0x04, DW1)
       │                   Marks endpInDma[4].valid = true
       │                   Configures DataWire channel 4 for USBHS IN
       │
       └─ Cy_USB_AppInitDmaIntr(EP 0x04, IN, Cy_HID_InEpDma_ISR)
            Maps cpuss_interrupts_dw1_4_IRQn → Cy_HID_InEpDma_ISR
```

### Runtime (every 8 ms)

```
┌─────────────────────────────────────────────────────────┐
│ 1. TIMER FIRES                                          │
│    FreeRTOS tick interrupt → timer daemon task wakes    │
│    Priority 14                                          │
│                                                         │
│    Cy_HID_TimerCb(xTimer)                               │
│      pAppCtxt = pvTimerGetTimerID(xTimer)               │
│      if (devState == CONFIGURED)                        │
│        xMsg.type = CY_USB_HID_SEND_REPORT               │
│        xQueueSendFromISR(hidMsgQueue, &xMsg)            │
└───────────────────┬─────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────┐
│ 2. TASK PROCESSES MESSAGE                               │
│    Priority 5                                           │
│                                                         │
│    Cy_HID_AppTaskHandler()                              │
│      xQueueReceive(hidMsgQueue, &msg, portMAX_DELAY)    │
│      switch (msg.type):                                 │
│        case CY_USB_HID_SEND_REPORT:                     │
│          if (!hidReportPending)                         │
│            Cy_HID_FillReport(pAppCtxt)                  │
│              // Fills hidReportBuffer[64] from SRAM     │
│            hidReportPending = true                      │
│            Cy_USB_AppQueueWrite(                        │
│              appCtxt,                                   │
│              HID_IN_ENDPOINT,  // 0x04                  │
│              hidReportBuffer,  // pointer to SRAM       │
│              HID_REPORT_SIZE   // 64                    │
│            )                                            │
└───────────────────┬─────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────┐
│ 3. APPLICATION QUEUE WRITE                              │
│                                                         │
│    Cy_USB_AppQueueWrite(appCtxt, 0x04, buffer, 64)     │
│      dmaset_p = &endpInDma[4]  // DataWire DMA set      │
│      if (!dmaset_p->valid || dataSize == 0) → return    │
│      Cy_USBHS_App_QueueWrite(dmaset_p, buffer, 64)      │
│        // Inside Cy_USBHS_App_QueueWrite:               │
│        //   1. Cy_DMA_Channel_Disable(ch4)              │
│        //   2. Cy_DMA_Descriptor_Init(...)  (up to 3    │
│        //      descriptors for packet-size splitting)   │
│        //   3. Cy_DMA_Channel_Init(ch4, &desc)          │
│        //   4. Cy_DMA_Channel_SetInterruptMask(ch4)     │
│        //   5. Cy_DMA_Channel_Enable(ch4)  ← arms DMA   │
│        //   6. Cy_TrigMux_SwTrigger(...) (first call    │
│        //      only — kickstarts the trigger chain)     │
└───────────────────┬─────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────┐
│ 4. HARDWARE: DataWire DMA Transfer                      │
│                                                         │
│    DataWire Channel 4 (DW1)                             │
│      DMA is armed by Cy_DMA_Channel_Enable (step 5)     │
│      Kickstarted by Cy_TrigMux_SwTrigger (step 6,       │
│        first call only — subsequent transfers are       │
│        auto-triggered via the trigger mux chain)        │
│      Reads 64 bytes from SRAM buffer                    │
│      Writes to USBHS Endpoint 4 IN packet buffer        │
│      DMA transfer completes                             │
│      → cpuss_interrupts_dw1_4_IRQn fires                │
│      (Note: DMA runs to completion regardless of        │
│       whether the USB host has polled yet. The data     │
│       sits in the EP FIFO until the host sends an       │
│       IN token at the next polling interval.)           │
└───────────────────┬─────────────────────────────────────┘
                    │
                    ▼
┌─────────────────────────────────────────────────────────┐
│ 5. DMA COMPLETION ISR                                   │
│    (interrupt context)                                  │
│                                                         │
│    Cy_HID_InEpDma_ISR()                                 │
│      Cy_USB_AppClearDmaInterrupt(appCtxt, 4, IN)        │
│        → Clears DW1 channel 4 interrupt flag            │
│      appCtxt.hidReportPending = false                   │
│        → Allows next timer tick to queue a new transfer │
│      portYIELD_FROM_ISR(true)                           │
│        → Context switch if HidTask was waiting          │
└─────────────────────────────────────────────────────────┘
```

### Key Data Structures in the Path

```
cy_stc_usb_app_ctxt_t
  ├── endpInDma[4]          cy_stc_app_endp_dma_set_t
  │     └── .valid = true   (set by EnableEpDmaSet)
  ├── hidReportBuffer[64]   uint8_t[] (SRAM, filled by FillReport)
  ├── hidReportPending      bool (guards single-transfer constraint)
  ├── hidMsgQueue           QueueHandle_t (4 × cy_stc_usbd_app_msg_t)
  ├── hidTaskHandle         TaskHandle_t
  └── hidTimer              TimerHandle_t (8 ms auto-reload)

Hardware:
  DW1 Channel 4             DataWire DMA engine (programmed by QueueWrite)
    src → hidReportBuffer   (set per-transfer)
    dst → USBHS EP4 IN FIFO (fixed by Cy_USBHS_App_EnableEpDmaSet)
  cpuss_interrupts_dw1_4    Interrupt line → Cy_HID_InEpDma_ISR
```

---

## How to Customize

### Change What Data Is Sent

Edit `Cy_HID_FillReport()` in `cy_hid_app.c`:

```c
static void Cy_HID_FillReport(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    // Example: send a struct
    my_sensor_data_t *sensor = (my_sensor_data_t *)pAppCtxt->hidReportBuffer;
    sensor->temperature = read_temp_sensor();
    sensor->humidity    = read_humidity_sensor();
    sensor->pressure    = read_pressure_sensor();
    sensor->timestamp   = xTaskGetTickCount();
}
```

### Change the Report Rate

Edit `HID_POLLING_INTERVAL_MS` in `cy_usb_uvc_device.h`:

```c
#define HID_POLLING_INTERVAL_MS  (1)   // 1000 Hz
```

And update `bInterval` in the endpoint descriptors in `cy_usb_descriptors.c`.
For HS interrupt endpoints: `period = 2^(bInterval-1) × 125 µs`.

| Desired interval | bInterval |
|---|---|
| 1 ms (1000 Hz) | 4 |
| 2 ms (500 Hz) | 5 |
| 4 ms (250 Hz) | 6 |
| 8 ms (125 Hz) | 7 |

### Change the Report Size

1. Update `HID_REPORT_SIZE` in `cy_usb_uvc_device.h`
2. Update the report count in `cy_hid_report_descriptor[]` in `cy_hid_app.c`
3. Recalculate `HID_REPORT_DESC_SIZE` (the report descriptor byte length)
4. Update `wMaxPacketSize` in the endpoint descriptors in
   `cy_usb_descriptors.c`

### Disable HID Completely

Set `HID_EN=0` (or remove the line) in the `Makefile`. All HID code is
guarded by `#if HID_EN` and will be excluded from the build with zero
runtime overhead.

### Add SS (USB 3.x) Support

For SS, the HID data path would need an HBDMA `MEM_TO_IP` channel, following
the UAC audio pattern:

1. In `Cy_USB_AppSetupEndpDmaParamsSs()`, add a branch for `HID_IN_ENDPOINT`
   that creates an HBDMA channel with:
   - `chType = CY_HBDMA_TYPE_MEM_TO_IP`
   - `prodSck[0] = CY_HBDMA_VIRT_SOCKET_WR` (software writes)
   - `consSck[0] = CY_HBDMA_USBEG_SOCKET_00 + HID_IN_ENDPOINT`
   - `bufferMode = false` (packet mode)
   - Callback for consume events

2. In the HID task, use `Cy_HBDma_Channel_CommitBuffer()` instead of
   `Cy_USB_AppQueueWrite()`

3. Handle the consume-event callback to free buffers

---

## Verification

### Build

```bash
make
```

Should produce a clean build with zero errors and zero warnings.

### Enumeration Check (Linux)

```bash
lsusb -v -d 04b4:4823 | grep -A 30 HID
```

Expected output shows:
- Interface descriptor with `bInterfaceClass = 3 (Human Interface Device)`
- HID descriptor with `bcdHID = 1.11`
- Endpoint descriptor with `bEndpointAddress = 0x84`, `bmAttributes = 0x03 (Interrupt)`

### Report Verification (Linux)

```bash
# Find the hidraw device
ls /dev/hidraw*

# Read reports (use the correct hidraw number for the HID interface)
# usbhid-dump can show which hidraw device corresponds to which interface
sudo usbhid-dump -a 04b4:4823

# Or read raw reports with a simple C program / Python script using hidraw
```

### Coexistence Test

Verify that UVC (video streaming) and UAC (audio streaming) still function
normally with the HID endpoint active. The HID endpoint uses a separate
DataWire channel (DW1 ch4) and does not share hardware resources with
UVC (DW1 ch1, HBDMA sockets) or UAC (DW1 ch3 for audio).

### Performance

At 64 bytes every 8 ms = 8 KB/s. Negligible compared to UVC video
(up to 3 Gbps at SS). No measurable impact on video/audio throughput.
