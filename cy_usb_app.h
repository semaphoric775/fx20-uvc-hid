/***************************************************************************//**
* \file cy_usb_app.h
* \version 1.0
*
* Defines the interfaces used in the FX20 USB Video Class application.
*
*******************************************************************************
* \copyright
* (c) (2024), Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.
*
* SPDX-License-Identifier: Apache-2.0
*
* Licensed under the Apache License, Version 2.0 (the "License");
* you may not use this file except in compliance with the License.
* You may obtain a copy of the License at
*
*     http://www.apache.org/licenses/LICENSE-2.0
*
* Unless required by applicable law or agreed to in writing, software
* distributed under the License is distributed on an "AS IS" BASIS,
* WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
* See the License for the specific language governing permissions and
* limitations under the License.
*******************************************************************************/

#ifndef _CY_USB_APP_H_
#define _CY_USB_APP_H_

#include "cy_pdl.h"
#include "cy_debug.h"
#include "cy_usbhs_dw_wrapper.h"

#if defined(__cplusplus)
extern "C" {
#endif

#define RED                             "\033[0;31m"
#define CYAN                            "\033[0;36m"
#define COLOR_RESET                     "\033[0m"

#define LOG_COLOR(...)                  Cy_Debug_AddToLog(1,CYAN);\
                                        Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                        Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_ERROR(...)                  Cy_Debug_AddToLog(1,RED);\
                                        Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                        Cy_Debug_AddToLog(1,COLOR_RESET);

#define LOG_CLR(CLR, ...)               Cy_Debug_AddToLog(1,CLR);\
                                        Cy_Debug_AddToLog(1,__VA_ARGS__); \
                                        Cy_Debug_AddToLog(1,COLOR_RESET);


#define LOG_TRACE()                     LOG_COLOR("-->[%s]:%d\r\n",__func__,__LINE__);


#define DELAY_MICRO(us)                 Cy_SysLib_DelayUs(us)
#define DELAY_MILLI(ms)                 Cy_SysLib_Delay(ms)

#define PHY_TRAINING_PATTERN_BYTE       (0x39)
#define LINK_TRAINING_PATTERN_BYTE      (0xAA55AA55)
#define FPS_DEFAULT                     (60)

#define SET_BIT(byte, mask)                (byte) |= (mask)
#define CLR_BIT(byte, mask)                (byte) &= ~(mask)
#define CHK_BIT(byte, mask)                (byte) & (mask)


#if !LVDS_LB_EN

#if INMD_EN
#define UVC_HEADER_BY_FPGA              (1)
#define UVC_HEADER_BY_FX                (0)
/* Do not change these value */
#define MANUAL_DMA_CHANNEL              (0)
#define AUTO_DMA_CHANNEL                (!MANUAL_DMA_CHANNEL)

#elif FPGA_ADDS_HEADER
#define UVC_HEADER_BY_FPGA              (1)
#define UVC_HEADER_BY_FX                (0)

/* DMA channel and UVC header configuration*/
#define MANUAL_DMA_CHANNEL              (0)
#define AUTO_DMA_CHANNEL                (!MANUAL_DMA_CHANNEL)

#else
#define UVC_HEADER_BY_FPGA              (0)
#define UVC_HEADER_BY_FX                (1)
/* Do not change these value for FX added header*/
#define MANUAL_DMA_CHANNEL              (1)
#define AUTO_DMA_CHANNEL                (!MANUAL_DMA_CHANNEL)
#endif /* FPGA_ADDS_HEADER */

/* Choose video source */
#define VIDEO_SOURCE
#define SOURCE_COLORBAR                 (1)

#if (LVCMOS_EN && (!LVCMOS_DDR_EN))
#define LINK_TRAINING                   (0)
#else
#define LINK_TRAINING                   (1)
#endif /* LVCMOS_EN && (!LVCMOS_DDR_EN) */

#define LINK_READY_CTL_PORT             (PORT1_EN)
#define LINK_READY_CTL_PIN              (cy_en_lvds_phy_gpio_index_t)(16+6)

#else 

#define MANUAL_DMA_CHANNEL              (1)
#define UVC_HEADER_BY_FX                (1)
#define AUTO_DMA_CHANNEL                (!MANUAL_DMA_CHANNEL)
#define UVC_HEADER_BY_FPGA              (AUTO_DMA_CHANNEL || (!UVC_HEADER_BY_FX))

#endif /*LVDS_LB_EN*/

#if (LVDS_LB_EN && FPGA_ENABLE)
#error LOG_COLOR(RED, "INVALID: LVDS_LB_EN WITH FPGA_ENABLE");
#endif /* LVDS_LB_EN && FPGA_ENABLE */

#if (UVC_HEADER_BY_FPGA && LVDS_LB_EN)
#error LOG_COLOR(RED, "INVALID: LOOPBACK WITH UVC_HEADER_BY_FPGA");
#endif /* UVC_HEADER_BY_FPGA && LVDS_LB_EN */

#if (PORT1_EN && INTERLEAVE_EN)
#error LOG_COLOR(RED, "INVALID: INTERLEAVE NOT SUPPORTED BY PORT1");
#endif /* PORT1_EN && INTERLEAVE_EN */

#if (LVDS_LB_EN && INMD_EN)
#error LOG_COLOR(RED, "INVALID: LVDS_LB_EN WITH INMD ");
#endif /* LVDS_LB_EN && INMD_EN */

#if (FPGA_ADDS_HEADER && INMD_EN)
#error LOG_COLOR(RED, "INVALID: FPGA_ADDS_HEADER WITH INMD_EN");
#endif /* FPGA_ADDS_HEADER && INMD_EN */

#if ((!WL_EN) && INMD_EN)
#error LOG_COLOR(RED, "INVALID: INMD WITH NARROW LINK");
#endif /* WL_EN && INMD_EN*/

#if (LVCMOS_EN && INMD_EN)
#error LOG_COLOR(RED, "INVALID: INMD WITH LVCMOS_EN");
#endif /* WL_EN && INMD_EN*/

#if ((!LVCMOS_EN) && LVCMOS_DDR_EN)
#error LOG_COLOR(RED, "INVALID: LVCMOS_DDR_EN WITH LVDS");
#endif /* WL_EN && INMD_EN*/


#define DMA_BUFFER_SIZE                 (CY_USB_UVC_STREAM_BUF_SIZE)
#if UVC_HEADER_BY_FX
#define FPGA_DMA_BUFFER_SIZE            (DMA_BUFFER_SIZE - CY_USB_UVC_MAX_HEADER)
#elif UVC_HEADER_BY_FPGA
#define FPGA_DMA_BUFFER_SIZE            (DMA_BUFFER_SIZE)
#endif

/* GPIO port pins*/
#define TI180_INIT_RESET_GPIO           (P4_3_GPIO)
#define TI180_INIT_RESET_PORT           (P4_3_PORT)
#define TI180_INIT_RESET_PIN            (P4_3_PIN)

#define TI180_CDONE_PIN                 (P4_4_PIN)
#define TI180_CDONE_PORT                (P4_4_PORT)

#define CDONE_WAIT_TIMEOUT              (1000)


#define ASSERT(condition, value)        Cy_CheckStatus(__func__, __LINE__, condition, value, true);
#define ASSERT_NON_BLOCK(condition, value) Cy_CheckStatus(__func__, __LINE__, condition, value, false);
#define ASSERT_AND_HANDLE(condition, value, failureHandler) Cy_Cy_CheckStatusAndHandleFailure(__func__, __LINE__, condition, value, false, Cy_FailHandler);

/* Loopback program color bands*/
#define BAND1_COLOR_YUYV                0x80ff80ff    // White
#define BAND2_COLOR_YUYV                0x94ff00ff    // Yellow
#define BAND3_COLOR_YUYV                0x1ac8bfc8    // Blue
#define BAND4_COLOR_YUYV                0x4aca55ca    // Green
#define BAND5_COLOR_YUYV                0xf3969f96    // Pink
#define BAND6_COLOR_YUYV                0xff4c544c    // Red
#define BAND7_COLOR_YUYV                0x9e40d340    // Violet
#define BAND8_COLOR_YUYV                0x80008000    // Black
#define COLORBAR_BAND_COUNT_4K          120
#define COLORBAR_BAND_COUNT_1080P       60
#define COLORBAR_BAND_COUNT_720P        40
#define COLORBAR_BAND_COUNT_480P        20
#define LOOPBACK_MEM_BUF_SIZE           0xF0C0

/* P4.0 is used for VBus detect functionality. */
#define VBUS_DETECT_GPIO_PORT           (P4_0_PORT)
#define VBUS_DETECT_GPIO_PIN            (P4_0_PIN)
#define VBUS_DETECT_GPIO_INTR           (ioss_interrupts_gpio_dpslp_4_IRQn)
#define VBUS_DETECT_STATE               (0u)

/* Number of buffers used for PDM data transfer. */
#define PDM_APP_BUFFER_CNT              (4u)

#define LVCMOS_GPIF_CTRLBUS_BITMAP      (0x0000038F)
#define LVCMOS_GPIF_CTRLBUS_BITMAP_WL   (0x000C008F)

typedef struct cy_stc_usb_app_ctxt_ cy_stc_usb_app_ctxt_t;

/* USBD layer return code shared between USBD layer and Application layer. */
typedef enum cy_en_usb_app_ret_code_ 
{
    CY_USB_APP_STATUS_SUCCESS=0,
    CY_USB_APP_STATUS_FAILURE,
}cy_en_usb_app_ret_code_t;

/*
 * USB application data structure which is bridge between USB system and device
 * functionality.
 * It maintains some usb system information which comes from USBD and it also
 * maintains info about functionality.
 */
struct cy_stc_usb_app_ctxt_
{
    uint8_t firstInitDone;
    cy_en_usb_device_state_t devState;
    cy_en_usb_device_state_t prevDevState;
    cy_en_usb_speed_t devSpeed;
    uint8_t devAddr;
    uint8_t activeCfgNum;
    cy_en_usb_enum_method_t enumMethod;
    uint8_t prevAltSetting;
    cy_en_usb_speed_t desiredSpeed;

    cy_stc_app_endp_dma_set_t endpInDma[CY_USB_MAX_ENDP_NUMBER];
    cy_stc_app_endp_dma_set_t endpOutDma[CY_USB_MAX_ENDP_NUMBER];
    DMAC_Type *pCpuDmacBase;
    DW_Type *pCpuDw0Base;
    DW_Type *pCpuDw1Base;

    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt;
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt;
    cy_stc_hbdma_channel_t *hbBulkInChannel;

    /* UVC specific fields. */
    uint8_t uvcInEpNum;                         /** Index of UVC streaming endpoint. */
    uint8_t uvcPendingBufCnt;                   /** Number of data buffers which are pending to be streamed. */
    bool uvcFlowCtrlFlag;                       /** Flag indicating that UVC channel is in flow control. */

    /* Global Task handles */
    TaskHandle_t uvcDevicetaskHandle;           /** UVC application task. */
    QueueHandle_t uvcMsgQueue;                  /** Message queue used to send messages to the UVC task. */

    bool isLpmEnabled;                          /** Whether LPM transitions are enabled. */
    uint32_t lpmEnableTime;                     /** Timestamp at which LPM should be re-enabled. */

    bool usbConnectDone;
    bool vbusChangeIntr;                        /** VBus change interrupt received flag. */
    bool vbusPresent;                           /** VBus presence indicator flag. */
    bool usbConnected;                          /** Whether USB connection is enabled. */
    TimerHandle_t vbusDebounceTimer;            /** VBus change debounce timer handle. */
    uint32_t *pUsbEvtLogBuf;
    TimerHandle_t evtLogTimer;                  /** Timer to print eventLog. */

#if AUDIO_IF_EN
    /* UAC interface specific fields. */
    cy_stc_hbdma_channel_t *pPDMToUsbChn;       /** PDM to USB DMA channel handle. */
    uint8_t *pPDMRxBuffer[PDM_APP_BUFFER_CNT];  /** Pointer of buffers to read PDM data into. */
    uint16_t pdmRxDataLen[PDM_APP_BUFFER_CNT];  /** Amount of data in each PDM RX buffer. */
    uint8_t  pdmRxBufIndex;                     /** Index of current PDM read buffer. */
    uint8_t  nxtAudioTxBufIndex;                /** Index of next audio buffer to be sent on USB EP. */
    uint8_t  pdmRxFreeBufCount;                 /** Number of free PDM RX buffers. */
    bool     pdmInXferPending;                  /** Whether a write has been queued on IN EP. */
    uint8_t  pdmPendingDmaFlag;                 /** Flag indicating whether DMA transfer is pending on
                                                    each PDM RX channel. */
    TaskHandle_t  uacAppTaskHandle;             /** Handle to the UAC application task. */
    QueueHandle_t uacMsgQueue;                  /** Handle to UAC application message queue. */
    TimerHandle_t pdmActivateTimer;             /** Handle of timer used to activate PDM channels. */ 
#endif /* AUDIO_IF_EN */

#if HID_EN
    /* HID interface specific fields. */
    uint8_t  hidInEpNum;                           /** Index of HID IN endpoint. */
    bool     hidReportPending;                     /** Whether a HID report write is in progress. */
    uint8_t  hidReportBuffer[HID_REPORT_SIZE];     /** Buffer for HID input report data. */
    QueueHandle_t hidMsgQueue;                     /** Message queue for HID task. */
    TaskHandle_t hidTaskHandle;                    /** HID application task handle. */
    TimerHandle_t hidTimer;                        /** Periodic HID report timer handle. */
#endif /* HID_EN */

    TimerHandle_t fpsTimer;
    uint8_t fpgaVersion;
    uint8_t *qspiWriteBuffer;
    uint8_t *qspiReadBuffer;

    uint32_t glfps;
    uint32_t glDmaBufCnt;
    uint32_t glDmaBufCnt_prv;
    volatile uint32_t glProd;
    volatile uint32_t glCons;
    uint32_t glProdCount;
    uint32_t glConsCount;
    uint32_t glFrameSizeTransferred;
    uint32_t glFrameSize;
    volatile uint8_t glPrintFlag;
    volatile uint32_t glFrameCount;
    volatile uint32_t glPartialBufSize;
};

typedef struct
{
    uint8_t *pBuffer;
    uint8_t start;
    uint8_t end;
    uint8_t dataMode;
    uint8_t dataSrc;
    uint8_t ctrlByte;
    uint16_t repeatCount;
    uint32_t dataL;
    uint32_t dataH;
    uint32_t ctrlBusVal;
    uint32_t lbPgmCount;
} cy_stc_lvds_loopback_config_t;

typedef struct
{
    uint32_t dataWord0;
    uint32_t dataWord1;
    uint32_t dataWord2;
    uint32_t dataWord3;
} cy_stc_lvds_loopback_mem_t;

typedef enum cy_en_fpgaRegMap_t
{
    /*Common Register Info*/
    FPGA_MAJOR_VERSION_ADDRESS             = 0x00,
    FPGA_MINOR_VERSION_ADDRESS             = 0x00,

    FPGA_UVC_U3V_SELECTION_ADDRESS         = 0x01,
    FPGA_UVC_ENABLE                        = 1,
    FPGA_U3V_ENABLE                        = 0,

    FPGA_UVC_HEADER_CTRL_ADDRESS           = 0x02,
    FPGA_UVC_HEADER_INMD                   = 2,
    FPGA_UVC_HEADER_ENABLE                 = 1,
    FPGA_UVC_HEADER_DISABLE                = 0, // FX10 will add UVC Header

    FPGA_LVDS_PHY_TRAINING_ADDRESS         = 0x03,
    FPGA_LVDS_PHY_TRAINING_DATA            = 0x39,

    FPGA_LVDS_LINK_TRAINING_BLK_P0_ADDRESS = 0x04,
    FPGA_LVDS_LINK_TRAINING_BLK_P1_ADDRESS = 0x05,
    FPGA_LVDS_LINK_TRAINING_BLK_P2_ADDRESS = 0x06,
    FPGA_LVDS_LINK_TRAINING_BLK_P3_ADDRESS = 0x07,
    
    FPGA_ACTIVE_DIVICE_MASK_ADDRESS        = 0x08,
    FPGA_LOW_PWR_MODE_ADDRESS              = 0x09,
    
    FPGA_PHY_LINK_CONTROL_ADDRESS          = 0x0A,
    FPGA_TRAINING_DISABLE                  = 0x00,
    FPGA_PHY_CONTROL                       = 0x01, // PHY Training is required
    FPGA_LINK_CONTROL                      = 0x02, //  Link Training is required

    P0_TRAINING_DONE                       = 0X40, // Port 0 training is completed
    P1_TRAINING_DONE                       = 0X80, // Port 1 training is completed

    FPGA_EXT_CONTROLLER_STATUS_ADDRESS     = 0X0B,
    DMA_READY_STATUS                       = 0x01, // DMA Ready flag status
    DDR_CONFIG_STATUS                      = 0x02, // DDR configuration status
    DDR_BUSY_STATUS                        = 0x04, // DDR Controller busy status
    DDR_CMD_QUEUE_FULL_STATUS              = 0x08, // Command queue full status
    DATPATH_IDLE_STATUS                    = 0x10, // Datapath is idle or not


    /*Device related Info*/
    DEVICE0_OFFSET                         = 0x20,
    DEVICE1_OFFSET                         = 0x3C,
    DEVICE2_OFFSET                         = 0x58,
    DEVICE3_OFFSET                         = 0x74,
    DEVICE4_OFFSET                         = 0x90,
    DEVICE5_OFFSET                         = 0xAC,
    DEVICE6_OFFSET                         = 0xC8,
    DEVICE7_OFFSET                         = 0xE4,

    FPGA_DEVICE_STREAM_ENABLE_ADDRESS      = 0x00,
    CAMERA_APP_DISABLE                     = 0x00,
    DMA_CH_RESET                           = 0x01,
    CAMERA_APP_ENABLE                      = 0x02,
    APP_STOP_NOTIFICATION                  = 0x04,

    FPGA_DEVICE_STREAM_MODE_ADDRESS        = 0x01,
    NO_CONVERSION                          = 0,
    INTERLEAVED_MODE                       = 0x01,
    STILL_CAPTURE                          = 0x02,
    MONO_8_CONVERSION                      = 0x04,
    YUV422_420_CONVERSION                  = 0x08,

    DEVICE_IMAGE_HEIGHT_LSB_ADDRESS        = 0x02,
    DEVICE_IMAGE_HEIGHT_MSB_ADDRESS        = 0x03,
    DEVICE_IMAGE_WIDTH_LSB_ADDRESS         = 0x04,
    DEVICE_IMAGE_WIDTH_MSB_ADDRESS         = 0x05,
    
    DEVICE_FPS_ADDRESS                     = 0x06,

    DEVICE_PIXEL_WIDTH_ADDRESS             = 0x07,
    _8_BIT_PIXEL                           = 8,
    _12BIT_PIXEL                           = 12,
    _16BIT_PIXEL                           = 16,
    _24BIT_PIXEL                           = 24,
    _36BIT_PIXEL                           = 36,

    DEVICE_SOURCE_TYPE_ADDRESS             = 0x08,
    INTERNAL_COLORBAR                      = 0x00,
    HDMI_SOURCE                            = 0x01,
    MIPI_SOURCE                            = 0x02,

    DEVICE_FLAG_STATUS_ADDRESS             = 0x09,
    SLAVE_FIFO_ALMOST_EMPTY                = 0x01, // Slave FIFO almost empty status
    INTER_MED_FIFO_EMPTY                   = 0x02, // Intermediate FIFO empty status
    INTER_MED_FIFO_FULL                    = 0x04, // Intermediate FIFO full status
    DDR_FULL_FRAME_WRITE_COMPLETE          = 0x10, // DDR write status (Full frame write complete)
    DDR_FULL_FRAME_READ_COMPLETE           = 0x20, // DDR write status (Full frame read complete)

    DEVICE_MIPI_STATUS_ADDRESS             = 0x0A,

    DEVICE_HDMI_SOURCE_INFO_ADDRESS        = 0x0B,
    HDMI_DISCONECT                         = 0x00, // Source connection status
    THIN_MIPI                              = 0x00,
    HDMI_CONNECT                           = 0x01, // Source connection status
    HDMI_DUAL_CH                           = 0x02, // 0 for single channel and 1 for dual channel
    SONY_CIS                               = 0x10, // Enable CIS ISP IP (Valid for only MIPI source)
    SONY_MIPI                              = 0x20, // Enable Crop Algorithm (Valid for only MIPI source)

    DEVICE_U3V_STREAM_MODE_ADDRESS         = 0x0C,
    DEVICE_U3V_CHUNK_MODE_ADDRESS          = 0x0D,
    DEVICE_U3V_TRIGGER_MODE_ADDRESS        = 0x0E,
    DEVICE_ACTIVE_TREAD_INFO_ADDRESS       = 0x0F,
    DEVICE_THREAD1_INFO_ADDRESS            = 0x10,
    DEVICE_THREAD2_INFO_ADDRESS            = 0x11,
    DEVICE_THREAD1_SOCKET_INFO_ADDRESS     = 0x12,
    DEVICE_THREAD2_SOCKET_INFO_ADDRESS     = 0x13,

    DEVICE_FLAG_INFO_ADDRESS               = 0x14,
    FX10_READY_TO_REC_DATA                 = 0x08,
    NEW_UVC_PACKET_START                   = 0x02,
    NEW_FRAME_START                        = 0x01,

    DEVICE_COUNTER_CRC_INFO_ADDRESS        = 0x15,
    DEVICE_BUFFER_SIZE_LSB_ADDRESS         = 0x16,
    DEVICE_BUFFER_SIZE_MSB_ADDRESS         = 0x17,

} cy_en_fpgaRegMap_t;

/* FPGA Configuration mode selection*/
typedef enum cy_en_fpgaConfigMode_t
{
    ACTIVE_SERIAL_MODE,
    PASSIVE_SERIAL_MODE
}cy_en_fpgaConfigMode_t;

typedef enum cy_en_streamControl_t
{
    STOP,
    START
}cy_en_streamControl_t;

extern cy_stc_hbdma_channel_t lvdsLbPgmChannel;
extern uint8_t glPhyLinkTrainControl;

/*****************************************************************************
* Function Name: Cy_LVDS_InitLbPgm(cy_stc_hbdma_buff_status_t *buffStat, 
                                    cy_stc_lvds_loopback_config_t *lbPgmConfig)
******************************************************************************
* Summary:
* Function to initialize Link Looback 
*
* \param buffStat
* HBDMA buffer status
*
* \param lbPgmConfig
* loopback config
*
* \return
* None
*
 *******************************************************************************/
void Cy_LVDS_InitLbPgm(cy_stc_hbdma_buff_status_t *buffStat, cy_stc_lvds_loopback_config_t *lbPgmConfig);

/*****************************************************************************
* Function Name: Cy_HbDma_LoopbackCb(cy_stc_hbdma_channel_t *handle, cy_en_hbdma_cb_type_t type,
                              cy_stc_hbdma_buff_status_t *pbufStat, void *userCtx)
******************************************************************************
* Summary:
* HBDMA callback function to commit data to loopback channel
*
* \param handle
* HBDMA channel handle
* 
* \param cy_en_hbdma_cb_type_t
* HBDMA channel type
*
* \param pbufStat
* fHBDMA buffer status
*
* \param userCtx
* user context
*
* \return
* None
*
 *******************************************************************************/
void Cy_HbDma_LoopbackCb(cy_stc_hbdma_channel_t *handle, cy_en_hbdma_cb_type_t type, cy_stc_hbdma_buff_status_t *pbufStat, void *userCtx);

/*****************************************************************************************
* Function Name: Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                    DMAC_Type *pCpuDmacBase, DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
                    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
*****************************************************************************************
* Summary:
* This function Initializes application related data structures, register callback
* creates task for device function.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD layer Context pointer
*
* \param pCpuDmacBase
* DMAC base address
*
* \param pCpuDw0Base
* DataWire 0 base address
*
* \param pCpuDw1Base
* DataWire 1 base address
*
* \param pHbDmaMgrCtxt
* HBDMA Manager Context
*
* \return
* None
*
 ************************************************************************************ */
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, DMAC_Type *pCpuDmacBase, DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base, cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt);

