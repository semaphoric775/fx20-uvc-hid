/***************************************************************************//**
* \file main.c
* \version 1.0
*
* Main source file of the FX20 Integrated Audio+Video class application.
*
*******************************************************************************
* \copyright
* (c) (2021-2023), Cypress Semiconductor Corporation (an Infineon company) or
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

#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "cy_pdl.h"
#include <string.h>
#include "cy_usb_common.h"
#include "cy_usbss_cal_drv.h"
#include "cy_usb_uvc_device.h"
#include "cy_usb_usbd.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "cy_usbd_version.h"
#include "cy_hbdma_version.h"
#include "cy_lvds.h"
#include "app_version.h"
#include "timers.h"
#include "cy_usb_i2c.h"
#include "cy_usb_qspi.h"
#include "cybsp.h"
#include "cy_fx_common.h"

#if LVCMOS_EN
#include "cy_gpif_header_lvcmos.h"
#else
#include "cy_gpif_header_lvds.h"
#endif /* LVCMOS_EN */

/* Global variables associated with High BandWidth DMA setup. */
cy_stc_hbdma_context_t HBW_DrvCtxt;     /* High BandWidth DMA driver context. */
cy_stc_hbdma_dscr_list_t HBW_DscrList;  /* High BandWidth DMA descriptor free list. */
cy_stc_hbdma_buf_mgr_t HBW_BufMgr;      /* High BandWidth DMA buffer manager. */
cy_stc_hbdma_mgr_context_t HBW_MgrCtxt; /* High BandWidth DMA manager context. */
cy_stc_usb_usbd_ctxt_t usbdCtxt;
cy_stc_usb_app_ctxt_t appCtxt;
cy_stc_usbss_cal_ctxt_t ssCalCtxt;
cy_stc_usb_cal_ctxt_t hsCalCtxt;

/* CPU DMA register pointers. */
DMAC_Type *pCpuDmacBase;
DW_Type   *pCpuDw0Base;
DW_Type   *pCpuDw1Base;

uint32_t hfclkFreq = BCLK__BUS_CLK__HZ;
static uint32_t g_UsbEvtLogBuf[512u];
static uint16_t gCurUsbEvtLogIndex = 0;

extern void xPortPendSVHandler(void);
extern void xPortSysTickHandler(void);
extern void vPortSVCHandler(void);

bool glIsLVDSPhyTrainingDone = false;
bool glIsLVDSLink0TrainingDone = false;
bool glIsLVDSLink1TrainingDone = false;
uint8_t glPhyLinkTrainControl   = 0;

cy_stc_lvds_context_t lvdsContext;

/* Select SCB interface used for UART based logging. */
#define LOGGING_SCB             (SCB1)
#define LOGGING_SCB_IDX         (1)
#define DEBUG_LEVEL             (3u)

#if DEBUG_INFRA_EN
/* Debug log related initilization */
#define LOGBUF_SIZE (1024u)
uint8_t logBuff[LOGBUF_SIZE];
cy_stc_debug_config_t dbgCfg = {
    .pBuffer         = logBuff,
    .traceLvl        = DEBUG_LEVEL,
    .bufSize         = LOGBUF_SIZE,
#if USBFS_LOGS_ENABLE
    .dbgIntfce       = CY_DEBUG_INTFCE_USBFS_CDC,
#else
    .dbgIntfce       = CY_DEBUG_INTFCE_UART_SCB1,
#endif/* USBFS_LOGS_ENABLE */
    .printNow        = true
};

TaskHandle_t printLogTaskHandle;
#endif /* DEBUG_INFRA_EN */

void Cy_SysTickIntrWrapper (void)
{
    Cy_USBD_TickIncrement(&usbdCtxt);
    xPortSysTickHandler();
}

void vPortSetupTimerInterrupt(void)
{
    /* Register the exception vectors. */
    Cy_SysInt_SetVector(PendSV_IRQn, xPortPendSVHandler);
    Cy_SysInt_SetVector(SVCall_IRQn, vPortSVCHandler);
    Cy_SysInt_SetVector(SysTick_IRQn, Cy_SysTickIntrWrapper);

    /* Start the SysTick timer with a period of 1 ms. */
    Cy_SysTick_SetClockSource(CY_SYSTICK_CLOCK_SOURCE_CLK_CPU);
    Cy_SysTick_SetReload(hfclkFreq / 1000U);
    Cy_SysTick_Clear();
    Cy_SysTick_Enable();
}

#if DEBUG_INFRA_EN
void Cy_PrintTaskHandler(void *pTaskParam)
{
    while (1)
    {
        /* Print any pending logs to the output console. */
        Cy_Debug_PrintLog();

        /* Put the thread to sleep for 5 ms */
        vTaskDelay(pdMS_TO_TICKS(5));
    }
}
#endif /* DEBUG_INFRA_EN */

