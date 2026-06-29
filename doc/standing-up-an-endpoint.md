# Standing Up a New USB Endpoint: The Complete FX20 Tutorial

This tutorial explains how to add a new USB endpoint to the FX20 firmware and
connect it to a DMA engine. It covers both USB 2.x HighSpeed (DataWire DMA)
and USB 3.x SuperSpeed (HBDMA) paths, the parts the stack handles
automatically, and the parts you must implement yourself.

## Table of Contents

1. [The Big Picture: What Are You Actually Building?](#the-big-picture)
2. [The Two DMA Engines](#the-two-dma-engines)
3. [Endpoint Lifecycle](#endpoint-lifecycle)
4. [What the Stack Does For You Automatically](#what-the-stack-does-automatically)
5. [Step-by-Step: Adding an HS Endpoint (DataWire DMA)](#hs-walkthrough)
6. [Step-by-Step: Adding an SS Endpoint (HBDMA)](#ss-walkthrough)
7. [How DataWire DMA Works Internally](#datawire-internals)
8. [How HBDMA Works Internally](#hbdma-internals)
9. [Endpoint Types and DMA Configurations](#endpoint-types)
10. [ISR Registration Reference](#isr-reference)
11. [Troubleshooting](#troubleshooting)

---

## The Big Picture

Adding a USB endpoint to this firmware means building a pipeline. Data
originates somewhere (an FPGA over LVDS, a PDM microphone, a CPU SRAM buffer),
flows through a DMA engine, and arrives at a USB endpoint where the host reads
it. The pipeline has three stages:

```
┌──────────────┐     ┌──────────────┐     ┌──────────────┐
│  DATA SOURCE │ ──▶ │  DMA ENGINE  │ ──▶ │ USB ENDPOINT │ ──▶ Host
└──────────────┘     └──────────────┘     └──────────────┘
     Stage 1              Stage 2             Stage 3
```

Your job is to:
1. **Declare** the endpoint in the USB descriptors so the host knows it exists
2. **Configure** the DMA engine so data can reach the endpoint
3. **Drive** the data flow — fill buffers, handle completions, manage pacing

The firmware's `SetCfgCallback` does step 2 automatically for every endpoint
declared in the descriptors. You only need to write the code for steps 1 and 3.

---

## The Two DMA Engines

The FX20 has two completely different DMA subsystems. Which one you use
depends on the USB speed the host negotiates.

### DataWire DMA (USB 2.x HighSpeed)

- **Engine**: DataWire (DW0 for OUT, DW1 for IN)
- **Channel**: One per endpoint — channel number equals endpoint number
- **Granularity**: Per-packet. You queue one transfer per USB packet.
- **API**: `Cy_USBHS_App_QueueWrite()` for IN, `Cy_USBHS_App_QueueRead()` for OUT
- **Trigger**: Software trigger on first transfer; subsequent transfers
  auto-trigger via the hardware trigger mux
- **CPU involvement**: You program a DMA descriptor for every packet

### HBDMA (USB 3.x SuperSpeed)

- **Engine**: High BandWidth DMA (socket-to-socket)
- **Channel**: Created dynamically via `Cy_HBDma_Channel_Create()`
- **Granularity**: Buffer-mode. Large buffers (up to 64 KB) flow autonomously.
- **API**: `Cy_HBDma_Channel_CommitBuffer()` / `Cy_HBDma_Channel_GetBuffer()`
- **Trigger**: Hardware events between producer and consumer sockets
- **CPU involvement**: You only handle buffer metadata (pointer, length);
  the hardware moves the actual data

### Which one do I need?

| Your endpoint... | HS | SS | Both |
|---|---|---|---|
| Bulk IN (e.g., UVC video) | DataWire | HBDMA `IP_TO_IP` or `IP_TO_MEM` | Both paths |
| Interrupt IN (e.g., HID) | DataWire | HBDMA `MEM_TO_IP` | Both paths |
| Isochronous IN (e.g., audio) | DataWire | HBDMA `MEM_TO_IP` | Both paths |
| Bulk/Interrupt OUT | DataWire | HBDMA `IP_TO_MEM` | Both paths |

---

## Endpoint Lifecycle

Every endpoint goes through these states during a USB session:

```
               Host sends
               Set Configuration
  ┌─────────┐  ───────────────▶  ┌─────────────┐
  │ POWERED │                     │ CONFIGURED  │
  └─────────┘                     └──────┬──────┘
                                         │
                          ┌──────────────┼──────────────┐
                          ▼              ▼              ▼
                     DataWire DMA   HBDMA channel   ISR registered
                     set enabled    created &       (HS path)
                     (HS path)      enabled (SS)
                                         │
                          Host sends     │
                          Set Interface  │
                          ───────────────┘
                          (alt settings)
                                │
                                ▼
                          ┌─────────────┐
                          │ RE-CONFIG   │
                          │ (UAC only)  │
                          └─────────────┘
                                         │
                          ┌──────────────┼──────────────┐
                          │              │              │
                          ▼              ▼              ▼
                     Old channel    Old DMA set    New alt
                     destroyed      disabled       configured

               Host resets bus / disconnects
               ─────────────────────────▶  ┌──────────┐
                                           │ CLEANUP  │
                                           └──────────┘
```

---

## What the Stack Does For You Automatically

When the host sends **Set Configuration**, the USBD stack invokes
`Cy_USB_AppSetCfgCallback()`. This callback iterates **every interface and
every endpoint** declared in the active configuration descriptor. For each
endpoint it finds, it performs these steps **automatically**:

### Step A — Endpoint Configuration (`Cy_USB_AppConfigureEndp()`)

```c
// cy_usb_app.c:2577 — called for EVERY endpoint in the descriptor table
void Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr)
{
    // Extracts from the endpoint descriptor:
    endpNumber   = ...;   // e.g., 0x04
    endpType     = ...;   // BULK, INTERRUPT, or ISOCHRONOUS
    endpDirection = ...;  // IN or OUT
    maxPktSize   = ...;   // e.g., 64 for HID, 512 for UVC HS
    burstSize    = ...;   // SS only, from companion descriptor

    // Tells the USBD layer to set up the hardware endpoint
    Cy_USB_USBD_EndpConfig(endpConfig);
}
```

This function is **completely generic**. It handles bulk, interrupt, and
isochronous endpoints without any endpoint-specific code. You do not need to
modify it when adding a new endpoint.

### Step B — DMA Setup (`Cy_USB_AppSetupEndpDmaParams()`)

```c
// cy_usb_app.c:2548 — called for EVERY endpoint
void Cy_USB_AppSetupEndpDmaParams(pUsbApp, pEndpDscr)
{
    Cy_USB_AppSetupEndpDmaParamsSs(pUsbApp, pEndpDscr);  // SS path

    if (devSpeed <= CY_USBD_USB_DEV_HS) {
        Cy_USB_AppSetupEndpDmaParamsHs(pUsbApp, pEndpDscr);  // HS path
    }
}
```

**HS path** (`Cy_USB_AppSetupEndpDmaParamsHs`, line 2501) — also completely
generic. It reads the endpoint number, direction, and max packet size from
ANY endpoint descriptor and enables the corresponding DataWire DMA channel:

```c
// For endpoint 0x04 (IN):
endpNumber    = 4;
endpDirection = CY_USB_ENDP_DIR_IN;
pEndpDmaSet   = &pUsbApp->endpInDma[4];
pDW           = pUsbApp->pCpuDw1Base;  // DW1 for IN endpoints

Cy_USBHS_App_EnableEpDmaSet(pEndpDmaSet, pDW, 4, 4, IN, maxPktSize);
// This:
//   - Stores channel config in endpInDma[4]
//   - Sets up trigger mux: USBHS EP4 ↔ DataWire ch4
//   - Sets valid = true
```

**SS path** (`Cy_USB_AppSetupEndpDmaParamsSs`, line 2283) — endpoint-specific.
It only handles known endpoints (0x01 for UVC, 0x03 for UAC). For any
unrecognized endpoint, it does nothing. If you need SS support for a new
endpoint, you add a branch here.

### What You MUST Do Manually

After the automatic configuration, you still need to:

1. **Register a completion ISR** (HS path) — the DataWire channel interrupt
   must be mapped to your handler function
2. **Create an HBDMA channel** (SS path, if needed) — add a branch in
   `Cy_USB_AppSetupEndpDmaParamsSs()`
3. **Drive the data flow** — write the code that fills buffers, queues
   transfers, and handles completions

---

## HS Walkthrough: Adding an Interrupt IN Endpoint with DataWire DMA

This is the simplest path and the recommended starting point. We'll add a
hypothetical IN endpoint at address 0x85 (endpoint 5).

### Step 1: Declare Constants

In `cy_usb_uvc_device.h`:

```c
#define MY_EP_NUMBER               (0x05)
#define MY_EP_MAX_PKT_SIZE         (64)
#define MY_EP_INTERVAL_MS          (8)
```

### Step 2: Add the Endpoint to the USB Descriptors

In `cy_usb_descriptors.c`, inside both `SuperSpeedConfigDescr[]` and
`HighSpeedConfigDescr[]`, add an endpoint descriptor after your interface
descriptor:

```c
/* Endpoint descriptor for MyDevice IN */
0x07,                           /* bLength: 7 bytes */
0x05,                           /* bDescriptorType: Endpoint */
0x80 | MY_EP_NUMBER,            /* bEndpointAddress: IN, endpoint 5 */
0x03,                           /* bmAttributes: Interrupt */
(MY_EP_MAX_PKT_SIZE & 0xFF),    /* wMaxPacketSize LSB */
(MY_EP_MAX_PKT_SIZE >> 8),      /* wMaxPacketSize MSB */
0x07,                           /* bInterval: 2^(7-1)*125us = 8ms */
```

For the SS config, also add the SuperSpeed Endpoint Companion:

```c
/* SuperSpeed endpoint companion descriptor */
0x06,                           /* bLength: 6 bytes */
0x30,                           /* bDescriptorType: SS EP Companion */
0x00,                           /* bMaxBurst: 1 packet per burst */
0x00,                           /* bmAttributes: Mult=1 */
(MY_EP_MAX_PKT_SIZE & 0xFF),    /* wBytesPerInterval LSB */
(MY_EP_MAX_PKT_SIZE >> 8),      /* wBytesPerInterval MSB */
```

Update `wTotalLength` and `bNumInterfaces` at the top of the config descriptor
to account for the new bytes.

### Step 3: Understand What Happens Automatically

At this point, when the host sends Set Configuration, the firmware
automatically:

1. Calls `Cy_USB_AppConfigureEndp()` for endpoint 5 — configures it as an
   interrupt IN endpoint in the USBD hardware layer

2. For HS connections, calls `Cy_USB_AppSetupEndpDmaParamsHs()` which calls
   `Cy_USBHS_App_EnableEpDmaSet()` — sets up DataWire channel 5, connects
   trigger mux, marks `endpInDma[5].valid = true`

3. For SS connections, `Cy_USB_AppSetupEndpDmaParamsSs()` sees an unknown
   endpoint number and does nothing (correct for HS-only operation)

**At this point, the endpoint is configured and the DataWire DMA channel is
ready.** The only missing piece is the ISR and the application logic.

### Step 4: Register the Completion ISR

In `Cy_USB_AppSetCfgCallback()`, after the existing ISR registrations (which
only run for HS connections):

```c
if (devSpeed <= CY_USBD_USB_DEV_HS)
{
    // ... existing ISR registrations ...

    Cy_USB_AppInitDmaIntr(MY_EP_NUMBER, CY_USB_ENDP_DIR_IN, MyEpDma_ISR);
}
```

`Cy_USB_AppInitDmaIntr()` does two things:
- Initializes the interrupt vector: maps `cpuss_interrupts_dw1_5_IRQn` to
  your ISR function
- Enables the NVIC interrupt for that vector

The interrupt number is computed as:
```
cpuss_interrupts_dw1_<endpointNumber>_IRQn   (for IN endpoints)
cpuss_interrupts_dw0_<endpointNumber>_IRQn   (for OUT endpoints)
```

### Step 5: Write the Completion ISR

```c
void MyEpDma_ISR(void)
{
    // 1. Clear the DataWire interrupt for this channel
    Cy_USB_AppClearDmaInterrupt(&appCtxt, MY_EP_NUMBER, CY_USB_ENDP_DIR_IN);

    // 2. Mark the transfer as complete
    g_myEpXferPending = false;

    // 3. Allow a context switch if a task is waiting
    portYIELD_FROM_ISR(true);
}
```

### Step 6: Write the Data Send Function

```c
void MyEp_SendData(uint8_t *pData, uint32_t dataSize)
{
    // Guard against overlapping transfers — DataWire can only handle
    // one transfer per channel at a time
    if (g_myEpXferPending)
        return;

    g_myEpXferPending = true;

    // This single call does everything:
    //   1. Cy_DMA_Channel_Disable(ch5)
    //   2. Cy_DMA_Descriptor_Init(...) — up to 3 descriptors
    //   3. Cy_DMA_Channel_Init(ch5, &desc)
    //   4. Cy_DMA_Channel_SetInterruptMask(ch5)
    //   5. Cy_DMA_Channel_Enable(ch5)  ← arms the DMA
    //   6. Cy_TrigMux_SwTrigger(...)   ← kickstart (first call only)
    Cy_USB_AppQueueWrite(&appCtxt, MY_EP_NUMBER, pData, dataSize);
}
```

### Complete HS Data Flow

```
Your code calls MyEp_SendData(buf, 64)
  → Cy_USB_AppQueueWrite(&appCtxt, 5, buf, 64)
    → Cy_USBHS_App_QueueWrite(&endpInDma[5], buf, 64)
      → Programs DW1 channel 5 descriptor: src=buf, dst=USBHS EP5 FIFO
      → Cy_DMA_Channel_Enable(DW1, 5) — arms channel
      → Cy_TrigMux_SwTrigger(...) — kickstarts (first call only)
        │
        ▼
  [DataWire DMA executes: moves 64 bytes from SRAM → EP5 FIFO]
        │
        ▼
  [DMA complete → cpuss_interrupts_dw1_5_IRQn]
        │
        ▼
  MyEpDma_ISR()
    → Cy_USB_AppClearDmaInterrupt(appCtxt, 5, IN)
    → g_myEpXferPending = false
        │
        ▼
  [USB host polls EP5 → reads 64 bytes from FIFO]
```

---

## SS Walkthrough: Adding an Interrupt IN Endpoint with HBDMA

For USB 3.x, you need an HBDMA `MEM_TO_IP` channel. Data flows from a virtual
socket (software writes) to a USB egress socket (endpoint).

### Step 1: Add a Branch in the SS DMA Setup

In `Cy_USB_AppSetupEndpDmaParamsSs()` (`cy_usb_app.c:2283`), after the
existing endpoint handlers:

```c
/* Handling MyDevice streaming endpoint. */
if ((endpNumber == MY_EP_NUMBER) && (dir != 0))
{
    cy_stc_hbdma_chn_config_t dmaConfig;

    dmaConfig.chType       = CY_HBDMA_TYPE_MEM_TO_IP;
    dmaConfig.prodSckCount = 1;
    dmaConfig.prodSck[0]   = CY_HBDMA_VIRT_SOCKET_WR;  // software source
    dmaConfig.prodSck[1]   = (cy_hbdma_socket_id_t)0;
    dmaConfig.consSckCount = 1;
    dmaConfig.consSck[0]   = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
    dmaConfig.consSck[1]   = (cy_hbdma_socket_id_t)0;
    dmaConfig.prodHdrSize  = 0;
    dmaConfig.eventEnable  = 1;    // AUTO: hardware manages buffer flow
    dmaConfig.intrEnable   = 0;    // No interrupts needed in AUTO mode
    dmaConfig.cb           = NULL;
    dmaConfig.size         = MY_EP_MAX_PKT_SIZE;
    dmaConfig.count        = 4;    // 4 buffers for queuing
    dmaConfig.prodBufSize  = MY_EP_MAX_PKT_SIZE;
    dmaConfig.bufferMode   = false; // Packet mode
    dmaConfig.userCtx      = (void *)pUsbApp;

    // If UAC audio is also enabled, destroy it first for alt-setting support
    if (pUsbApp->pMyDeviceChn != NULL) {
        Cy_HBDma_Channel_Disable(pUsbApp->pMyDeviceChn);
        Cy_HBDma_Channel_Destroy(pUsbApp->pMyDeviceChn);
        pUsbApp->pMyDeviceChn = NULL;
    }

    Cy_HBDma_Channel_Create(pUsbApp->pHbDmaMgrCtxt, &dmaConfig,
                            &pUsbApp->pMyDeviceChn);
    Cy_HBDma_Channel_Enable(pUsbApp->pMyDeviceChn, 0);
}
```

Key decisions in this configuration:

- **`CY_HBDMA_TYPE_MEM_TO_IP`**: Data comes from memory (CPU) and goes to an
  IP block (USB egress)
- **`CY_HBDMA_VIRT_SOCKET_WR`**: The producer is software — your code writes
  data into this socket by committing buffers
- **`CY_HBDMA_USBEG_SOCKET_00 + endpNumber`**: The consumer is the USB egress
  adapter, socket offset by endpoint number
- **`bufferMode = false`**: Packet mode. Each commit is one USB packet.
  Buffer mode (`true`) is for streaming (multi-buffer pipeline).
- **`eventEnable = 1`**: AUTO mode lets the hardware manage buffer flow
  between producer and consumer sockets without CPU intervention
- **`count = 4`**: Up to 4 buffers can be queued ahead

### Step 2: Send Data via HBDMA

```c
void MyEp_SendDataSS(cy_stc_usb_app_ctxt_t *pAppCtxt,
                     uint8_t *pData, uint32_t dataSize)
{
    cy_stc_hbdma_buff_status_t dmaBufStat;
    cy_en_hbdma_mgr_status_t   stat;

    dmaBufStat.pBuffer = pData;
    dmaBufStat.size    = dataSize;
    dmaBufStat.count   = dataSize;
    dmaBufStat.status  = 0;

    stat = Cy_HBDma_Channel_CommitBuffer(pAppCtxt->pMyDeviceChn, &dmaBufStat);
    if (stat != CY_HBDMA_MGR_SUCCESS) {
        // All buffers in use — try again later or drop
    }
}
```

### Complete SS Data Flow

```
Your code calls MyEp_SendDataSS(appCtxt, buf, 64)
  → Cy_HBDma_Channel_CommitBuffer(pMyDeviceChn, &dmaBufStat)
    → Buffer attached to VIRT_SOCKET_WR (producer socket)
      │
      ▼
  [HBDMA hardware: VIRT_SOCKET_WR → USBEG_SOCKET_05, zero CPU copy]
      │
      ▼
  [USB egress adapter sends data on EP5 when host polls]
      │
      ▼
  [Buffer consumed → returned to free pool automatically (AUTO mode)]
```

---

## How DataWire DMA Works Internally

Understanding the internals helps debug problems. Here's the complete
sequence inside `Cy_USBHS_App_QueueWrite()`:

### Setup (called once at Set Configuration)

`Cy_USBHS_App_EnableEpDmaSet()` configures the endpoint DMA set:

```
┌──────────────────────────────────────────────────────┐
│ endpInDma[endpointNumber]                            │
│   .pDwStruct   = DW1 base address                   │
│   .channel     = endpointNumber (hardware channel)   │
│   .epNumber    = endpointNumber                      │
│   .endpDirection = IN                                │
│   .maxPktSize  = max packet size from descriptor     │
│   .valid       = true  ← QueueWrite checks this      │
│   .firstRqtDone = false ← enables software trigger   │
│                                                      │
│ Trigger Mux Setup (done by EnableEpDmaSet):          │
│   TRIG_OUT_MUX_1_USBHSDEV_TR_IN16 + epNumber        │
│       ↕  (bidirectional)                             │
│   TRIG_IN_MUX_1_DW1_TR_IN0 + channel                │
│                                                      │
│ This connects: USBHS EP FIFO empty → trigger DMA,    │
│               and DMA done → notify USBHS            │
└──────────────────────────────────────────────────────┘
```

### Per-Transfer (called by QueueWrite)

`Cy_USBHS_App_QueueWrite()` programs descriptors and arms the channel:

```
Step 1: Cy_DMA_Channel_Disable(DW1, channel)
        // Prevents partial reconfiguration of a running channel

Step 2: Cy_DMA_Descriptor_Init(desc0, ...)
        Cy_DMA_Descriptor_Init(desc1, ...)  // if short packet
        Cy_DMA_Descriptor_Init(desc2, ...)  // if unaligned bytes
        // Descriptor 0: 2-D transfer for full max-packet-size chunks
        //   X count = number of full packets
        //   Y count = max packet size (in DWORDs)
        //   src = pData, srcIncr = 1 (sequential read from SRAM)
        //   dst = USBHS EP FIFO, dstIncr = 0 (fixed FIFO address)
        // Descriptor 1: 1-D transfer for DWORD-aligned remainder
        // Descriptor 2: 1-D transfer for 1-3 trailing bytes

Step 3: Cy_DMA_Channel_Init(DW1, channel, &chanCfg)
        // Binds descriptor chain to the DataWire channel
        // chanCfg.descriptor = &desc0
        // chanCfg.preemptable = false
        // chanCfg.enable = false (will enable in step 5)

Step 4: Cy_DMA_Channel_SetInterruptMask(DW1, channel, CY_DMA_INTR_MASK)
        // Enables the "descriptor complete" interrupt
        // When the last descriptor in the chain finishes, the
        // interrupt fires → your ISR runs

Step 5: Cy_DMA_Channel_Enable(DW1, channel)
        // Arms the channel. The DMA is now ready but may not
        // start immediately — it waits for a trigger.

Step 6: Cy_TrigMux_SwTrigger(TRIG_IN_MUX_1_USBHSDEV_TR_OUT16 + epNumber,
                             CY_TRIGGER_TWO_CYCLES)
        // First call only (firstRqtDone flag).
        // Manually fires a trigger edge to start the first transfer.
        // After the first transfer completes, the trigger mux
        // connection auto-propagates subsequent triggers.
```

### The Trigger Chain

The trigger mux creates a self-sustaining chain:

```
USB Host sends IN token on EP5
  → USBHS hardware: EP5 FIFO empty, needs data
    → TRIG_OUT (USBHSDEV_TR_OUT21) fires
      → TRIG_IN (DW1_TR_IN5) receives trigger
        → DataWire DMA executes: SRAM → EP5 FIFO
          → DMA descriptor complete
            → Interrupt: cpuss_interrupts_dw1_5_IRQn
              → Your ISR fires

For the HBDMA path (SS), there is no per-packet trigger chain. Data flows
continuously from the producer socket to the consumer socket via internal
hardware events — the CPU is not in the data path.
```

---

## How HBDMA Works Internally

HBDMA is a socket-to-socket DMA engine. Think of it as a hardware FIFO
pipeline:

```
┌──────────────────────────────────────────────────────┐
│                  HBDMA Channel                        │
│                                                      │
│  ┌──────────────┐    ┌──────────┐    ┌────────────┐ │
│  │ Producer     │═══▶│ Buffers  │═══▶│ Consumer   │ │
│  │ Socket(s)    │    │ (0..N)   │    │ Socket(s)  │ │
│  └──────────────┘    └──────────┘    └────────────┘ │
│       ▲                                   │          │
│       │ PROD_EVENT                        │ CONS_EVENT│
│       │ (new data)                        │ (data sent)│
│       │                                   ▼          │
│  [Callback]                          [Callback]      │
└──────────────────────────────────────────────────────┘
```

### Buffer Mode (`bufferMode = true`)

Used for streaming. A pool of N large buffers cycles between producer and
consumer:

```
Free Pool → Producer fills → Committed → Consumer drains → Free Pool
```

Your callback receives `CY_HBDMA_CB_PROD_EVENT` when the producer fills a
buffer (you call `GetBuffer()` to claim it, modify data if needed, then
`CommitBuffer()` to forward it to the consumer). You receive
`CY_HBDMA_CB_CONS_EVENT` when the consumer drains it (you call
`DiscardBuffer()` to return it to the free pool).

### Packet Mode (`bufferMode = false`)

Used for message-oriented transfers. Each `CommitBuffer()` sends one packet:

```c
dmaBufStat.pBuffer = myData;
dmaBufStat.size    = packetSize;
dmaBufStat.count   = packetSize;
Cy_HBDma_Channel_CommitBuffer(pChannel, &dmaBufStat);
// Data flows to the consumer socket automatically.
// No PROD_EVENT — the commit IS the produce.
```

### Channel Types

| Type | Producer | Consumer | Use Case |
|---|---|---|---|
| `IP_TO_IP` | Hardware socket (LVDS) | Hardware socket (USB EG) | FPGA video → USB (SS) |
| `IP_TO_MEM` | Hardware socket (LVDS) | Memory (DataWire) | FPGA video → RAM → USB (HS fallback) |
| `MEM_TO_IP` | Virtual socket (CPU) | Hardware socket (USB EG) | CPU data → USB (SS audio, HID) |

---

## Endpoint Types and DMA Configurations

### Bulk IN Endpoint

| | HS | SS |
|---|---|---|
| DMA type | DataWire `QueueWrite` | HBDMA `IP_TO_IP` or `IP_TO_MEM` |
| Buffer mode | Per-packet | `true` (streaming) |
| Typical buffer size | Up to 512 bytes | Up to 64 KB |
| Completion | DataWire ISR | HBDMA CONS_EVENT callback |
| Use case | UVC video (EP 0x01) | UVC video |

**SS bulk config pattern** — see `Cy_USB_AppSetupEndpDmaParamsSs()` for
endpoint 0x01, starting at line 2303.

### Interrupt IN Endpoint

| | HS | SS |
|---|---|---|
| DMA type | DataWire `QueueWrite` | HBDMA `MEM_TO_IP` |
| Buffer mode | Per-packet | `false` (packet mode) |
| Typical buffer size | Up to 64 bytes (FS) or 1024 (HS) | Same |
| Completion | DataWire ISR | HBDMA CONS_EVENT callback or none (AUTO) |
| Use case | HID (EP 0x04), UVC control (EP 0x02) | Any periodic data |

**HS interrupt IN** — the simplest endpoint type. See the
[HS Walkthrough](#hs-walkthrough) above.

### Isochronous IN Endpoint

| | HS | SS |
|---|---|---|
| DMA type | DataWire `QueueWrite` | HBDMA `MEM_TO_IP` |
| Buffer mode | Per-packet | `false` (packet mode) |
| Typical buffer size | Up to 1024 bytes (HS) | Up to 1024 bytes |
| Completion | DataWire ISR | HBDMA CONS_EVENT callback |
| Use case | UAC audio (EP 0x03) | UAC audio |

**SS isochronous config pattern** — see `Cy_USB_AppSetupEndpDmaParamsSs()`
for endpoint 0x03, starting at line 2399.

### OUT Endpoints

| | HS | SS |
|---|---|---|
| DMA type | DataWire `QueueRead` (DW0) | HBDMA `IP_TO_MEM` |
| Completion | DataWire ISR | HBDMA PROD_EVENT callback |
| Direction | Host → Device | Host → Device |

**HS OUT** uses `Cy_USBHS_App_QueueRead()` on `pCpuDw0Base` instead of
`QueueWrite` on `pCpuDw1Base`. The DataWire channel number still equals the
endpoint number, but uses the DW0 controller.

---

## ISR Registration Reference

### Function: `Cy_USB_AppInitDmaIntr()`

```c
void Cy_USB_AppInitDmaIntr(
    uint32_t endpNumber,           // endpoint number (1..MAX)
    cy_en_usb_endp_dir_t endpDirection,  // CY_USB_ENDP_DIR_IN or _OUT
    cy_israddress userIsr          // your ISR function, or NULL to disable
);
```

### Interrupt Vector Mapping

**For IN endpoints (DW1):**
```
cpuss_interrupts_dw1_<endpNumber>_IRQn
```

| Endpoint | Interrupt Vector |
|---|---|
| 0x01 (UVC video) | `cpuss_interrupts_dw1_1_IRQn` |
| 0x02 (UVC control) | `cpuss_interrupts_dw1_2_IRQn` |
| 0x03 (UAC audio) | `cpuss_interrupts_dw1_3_IRQn` |
| 0x04 (HID) | `cpuss_interrupts_dw1_4_IRQn` |
| 0x05+ | `cpuss_interrupts_dw1_<N>_IRQn` |

**For OUT endpoints (DW0):**
```
cpuss_interrupts_dw0_<endpNumber>_IRQn
```

### ISR Implementation Template

```c
void MyEpDma_ISR(void)
{
    // Always clear the interrupt first
    Cy_USB_AppClearDmaInterrupt(&appCtxt, MY_EP_NUMBER, CY_USB_ENDP_DIR_IN);

    // Update your application state
    g_xferPending = false;

    // Optionally signal a task
    portYIELD_FROM_ISR(true);
}
```

### Where to Register

HS ISRs are registered in `Cy_USB_AppSetCfgCallback()`, inside the
`if (devSpeed <= CY_USBD_USB_DEV_HS)` block. This ensures they are only
active for HS connections. For SS, the HBDMA callback handles completion
notification.

---

## Troubleshooting

### "QueueWrite Err1" in debug log

This means `Cy_USB_AppQueueWrite` failed its validation check. The
`endpInDma[endpointNumber].valid` flag is `false`.

**Cause**: `Cy_USBHS_App_EnableEpDmaSet()` was never called for this endpoint.

**Check**:
1. Is the endpoint descriptor present in the config descriptor?
2. Is `bNumInterfaces` updated to include the interface containing this
   endpoint?
3. Is `wTotalLength` correct (does it include the endpoint descriptor bytes)?

### DataWire ISR never fires after QueueWrite

**Check**:
1. Did you register the ISR via `Cy_USB_AppInitDmaIntr()`? The call must be
   inside `Cy_USB_AppSetCfgCallback()` in the HS block.
2. Is the endpoint number correct? The interrupt vector is
   `cpuss_interrupts_dw1_<endpNumber>_IRQn` — double-check that endpoint
   number matches.
3. Is the NVIC enabled for this interrupt? `Cy_USB_AppInitDmaIntr()` calls
   `NVIC_EnableIRQ()` — verify it's not later disabled elsewhere.

### Host doesn't see the endpoint

**Check**:
1. Run `lsusb -v -d <vid>:<pid>`. Does the endpoint descriptor appear?
2. Is the `bEndpointAddress` correct? `0x80 | endpointNumber` for IN,
   plain `endpointNumber` for OUT.
3. Is `wTotalLength` correct? If it's too small, descriptors at the end
   of the array are truncated.

### HBDMA CommitBuffer returns error

**Cause**: All buffers are in use (channel buffer pool exhausted).

**Fix**: Increase `count` in the HBDMA channel config, or ensure the
consumer (USB host) is polling fast enough to drain buffers.

### SS vs HS: "My endpoint works on HS but not SS"

For SS, two things need to happen:
1. `Cy_USB_AppSetupEndpDmaParamsSs()` must recognize your endpoint and create
   an HBDMA channel — you need to add a branch
2. The HBDMA channel must be the correct type for your data source

For HS, `Cy_USB_AppSetupEndpDmaParamsHs()` is generic and handles any
endpoint automatically. The SS path requires explicit per-endpoint code.

---

## Quick Reference: Adding an Endpoint Checklist

- [ ] Add `#define` constants for endpoint number and parameters in
  `cy_usb_uvc_device.h`
- [ ] Add endpoint descriptor to `SuperSpeedConfigDescr[]` in
  `cy_usb_descriptors.c`
- [ ] Add endpoint descriptor to `HighSpeedConfigDescr[]` in
  `cy_usb_descriptors.c`
- [ ] Add SS Endpoint Companion descriptor (SS config only, 6 bytes)
- [ ] Update `wTotalLength` in both config descriptors
- [ ] Update `bNumInterfaces` in both config descriptors (if adding a new
  interface)
- [ ] Register DataWire ISR in `Cy_USB_AppSetCfgCallback()` (HS path)
- [ ] Add HBDMA channel creation branch in
  `Cy_USB_AppSetupEndpDmaParamsSs()` (SS path, if needed)
- [ ] Write completion ISR function
- [ ] Write data send/receive functions
- [ ] Add any needed fields to `cy_stc_usb_app_ctxt_t` in `cy_usb_app.h`
- [ ] Add init code (task, timer, queue) in `Cy_USB_AppInit()` or a new
  `_AppInit()` function
- [ ] Verify with `lsusb -v`
- [ ] Test data transfer at the target speed (HS or SS)