/*****************************************************************************
* Function Name: Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt)
******************************************************************************
* Summary:
*  This function will register all calback with USBD layer.
*
* \param pAppCtxt
* application layer context pointer.
*
* \return
* None
*
*******************************************************************************/
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt);

/****************************************************************************
* Function Name: Cy_USB_AppSetCfgCallback(void *pUserCtxt, 
                                        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                        cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when set configuration is received
*
* \param pUserCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD layer context pointer.
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppSetCfgCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppBusResetCallback(void *pAppCtxt,cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when bus detects RESET
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppBusResetCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
* 									cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              	    cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when RESET is completed
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppBusSpeedCallback(void *pAppCtxt,
* 									cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              	    cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when speed is identified or
* speed change is detected
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppSetupCallback(void *pAppCtxt,
* 									cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              	    cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when SETUP packet is received
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/*******************************************************************************
* Function name: Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                       cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* Callback function will be invoked by USBD when Suspend signal/message is detected
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/*******************************************************************************
* Function name: Cy_USB_AppResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                         cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* Callback function will be invoked by USBD when Resume signal/message is detected
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppResumeCallback (void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/****************************************************************************
* Function Name: Cy_USB_AppSetIntfCallback(void *pAppCtxt,
* 									cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              	    cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when SET Interface is called
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppSetIntfCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);


/*******************************************************************************
* Function name: Cy_USB_AppL1SleepCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                         cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when L1 Sleep message comes.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppL1SleepCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/*******************************************************************************
* Function name: Cy_USB_AppL1ResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                            cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when L1 Resume message comes.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppL1ResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);
 
/*******************************************************************************
* Function name: Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when ZLP message comes
*
* \param pUsbApp
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/*******************************************************************************
* Function name: Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when SLP message comes.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/*******************************************************************************
* Function name: Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when SET Feature message comes.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/*******************************************************************************
* Function name: Cy_USB_AppClearFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when Clear Feature message comes.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pUsbdCtxt
* USBD context
*
* \param pMsg
* USB Message
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppClearFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, cy_stc_usb_cal_msg_t *pMsg);

/*******************************************************************************
* Function name: CyApp_RegisterUsbDescriptors(cy_stc_usb_app_ctxt_t *pAppCtxt, 
                                              cy_en_usb_speed_t usbSpeed)
****************************************************************************//**
*
* This function register USB descriptor with USBD layer
*
* \param pAppCtxt
* application layer context pointer.
*
* \param usbSpeed
* USBD speed
*
* \return
* None
*
********************************************************************************/
void CyApp_RegisterUsbDescriptors(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_en_usb_speed_t usbSpeed);