/*****************************************************************************
 * Function Name: Cy_LVDS_GpifEventCb(uint8_t smNo, cy_en_lvds_gpif_event_type_t gpifEvent,
 *                                    void *cntxt)
 *****************************************************************************
 * Description: GPIF error callback function.
 *
 * Parameters:
 * \param smNo
 * state machine number
 * 
 * \param gpifEvent
 * GPIF event
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_LVDS_GpifEventCb(uint8_t smNo, cy_en_lvds_gpif_event_type_t gpifEvent, void *cntxt)
{
    Cy_UVC_AppGpifIntr(&appCtxt);
}

/*****************************************************************************
 * Function Name: Cy_LVDS_PhyEventCb(uint8_t smNo, cy_en_lvds_phy_events_t phyEvent,
 *                                    void *cntxt)
 *****************************************************************************
 * Description: GPIF error callback function.
 *
 * Parameters:
 * \param smNo
 * state machine number
 * 
 * \param phyEvent
 * LVDS/LVCMOS PHY event
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_LVDS_PhyEventCb(uint8_t smNo, cy_en_lvds_phy_events_t phyEvent, void *cntxt)
{
#if FPGA_ENABLE
    cy_en_scb_i2c_status_t status;
#endif
    if(phyEvent == CY_LVDS_PHY_L1_EXIT)
    {
        if(smNo == 0)
        {
           DBG_APP_INFO("P0_L1_Exit\r\n");
        }
        else
        {
           DBG_APP_INFO("P1_L1_Exit\r\n");
        }
    }
    if(phyEvent == CY_LVDS_PHY_L1_ENTRY)
    {
        if(smNo == 0)
        {
            DBG_APP_INFO("P0_L1_Entry\r\n");
        }
        else
        {
            DBG_APP_INFO("P1_L1_Entry\r\n");
        }
    }
    if(phyEvent == CY_LVDS_PHY_L3_ENTRY)
    {
        if(smNo == 0)
        {
            DBG_APP_INFO("P0_L3_Entry\r\n");
        }
        else
        {
            DBG_APP_INFO("P1_L3_Entry\r\n");
        }
    }

#if FPGA_ENABLE
    if (phyEvent == CY_LVDS_PHY_TRAINING_DONE)
    {
        DBG_APP_INFO("Port %d PHY Train Done\r\n",smNo);
        glIsLVDSPhyTrainingDone = true;
    }

    if (phyEvent == CY_LVDS_PHY_LNK_TRAIN_BLK_DET)
    {
        DBG_APP_INFO("Port %d Training Block Detected\r\n",smNo);
        if(smNo)
        {
            glIsLVDSLink1TrainingDone = true;
            SET_BIT (glPhyLinkTrainControl, P1_TRAINING_DONE);
            status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_PHY_LINK_CONTROL_ADDRESS,glPhyLinkTrainControl,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
        }
        else
        {
#if !PORT1_EN
            glIsLVDSLink0TrainingDone = true;
            SET_BIT (glPhyLinkTrainControl, P0_TRAINING_DONE);
            status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_PHY_LINK_CONTROL_ADDRESS,glPhyLinkTrainControl,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
            ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* !PORT1_EN */
        }
    }
    if (phyEvent == CY_LVDS_PHY_LNK_TRAIN_BLK_DET_FAIL)
    {
        DBG_APP_ERR("Port %d Training Block Detect Failed\r\n",smNo);
    }
#endif /* FPGA_ENABLE */
}

/*****************************************************************************
 * Function Name: Cy_LVDS_LowPowerEventCb(cy_en_lvds_low_power_events_t lowPowerEvent, 
 *                                        void *cntxt)
 *****************************************************************************
 * Description: GPIF error callback function.
 *
 * Parameters:
 * \param lowPowerEvent
 * low power event
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_LVDS_LowPowerEventCb(cy_en_lvds_low_power_events_t lowPowerEvent, void *cntxt)
{
    if(lowPowerEvent == CY_LVDS_LOW_POWER_LNK0_L3_EXIT)
    {
        DBG_APP_INFO("P0_L3_Exit\r\n");
    }
    if(lowPowerEvent == CY_LVDS_LOW_POWER_LNK1_L3_EXIT)
    {
        DBG_APP_INFO("P1_L3_Exit\r\n");
    }
}

/*****************************************************************************
 * Function Name: Cy_LVDS_GpifErrorCb(uint8_t smNo, cy_en_lvds_gpif_error_t gpifError, 
 *                                    void *cntxt)
 *****************************************************************************
 * Description: GPIF error callback function.
 *
 * Parameters:
 * \param smNo
 * state machine number
 * 
 * \param gpifError
 * GPIF error
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_LVDS_GpifErrorCb(uint8_t smNo, cy_en_lvds_gpif_error_t gpifError, void *cntxt)
{
    switch (gpifError)
    {
        case CY_LVDS_GPIF_ERROR_IN_ADDR_OVER_WRITE:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_IN_ADDR_OVER_WRITE\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_EG_ADDR_NOT_VALID:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_EG_ADDR_NOT_VALID\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_DMA_DATA_RD_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_RD_ERROR\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_DMA_ADDR_RD_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_DATA_WR_ERROR\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_DMA_ADDR_WR_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_DMA_ADDR_WR_ERROR\n\r");
        break;

        case CY_LVDS_GPIF_ERROR_INVALID_STATE_ERROR:
        DBG_APP_ERR("CY_LVDS_GPIF_ERROR_INVALID_STATE_ERROR\n\r");
        break;
    }
}

/*****************************************************************************
 * Function Name: Cy_LVDS_GpifThreadErrorCb(cy_en_lvds_gpif_thread_no_t ThNo, 
 *                          cy_en_lvds_gpif_thread_error_t ThError, void *cntxt)
 *****************************************************************************
 * Description: GPIF thread error callback function.
 *
 * Parameters:
 * \param ThNo
 * thread number
 * 
 * \param ThError
 * thread error
 * 
 * \param cntxt
 * app context
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_LVDS_GpifThreadErrorCb (cy_en_lvds_gpif_thread_no_t ThNo, cy_en_lvds_gpif_thread_error_t ThError, void *cntxt)
{
    switch (ThError)
    {
        case CY_LVDS_GPIF_THREAD_DIR_ERROR:
        DBG_APP_ERR("Thread: %d - CY_LVDS_GPIF_THREAD_DIR_ERROR\n\r",ThNo);
        break;

        case CY_LVDS_GPIF_THREAD_WR_OVERFLOW:
        DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_WR_OVERFLOW\n\r",ThNo);
        break;

        case CY_LVDS_GPIF_THREAD_RD_UNDERRUN:
        DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_RD_UNDERRUN\n\r",ThNo);
        break;

        case CY_LVDS_GPIF_THREAD_SCK_ACTIVE:
        DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_SCK_ACTIVE\n\r",ThNo);
        break;

        case CY_LVDS_GPIF_THREAD_ADAP_OVERFLOW:
        DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_ADAP_OVERFLOW\n\r",ThNo);
        break;

        case CY_LVDS_GPIF_THREAD_ADAP_UNDERFLOW:
        DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_ADAP_UNDERFLOW\n\r",ThNo);
        break;

        case CY_LVDS_GPIF_THREAD_READ_FORCE_END:
        DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_READ_FORCE_END\n\r",ThNo);
        break;

        case CY_LVDS_GPIF_THREAD_READ_BURST_ERR:
        DBG_APP_ERR("Thread: %d -CY_LVDS_GPIF_THREAD_READ_BURST_ERR\n\r",ThNo);
        break;
    } 
}

cy_stc_lvds_app_cb_t cb =
{
    .gpif_events = Cy_LVDS_GpifEventCb,
    .gpif_error = Cy_LVDS_GpifErrorCb,
    .gpif_thread_error = Cy_LVDS_GpifThreadErrorCb,
    .gpif_thread_event = NULL,
    .phy_events = Cy_LVDS_PhyEventCb,
    .low_power_events   = Cy_LVDS_LowPowerEventCb
};

/* INMD config structure */
cy_stc_lvds_md_config_t uvcMdArray0[16] = {
    {CY_LVDS_MD_VARIABLE0,             CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_PTS_TIMER_L,           CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_PTS_TIMER_H,           CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_SCR_TIMER0_L,          CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_SCR_TIMER0_H,          CY_LVDS_MD_VARIABLE},
    {CY_LVDS_MD_SCR_TIMER1_L,          CY_LVDS_MD_VARIABLE},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT},
    {0x0000,                           CY_LVDS_MD_CONSTANT}
};