/*******************************************************************************
* Function name: Cy_USB_AppQueueWrite(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber,
                          uint8_t *pBuffer, uint32_t dataSize)
****************************************************************************//**
*
* Queue USBHS Write on the USB endpoint
*
* \param pAppCtxt
* application layer context pointer.
*
* \param endpNumber
* Endpoint number
*
* \param pBuffer
* Data Buffer Pointer
*
* \param dataSize
* DataSize to send on USB bus
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppQueueWrite(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber, uint8_t *pBuffer, uint32_t dataSize);

/*******************************************************************************
* Function name: Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection,
                                       cy_israddress userIsr)
****************************************************************************//**
*
* Function to register an ISR for the DMA channel associated with an endpoint
*
* \param endpNumber
* USB endpoint number
*
* \param endpDirection
* Endpoint direction
*
* \param userIsr
*  ISR function pointer. Can be NULL if interrupt is to be disabled.
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection, cy_israddress userIsr);

/*******************************************************************************
* Function name: Cy_USB_AppClearDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                 uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection)
****************************************************************************//**
*
* Clear DMA Interrupt
*
* \param pAppCtxt
* application layer context pointer.
*
* \param endpNumber
* Endpoint number
*
* \param endpDirection
* Endpoint direction
*
* \return
* None
*
********************************************************************************/
void Cy_USB_AppClearDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt, uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection);