/*****************************************************************************
 * Function Name: Cy_LVDS_LVCMOS_Init
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
void Cy_LVDS_LVCMOS_Init (void)
{
    cy_en_lvds_status_t status = CY_LVDS_SUCCESS;
    uint32_t smIntrMask = LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF0_INTERRUPT_Msk;
    uint8_t portIdx = 0;
    uint8_t thrdIdx = 0;

    (void)smIntrMask;
    (void)thrdIdx;

#if LVDS_LB_EN
    cy_en_hbdma_mgr_status_t mgrstat;
    cy_stc_hbdma_chn_config_t chn_conf = 
    {
        .size = 0xF0E0,
        .count = 2,
        .chType = CY_HBDMA_TYPE_MEM_TO_IP,
        .bufferMode = true,
        .prodSckCount = 1,
        .prodSck = {CY_HBDMA_VIRT_SOCKET_WR, CY_HBDMA_VIRT_SOCKET_WR},
        .consSckCount = 1,
        .consSck = {CY_HBDMA_LVDS_SOCKET_17, CY_HBDMA_LVDS_SOCKET_17},
        .eventEnable = 0,
        .intrEnable = 0x3FF,
        .cb = Cy_HbDma_LoopbackCb,
        .userCtx = (void *)&appCtxt
    };

    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_INIT_LVDS_LB_EN);
    Cy_LVDS_Init(LVDSSS_LVDS, 1, &cy_lvds1_config, &lvdsContext);
    Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 3, 1, 0, 0, 0);
    LOG_COLOR(" LVDS Link Loop back Enabled \n\r");

    mgrstat = Cy_HBDma_Channel_Create(&HBW_MgrCtxt, &lvdsLbPgmChannel, &chn_conf);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("Loopback channel create failed 0x%x\r\n", mgrstat);
    }
#else
    Cy_LVDS_SetInterruptMask(LVDSSS_LVDS,
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_DONE_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_DONE_Msk|
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DETECTED_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DETECTED_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_TRAINING_BLK_DET_FAILD_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_TRAINING_BLK_DET_FAILD_Msk|
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L1_ENTRY_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L1_ENTRY_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L1_EXIT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L1_EXIT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK0_L3_ENTRY_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_LNK1_L3_ENTRY_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK0_INTERRUPT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_PHY_LINK1_INTERRUPT_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_THREAD0_ERR_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_THREAD1_ERR_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_THREAD2_ERR_Msk |
            LVDSSS_LVDS_LVDS_INTR_WD0_THREAD3_ERR_Msk);
#endif /* !LVDS_LB_EN */

#if PORT1_EN
    LOG_COLOR("Enabling PORT-1\n\r");
    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_PPORT1_EN);
    portIdx    = 1;
    thrdIdx    = 2;
    smIntrMask = LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF1_INTERRUPT_Msk;
#else
    LOG_COLOR("Enabling PORT-0\n\r");
    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_PPORT0_EN);
    portIdx    = 0;
    thrdIdx    = 0;
    smIntrMask = LVDSSS_LVDS_LVDS_INTR_MASK_WD0_GPIF0_INTERRUPT_Msk;
#endif /* PORT1_EN */

    /* The same configuration is used regardless of the port to be initialized. */
    status = Cy_LVDS_Init(LVDSSS_LVDS, portIdx, &cy_lvds0_config, &lvdsContext);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
    DBG_APP_INFO("SIP AFE Init done\r\n");
    
    Cy_LVDS_SetInterruptMask(LVDSSS_LVDS, smIntrMask);
    DBG_APP_INFO("Set Interrupt Mask for GPIF-SM\r\n");
    
   /* Register callbacks with the LVDS driver. */
    Cy_LVDS_RegisterCallback(LVDSSS_LVDS, &cb, &lvdsContext, &appCtxt);
	
    Cy_LVDS_Enable(LVDSSS_LVDS);
    DBG_APP_INFO("SIP enabled\r\n");

#if (FPGA_ENABLE && LINK_TRAINING)
    Cy_LVDS_PhyGpioSet(LVDSSS_LVDS, LINK_READY_CTL_PORT, LINK_READY_CTL_PIN);
#endif /* (FPGA_ENABLE && LINK_TRAINING) */

#if ((!LVCMOS_EN) && (!LVDS_LB_EN))
    /* Perform PHY training. */
    status = Cy_LVDS_PhyTrainingStart(LVDSSS_LVDS, portIdx, cy_lvds0_config.phyConfig);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
#endif /* ((!LVCMOS_EN) && (!LVDS_LB_EN)) */

#if ((LVCMOS_EN) && (!INMD_EN))
    /* In LVCMOS use case, thread needs to be enabled manually. */
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, thrdIdx, 0, 0, 0, portIdx);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);

#if INTERLEAVE_EN
    status = Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 1, 1, 0, 0, 0);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
#endif /* INTERLEAVE_EN */

    DBG_APP_INFO("GPIF thread configured\r\n");
#endif /* ((LVCMOS_EN) && (!INMD_EN)) */

#if INMD_EN
    Cy_LVDS_InitMetadata(LVDSSS_LVDS, portIdx, 0, 16, uvcMdArray0);
#if INTERLEAVE_EN
    Cy_LVDS_InitMetadata(LVDSSS_LVDS, 1, 0, 16, uvcMdArray0);
#endif /* INTERLEAVE_EN */
    LOG_COLOR("Port#0: INMD Enable \r\n");
#endif /* INMD_EN */

#if LVCMOS_EN
    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_LVCMOS_EN);
#else
    Cy_USBD_AddEvtToLog(&usbdCtxt, CY_USB_EVT_LVDS_EN);
#endif /* LVCMOS_EN */
    status = Cy_LVDS_GpifSMStart(LVDSSS_LVDS, portIdx, START_STATE_ID, ALPHA_START_STATE);
    ASSERT_NON_BLOCK(CY_LVDS_SUCCESS == status, status);
    DBG_APP_INFO("GPIF State Machine started\r\n");

#if LVDS_LB_EN
    Cy_LVDS_GpifThreadConfig(LVDSSS_LVDS, 0, 0, 0, 0, 0);
    Cy_LVDS_GpifSMStart(LVDSSS_LVDS, 1, 0, 0);
#endif /* LVDS_LB_EN */
}

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
void Cy_Update_LvdsLinkClock (bool isHs)
{
#if LVDS_LB_EN
    cy_stc_lvds_phy_config_t lpbkPhyConfig = {
        .loopbackModeEn      = true,
        .isPutLoopbackMode   = false,
        .wideLink            = 0u,
        .modeSelect          = CY_LVDS_PHY_MODE_LVCMOS,
        .dataBusWidth        = CY_LVDS_PHY_LVCMOS_MODE_NUM_LANE_16,
        .gearingRatio        = CY_LVDS_PHY_GEAR_RATIO_1_1,
        .clkSrc              = CY_LVDS_GPIF_CLOCK_USB2,
        .clkDivider          = CY_LVDS_GPIF_CLOCK_DIV_4,
        .interfaceClock      = CY_LVDS_PHY_INTERFACE_CLK_160_MHZ,
        .interfaceClock_kHz  = 120000UL,
        .phyTrainingPattern  = 0x00,
        .linkTrainingPattern = 0x00000000,
        .ctrlBusBitMap       = 0x00000000,
        .slaveFifoMode       = CY_LVDS_NORMAL_MODE,
        .dataBusDirection    = CY_LVDS_PHY_AD_BUS_DIR_INPUT,
        .lvcmosClkMode       = CY_LVDS_LVCMOS_CLK_SLAVE
    };

    if (isHs) {
        /* Slow down the LVCMOS link clock in USB-HS mode of operation. */
        lpbkPhyConfig.clkSrc             = CY_LVDS_GPIF_CLOCK_HF;
        lpbkPhyConfig.clkDivider         = CY_LVDS_GPIF_CLOCK_DIV_4;
        lpbkPhyConfig.interfaceClock_kHz = 37500UL;
    }

    Cy_LVDS_GpifConfigClock(LVDSSS_LVDS, 0, &lvdsContext, &lpbkPhyConfig);
    Cy_LVDS_GpifConfigClock(LVDSSS_LVDS, 1, &lvdsContext, &lpbkPhyConfig);
#endif /* LVDS_LB_EN */
}

/*******************************************************************************
 * Function name: Cy_Fx3g2_InitPeripheralClocks(bool adcClkEnable,
                                                bool usbfsClkEnable)
 ****************************************************************************//**
 *
 * Function used to enable clocks to different peripherals on the FX10/FX20 device.
 *
 * \param adcClkEnable
 * Whether to enable clock to the ADC in the USBSS block.
 *
 * \param usbfsClkEnable
 * Whether to enable bus reset detect clock input to the USBFS block.
 *
 *******************************************************************************/
void Cy_Fx3g2_InitPeripheralClocks (
        bool adcClkEnable,
        bool usbfsClkEnable)
{
    if (adcClkEnable) {
        /* Divide PERI clock at 75 MHz by 75 to get 1 MHz clock using 16-bit divider #1. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 1, 74);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 1);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_LVDS2USB32SS_CLOCK_SAR, CY_SYSCLK_DIV_16_BIT, 1);
    }

    if (usbfsClkEnable) {
        /* Divide PERI clock at 75 MHz by 750 to get 100 KHz clock using 16-bit divider #2. */
        Cy_SysClk_PeriphSetDivider(CY_SYSCLK_DIV_16_BIT, 2, 749);
        Cy_SysClk_PeriphEnableDivider(CY_SYSCLK_DIV_16_BIT, 2);
        Cy_SysLib_DelayUs(10U);
        Cy_SysClk_PeriphAssignDivider(PCLK_USB_CLOCK_DEV_BRS, CY_SYSCLK_DIV_16_BIT, 2);
    }
}

/*****************************************************************************
 * Function Name: Cy_LVDS_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for LVDS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_LVDS_ISR(void)
{
    Cy_LVDS_IrqHandler(LVDSSS_LVDS, &lvdsContext);
}

/*****************************************************************************
 * Function Name: Cy_LVDS_LPM_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for LVDS LPM Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_LVDS_LPM_ISR(void)
{
    Cy_LVDS_LowPowerIrqHandler(LVDSSS_LVDS, &lvdsContext);
}

/*****************************************************************************
 * Function Name: Cy_LVDS_Port0Dma_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for LVDS Port0 Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_LVDS_Port0Dma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_LVDS_0);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_LVDS_Port1Dma_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for LVDS Port1 Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_LVDS_Port1Dma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_LVDS_1);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_USB_SS_ISR(void)
 ******************************************************************************
 * Summary:
 *  Handler for USB-SS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_SS_ISR(void)
{
    /* Call the USB32DEV interrupt handler. */
    Cy_USBSS_Cal_IntrHandler(&ssCalCtxt);
}