/*****************************************************************************
* Function Name: Cy_UVC_AppHandleSendCompletion(cy_stc_usb_app_ctxt_t  *pAppCtxt)
******************************************************************************
* Summary:
* Function that handles DMA transfer completion on the USB-HS BULK-IN
* endpoint. This is equivalent to the receipt of a cosnume event in the USB-SS use
* case.
*
* \param pAppCtxt
* application layer context pointer.
*
* \return
* None
*
 *******************************************************************************/
void Cy_UVC_AppHandleSendCompletion(cy_stc_usb_app_ctxt_t *pUsbApp);

/*****************************************************************************
 * Function Name: Cy_U3V_CommandChannel_ISR(void)
 *****************************************************************************
 * 
 * Handler for interrupts from the DataWire channel used to receive UVC  data
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_UVC_DataWire_ISR(void);

/*****************************************************************************
 * Function Name: Cy_USB_SSConnectionEnable(cy_stc_usb_app_ctxt_t *pAppCtxt)
 *****************************************************************************
 * Summary
 *  PSVP specific USB connect function.
 *
 * Parameters:
 *  \param pAppCtxt
 *  Pointer to application context structure.
 *
 * Return:
 *  void
 ****************************************************************************/
bool Cy_USB_SSConnectionEnable(cy_stc_usb_app_ctxt_t *pAppCtxt);