/*****************************************************************************
 * Function Name: Cy_USB_HS_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB-HS Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_HS_ISR(void)
{
#if FREERTOS_ENABLE
    if (Cy_USBHS_Cal_IntrHandler(&hsCalCtxt))
    {
        portYIELD_FROM_ISR(true);
    }
#else
    Cy_USBHS_Cal_IntrHandler(&hsCalCtxt);
#endif /* FREERTOS_ENABLE */
}

/*****************************************************************************
 * Function Name: Cy_USB_IngressDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB Ingress DMA Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_IngressDma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_IN);
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_USB_EgressDma_ISR
 ******************************************************************************
 * Summary:
 *  Handler for USB Egress DMA Interrupts.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_EgressDma_ISR(void)
{
    /* Call the HBDMA interrupt handler with the appropriate adapter ID. */
    Cy_HBDma_HandleInterrupts(&HBW_DrvCtxt, CY_HBDMA_ADAP_USB_EG);
    portYIELD_FROM_ISR(true);
}

void Cy_UVC_DataWire_ISR (void)
{
    /* Clear the interrupt first. */
    Cy_USB_AppClearDmaInterrupt(&appCtxt, appCtxt.uvcInEpNum, CY_USB_ENDP_DIR_IN);

    /* The current data buffer has been consumed. Move to another buffer if available. */
    Cy_UVC_AppHandleSendCompletion(&appCtxt);
}

/*****************************************************************************
 * Function Name: Cy_VbusDetGpio_ISR
 *****************************************************************************
 * Summary
 *  Interrupt handler for the Vbus detect GPIO transition detection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
static void Cy_VbusDetGpio_ISR(void)
{
    BaseType_t xHigherPriorityTaskWoken;
    cy_stc_usbd_app_msg_t xMsg;

    /* Send VBus changed message to the task thread. */
    xMsg.type = CY_USB_UVC_VBUS_CHANGE_INTR;
    xQueueSendFromISR(appCtxt.uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

    /* Remember that VBus change has happened and disable the interrupt. */
    appCtxt.vbusChangeIntr = true;
    Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 0);
}

/*****************************************************************************
 * Function Name: Cy_PrintVersionInfo
 ******************************************************************************
 * Summary:
 *  Function to print version information to UART console.
 *
 * Parameters:
 *  type: Type of version string.
 *  version: Version number including major, minor, patch and build number.
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_PrintVersionInfo(const char *type, uint32_t version)
{
    char tString[32];
    uint16_t vBuild;
    uint8_t vMajor, vMinor, vPatch;
    uint8_t typeLen = strlen(type);

    vMajor = (version >> 28U);
    vMinor = ((version >> 24U) & 0x0FU);
    vPatch = ((version >> 16U) & 0xFFU);
    vBuild = (uint16_t)(version & 0xFFFFUL);

    memcpy(tString, type, typeLen);
    tString[typeLen++] = '0' + (vMajor / 10);
    tString[typeLen++] = '0' + (vMajor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vMinor / 10);
    tString[typeLen++] = '0' + (vMinor % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vPatch / 10);
    tString[typeLen++] = '0' + (vPatch % 10);
    tString[typeLen++] = '.';
    tString[typeLen++] = '0' + (vBuild / 1000);
    tString[typeLen++] = '0' + ((vBuild % 1000) / 100);
    tString[typeLen++] = '0' + ((vBuild % 100) / 10);
    tString[typeLen++] = '0' + (vBuild % 10);
    tString[typeLen++] = '\r';
    tString[typeLen++] = '\n';
    tString[typeLen] = 0;

    Cy_Debug_AddToLog(1, "%s", tString);
}

/*****************************************************************************
 * Function Name: Cy_USB_USBSSInit
 *****************************************************************************
 * Summary
 *  Initialize USBSS block and attempt device enumeration.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
void Cy_USB_USBSSInit (void)
{
    cy_stc_gpio_pin_config_t pinCfg;
    cy_stc_sysint_t intrCfg;

#if FPGA_ENABLE
    /* Initialize I2C SCB*/
    Cy_USB_I2CInit ();
#endif /* FPGA_ENABLE */

    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    cy_en_gpio_status_t gpio_status = CY_GPIO_SUCCESS;

    /* Configure VBus detect GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom     = HSIOM_SEL_GPIO;
    pinCfg.intEdge   = CY_GPIO_INTR_BOTH;
    pinCfg.intMask   = 0x01UL;
    gpio_status = Cy_GPIO_Pin_Init(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == gpio_status, gpio_status);

    /* Register edge detect interrupt for Vbus detect GPIO. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrPriority = 7;
#else
    intrCfg.cm0pSrc = VBUS_DETECT_GPIO_INTR;
    intrCfg.intrSrc = NvicMux5_IRQn;
    intrCfg.intrPriority = 3;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_VbusDetGpio_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

#if FPGA_ENABLE
    memset((void *)&pinCfg, 0, sizeof(pinCfg));

    /* Configure input GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_HIGHZ;
    pinCfg.hsiom = HSIOM_SEL_GPIO;
    gpio_status = Cy_GPIO_Pin_Init(TI180_CDONE_PORT, TI180_CDONE_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == gpio_status, gpio_status);

    /* Configure RESET FPGA GPIO. */
    pinCfg.driveMode = CY_GPIO_DM_STRONG_IN_OFF;
    pinCfg.hsiom     = TI180_INIT_RESET_GPIO;
    gpio_status = Cy_GPIO_Pin_Init(TI180_INIT_RESET_PORT, TI180_INIT_RESET_PIN, &pinCfg);
    ASSERT_NON_BLOCK(CY_GPIO_SUCCESS == gpio_status, gpio_status);
    
    Cy_GPIO_Clr(TI180_INIT_RESET_PORT, TI180_INIT_RESET_PIN);
    Cy_SysLib_Delay(20);
    Cy_GPIO_Set(TI180_INIT_RESET_PORT, TI180_INIT_RESET_PIN);
#endif /* FPGA_ENABLE */

    /* Register the LVDS ISR and enable the interrupt for LVDS. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_lvds_int_o_IRQn;
    intrCfg.intrPriority = 6;
#else
    intrCfg.intrSrc = NvicMux0_IRQn;
    intrCfg.intrPriority = 2;
    intrCfg.cm0pSrc = lvds2usb32ss_lvds_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_LVDS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register the ISR and enable the interrupt for SIP0 DMA adapter. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux4_IRQn;
    intrCfg.intrPriority = 1;
    intrCfg.cm0pSrc = lvds2usb32ss_lvds_dma_adap0_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_LVDS_Port0Dma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register the ISR and enable the interrupt for SIP1 DMA adapter. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_lvds_dma_adap1_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux4_IRQn;
    intrCfg.intrPriority = 1;
    intrCfg.cm0pSrc = lvds2usb32ss_lvds_dma_adap1_int_o_IRQn;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_LVDS_Port1Dma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register the USBSS ISR and enable the interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.intrSrc = NvicMux1_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_SS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.intrSrc = NvicMux1_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_SS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* ISR for the USB Ingress DMA adapter */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux3_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_IngressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

/* ISR for the USB Egress DMA adapter */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 3;
#else
    intrCfg.intrSrc = NvicMux2_IRQn;
    intrCfg.cm0pSrc = lvds2usb32ss_usb32_egrs_dma_int_o_IRQn;
    intrCfg.intrPriority = 0;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, &Cy_USB_EgressDma_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for and enable USBHS Interrupt. */
#if CY_CPU_CORTEX_M4
    intrCfg.intrSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrPriority = 4;
#else
    intrCfg.cm0pSrc      = usbhsdev_interrupt_u2d_active_o_IRQn;
    intrCfg.intrSrc      = NvicMux5_IRQn;
    intrCfg.intrPriority = 2;
#endif /* CY_CPU_CORTEX_M4 */
    Cy_SysInt_Init(&intrCfg, Cy_USB_HS_ISR);
    NVIC_EnableIRQ(intrCfg.intrSrc);

    /* Register ISR for LVDS LPM */
    intrCfg.intrSrc      = lvds2usb32ss_lvds_wakeup_int_o_IRQn;
    intrCfg.intrPriority = 0;
    Cy_SysInt_Init(&intrCfg, Cy_LVDS_LPM_ISR);
    NVIC_EnableIRQ(lvds2usb32ss_lvds_wakeup_int_o_IRQn);
}

/*
 * Notes on DMA Buffer RAM Usage:
 * 1. The initial part of the buffer RAM is reserved for the descriptors used by the DMA manager.
 *    The space used for this is reserved using the gHbDmaDescriptorSpace array which is placed
 *    in a section named ".hbDmaDescriptor". This array should have a minimum size of 8192 (8 KB)
 *    and has a default size allocation of 16384 bytes (16 KB). No other data should be placed
 *    in this section.
 *
 * 2. The descriptor region is followed by RW data structures which are placed in the ".descSection".
 *    Only data members placed in this section will be initialized during the firmware load
 *    process.
 *
 * 3. The ".descSection" is followed by the ".hbBufSection" which will hold data structures
 *    which do not need to be explicitly initialized (equivalent of ".bss" section).
 *
 * 4. This is followed by the ".hbDmaBufferHeap" section which will be used to allocate all
 *    the DMA buffers from. The gHbDmaBufferHeap array represents the memory region which will
 *    be given to the DMA buffer manager to allocate buffers from and can be sized based on the
 *    available memory. No other data or variables should be placed in this section.
 *
 * Any pre-initialized data which is to be placed in the High BandWidth Buffer RAM should be
 * added to the ".descSection". Any non-initialized data which is to be placed in the High
 * BandWidth Buffer RAM should be added to the ".hbBufSection".
 */

/* Region of 16 KB reserved for High BandWidth DMA descriptors. */
static __attribute__ ((section(".hbDmaDescriptor"), used)) uint32_t gHbDmaDescriptorSpace[16384 / 4];

/* Region of 960 KB reserved for DMA buffer heap. */
/*
 * Note: Since this application requires a large amount of buffer RAM, it is only supported on parts
 * that support 1 MB of DMA buffer RAM.
 */
static __attribute__ ((section(".hbDmaBufferHeap"), used)) uint32_t gHbDmaBufferHeap[960 * 1024 / 4];

/*****************************************************************************
 * Function Name: Cy_InitHbDma
 *****************************************************************************
 * Summary
 *  Initialize HBDMA block
 *
 * Parameters:
 *  None
 *
 * Return:
 *  void
 ****************************************************************************/
bool Cy_InitHbDma(void)
{
    cy_en_hbdma_status_t drvstat;
    cy_en_hbdma_mgr_status_t mgrstat;

    /* Initialize the HBW DMA driver layer. */
    drvstat = Cy_HBDma_Init(LVDSSS_LVDS, USB32DEV, &HBW_DrvCtxt, 0, 0);
    if (drvstat != CY_HBDMA_SUCCESS)
    {
        return false;
    }

    /* Verify that gHbDmaDescriptorSpace is located at the base of the DMA buffer SRAM. */
    if ((uint32_t)gHbDmaDescriptorSpace != CY_HBW_SRAM_BASE_ADDR) {
        DBG_APP_ERR("High BandWidth DMA descriptors not placed at the correct address\r\n");
        return false;
    }

    /* Setup a HBW DMA descriptor list using the space reserved in gHbDmaDescriptorSpace. */
    mgrstat = Cy_HBDma_DscrList_Create(&HBW_DscrList, sizeof(gHbDmaDescriptorSpace) / 16);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the DMA buffer manager to use the gHbDmaBufferHeap region. */
    mgrstat = Cy_HBDma_BufMgr_Create(&HBW_BufMgr, (uint32_t *)gHbDmaBufferHeap, sizeof(gHbDmaBufferHeap));
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Initialize the HBW DMA channel manager. */
    mgrstat = Cy_HBDma_Mgr_Init(&HBW_MgrCtxt, &HBW_DrvCtxt, &HBW_DscrList, &HBW_BufMgr);
    if (mgrstat != CY_HBDMA_MGR_SUCCESS)
    {
        return false;
    }

    /* Request DMA callback handling in ISR context. */
    Cy_HBDma_Mgr_DmaCallbackConfigure(&HBW_MgrCtxt, true);

#if (!LVDS_LB_EN)
    /* Both LVDS DMA adapters are used only for ingress transfers. */
    Cy_HBDma_Mgr_SetLvdsAdapterIngressMode(&HBW_MgrCtxt, true, true);
#else
    /* LVDS adapter 0 is ingress only in this case. */
    Cy_HBDma_Mgr_SetLvdsAdapterIngressMode(&HBW_MgrCtxt, true, false);
#endif /* (!LVDS_LB_EN) */

    return true;
}