/*****************************************************************************
 * Function Name: Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt)
 *****************************************************************************
 * Summary
 *  PSVP specific USB disconnect function.
 *
 * Parameters: 
 *  \param pAppCtxt
 *  Pointer to application context structure.
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_USB_SSConnectionDisable(cy_stc_usb_app_ctxt_t *pAppCtxt);

/*****************************************************************************
 * Function Name: Cy_LVDS_LVCMOS_Init(void)
 *****************************************************************************
 * Summary
 *  Initialize the LVDS interface. Currently, only the SIP #0 is being initialized
 *  and configured to allow transfers into the HBW SRAM through DMA.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_LVDS_LVCMOS_Init(void);

/*****************************************************************************
 * Function Name: Cy_Update_LvdsLinkClock
 *****************************************************************************
 * Summary
 *  This function updates the clock which the LVCMOS link layer is using while
 *  operating in link loopback mode. If the active USB connection is USB-HS,
 *  the clock needs to be slowed down so that there is no overflow happening
 *  on the interface.
 *
 * Parameters:
 *  isHs: Whether active USB connection is USB-HS.
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_Update_LvdsLinkClock(bool isHs);

/****************************************************************************
* Function Name: Cy_USB_AppConfigureEndp(cy_stc_usb_app_ctxt_t *pUsbdCtxt,
                                         uint8_t *pEndpDscr)
******************************************************************************
* Summary:
*  Configure all endpoints used by application (except EP0)
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \param pEndpDscr
* Endpoint descriptor pointer
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppConfigureEndp(cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pEndpDscr);

/*****************************************************************************
 * Function Name: Cy_USB_AppSetupEndpDmaParamsSs
 ******************************************************************************
 * Summary:
 *  Perform high bandwidth DMA initialization associated with a USB endpoint.
 *
 * Parameters:
 *  \param pUsbApp
 *  Pointer to application context structure.
 *
 *  \param pEndpDscr
 * Endpoint descriptor
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_USB_AppSetupEndpDmaParams(cy_stc_usb_app_ctxt_t *pUsbApp, uint8_t *pEndpDscr);

/*****************************************************************************
 * Function Name: Cy_PDM_AppMxPdmInit(void)
 *****************************************************************************
 * Summary
 *  Initialize the PDM module as required for the USB audio class interface.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_PDM_AppMxPdmInit(void);

/*****************************************************************************
 * Function Name: Cy_PDM_AppMxPdmDeinit(void)
 *****************************************************************************
 * Summary
 *  Disable the PDM receive functionality when the audio stream is stopped.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_PDM_AppMxPdmDeinit(void);

/*****************************************************************************
 * Function Name: Cy_UAC_AppInitDmaIntr
 ******************************************************************************
 * Summary:
 *  Enable or disable the interrupt handlers for the DataWire channels reading
 *  data out from the PDM receive FIFOs.
 *
 * Parameters:
 *  \param enable
 * Whether the interrupts are to be enabled or disabled.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_UAC_AppInitDmaIntr(bool enable);

/*****************************************************************************
 * Function Name: Cy_PDM_InEpDma_ISR(void)
 ******************************************************************************
 * Summary:
 *  Interrupt service routine for completion of data transfer on USB 2.x
 *  IN endpoint used for audio streaming.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_InEpDma_ISR(void);


/*****************************************************************************
 * Function Name: Cy_UAC_AppInit
 ******************************************************************************
 * Summary:
 *  Perform all UAC specific application initialization.
 *
 * Parameters:
 *  \param pAppCtxt
 * Handle to the application context data structure.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_UAC_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt);
/*****************************************************************************
 * Function Name: Cy_PDM_AppQueueRead
 ******************************************************************************
 * Summary:
 * Initiate DataWire transfer to read data from PDM receive FIFO(s) to RAM
 * buffers allocated as part of DMA channels.
 *
 * Parameters:
 *  \param pAppCtxt
 * Pointer to application context structure.
 * 
 *  \param dataLength 
 * Length of data (in bytes) to read from each RX FIFO.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_PDM_AppQueueRead(cy_stc_usb_app_ctxt_t *pAppCtxt, uint32_t dataLength);
/*****************************************************************************
 * Function Name: Cy_UAC_AppSetIntfHandler
 ******************************************************************************
 * Summary:
 *  Set Interface request handler for the UAC audio streaming interface.
 *
 * Parameters:
 *  \param pAppCtxt 
 * Pointer to the application context structure.
 * 
 *  \param altSetting
 * Selected alternate setting for the audio streaming interface.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_UAC_AppSetIntfHandler(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t altSetting);

/*****************************************************************************
* Function Name: Cy_USB_AppPrintUsbEventLog(cy_stc_usb_app_ctxt_t *pAppCtxt, 
                                            cy_stc_usbss_cal_ctxt_t *pSSCal)
******************************************************************************
* Summary:
*  Function to print out the USB event log buffer content.
*
* Parameters:
*  \param pAppCtxt
*  Pointer to application context data structure.
*  \param pSSCal
*  Pointer to SSCAL context data structure.
*
* Return:
*  void
*****************************************************************************/
void Cy_USB_AppPrintUsbEventLog(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usbss_cal_ctxt_t *pSSCal);

/*******************************************************************************
* Function name: Cy_CheckStatus(const char *function, uint32_t line,
* 								uint8_t condition, uint32_t value,
* 								uint8_t isBlocking)
****************************************************************************//**
*
* Description: Function that handles prints error log
*
* \param function
* Pointer to function
*
* \param line
* Line number where error is seen
*
* \param condition
*  condition of failure
*
* \param value
*  error code
*
* \param isBlocking
*  blocking function
*
* \return
* None
*
********************************************************************************/
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking);

/*******************************************************************************
* Function name: Cy_USB_FailHandler(void)
****************************************************************************//**
*
* Description: Error Handler
*
*
* \return
* None
*
********************************************************************************/
void Cy_FailHandler(void);

/*******************************************************************************
* Function name: Cy_CheckStatusHandleFailure(const char *function, uint32_t line,
* 								uint8_t condition, uint32_t value,
* 								uint8_t isBlocking, void (*failureHandler)(void))
****************************************************************************//**
*
* Description: Function that handles prints error log
*
* \param function
* Pointer to function
*
* \param line
* LineNumber where error is seen
*
* \param condition
* Line number where error is seen
*
* \param value
*  error code
*
* \param isBlocking
*  blocking function
*
* \param failureHandler
*  failure handler function
*
* \return
* None
*
********************************************************************************/
void Cy_CheckStatusAndHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)());