/*****************************************************************************
 * Function Name: Cy_USBSS_DeInit(cy_stc_usbss_cal_ctxt_t *pCalCtxt)
 ******************************************************************************
 * Summary:
 *  Function to de initialize USBSS device
 *
 * Parameters:
 *  \param pCalCtxt
 * USB CAL layer context pointer
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USBSS_DeInit(cy_stc_usbss_cal_ctxt_t *pCalCtxt)
{
    USB32DEV_Type *base = pCalCtxt->regBase;
    USB32DEV_MAIN_Type *USB32DEV_MAIN = &base->USB32DEV_MAIN;

    /* Disable the clock for USB3.2 function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_CLK_EN_Msk;

    /* Disable PHYSS */
    base->USB32DEV_PHYSS.USB40PHY[0].USB40PHY_TOP.TOP_CTRL_0 &=
        ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
                USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    base->USB32DEV_PHYSS.USB40PHY[1].USB40PHY_TOP.TOP_CTRL_0 &=
                    ~(USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_RX_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PWR_GOOD_CORE_PLL_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_VBUS_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_REG_PHYSS_EN_Msk |
                     USB32DEV_PHYSS_USB40PHY_TOP_CTRL_0_PCLK_EN_Msk);

    /* Disable the SuperSpeed Device function */
    USB32DEV_MAIN->CTRL &= ~USB32DEV_MAIN_CTRL_SSDEV_ENABLE_Msk;
}

/*****************************************************************************
 * Function Name: Cy_USB_DisableUsbBlock(void)
 ******************************************************************************
 * Summary:
 *  Function to disable the USB32DEV IP block after terminating current
 *  connection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_DisableUsbBlock (void)
{
    /* Disable the USB32DEV IP. */
    USB32DEV->USB32DEV_MAIN.CTRL &= ~USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;

    /* Disable HBDMA adapter interrupts and the adapter itself. */
    NVIC_DisableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_DisableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);
    Cy_HBDma_DeInit(&HBW_DrvCtxt);

    DBG_APP_TRACE("Disabled HBWSS DMA adapters\r\n");
}

/*****************************************************************************
 * Function Name: Cy_USB_EnableUsbBlock(void)
 ******************************************************************************
 * Summary:
 *  Function to enable the USB32DEV IP block before enabling a new USB
 *  connection.
 *
 * Parameters:
 *  None
 *
 * Return:
 *  None
 *****************************************************************************/
void Cy_USB_EnableUsbBlock (void)
{
    /* Enable the USB DMA adapters and respective interrupts. */
    Cy_HBDma_Init(NULL, USB32DEV, &HBW_DrvCtxt, 0, 0);

    NVIC_EnableIRQ(lvds2usb32ss_usb32_ingrs_dma_int_o_IRQn);
    NVIC_EnableIRQ(lvds2usb32ss_usb32_egrs_dma_int_o_IRQn);

#if USBSS_GEN2_ENABLE
    USB32DEV->USB32DEV_PROT.PROT_SUBLINK_LSM = 0x000A000A;
#endif /* USBSS_GEN2_ENABLE */

    /* Make sure to enable USB32DEV IP. */
    USB32DEV->USB32DEV_MAIN.CTRL |= USB32DEV_MAIN_CTRL_IP_ENABLED_Msk;
}

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
bool Cy_USB_SSConnectionEnable (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
	pAppCtxt->desiredSpeed = USB_CONN_TYPE;
    Cy_USBD_ConnectDevice(pAppCtxt->pUsbdCtxt, pAppCtxt->desiredSpeed);
    pAppCtxt->usbConnected = true;
	LOG_COLOR("USB Speed %d \n\r",pAppCtxt->desiredSpeed);
    return true;
}

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
void Cy_USB_SSConnectionDisable (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    Cy_USBD_DisconnectDevice(pAppCtxt->pUsbdCtxt);
    Cy_USBSS_DeInit(pAppCtxt->pUsbdCtxt->pSsCalCtxt);
    pAppCtxt->usbConnected = false;
    pAppCtxt->devState     = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
}

/*****************************************************************************
* Function Name: main(void)
******************************************************************************
* Summary:
*  Entry to the application.
*
* Parameters:
*  void

* Return:
*  Does not return.
*****************************************************************************/
int main (void)
{
    /* Initialize the PDL driver library and set the clock variables. */
    Cy_PDL_Init (&cy_deviceIpBlockCfgFX3G2);

    /* Initialize the device clocks to the desired values. */
    cybsp_init();
    Cy_Fx3g2_InitPeripheralClocks(true, true);

    hfclkFreq = Cy_SysClk_ClkFastGetFrequency();

    /* Initialize the PDL and register ISR for USB block. */
    Cy_USB_USBSSInit();

    /* Unlock and then disable the watchdog. */
    Cy_WDT_Unlock();
    Cy_WDT_Disable();

#if CY_CPU_CORTEX_M4
    /*
     * If logging is done through the USBFS port, ISR execution is required in this application.
     * Set BASEPRI value to 0 to ensure all exceptions can run and then enable interrupts.
     */
    __set_BASEPRI(0);
#endif /* CY_CPU_CORTEX_M4 */
    __enable_irq ();

#if DEBUG_INFRA_EN
#if !USBFS_LOGS_ENABLE
    /* Initialize the UART for logging. */
    InitUart(LOGGING_SCB_IDX);

#endif /* USBFS_LOGS_ENABLE */

    Cy_Debug_LogInit(&dbgCfg);
    Cy_SysLib_Delay(500);

    Cy_Debug_AddToLog(1, "***** FX20: USB Video (UVC) Application *****\r\n");
    /* Print application, USBD stack and HBDMA version information. */
    Cy_PrintVersionInfo("APP_VERSION: ", APP_VERSION_NUM);
    Cy_PrintVersionInfo("USBD_VERSION: ", USBD_VERSION_NUM);
    Cy_PrintVersionInfo("HBDMA_VERSION: ", HBDMA_VERSION_NUM);

    /* Create task for printing logs and check status. */
    xTaskCreate(Cy_PrintTaskHandler, "PrintLogTask", 512, NULL, 5, &printLogTaskHandle);
#endif /* DEBUG_INFRA_EN */

    memset((uint8_t *)&appCtxt, 0, sizeof(appCtxt));
    memset((uint8_t *)&ssCalCtxt, 0, sizeof(ssCalCtxt));
    memset((uint8_t *)&hsCalCtxt, 0, sizeof(hsCalCtxt));
    memset((uint8_t *)&usbdCtxt, 0, sizeof(usbdCtxt));

    pCpuDmacBase = ((DMAC_Type *)DMAC_BASE);
    pCpuDw0Base = ((DW_Type *)DW0_BASE);
    pCpuDw1Base = ((DW_Type *)DW1_BASE);

#if (FPGA_ENABLE && LINK_TRAINING)
    Cy_LVDS_PhyGpioModeEnable(LVDSSS_LVDS, LINK_READY_CTL_PORT,LINK_READY_CTL_PIN,
        CY_LVDS_PHY_GPIO_OUTPUT, CY_LVDS_PHY_GPIO_NO_INTERRUPT);
    Cy_LVDS_PhyGpioClr(LVDSSS_LVDS, LINK_READY_CTL_PORT, LINK_READY_CTL_PIN);
#endif /* FPGA_ENABLE && LINK_TRAINING */

#if AUDIO_IF_EN
    Cy_PDM_AppMxPdmInit();
#endif /* AUDIO_IF_EN */

    /* Store IP base address in CAL context. */
    ssCalCtxt.regBase = USB32DEV;
    hsCalCtxt.pCalBase = MXS40USBHSDEV_USBHSDEV;
    hsCalCtxt.pPhyBase = MXS40USBHSDEV_USBHSPHY;

    /*
     * Make sure any previous USB connection state is cleared. Give some delay to allow the host to process
     * disconnection.
     */
    Cy_USBSS_DeInit(&ssCalCtxt);
    Cy_SysLib_Delay(500);

    /* Initialize the HbDma IP and DMA Manager */
    Cy_InitHbDma();
    DBG_APP_INFO("InitHbDma done\r\n");

    /* Initialize the USBD layer */
    Cy_USB_USBD_Init(&appCtxt, &usbdCtxt, pCpuDmacBase, &hsCalCtxt, &ssCalCtxt, &HBW_MgrCtxt);
    DBG_APP_INFO("USBD_Init done\r\n");

    /* Specify that DMA clock should be set to 240 MHz once USB 3.x connection is active. */
    Cy_USBD_SetDmaClkFreq(&usbdCtxt, CY_HBDMA_CLK_240_MHZ);

    /* Enable stall cycles between back-to-back AHB accesses to high bandwidth RAM. */
    MAIN_REG->CTRL = (MAIN_REG->CTRL & 0xF00FFFFFUL) | 0x09900000UL;
    /*
     * Make sure InitEventLog called before AppInit or before creating
     * thread for application.
     * Allocate a memory block and register it as USB event log buffer.
     */
    appCtxt.pUsbEvtLogBuf = (uint32_t *)g_UsbEvtLogBuf;
    Cy_USBD_InitEventLog(&usbdCtxt, appCtxt.pUsbEvtLogBuf, 512u);

    /* Initialize the application and create echo device thread. */
    Cy_USB_AppInit(&appCtxt, &usbdCtxt, pCpuDmacBase, pCpuDw0Base, pCpuDw1Base, &HBW_MgrCtxt);
    /* Register USB descriptors with the stack. */
    CyApp_RegisterUsbDescriptors(&appCtxt, CY_USBD_USB_DEV_SS_GEN1);

    /* Invokes scheduler: Not expected to return. */
    vTaskStartScheduler();
    while (1);

    return 0;
}

/*****************************************************************************
 * Function Name: Cy_OnResetUser(void)
 ******************************************************************************
 * Summary:
 *  Init function which is executed before the load regions in RAM are updated.
 *  The High BandWidth subsystem needs to be enable here to allow variables
 *  placed in the High BandWidth SRAM to be updated.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_OnResetUser(void)
{
    Cy_UsbFx_OnResetInit();
}

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
void Cy_USB_AppPrintUsbEventLog (cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usbss_cal_ctxt_t *pSSCal)
{
    uint16_t prevLogIdx = gCurUsbEvtLogIndex;

    (void)pSSCal;

    /* Print out any pending USB event log data. */
    gCurUsbEvtLogIndex = Cy_USBD_GetEvtLogIndex(pAppCtxt->pUsbdCtxt);
    while (gCurUsbEvtLogIndex != prevLogIdx) {
        DBG_APP_INFO("USBEVT: %x\r\n", pAppCtxt->pUsbEvtLogBuf[prevLogIdx]);
        prevLogIdx++;
        if (prevLogIdx == 512u) {
            prevLogIdx = 0u;
        }
    }
}

/* [] END OF FILE */