/*****************************************************************************
* Function Name: Cy_FPGAPhyLnkTraining(void)
******************************************************************************
*
*  I2C wRites to FPGA to set up Phy & Link trianing patterns
*
* Parameters:
* 
*
* Return:
*  0 for read success, error code for unsuccess.
*****************************************************************************/
cy_en_scb_i2c_status_t Cy_FPGAPhyLnkTraining (void);

/******************************************************************************
 * Function Name: Cy_UvcInMem_AllocateBuffers
 ******************************************************************************
 *
 * Allocate the DMA buffers used to hold the colorbar data which is repeatedly
 * sent when UVC streaming from internal memory is enabled.
 *
 * \param pAppCtxt
 * Pointer to UVC application context structure.
 *
 * \return
 * true if allocation is successful, false otherwise.
 *****************************************************************************/
bool
Cy_UvcInMem_AllocateBuffers(
        cy_stc_usb_app_ctxt_t *pAppCtxt);

/******************************************************************************
 * Function Name: Cy_UvcInMem_PrepareBuffers
 ******************************************************************************
 *
 * Fill the pre-allocated RAM buffers with the UVC header and colorbar video
 * data. Also updates the DMA descriptors in the streaming DMA channel to
 * point to these RAM buffers.
 *
 * \param pAppCtxt
 * Pointer to UVC application context structure.
 *
 * \param pChannel
 * Handle to the UVC streaming channel.
 *
 * \return
 * true if buffer updates are successful, false otherwise.
 *****************************************************************************/
bool
Cy_UvcInMem_PrepareBuffers(
        cy_stc_usb_app_ctxt_t  *pAppCtxt,
        cy_stc_hbdma_channel_t *pChannel);

/******************************************************************************
 * Function Name: Cy_UvcInMem_ClearBufPointers
 ******************************************************************************
 *
 * Clear the buffer pointers in all descriptors associated with the UVC streaming
 * channel before the channel is destroyed. This ensures that the DMA buffers
 * are not freed when the channel gets destroyed.
 *
 * \param pAppCtxt
 * Pointer to UVC application context structure.
 *
 * \param pChannel
 * Handle to the UVC video streaming DMA channel.
 *
 * \return
 * true if the operation is successful, false otherwise.
 *****************************************************************************/
bool
Cy_UvcInMem_ClearBufPointers(
        cy_stc_usb_app_ctxt_t  *pAppCtxt,
        cy_stc_hbdma_channel_t *pChannel);

/******************************************************************************
 * Function Name: Cy_UvcInMem_CommitBuffers
 ******************************************************************************
 *
 * Start the video stream from the pre-filled RAM buffers by committing all
 * available descriptors in the DMA channel.
 *
 * \param pAppCtxt
 * Pointer to UVC application context structure.
 *
 * \param pChannel
 * Handle to the UVC video streaming DMA channel.
 *
 * \param frmIndex
 * Index of the currently selected video frame.
 *
 *****************************************************************************/
void Cy_UvcInMem_CommitBuffers(
        cy_stc_usb_app_ctxt_t  *pAppCtxt,
        cy_stc_hbdma_channel_t *pChannel,
        uint8_t                 frmIndex);

/******************************************************************************
 * Function Name: Cy_UvcInMem_DmaCallback
 ******************************************************************************
 *
 * Callback function which gets invoked whenever the device has finished
 * sending one of the RAM buffers with pre-filled video data. This function queues
 * the buffer for transfer again so that we have a continuous pipeline of buffers
 * ready for transfer.
 *
 * \param handle
 * Handle to the UVC video streaming DMA channel.
 *
 * \param type
 * Type of DMA event (can only be a consume event).
 *
 * \param pbufStat
 * Buffer status associated with the event. Not used in this implementation.
 *
 * \param userCtx
 * Application context passed back to the callback as an opaque pointer.
 *
 *****************************************************************************/
void
Cy_UvcInMem_DmaCallback(
        cy_stc_hbdma_channel_t      *handle,
        cy_en_hbdma_cb_type_t       type,
        cy_stc_hbdma_buff_status_t *pbufStat,
        void                       *userCtx);

#if HID_EN
void Cy_HID_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt);
void Cy_HID_InEpDma_ISR(void);
extern const uint8_t cy_hid_report_descriptor[];
#endif /* HID_EN */

#if defined(__cplusplus)
}
#endif

#endif /* _CY_USB_APP_H_ */

/* End of File */

