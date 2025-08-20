/***************************************************************************//**
* \file cy_usb_app.c
* \version 1.0
*
* Implements the USB data handling part of the USB Video Class application.
*
*******************************************************************************
* \copyright
* (c) (2021-2024), Cypress Semiconductor Corporation (an Infineon company) or
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
#include "cy_device.h"
#include "cy_usb_common.h"
#include "cy_usbhs_cal_drv.h"
#include "cy_usbss_cal_drv.h"
#include "cy_hbdma.h"
#include "cy_hbdma_mgr.h"
#include "cy_usb_usbd.h"
#include "cy_usbhs_dw_wrapper.h"
#include "cy_usb_uvc_device.h"
#include "cy_usb_app.h"
#include "cy_debug.h"
#include "cy_lvds.h"
#include "cy_usb_i2c.h"
#include "cy_usb_qspi.h"


uint32_t cy_uvc_commitLength = 0;                               /* Data buffer commit length. */
uint16_t cy_uvc_buffer_counter = 1;                             /* Buffer counter for different resolutions. */

/* Default Frame Index is  4K  as 1st index */
uint16_t cy_uvc_currentFrameIndex = CY_USB_UVC_SS_4K_FRAME_INDEX;
uint16_t cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_3840_2160;
uint16_t cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_3840_2160;
uint16_t cy_usb_colorbar_size = COLORBAR_BAND_COUNT_4K;
static volatile bool cy_uvc_IsApplnActive = false;

#if LVDS_LB_EN
volatile uint32_t lvdsConsCount = 0;
volatile bool lvdsLpbkBlocked = false;
cy_stc_hbdma_channel_t lvdsLbPgmChannel;
#endif /* LVDS_LB_EN */

#if FPGA_ENABLE
bool glIsFPGAConfigComplete = false;
bool glIsFPGARegConfigured = false;
extern bool glIsLVDSPhyTrainingDone;
extern bool glIsLVDSLink0TrainingDone;
extern bool glIsLVDSLink1TrainingDone;
extern uint8_t glPhyLinkTrainControl;
extern cy_stc_hbdma_buf_mgr_t HBW_BufMgr;

#if !FPGA_CONFIG_EN
/*****************************************************************************
* Function Name: Cy_IsFPGAConfigured(void)
******************************************************************************
* Summary:
* Function to check for CDONE status
*
* Parameters:
* None
*
* Return:
*  0 if FPGA not configured, 1u if FPGA configured.
*****************************************************************************/
static bool Cy_IsFPGAConfigured(void)
{
    bool cdoneVal = false;
    uint32_t maxWait = CDONE_WAIT_TIMEOUT;

    while (cdoneVal == false)
    {
        /*Check if CDONE is HIGH or FPGA is configured */
        cdoneVal = Cy_GPIO_Read(TI180_CDONE_PORT, TI180_CDONE_PIN);
        Cy_SysLib_Delay(1);
        maxWait--;
        if (!maxWait)
        {
            break;
        }
    }

    if((maxWait == 0) && (cdoneVal == false))
    {
        LOG_ERROR("FPGA not configured \r\n");
        return false;
    }
    else
    {
        DBG_APP_INFO("FPGA is configured \r\n");
    }

    return true;
}
#endif /* !FPGA_CONFIG_EN */

/*****************************************************************************
* Function Name: Cy_UVC_SendResolution()
******************************************************************************
* Summary:
*  I2C writes to FGPA to set current video resolution
*
* Parameters:
*
*
* Return:
*  0 for read success, error code for unsuccess.
*****************************************************************************/
uint32_t Cy_UVC_SendResolution (uint16_t width, uint16_t hight, uint8_t device_offset)
{
    cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;

    status = Cy_I2C_Write(FPGASLAVE_ADDR,device_offset+DEVICE_IMAGE_WIDTH_MSB_ADDRESS,CY_USB_GET_MSB(width),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,device_offset+DEVICE_IMAGE_WIDTH_LSB_ADDRESS,CY_USB_GET_LSB(width),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,device_offset+DEVICE_IMAGE_HEIGHT_MSB_ADDRESS,CY_USB_GET_MSB(hight),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,device_offset+DEVICE_IMAGE_HEIGHT_LSB_ADDRESS,CY_USB_GET_LSB(hight),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,device_offset+FPGA_DEVICE_STREAM_MODE_ADDRESS,NO_CONVERSION,
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,device_offset+DEVICE_FPS_ADDRESS,FPS_DEFAULT,
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    DBG_APP_TRACE("I2C status = 0x%x FPS :%d \n\r",status, FPS_DEFAULT);
    return status;
} /* End of Cy_UVC_SendResolution() */

/*****************************************************************************
* Function Name: Cy_UVC_StreamStartStop(uint8_t device_offset,
                                        cy_en_streamControl_t IsStreamStart)
******************************************************************************
* Summary:
* Starts/Stops FPGA data streaming
*
* Parameters:
* \param device_offset
* device number (fpga implementation specific)
*
* \param IsStreamStart
* Pass 1 to start streaming from fpga elase 0
*
* Return:
*  0 for read success, error code for unsuccess.
*****************************************************************************/
cy_en_scb_i2c_status_t Cy_UVC_StreamStartStop(uint8_t device_offset, uint8_t IsStreamStart)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;

    if (device_offset == DEVICE0_OFFSET)
        cy_uvc_IsApplnActive = IsStreamStart?true:false;

    if(IsStreamStart)
    {
        status = Cy_I2C_Write(FPGASLAVE_ADDR,(device_offset+FPGA_DEVICE_STREAM_ENABLE_ADDRESS),CAMERA_APP_ENABLE,
                                            FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    }
    else
    {
        status = Cy_I2C_Write(FPGASLAVE_ADDR,(device_offset+FPGA_DEVICE_STREAM_ENABLE_ADDRESS),CAMERA_APP_DISABLE,
                                            FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    }


    if(status == CY_SCB_I2C_SUCCESS)
    {
        DBG_APP_INFO("Cy_UVC_StreamStartStop: Video App started = %d device_offset 0x%x\n\r",IsStreamStart, device_offset);
    }
    else
    {
        DBG_APP_ERR(" Cy_UVC_StreamStartStop: Video App start failed \n\r");
    }

    return status;
}/* End of Cy_UVC_StreamStartStop() */


/*****************************************************************************
* Function Name: Cy_UVC_SetVideoResolution
******************************************************************************
* Summary:
*  Read the i2c
*
* Parameters:
* \param format_index
* Video format Index
*
* \param resolution_index
* Video resolution Index
*
* \param device_offset
* device number (fpga implementation specific)
*
* \param devSpeed
* device USB speed
*
* Return:
*  0 for read success, error code for unsuccess.
*****************************************************************************/
uint32_t Cy_UVC_SetVideoResolution (uint8_t format_index, uint8_t resolution_index, uint8_t device_offset, uint8_t devSpeed)
{
    uint32_t status = 0;
    DBG_APP_INFO("format_index %d resolution_index %d devSpeed = %d\r\n", format_index,resolution_index, devSpeed);
    if(devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
    {
        switch(resolution_index)
        {
            case CY_USB_UVC_SS_4K_FRAME_INDEX:
            /*I2C sensor reg writes can be added here*/
            status = Cy_UVC_SendResolution (3840,  2160, device_offset);
            Cy_SysLib_DelayUs (1000);
            break;

            case CY_USB_UVC_SS_720P_FRAME_INDEX:
            /*I2C sensor reg writes can be added here*/
            if(status)
            DBG_APP_ERR("Error I2C write with Sensor\r\n");
            Cy_SysLib_DelayUs (1000);
             status = Cy_UVC_SendResolution (1280,  720, device_offset);
            break;

            case CY_USB_UVC_SS_VGA_FRAME_INDEX:
            /*I2C sensor reg writes can be added here*/
            Cy_SysLib_DelayUs (1000);
             status = Cy_UVC_SendResolution (640,  480, device_offset);
            break;

            default:
            break;
        }
    }
    else
    {
        switch(resolution_index)
        {
            case CY_USB_UVC_HS_VGA_FRAME_INDEX:
            /*I2C sensor reg writes can be added here*/
            status = Cy_UVC_SendResolution (640,  480, device_offset);
            break;
        }

    }
    return status;
} /* End of Cy_UVC_SetVideoResolution() */

/*****************************************************************************
* Function Name: Cy_ConfigFpgaRegister(void)
******************************************************************************
* Summary:
*  FPGA Register Writes. FPGA is configured to send internally generated
*  colorbar data over FX2G3's SlaveFIFO Interface
*
* Parameters:
*
*
* Return:
*  0 for read success, error code for unsuccess.
*****************************************************************************/
static cy_en_scb_i2c_status_t Cy_ConfigFpgaRegister (void)
{
    cy_en_scb_i2c_status_t status = CY_SCB_I2C_SUCCESS;
    /* Defalut resolution*/
    uint16_t width = 3840;
    uint16_t height = 2160;

    /* Disable camera before configuring FPGA register */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+FPGA_DEVICE_STREAM_ENABLE_ADDRESS,CAMERA_APP_DISABLE,
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* write FPGA register to enable UVC */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_U3V_SELECTION_ADDRESS,FPGA_UVC_ENABLE,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#if INMD_EN
    DBG_APP_INFO("INMD Header \n\r");
     /* Enable INMD added UVC header */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_HEADER_CTRL_ADDRESS,FPGA_UVC_HEADER_INMD,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
#elif UVC_HEADER_BY_FPGA
    DBG_APP_INFO("UVC Header by FPGA! \n\r");

    /* Enable adding UVC header by FPGA. */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_HEADER_CTRL_ADDRESS,FPGA_UVC_HEADER_ENABLE,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
#elif UVC_HEADER_BY_FX
    DBG_APP_INFO("UVC Header by FX! \n\r");
    /* Disable adding UVC header by FPGA. UVC header is added by FX20 */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_UVC_HEADER_CTRL_ADDRESS,FPGA_UVC_HEADER_DISABLE,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
#endif /* INMD_EN */

    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(500);


    /* Number of active device list*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_ACTIVE_DIVICE_MASK_ADDRESS,0x01,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);

    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(500);

    /* write FPGA register to Disable format converstion */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+FPGA_DEVICE_STREAM_MODE_ADDRESS,NO_CONVERSION,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Select colorbar mode by default if DYNAMIC_VIDEOSOURCE is true*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_SOURCE_TYPE_ADDRESS,INTERNAL_COLORBAR,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#if INTERLEAVE_EN
    /*
     * In Wide link mode, actual socket number each thread is connected to is derived from SSAD[kkk] control byte sent by FPGA.
     * Actual socket number =  (3'bkkk x 2) + tt[0] on adapter tt[1], where tt is the thread number sent in STAD[tt]
     * if FPGA sets kkk as 0 for Thread 0, actual socket number connected to Thread 0 is 0
     * if FPGA sets kkk as 0 for Thread 1, actual socket number connected to Thread 1 is 1
     */
    /* Inform active threads*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_ACTIVE_TREAD_INFO_ADDRESS,2,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);


    /* inform active sockets*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_SOCKET_INFO_ADDRESS,0,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_INFO_ADDRESS,1,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#else /* Single Thread*/
    /* Inform active threads*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_ACTIVE_TREAD_INFO_ADDRESS,1,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#if PORT1_EN
    /* inform active sockets*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_SOCKET_INFO_ADDRESS,0,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Thread 2 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_INFO_ADDRESS,2,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#else
    /* inform active sockets*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_SOCKET_INFO_ADDRESS,0,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Thread 2 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD2_INFO_ADDRESS,0,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* PORT1_EN */
#endif /* INTERLEAVE_EN */

#if PORT1_EN
    /* Thread 1 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_INFO_ADDRESS,2,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Thread 1 socket information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_SOCKET_INFO_ADDRESS,0,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

#else

    /* Thread 1 information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_INFO_ADDRESS,0,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Thread 1 socket information*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_THREAD1_SOCKET_INFO_ADDRESS,0,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
#endif /* PORT1_EN */

    /* Clear FPGA register during power up, this will get update when firmware detects HDMI */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_HDMI_SOURCE_INFO_ADDRESS,HDMI_DISCONECT,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    DBG_APP_INFO("DMA Buffer SIZE %d \n\r",FPGA_DMA_BUFFER_SIZE);

    /* Update DMA buffer size used by Firmware */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_BUFFER_SIZE_MSB_ADDRESS,CY_GET_MSB(FPGA_DMA_BUFFER_SIZE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_BUFFER_SIZE_LSB_ADDRESS,CY_GET_LSB(FPGA_DMA_BUFFER_SIZE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Update default resolution width size used by Firmware */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_WIDTH_MSB_ADDRESS,CY_GET_MSB(width),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_WIDTH_LSB_ADDRESS,CY_GET_LSB(width),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* Update default resolution height size used by Firmware */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_HEIGHT_MSB_ADDRESS,CY_GET_MSB(height),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_IMAGE_HEIGHT_LSB_ADDRESS,CY_GET_LSB(height),
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    /* default fps*/
    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+DEVICE_FPS_ADDRESS,FPS_DEFAULT,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);

    return status;
} /* End of Cy_ConfigFpgaRegister() */

/*****************************************************************************
* Function Name: Cy_FPGAPhyLnkTraining(void)
******************************************************************************
* Summary:
*  I2C wRites to FPGA to set up Phy & Link trianing patterns
*
* Parameters:
*
*
* Return:
*  0 for read success, error code for unsuccess.
*****************************************************************************/
cy_en_scb_i2c_status_t Cy_FPGAPhyLnkTraining (void)
{
    cy_en_scb_i2c_status_t  status = CY_SCB_I2C_SUCCESS;
    /* write FPGA register to Disable format converstion */

    status = Cy_I2C_Write(FPGASLAVE_ADDR,DEVICE0_OFFSET+FPGA_DEVICE_STREAM_ENABLE_ADDRESS,CAMERA_APP_DISABLE,
                                              FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(10);
#if LINK_TRAINING
#if LVCMOS_DDR_EN
    SET_BIT (glPhyLinkTrainControl, FPGA_LINK_CONTROL);
    DBG_APP_INFO("Link training \n\r");
#else
    SET_BIT (glPhyLinkTrainControl, FPGA_LINK_CONTROL|FPGA_PHY_CONTROL);
#endif /* LVCMOS_DDR_EN */
    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_PHY_LINK_CONTROL_ADDRESS,glPhyLinkTrainControl,
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR, FPGA_LVDS_PHY_TRAINING_ADDRESS, PHY_TRAINING_PATTERN_BYTE,
                           FPGA_I2C_ADDRESS_WIDTH, FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs(10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_LINK_TRAINING_BLK_P0_ADDRESS,CY_USB_DWORD_GET_BYTE0(LINK_TRAINING_PATTERN_BYTE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_LINK_TRAINING_BLK_P1_ADDRESS,CY_USB_DWORD_GET_BYTE1(LINK_TRAINING_PATTERN_BYTE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_LINK_TRAINING_BLK_P2_ADDRESS,CY_USB_DWORD_GET_BYTE2(LINK_TRAINING_PATTERN_BYTE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (10);

    status = Cy_I2C_Write(FPGASLAVE_ADDR,FPGA_LVDS_LINK_TRAINING_BLK_P3_ADDRESS,CY_USB_DWORD_GET_BYTE3(LINK_TRAINING_PATTERN_BYTE),
                                          FPGA_I2C_ADDRESS_WIDTH,FPGA_I2C_DATA_WIDTH);
    ASSERT_NON_BLOCK(status == CY_SCB_I2C_SUCCESS, status);
    Cy_SysLib_DelayUs (10);
#endif /* LINK_TRAINING */

    return status;
} /* End of Cy_FPGAPhyLnkTraining() */
#endif /* FPGA_ENABLE */

/*****************************************************************************
* Function Name: Cy_UVC_AppStart
******************************************************************************
* Summary:
* Start the data stream channel/s
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* \param format_index
* Video format index
*
* \param frame_index
* Video resolution index
*
* \param DeviceIndex
* device number (fpga implementation specific)
*
* Return:
* 0 for read success, error code for unsuccess
*****************************************************************************/
static cy_en_hbdma_mgr_status_t Cy_UVC_AppStart(cy_stc_usb_app_ctxt_t *pAppCtxt,uint32_t format_index, uint32_t frame_index, uint16_t DeviceIndex)
{
    cy_en_hbdma_mgr_status_t mgrStatus = CY_HBDMA_MGR_SUCCESS;
    DBG_APP_INFO("AppStart\r\n");

    if (DEVICE0_OFFSET == DeviceIndex)
    {
        mgrStatus = Cy_HBDma_Channel_Enable(pAppCtxt->hbBulkInChannel, 0);
        ASSERT_NON_BLOCK(mgrStatus == CY_HBDMA_MGR_SUCCESS, mgrStatus);

        pAppCtxt->glfps                  = 0;
        pAppCtxt->glDmaBufCnt            = 0;
        pAppCtxt->glDmaBufCnt_prv        = 0;
        pAppCtxt->glProd                 = 0;
        pAppCtxt->glCons                 = 0;
        pAppCtxt->glProdCount            = 0;
        pAppCtxt->glConsCount            = 0;
        pAppCtxt->glFrameSizeTransferred = 0;
        pAppCtxt->glFrameSize            = 0;
        pAppCtxt->glPrintFlag            = 0;
        pAppCtxt->glFrameCount           = 0;
        pAppCtxt->glPartialBufSize       = 0;
    }
#if FPGA_ENABLE
    if (Cy_UVC_SetVideoResolution(format_index, frame_index, DeviceIndex, pAppCtxt->devSpeed) != 0)
    {
        DBG_APP_ERR("Error Sending Resolution\r\n");
    }
    if (!cy_uvc_IsApplnActive)
    {
        Cy_LVDS_GpifSMSwitch(LVDSSS_LVDS, 0, 255, 0, 0, 12, 2 );
        DBG_APP_INFO("Cy_LVDS_GpifSMSwitch line %d\r\n", __LINE__);
    }
    Cy_UVC_StreamStartStop(DeviceIndex, START);
#endif /* FPGA_ENABLE */
    cy_uvc_IsApplnActive = true;
    return mgrStatus;
}

/*****************************************************************************
* Function Name: Cy_UVC_AppStop(cy_stc_usb_app_ctxt_t *pAppCtxt,
                cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint16_t wIndex)
******************************************************************************
* Summary:
* Stop the data stream channels
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* \param pUsbdCtxt
* USBD layer context pointer
*
* \param wIndex
* USB streaming endpoint numbder
*
* Return:
* void
*****************************************************************************/
static void Cy_UVC_AppStop(cy_stc_usb_app_ctxt_t *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint16_t wIndex)
{
    cy_en_hbdma_mgr_status_t status = CY_HBDMA_MGR_SUCCESS;
    uint32_t epNumber = ((uint32_t)wIndex & 0x7FUL);
    uint8_t index = 0;
    cy_stc_hbdma_sock_t sckStat;
    uint32_t pollCnt = 0;

    DBG_APP_INFO("App Stop:windex=0x%x\r\n",wIndex);

    if ((cy_uvc_IsApplnActive == true) && (pAppCtxt->hbBulkInChannel != NULL))
    {
        /* Wait for DMA sockets to stall */
        for(index = 0; index < pAppCtxt->hbBulkInChannel->prodSckCount; index++)
        {
            do {
                Cy_SysLib_DelayUs(10);
                Cy_HBDma_GetSocketStatus(pAppCtxt->pHbDmaMgrCtxt->pDrvContext, pAppCtxt->hbBulkInChannel->prodSckId[index], &sckStat);
            } while(
                    ((_FLD2VAL(LVDSSS_LVDS_ADAPTER_DMA_SCK_STATUS_STATE,sckStat.status)) != 0x1) &&
                    (pollCnt++ < 500)
                   );

            DBG_APP_INFO("DMA Socket %x is stalled (%x)\r\n", pAppCtxt->hbBulkInChannel->prodSckId[index],
                    sckStat.status);
        }
    }

#if FPGA_ENABLE
    uint16_t DeviceIndex = 0x00;
    if(UVC_STREAM_ENDPOINT == epNumber)
        DeviceIndex = DEVICE0_OFFSET;

    Cy_UVC_StreamStartStop(DeviceIndex, STOP);
#endif /* FPGA_ENABLE */

    if (UVC_STREAM_ENDPOINT == epNumber)
    {
        /* Reset the DMA channel through which data is received from the LVDS side. */
        status = Cy_HBDma_Channel_Reset(pAppCtxt->hbBulkInChannel);
        ASSERT_NON_BLOCK(CY_HBDMA_MGR_SUCCESS == status,status);

        cy_uvc_IsApplnActive             = false;
        pAppCtxt->uvcPendingBufCnt       = 0;
        pAppCtxt->glfps                  = 0;
        pAppCtxt->glDmaBufCnt            = 0;
        pAppCtxt->glDmaBufCnt_prv        = 0;
        pAppCtxt->glProd                 = 0;
        pAppCtxt->glCons                 = 0;
        pAppCtxt->glProdCount            = 0;
        pAppCtxt->glConsCount            = 0;
        pAppCtxt->glFrameSizeTransferred = 0;
        pAppCtxt->glFrameSize            = 0;
        pAppCtxt->glPrintFlag            = 0;
        pAppCtxt->glFrameCount           = 0;
        pAppCtxt->glPartialBufSize       = 0;
        DBG_APP_INFO("UVC DMA Reset and Variable Cleared\r\n");
    }
    /* On USB 2.0 connection, reset the DataWire channel used to send data to the EPM. */
    if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS)
    {
        Cy_USBHS_App_ResetEpDma(&(pAppCtxt->endpInDma[epNumber]));
    }

    if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        Cy_USBSS_Cal_ClkStopOnEpRstEnable(pUsbdCtxt->pSsCalCtxt, true);
    }

    /* Flush and reset the endpoint and clear the STALL bit. */
    Cy_USBD_FlushEndp(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN);
    Cy_USBD_ResetEndp(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN, false);
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, epNumber, CY_USB_ENDP_DIR_IN, false);

    if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        Cy_USBSS_Cal_ClkStopOnEpRstEnable(pUsbdCtxt->pSsCalCtxt, false);
    }
}

/* UVC Probe Control Settings for a USB 3.0 connection. */

/* 3840*2160 @30 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_4K[CY_USB_UVC_MAX_PROBE_SETTING];
uint8_t cy_uvc_probectrl_4K_[]= {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_SS_4K_FRAME_INDEX,       /* Use 1st Video frame index */
    0x15, 0x16, 0x05, 0x00,             /* Desired frame interval in the unit of 100ns: 30 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units */
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate*/
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x20, 0xFD, 0x00,             /* Max video frame size in bytes: 3840*2160*2 = 0x00FD2000*/
#if (UVC_HEADER_BY_FPGA || INMD_EN)
    CY_USB_DWORD_GET_BYTE0(CY_USB_FULL_FRAME_SIZE_4K), /* No. of bytes device can rx in single payload*/
    CY_USB_DWORD_GET_BYTE1(CY_USB_FULL_FRAME_SIZE_4K),
    CY_USB_DWORD_GET_BYTE2(CY_USB_FULL_FRAME_SIZE_4K),
    CY_USB_DWORD_GET_BYTE3(CY_USB_FULL_FRAME_SIZE_4K),
#else
    CY_USB_DWORD_GET_BYTE0(CY_USB_UVC_STREAM_BUF_SIZE), /* No. of bytes device can rx in single payload*/
    CY_USB_DWORD_GET_BYTE1(CY_USB_UVC_STREAM_BUF_SIZE),
    CY_USB_DWORD_GET_BYTE2(CY_USB_UVC_STREAM_BUF_SIZE),
    CY_USB_DWORD_GET_BYTE3(CY_USB_UVC_STREAM_BUF_SIZE),
#endif /* UVC_HEADER_BY_FPGA */
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* 1280*720 @30 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_720p[CY_USB_UVC_MAX_PROBE_SETTING];
uint8_t cy_uvc_probectrl_720p_[]= {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_SS_720P_FRAME_INDEX,     /* Use 2nd Video frame index */
    0x15, 0x16, 0x05, 0x00,             /* Desired frame interval in the unit of 100ns: 30 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units */
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate*/
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x20, 0x1C, 0x00,             /* Max video frame size in bytes: 1280*720*2 = 0x001C2000 */
#if (UVC_HEADER_BY_FPGA || INMD_EN)
    CY_USB_DWORD_GET_BYTE0(CY_USB_FULL_FRAME_SIZE_720P), /* No. of bytes device can rx in single payload*/
    CY_USB_DWORD_GET_BYTE1(CY_USB_FULL_FRAME_SIZE_720P),
    CY_USB_DWORD_GET_BYTE2(CY_USB_FULL_FRAME_SIZE_720P),
    CY_USB_DWORD_GET_BYTE3(CY_USB_FULL_FRAME_SIZE_720P),
#else
    CY_USB_DWORD_GET_BYTE0(CY_USB_UVC_STREAM_BUF_SIZE), /* No. of bytes device can rx in single payload*/
    CY_USB_DWORD_GET_BYTE1(CY_USB_UVC_STREAM_BUF_SIZE),
    CY_USB_DWORD_GET_BYTE2(CY_USB_UVC_STREAM_BUF_SIZE),
    CY_USB_DWORD_GET_BYTE3(CY_USB_UVC_STREAM_BUF_SIZE),
#endif /* UVC_HEADER_BY_FPGA || INMD_EN */
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* 640*480  @30 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_VGA[CY_USB_UVC_MAX_PROBE_SETTING];
uint8_t cy_uvc_probectrl_VGA_[] = {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_SS_VGA_FRAME_INDEX,      /* Use 3rd Video frame index */
    0x15, 0x16, 0x05, 0x00,             /* Desired frame interval in the unit of 100ns: 30 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units*/
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate */
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x60, 0x09, 0x00,             /* Max video frame size in bytes: 640*480*2 = 0x00096000 */
#if (UVC_HEADER_BY_FPGA || INMD_EN)
    CY_USB_DWORD_GET_BYTE0(CY_USB_FULL_FRAME_SIZE_VGA), /* No. of bytes device can rx in single payload*/
    CY_USB_DWORD_GET_BYTE1(CY_USB_FULL_FRAME_SIZE_VGA),
    CY_USB_DWORD_GET_BYTE2(CY_USB_FULL_FRAME_SIZE_VGA),
    CY_USB_DWORD_GET_BYTE3(CY_USB_FULL_FRAME_SIZE_VGA),
#else
    CY_USB_DWORD_GET_BYTE0(CY_USB_UVC_STREAM_BUF_SIZE), /* No. of bytes device can rx in single payload*/
    CY_USB_DWORD_GET_BYTE1(CY_USB_UVC_STREAM_BUF_SIZE),
    CY_USB_DWORD_GET_BYTE2(CY_USB_UVC_STREAM_BUF_SIZE),
    CY_USB_DWORD_GET_BYTE3(CY_USB_UVC_STREAM_BUF_SIZE),
#endif /* UVC_HEADER_BY_FPGA */
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* USBHS: 640*480  @15 fps */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_probectrl_HS_VGA[CY_USB_UVC_MAX_PROBE_SETTING];
uint8_t cy_uvc_probectrl_HS_VGA_[] = {
    0x00, 0x00,                         /* bmHint : no hit */
    0x01,                               /* Use 1st Video format index */
    CY_USB_UVC_HS_VGA_FRAME_INDEX,      /* Use 1st Video frame index */
    0x2A, 0x2C, 0x0A, 0x00,             /* Desired frame interval in the unit of 100ns: 15 fps */
    0x00, 0x00,                         /* Key frame rate in key frame/video frame units*/
    0x00, 0x00,                         /* PFrame rate in PFrame / key frame units */
    0x00, 0x00,                         /* Compression quality control */
    0x00, 0x00,                         /* Window size for average bit rate */
    0x00, 0x00,                         /* Internal video streaming i/f latency in ms */
    0x00, 0x60, 0x09, 0x00,             /* Max video frame size in bytes: 640*480*2 = 0x00096000 */
#if (UVC_HEADER_BY_FPGA || INMD_EN)
    CY_USB_DWORD_GET_BYTE0(CY_USB_FULL_FRAME_SIZE_VGA), /* No. of bytes device can rx in single payload*/
    CY_USB_DWORD_GET_BYTE1(CY_USB_FULL_FRAME_SIZE_VGA),
    CY_USB_DWORD_GET_BYTE2(CY_USB_FULL_FRAME_SIZE_VGA),
    CY_USB_DWORD_GET_BYTE3(CY_USB_FULL_FRAME_SIZE_VGA),
#else
    CY_USB_DWORD_GET_BYTE0(CY_USB_UVC_STREAM_BUF_SIZE), /* No. of bytes device can rx in single payload*/
    CY_USB_DWORD_GET_BYTE1(CY_USB_UVC_STREAM_BUF_SIZE),
    CY_USB_DWORD_GET_BYTE2(CY_USB_UVC_STREAM_BUF_SIZE),
    CY_USB_DWORD_GET_BYTE3(CY_USB_UVC_STREAM_BUF_SIZE),
#endif /* UVC_HEADER_BY_FPGA */
    0x00, 0x60, 0xE3, 0x16,             /* Device Clock */
    0x00, 0x00, 0x00, 0x00              /* Framing and format information. */
};

/* Video Probe Commit Control */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_commitctrl[CY_USB_UVC_MAX_PROBE_SETTING_ALIGNED];

/* Whether SET_CONFIG is complete or not. */
static volatile bool cy_uvc_devconfigured = false;

/* UVC Header */
USB3_DESC_ATTRIBUTES uint8_t cy_uvc_header[CY_USB_UVC_MAX_HEADER];
uint8_t cy_uvc_header_[] = {
    32,                                 /* Header Length */
    0x8C,                               /* Bit field header field */
    0x00, 0x00, 0x00, 0x00,             /* Presentation time stamp field */
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, /* Source clock reference field */
    0x00, 0x00, 0x00, 0x00, 0x00,       /* Zero padding for 32 byte align */
    0x00, 0x00, 0x00, 0x00, 0x00,       /* Zero padding for 32 byte align */
    0x00, 0x00, 0x00, 0x00, 0x00,       /* Zero padding for 32 byte align */
    0x00, 0x00, 0x00, 0x00, 0x00        /* Zero padding for 32 byte align */
};

HBDMA_BUF_ATTRIBUTES uint32_t Ep0TestBuffer[32U];
HBDMA_BUF_ATTRIBUTES uint32_t SetSelDataBuffer[8];

/*****************************************************************************
* Function Name: Cy_UVC_AppGpifIntr(cy_stc_usb_app_ctxt_t *pApp)
******************************************************************************
* Summary:
* GPIF error handler
*
* Parameters:
* \param pApp
* application layer context pointer
*
* Return:
* void
*****************************************************************************/
void
Cy_UVC_AppGpifIntr(void *pApp)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    pAppCtxt = (cy_stc_usb_app_ctxt_t*)pApp;

    /* Reset the DMA channel through which data is received from the LVDS side. */
    Cy_HBDma_Channel_Reset(pAppCtxt->hbBulkInChannel);

    if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        Cy_USBSS_Cal_ClkStopOnEpRstEnable(pAppCtxt->pUsbdCtxt->pSsCalCtxt, true);
    }

    /* Flush and reset the endpoint and clear the STALL bit. */
    Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum, CY_USB_ENDP_DIR_IN);
    Cy_USBD_ResetEndp(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum, CY_USB_ENDP_DIR_IN, false);
    Cy_USB_USBD_EndpSetClearStall(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum,CY_USB_ENDP_DIR_IN, false);

    if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
        Cy_USBSS_Cal_ClkStopOnEpRstEnable(pAppCtxt->pUsbdCtxt->pSsCalCtxt, false);
    }

    pAppCtxt->uvcPendingBufCnt = 0;
    Cy_USBD_SendAckSetupDataStatusStage(pAppCtxt->pUsbdCtxt);

    return;
}

#if LVDS_LB_EN
void Cy_LVDS_InitLbPgm(cy_stc_hbdma_buff_status_t *buffStat, cy_stc_lvds_loopback_config_t *lbPgmConfig)
{
    cy_stc_lvds_loopback_mem_t lbPgm;
    lbPgm.dataWord0 =   ((0x00000001) |
                        (lbPgmConfig->start << 1) |
                        (lbPgmConfig->end << 2) |
                        (lbPgmConfig->dataMode << 4) |
                        (lbPgmConfig->repeatCount << 8) |
                        (lbPgmConfig->dataSrc << 20));
    lbPgm.dataWord1 =   ((lbPgmConfig->ctrlByte) |
                        (lbPgmConfig->ctrlBusVal << 12));
    lbPgm.dataWord2 = lbPgmConfig->dataL;
    lbPgm.dataWord3 = lbPgmConfig->dataH;
    lbPgmConfig->pBuffer = buffStat->pBuffer + lbPgmConfig->lbPgmCount * 16;
    memcpy(lbPgmConfig->pBuffer, &lbPgm, 16);
    lbPgmConfig->lbPgmCount += 1;
}

void Cy_LVDS_CommitColorbarData(void)
{
    uint32_t loop = 0, lineCount = 0;
    cy_stc_hbdma_buff_status_t buffStat;
    cy_stc_lvds_loopback_config_t lbPgmConfig = {
        .lbPgmCount = 0,
        .pBuffer = NULL,
        .start = 0,
        .end = 0,
        .dataMode = 0x00,
        .dataSrc = 0x00,
        .ctrlByte = 0x01,
        .repeatCount = 0x0001,
        .dataL = 0x00000000,
        .dataH = 0xDEADBEEF,
        .ctrlBusVal = 0x00000000
    };

    if (Cy_HBDma_Channel_GetBuffer(&lvdsLbPgmChannel, &buffStat) != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("GetLpbkBuf 1 failed\r\n");
        return;
    }

    lbPgmConfig.pBuffer = buffStat.pBuffer;

    /* Start command */
    lbPgmConfig.start = 1;
    Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);

    /* Few cycles of IDLE */
    lbPgmConfig.start = 0;
    for(loop = 0; loop < 10; loop++)
    {
        Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
    }

    /* DATA control byte*/
    lbPgmConfig.ctrlByte = 0x80;
    for (lineCount = 0; lineCount < (480 / cy_usb_colorbar_size); lineCount++)
    {
        /* BAND1 of colorbar */
        lbPgmConfig.dataL = BAND1_COLOR_YUYV;
        lbPgmConfig.dataH = BAND1_COLOR_YUYV;
        for(loop = 0; loop < cy_usb_colorbar_size; loop++)
        {
            Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
        }

        /* BAND2 of colorbar */
        lbPgmConfig.dataL = BAND2_COLOR_YUYV;
        lbPgmConfig.dataH = BAND2_COLOR_YUYV;
        for(loop = 0; loop < cy_usb_colorbar_size; loop++)
        {
            Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
        }

        /* BAND3 of colorbar */
        lbPgmConfig.dataL = BAND3_COLOR_YUYV;
        lbPgmConfig.dataH = BAND3_COLOR_YUYV;
        for(loop = 0; loop < cy_usb_colorbar_size; loop++)
        {
            Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
        }

        /* BAND4 of colorbar */
        lbPgmConfig.dataL = BAND4_COLOR_YUYV;
        lbPgmConfig.dataH = BAND4_COLOR_YUYV;
        for(loop = 0; loop < cy_usb_colorbar_size; loop++)
        {
            Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
        }

        /* BAND5 of colorbar */
        lbPgmConfig.dataL = BAND5_COLOR_YUYV;
        lbPgmConfig.dataH = BAND5_COLOR_YUYV;
        for(loop = 0; loop < cy_usb_colorbar_size; loop++)
        {
            Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
        }

        /* BAND6 of colorbar */
        lbPgmConfig.dataL = BAND6_COLOR_YUYV;
        lbPgmConfig.dataH = BAND6_COLOR_YUYV;
        for(loop = 0; loop < cy_usb_colorbar_size; loop++)
        {
            Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
        }

        /* BAND7 of colorbar */
        lbPgmConfig.dataL = BAND7_COLOR_YUYV;
        lbPgmConfig.dataH = BAND7_COLOR_YUYV;
        for(loop = 0; loop < cy_usb_colorbar_size; loop++)
        {
            Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
        }

        /* BAND8 of colorbar */
        lbPgmConfig.dataL = BAND8_COLOR_YUYV;
        lbPgmConfig.dataH = BAND8_COLOR_YUYV;
        for(loop = 0; loop < cy_usb_colorbar_size; loop++)
        {
            Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);
        }
    }

    /* End of program command */
    lbPgmConfig.end = 0x1;
    lbPgmConfig.ctrlByte = 0x01;
    lbPgmConfig.repeatCount = 0x00000000;
    Cy_LVDS_InitLbPgm(&buffStat, &lbPgmConfig);

    /* Commit the data into buffer */
    buffStat.count = 16 * (lbPgmConfig.lbPgmCount);
    Cy_HBDma_Channel_CommitBuffer(&lvdsLbPgmChannel, &buffStat);
}
#endif /*LVDS_LB_EN*/

/*****************************************************************************
* Function Name: Cy_USB_App_KeepLinkActive(cy_stc_usb_app_ctxt_t *pAppCtxt)
******************************************************************************
* Summary:
* Function to keep USB link active
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* Return:
* void
*****************************************************************************/
static void Cy_USB_App_KeepLinkActive (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
#if USB3_LPM_ENABLE
    if (pAppCtxt->isLpmEnabled) {
        /* Disable LPM for 100 ms after any DMA transfers have been completed. */
        DBG_APP_INFO("Disabling LPM\r\n");
        Cy_USBD_LpmDisable(pAppCtxt->pUsbdCtxt);
        pAppCtxt->isLpmEnabled  = false;
    }

    pAppCtxt->lpmEnableTime = Cy_USBD_GetTimerTick() + 100;
#endif /* USB3_LPM_ENABLE */
}

/*****************************************************************************
* Function Name: Cy_USB_UvcAddHeader(uint8_t *buffer_p, uint8_t frameInd)
******************************************************************************
* Summary:
* UVC header addition function
*
* Parameters:
* \param buffer_p
* Buffer pointer
*
*\param frameInd
* Video fram ID
*
* Return:
* void
*****************************************************************************/
void Cy_UVC_AppAddHeader(
    uint8_t *buffer_p, /* Buffer pointer */
    uint8_t frameInd   /* EOF or normal frame indication */
)
{
    /* Copy header to buffer */
    memcpy(buffer_p, (uint8_t *)cy_uvc_header, CY_USB_UVC_MAX_HEADER);

    /* Check if last packet of the frame. */
    if (frameInd == CY_USB_UVC_HEADER_EOF)
    {
        /* Modify UVC header to toggle Frame ID */
        cy_uvc_header[1] ^= CY_USB_UVC_HEADER_FRAME_ID;

        /* Indicate End of Frame in the buffer */
        buffer_p[1] |= CY_USB_UVC_HEADER_EOF;
    }
} /* end of function */

/*****************************************************************************
* Function Name: Cy_USB_AppDisableEndpDma(cy_stc_usb_app_ctxt_t *pAppCtxt)
******************************************************************************
* Summary:
* This function de-inits all active USB DMA channels as part of USB disconnect process.
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* Return:
* void
*****************************************************************************/
void
Cy_USB_AppDisableEndpDma (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    uint8_t i;

#if LVDS_LB_EN
    /*
     * We need to stop the loopback source channel first so that data does not accumulate on the
     * ingress thread interface after the streaming channel is reset. We want to ensure that the
     * loopback source is stopped cleanly at the end of a buffer. Since the transfer of each
     * loopback DMA buffer takes the order of 68 us, we set a flag indicating that no more data
     * should be committed and then wait for 150 us (more than 2 * 68 us). By this time, it
     * is guaranteed that the socket will reach an idle state.
     */
    pAppCtxt->uvcFlowCtrlFlag = false;

    lvdsLpbkBlocked = true;
    Cy_SysLib_DelayUs(150);
    if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS) {
        Cy_SysLib_DelayUs(500);
    }

    Cy_HBDma_Channel_Reset(&lvdsLbPgmChannel);
    Cy_HBDma_Channel_Enable(&lvdsLbPgmChannel, 0);
    lvdsConsCount = 0;
#endif /* LVDS_LB_EN */

    /* On USB 2.x connections, make sure the DataWire channels are disabled and reset. */
    if (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS) {
        for (i = 1; i < CY_USB_MAX_ENDP_NUMBER; i++) {
            if (pAppCtxt->endpInDma[i].valid) {
                /* DeInit the DMA channel and disconnect the triggers. */
                Cy_USBHS_App_DisableEpDmaSet(&(pAppCtxt->endpInDma[i]));
            }

            if (pAppCtxt->endpOutDma[i].valid) {
                /* DeInit the DMA channel and disconnect the triggers. */
                Cy_USBHS_App_DisableEpDmaSet(&(pAppCtxt->endpOutDma[i]));
            }
        }
    }

    /* Disable and destroy the High BandWidth DMA channels. */
    if (pAppCtxt->hbBulkInChannel != NULL) {
#if UVC_INMEM_EN
        Cy_UvcInMem_ClearBufPointers(pAppCtxt, pAppCtxt->hbBulkInChannel);
#endif /* UVC_INMEM_EN */
        Cy_HBDma_Channel_Disable(pAppCtxt->hbBulkInChannel);
        Cy_HBDma_Channel_Destroy(pAppCtxt->hbBulkInChannel);
        pAppCtxt->hbBulkInChannel = NULL;

        /* Flush the EPM in case of USB 3.x connection. */
        if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
            Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum, CY_USB_ENDP_DIR_IN);
        }
    }

#if AUDIO_IF_EN
    if (pAppCtxt->pPDMToUsbChn != NULL) {
        Cy_HBDma_Channel_Disable(pAppCtxt->pPDMToUsbChn);
        Cy_HBDma_Channel_Destroy(pAppCtxt->pPDMToUsbChn);
        pAppCtxt->pPDMToUsbChn = NULL;

        /* Flush the EPM in case of USB 3.x connection. */
        if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
            Cy_USBD_FlushEndp(pAppCtxt->pUsbdCtxt, UAC_IN_ENDPOINT, CY_USB_ENDP_DIR_IN);
        }
    }
#endif /* AUDIO_IF_EN */
}

/*****************************************************************************
* Function Name: Cy_UVC_AppGetCurRqtHandler(cy_stc_usb_app_ctxt_t *pAppCtxt)
******************************************************************************
* Summary:
* This function handles the GET_CUR request addresses to the UVC video streaming interface.
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* \param wLength
* wLength field of control request
*
* \param wValue
* wValue field of control request
*
* Return:
* void
*****************************************************************************/
void Cy_UVC_AppGetCurRqtHandler (cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;
    if (pAppCtxt->devSpeed > CY_USBD_USB_DEV_HS)
    {

        if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_4K_FRAME_INDEX)
        {
            retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                    (uint8_t *)cy_uvc_probectrl_4K, CY_USB_UVC_MAX_PROBE_SETTING);
        }
        else if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_720P_FRAME_INDEX)
        {
            retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                    (uint8_t *)cy_uvc_probectrl_720p, CY_USB_UVC_MAX_PROBE_SETTING);
        }
        else if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_VGA_FRAME_INDEX)
        {
            retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                    (uint8_t *)cy_uvc_probectrl_VGA, CY_USB_UVC_MAX_PROBE_SETTING);
        }
        else
        {
            DBG_APP_ERR("currentFrameIndex\r\n");
        }

        if (retStatus != CY_USBD_STATUS_SUCCESS)
        {
            DBG_APP_ERR("Error SendEndp0Data\r\n");
        }
    }
    else
    {
        if (cy_uvc_currentFrameIndex == CY_USB_UVC_HS_VGA_FRAME_INDEX)
        {
            retStatus = Cy_USB_USBD_SendEndp0Data(pAppCtxt->pUsbdCtxt,
                    (uint8_t *)cy_uvc_probectrl_HS_VGA, CY_USB_UVC_MAX_PROBE_SETTING);
        }
        else
        {
            DBG_APP_ERR("currentFrameIndex\r\n");
        }

        if (retStatus != CY_USBD_STATUS_SUCCESS)
        {
            DBG_APP_ERR("Error SendEndp0Data\r\n");
        }
    }
}
/*****************************************************************************
* Function Name: Cy_UVC_AppSetCurRqtHandler(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                            uint16_t wLength, uint16_t wValue)
******************************************************************************
* Summary:
* This function handles the SET_CUR request addresses to the UVC video streaming interface.
*
* Parameters:
* \param pAppCtxt
* application layer context pointer
*
* \param wLength
* wLength field of control request
*
* \param wValue
* wValue field of control request
*
* Return:
* void
*****************************************************************************/
void Cy_UVC_AppSetCurRqtHandler (cy_stc_usb_app_ctxt_t *pAppCtxt, uint16_t wLength, uint16_t wValue)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;
    cy_en_usbd_ret_code_t retStatus;
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    uint16_t loopCnt = 250u;
    cy_en_hbdma_mgr_status_t status = 0;

#if INMD_EN
#if PORT1_EN
    uint8_t cy_smNo = 1;
#else
    uint8_t cy_smNo = 0;
#endif
#endif /* INMD_EN */

    DBG_APP_INFO("UVC_UVC_SET_CUR_REQ\r\n");

    /* Read the data out commit ctrl buffer */
    retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t*)cy_uvc_commitctrl, wLength);
    if (retStatus == CY_USBD_STATUS_SUCCESS)
    {
        /* Wait until receive DMA transfer has been completed. */
        while ((!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) && (loopCnt--)) {
            Cy_SysLib_DelayUs(10);
        }

        if (!Cy_USBD_IsEp0ReceiveDone(pUsbdCtxt)) {
            DBG_APP_ERR("SET_CUR data timed out\r\n");
            Cy_USB_USBD_RetireRecvEndp0Data(pUsbdCtxt);
            Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
            return;
        }

        cy_uvc_currentFrameIndex = cy_uvc_commitctrl[3];
        DBG_APP_INFO("SET_CUR cy_uvc_currentFrameIndex:%x\r\n", cy_uvc_currentFrameIndex);

        /* Reset the Counter */
        cy_uvc_buffer_counter = 1;

        if (pAppCtxt->devSpeed > CY_USBD_USB_DEV_HS)
        {
            /* Copy the relevant settings from the host provided data into the
               active data structure. */
            if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_720P_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_720p[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_1280_720;
                cy_usb_colorbar_size = COLORBAR_BAND_COUNT_720P;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_1280_720;
                LOG_COLOR("Video Resolution 1280 x 720 \r\n");
            }
            else if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_VGA_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_VGA[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_640_480;
                cy_usb_colorbar_size = COLORBAR_BAND_COUNT_480P;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_640_480;
                LOG_COLOR("Video Resolution 640 x 480 \r\n");
            }
            else if (cy_uvc_currentFrameIndex == CY_USB_UVC_SS_4K_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_4K[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_3840_2160;
                cy_usb_colorbar_size = COLORBAR_BAND_COUNT_4K;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_3840_2160;
                LOG_COLOR("Video Resolution 3840 x 2160 \r\n");
            }
            else
            {
                DBG_APP_ERR("Error FrameIndex\r\n");
                cy_uvc_currentFrameIndex = CY_USB_UVC_SS_4K_FRAME_INDEX;
            }
        }
        else
        {
            if (cy_uvc_currentFrameIndex == CY_USB_UVC_HS_VGA_FRAME_INDEX)
            {
                memcpy(&cy_uvc_probectrl_HS_VGA[2], &cy_uvc_commitctrl[2], CY_USB_UVC_PROBE_CONTROL_UPDATE_SIZE);
                cy_usb_full_buffer_no = CY_USB_FULL_BUFFER_NO_640_480;
                cy_usb_colorbar_size = COLORBAR_BAND_COUNT_480P;
                cy_usb_no_bytes_last_buffer = CY_USB_NO_BYTE_LAST_BUFFER_640_480;
                LOG_COLOR("Video Resolution 640 x 480 \r\n");
            }
            else
            {
                DBG_APP_ERR("Error FrameIndex\r\n");
                cy_uvc_currentFrameIndex = CY_USB_UVC_HS_VGA_FRAME_INDEX;
            }
        }

#if INMD_EN
        Cy_LVDS_GpifClearFwTrig(LVDSSS_LVDS, cy_smNo);
#endif /* INMD_EN */

        if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
            /* Allow data from multiple DMA buffers to be combined into one burst. */
            Cy_USBD_SetEpBurstMode(pAppCtxt->pUsbdCtxt, pAppCtxt->uvcInEpNum, CY_USB_ENDP_DIR_IN, true);
        }
        if (wValue == CY_USB_UVC_VS_COMMIT_CONTROL)
        {
            DBG_APP_INFO("CY_USB_UVC_VS_COMMIT_CONTROL\r\n");

            /* Reset the Counter */
            cy_uvc_buffer_counter = 1;
            status = Cy_UVC_AppStart(pAppCtxt,cy_uvc_commitctrl[2],cy_uvc_commitctrl[3], DEVICE0_OFFSET);
            if(status == CY_HBDMA_MGR_SUCCESS)
            {
                /* Notify the task that Channel enable complete. */
                xMsg.type = CY_USB_UVC_VIDEO_STREAMING_START;
                xQueueSendFromISR(pAppCtxt->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
            }
        }
    }
    else
    {
        DBG_APP_ERR("Error RecvEndp0Data\r\n");
    }
}

/***********************************************************************************************
* Function Name: Cy_UVC_AppDeviceTaskHandler(void *pTaskParam)
**************************************************************************************************
* Summary:
* Application task handler
*
* Parameters:
* \param pTaskParam
* task param
*
* Return:
* void
************************************************************************************************/
void Cy_UVC_AppDeviceTaskHandler(void *pTaskParam)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;
    cy_stc_usbd_app_msg_t queueMsg;
    BaseType_t xStatus;
    cy_en_usbss_lnk_power_mode_t curLinkState;
    uint32_t lpEntryTime = 0;
    uint16_t wIndex;

    vTaskDelay(250);
    /* Reset Frame Id in UVC Header */
    cy_uvc_header_[1] = CY_USB_UVC_HEADER_DEFAULT_BFH;
        /*  All UVC control structures copied from flash to the HBW SRAM buffers */
    memcpy (cy_uvc_probectrl_4K, cy_uvc_probectrl_4K_, sizeof(cy_uvc_probectrl_4K_));
    memcpy (cy_uvc_probectrl_720p, cy_uvc_probectrl_720p_, sizeof(cy_uvc_probectrl_720p_));
    memcpy (cy_uvc_probectrl_VGA, cy_uvc_probectrl_VGA_, sizeof(cy_uvc_probectrl_VGA_));
    memcpy (cy_uvc_probectrl_HS_VGA, cy_uvc_probectrl_HS_VGA_, sizeof(cy_uvc_probectrl_HS_VGA_));
    memcpy (cy_uvc_header, cy_uvc_header_, sizeof(cy_uvc_header_));

#if FPGA_ENABLE
#if FPGA_CONFIG_EN
    Cy_FPGAConfigPins(pAppCtxt,FPGA_CONFIG_MODE);
    Cy_QSPI_Start(pAppCtxt,&HBW_BufMgr);
    Cy_SPI_FlashInit(SPI_FLASH_0, true, false);;

    if(Cy_FPGAConfigure(pAppCtxt,FPGA_CONFIG_MODE) == true)
    {
        DBG_APP_INFO("FPGA configuration complete \r\n");
        glIsFPGAConfigComplete = true;
    }
    else
    {
        LOG_ERROR("Failed to configure FPGA \r\n");
    }

#else
    if(true == Cy_IsFPGAConfigured())
    {
        glIsFPGAConfigComplete = true;
    }
#endif /* FPGA_CONFIG_EN */

    if((glIsFPGARegConfigured == false) && (glIsFPGAConfigComplete == true))
    {
        Cy_FPGAPhyLnkTraining();
        Cy_FPGAGetVersion(pAppCtxt);

        if(0 == Cy_ConfigFpgaRegister())
        {
            glIsFPGARegConfigured = true;
            DBG_APP_INFO("FPGA register configuration complete \n\r");
        }
        else
        {
            LOG_ERROR("Failed to configure FPGA register via I2C \r\n");
        }
    }
#endif /* FPGA_ENABLE */

    /* Initialize the LVDS interface. */
    Cy_LVDS_LVCMOS_Init();
    vTaskDelay(100);

    /* If VBus is present, enable the USB connection. */
    pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE);
    if (pAppCtxt->vbusPresent) {
        Cy_USB_SSConnectionEnable(pAppCtxt);
    }

    DBG_APP_INFO("UvcDeviceThreadCreated\r\n");

#if USB3_LPM_ENABLE
    if ((pAppCtxt->isLpmEnabled == false) && (pAppCtxt->lpmEnableTime != 0)) {
        Cy_USBSS_Cal_GetLinkPowerState(pAppCtxt->pUsbdCtxt->pSsCalCtxt, &curLinkState);
        if (
                (Cy_USBD_GetTimerTick() >= pAppCtxt->lpmEnableTime) &&
                (curLinkState != CY_USBSS_LPM_U3)
           ) {
            DBG_APP_INFO("LPM re-enable\r\n");
            pAppCtxt->isLpmEnabled  = true;
            pAppCtxt->lpmEnableTime = 0;
            Cy_USBD_LpmEnable(pAppCtxt->pUsbdCtxt);
        }
    }
#endif /* USB3_LPM_ENABLE */

    for (;;)
    {
        /*
         * Wait until some data is received from the queue.
         * Timeout after 100 ms.
         */
        xStatus = xQueueReceive(pAppCtxt->uvcMsgQueue, &queueMsg, 100);
        if (xStatus == pdPASS) {

        switch (queueMsg.type) {
            case CY_USB_UVC_VBUS_CHANGE_INTR:
                /* Start the debounce timer. */
                xTimerStart(pAppCtxt->vbusDebounceTimer, 0);
                break;

            case CY_USB_UVC_VBUS_CHANGE_DEBOUNCED:
                /* Check whether VBus state has changed. */
                pAppCtxt->vbusPresent = (Cy_GPIO_Read(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN) == VBUS_DETECT_STATE);

                if (pAppCtxt->vbusPresent) {
                    if (!pAppCtxt->usbConnected) {
                        DBG_APP_INFO("Enabling USB connection due to VBus detect\r\n");
                        Cy_USB_SSConnectionEnable(pAppCtxt);
                    }
                } else {
                    if (pAppCtxt->usbConnected) {
                        Cy_USB_AppDisableEndpDma(pAppCtxt);
                        DBG_APP_INFO("Disabling USB connection due to VBus removal\r\n");
                        Cy_USB_SSConnectionDisable(pAppCtxt);
                    }
                }
                break;

            case CY_USB_UVC_VIDEO_STREAMING_START:

#if LVDS_LB_EN
                if ((lvdsConsCount == 0) && (cy_uvc_IsApplnActive == true))
                {
                    Cy_Update_LvdsLinkClock(pAppCtxt->devSpeed == CY_USBD_USB_DEV_HS);
                    DBG_APP_INFO("Starting LVDS Link Loop back\r\n");
                    pAppCtxt->uvcFlowCtrlFlag = false;
                    Cy_HBDma_Channel_Reset(&lvdsLbPgmChannel);
                    Cy_HBDma_Channel_Enable(&lvdsLbPgmChannel, 0);
                    lvdsLpbkBlocked = false;
                    Cy_LVDS_CommitColorbarData();
                    Cy_LVDS_CommitColorbarData();
                    LVDSSS_LVDS->GPIF[1].GPIF_WAVEFORM_CTRL_STAT |= LVDSSS_LVDS_GPIF_GPIF_WAVEFORM_CTRL_STAT_CPU_LAMBDA_Msk;
                }
#endif /* LVDS_LB_EN */
#if UVC_INMEM_EN
                Cy_UvcInMem_CommitBuffers(pAppCtxt, pAppCtxt->hbBulkInChannel, cy_uvc_currentFrameIndex);
#endif /* UVC_INMEM_EN */
                Cy_USBD_AddEvtToLog(pAppCtxt->pUsbdCtxt, CY_USB_UVC_EVT_VSTREAM_START);
                break;

            case CY_USB_UVC_DEVICE_GET_CUR_RQT:
                Cy_UVC_AppGetCurRqtHandler(pAppCtxt);
                break;

            case CY_USB_UVC_DEVICE_SET_CUR_RQT:
                Cy_UVC_AppSetCurRqtHandler(pAppCtxt, queueMsg.data[0], queueMsg.data[1]);
                Cy_USBD_AddEvtToLog(pAppCtxt->pUsbdCtxt, CY_USB_UVC_EVT_SET_CUR_REQ);
                break;

            case CY_USB_UVC_VIDEO_STREAM_STOP_EVENT:
                DBG_APP_INFO("CY_USB_UVC_VIDEO_STREAM_STOP_EVENT\r\n");
                wIndex = (uint16_t)queueMsg.data[0];
                Cy_UVC_AppStop(pAppCtxt,pAppCtxt->pUsbdCtxt, wIndex);
                break;

            case CY_USB_PRINT_FPS:
                DBG_APP_INFO("UVC FPS : %d\n\r", queueMsg.data[0]);
                DBG_APP_INFO("UVC Bytes Transferred : %d\n\r", queueMsg.data[1]);
            break;
            default:
                break;
        } /* end of switch() */

        curLinkState = CY_USBSS_LPM_UNKNOWN;
        if ((cy_uvc_devconfigured) && (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)) {
            Cy_USBSS_Cal_GetLinkPowerState(pAppCtxt->pUsbdCtxt->pSsCalCtxt, &curLinkState);
        }

        /*
         * If the link has been in USB2-L1 or in USB3-U2 for more than 0.5 second, initiate LPM exit so that
         * transfers do not get delayed significantly.
         */
        if (
                ((curLinkState == CY_USBSS_LPM_U2) && (!(pAppCtxt->pUsbdCtxt->pSsCalCtxt->forceLPMAccept))) ||
                (
                 (pAppCtxt->devSpeed <= CY_USBD_USB_DEV_HS) &&
                 ((MXS40USBHSDEV_USBHSDEV->DEV_PWR_CS & USBHSDEV_DEV_PWR_CS_L1_SLEEP) != 0)
                )
           ) {
            if ((Cy_USBD_GetTimerTick() - lpEntryTime) > 500UL) {
                lpEntryTime = Cy_USBD_GetTimerTick();
                Cy_USBD_GetUSBLinkActive(pAppCtxt->pUsbdCtxt);
            }
        } else {
            lpEntryTime = Cy_USBD_GetTimerTick();
        }
    }
    }
}

/***********************************************************************************************
* Function Name: Cy_USB_PrintEvtLogTimerCb(TimerHandle_t xTimer)
**************************************************************************************************
* Summary:
* This Function will be called when timer expires.This function print event log.
*
* Parameters:
* \param xTimer
* timer handle
*
* Return:
* void
************************************************************************************************/
void
Cy_USB_PrintEvtLogTimerCb(TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    /* retrieve pAppCtxt */
    pAppCtxt = ( cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    if (pAppCtxt->devState == CY_USB_DEVICE_STATE_CONFIGURED) {
        /*
         * Print out the contents of the USB event log buffer which has
         * logged event related to enumeration and line training.
         */
        Cy_USB_AppPrintUsbEventLog(pAppCtxt, pAppCtxt->pUsbdCtxt->pSsCalCtxt);
    }
}   /* end of function() */

/***********************************************************************************************
* Function Name: Cy_USB_PrintFpsCb(TimerHandle_t xTimer)
**************************************************************************************************
* Summary:
* This Function will be called when timer expires.This function print video fps status.
*
* Parameters:
* \param xTimer
* timer handle
*
* Return:
* void
************************************************************************************************/
void
Cy_USB_PrintFpsCb(TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;

    /* retrieve pAppCtxt */
    pAppCtxt = ( cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    if (pAppCtxt->devState == CY_USB_DEVICE_STATE_CONFIGURED) {

#if (UVC_HEADER_BY_FX || MANUAL_DMA_CHANNEL || UVC_INMEM_EN)
        cy_stc_usbd_app_msg_t xMsg;
        BaseType_t xHigherPriorityTaskWoken = pdFALSE;

        if (cy_uvc_IsApplnActive == true) {
            xMsg.type    = CY_USB_PRINT_FPS;
            xMsg.data[0] = pAppCtxt->glfps;
            xMsg.data[1] = pAppCtxt->glFrameSize;
            xQueueSendFromISR(pAppCtxt->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

            pAppCtxt->glfps = 0;
        }
#endif /* UVC_HEADER_BY_FX || MANUAL_DMA_CHANNEL || UVC_INMEM_EN */
    }
}   /* end of function() */

/*****************************************************************************
* Function Name: Cy_USB_VbusDebounceTimerCb(TimerHandle_t xTimer)
******************************************************************************
* Summary:
* Timer used to do debounce on VBus changed interrupt notification.
*
* \param xTimer
* Timer Handle
*
* \return
* None
*
 *******************************************************************************/
void
Cy_USB_VbusDebounceTimerCb (TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;

    DBG_APP_INFO("VbusDebounce_CB\r\n");
    if (pAppCtxt->vbusChangeIntr) {
        /* Notify the VCOM task that VBus debounce is complete. */
        xMsg.type = CY_USB_UVC_VBUS_CHANGE_DEBOUNCED;
        xQueueSendFromISR(pAppCtxt->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));

        /* Clear and re-enable the interrupt. */
        pAppCtxt->vbusChangeIntr = false;
        Cy_GPIO_ClearInterrupt(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN);
        Cy_GPIO_SetInterruptMask(VBUS_DETECT_GPIO_PORT, VBUS_DETECT_GPIO_PIN, 1);
    }
}   /* end of function  */

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
void Cy_USB_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt,
                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, DMAC_Type *pCpuDmacBase,
                    DW_Type *pCpuDw0Base, DW_Type *pCpuDw1Base,
                    cy_stc_hbdma_mgr_context_t *pHbDmaMgrCtxt)
{
    uint32_t index;
    BaseType_t status = pdFALSE;
    cy_stc_app_endp_dma_set_t *pEndpInDma;
    cy_stc_app_endp_dma_set_t *pEndpOutDma;

    pAppCtxt->devState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DISABLE;
    pAppCtxt->devSpeed = CY_USBD_USB_DEV_FS;
    pAppCtxt->devAddr = 0x00;
    pAppCtxt->activeCfgNum = 0x00;
    pAppCtxt->prevAltSetting = 0x00;
    pAppCtxt->enumMethod = CY_USB_ENUM_METHOD_FAST;
    pAppCtxt->pHbDmaMgrCtxt = pHbDmaMgrCtxt;

    for (index = 0x00; index < CY_USB_MAX_ENDP_NUMBER; index++)
    {
        pEndpInDma = &(pAppCtxt->endpInDma[index]);
        memset((void *)pEndpInDma, 0, sizeof(cy_stc_app_endp_dma_set_t));

        pEndpOutDma = &(pAppCtxt->endpOutDma[index]);
        memset((void *)pEndpOutDma, 0, sizeof(cy_stc_app_endp_dma_set_t));
    }

    pAppCtxt->pCpuDmacBase = pCpuDmacBase;
    pAppCtxt->pCpuDw0Base = pCpuDw0Base;
    pAppCtxt->pCpuDw1Base = pCpuDw1Base;
    pAppCtxt->pUsbdCtxt = pUsbdCtxt;
    pAppCtxt->uvcPendingBufCnt = 0;
    pAppCtxt->uvcFlowCtrlFlag = false;

    /*
     * Callbacks registered with USBD layer. These callbacks will be called
     * based on appropriate event.
     */
    Cy_USB_AppRegisterCallback(pAppCtxt);

    if (!(pAppCtxt->firstInitDone))
    {
        pAppCtxt->vbusChangeIntr = false;
        pAppCtxt->vbusPresent    = false;
        pAppCtxt->usbConnected   = false;

        /* Create the message queue and register it with the kernel. */
        pAppCtxt->uvcMsgQueue = xQueueCreate(CY_USB_UVC_DEVICE_MSG_QUEUE_SIZE,
                CY_USB_UVC_DEVICE_MSG_SIZE);
        if (pAppCtxt->uvcMsgQueue == NULL) {
            DBG_APP_ERR("QueuecreateFail\r\n");
            return;
        }

        vQueueAddToRegistry(pAppCtxt->uvcMsgQueue, "UVCDeviceMsgQueue");

        /* Create task and check status to confirm task created properly. */
        status = xTaskCreate(Cy_UVC_AppDeviceTaskHandler, "UvcDeviceTask", 2048,
                             (void *)pAppCtxt, 5, &(pAppCtxt->uvcDevicetaskHandle));
        if (status != pdPASS)
        {
            DBG_APP_ERR("TaskcreateFail\r\n");
            return;
        }

#if AUDIO_IF_EN
        Cy_UAC_AppInit(pAppCtxt);
#endif /* AUDIO_IF_EN */

        pAppCtxt->vbusDebounceTimer = xTimerCreate("VbusDebounceTimer", 200, pdFALSE,
                (void *)pAppCtxt, Cy_USB_VbusDebounceTimerCb);
        if (pAppCtxt->vbusDebounceTimer == NULL) {
            DBG_APP_ERR("TimerCreateFail\r\n");
            return;
        }
        DBG_APP_TRACE("VBus debounce timer created\r\n");

        pAppCtxt->evtLogTimer = xTimerCreate("EventLogTimer", 10000, pdTRUE,
                                             (void *)pAppCtxt,
                                             Cy_USB_PrintEvtLogTimerCb);
        if (pAppCtxt->evtLogTimer == NULL) {
            DBG_APP_ERR("evtLogTimer Create Fail\r\n");
            return;
        } else {
            /* Start the debounce timer. */
            DBG_APP_TRACE("evtLogTimer Start\r\n");
            xTimerStart(pAppCtxt->evtLogTimer, 0);
        }

        pAppCtxt->fpsTimer = xTimerCreate("fpsTimer", 1000, pdTRUE, (void *)pAppCtxt, Cy_USB_PrintFpsCb);
        if (pAppCtxt->fpsTimer == NULL) {
            DBG_APP_ERR("FPS timer Create Fail\r\n");
            return;
        } else {
            /* Start the debounce timer. */
            DBG_APP_TRACE("FPS Timer Start\r\n");
            xTimerStart(pAppCtxt->fpsTimer, 0);
        }

#if UVC_INMEM_EN
        /* Allocate the RAM buffers used for in-memory video streaming. */
        if (!Cy_UvcInMem_AllocateBuffers(pAppCtxt)) {
            return;
        }
#endif /* UVC_INMEM_EN */

        pAppCtxt->firstInitDone = 0x01;
    }

    /* Zero out the EP0 test buffer. */
    memset((uint8_t *)Ep0TestBuffer, 0, sizeof(Ep0TestBuffer));

    return;
} /* end of function. */

/*****************************************************************************
* Function Name: Cy_USB_AppSetAddressCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                       cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
*  This Function will be called by USBD layer when  a USB address has been assigned to the device.
*
* \param pAppCtxt
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
void
Cy_USB_AppSetAddressCallback (void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                       cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)pUsbApp;

    /* Update the state variables. */
    pAppCtxt->devState     = CY_USB_DEVICE_STATE_ADDRESS;
    pAppCtxt->prevDevState = CY_USB_DEVICE_STATE_DEFAULT;
    pAppCtxt->devAddr      = pUsbdCtxt->devAddr;
    pAppCtxt->devSpeed     = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);

    /* Check the type of USB connection and register appropriate descriptors. */
    CyApp_RegisterUsbDescriptors(pAppCtxt, pAppCtxt->devSpeed);
}

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
void Cy_USB_AppRegisterCallback(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt = pAppCtxt->pUsbdCtxt;

    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET,
                             Cy_USB_AppBusResetCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESET_DONE,
                             Cy_USB_AppBusResetDoneCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_BUS_SPEED,
                             Cy_USB_AppBusSpeedCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETUP,
                             Cy_USB_AppSetupCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SUSPEND,
                             Cy_USB_AppSuspendCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_RESUME,
                             Cy_USB_AppResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_CONFIG,
                             Cy_USB_AppSetCfgCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SET_INTF,
                             Cy_USB_AppSetIntfCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_SLEEP,
                             Cy_USB_AppL1SleepCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_L1_RESUME,
                             Cy_USB_AppL1ResumeCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_ZLP,
                             Cy_USB_AppZlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SLP,
                             Cy_USB_AppSlpCallback);
    Cy_USBD_RegisterCallback(pUsbdCtxt, CY_USB_USBD_CB_SETADDR,
                             Cy_USB_AppSetAddressCallback);
    return;
} /* end of function. */

/*****************************************************************************
* Function Name: CyUvcAppHandleProduceEvent(cy_stc_usb_app_ctxt_t  *pUsbApp,
                                        cy_stc_hbdma_channel_t *pChHandle)
******************************************************************************
* Summary:
*  Function that handles a produce event indicating receipt of data through
*  the LVDS ingress socket.
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pChHandle
* Pointer to DMA channel structure.
*
* \return
* None
*
 *******************************************************************************/
static void
Cy_UVC_AppHandleProduceEvent (
        cy_stc_usb_app_ctxt_t  *pUsbApp,
        cy_stc_hbdma_channel_t *pChHandle)
{
    cy_en_hbdma_mgr_status_t   status;
    cy_stc_hbdma_buff_status_t buffStat;

    /* Wait for a free buffer. */
    status = Cy_HBDma_Channel_GetBuffer(pChHandle, &buffStat);
    if (status != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("HB-DMA GetBuffer Error\r\n");
        return;
    }


#if INMD_EN
    if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS)
    {
        Cy_USB_AppQueueWrite(pUsbApp, pUsbApp->uvcInEpNum, buffStat.pBuffer, buffStat.count);
    }
#else
    pUsbApp->glDmaBufCnt = buffStat.count;
    pUsbApp->glFrameSizeTransferred += pUsbApp->glDmaBufCnt;
    /* Add headers on every frame. Need to check if the EOF bit has to be set. */
    if (cy_uvc_buffer_counter <= cy_usb_full_buffer_no)
    {
#if UVC_HEADER_BY_FX
        /* Not the end of frame. */
        Cy_UVC_AppAddHeader(buffStat.pBuffer - CY_USB_UVC_MAX_HEADER, CY_USB_UVC_HEADER_FRAME);
#endif /* UVC_HEADER_BY_FX */
        cy_uvc_buffer_counter++;
        cy_uvc_commitLength = CY_USB_UVC_STREAM_BUF_SIZE;
    }
    else
    {
#if UVC_HEADER_BY_FX
        /* Short packet: End of frame. */
        Cy_UVC_AppAddHeader(buffStat.pBuffer - CY_USB_UVC_MAX_HEADER, CY_USB_UVC_HEADER_EOF);
        cy_uvc_commitLength = buffStat.count + CY_USB_UVC_MAX_HEADER;
#else
        cy_uvc_commitLength = buffStat.count;
#endif /* UVC_HEADER_BY_FX */

        cy_uvc_buffer_counter = 1;
        pUsbApp->glFrameSize = pUsbApp->glFrameSizeTransferred;
        pUsbApp->glFrameSizeTransferred = 0;
        pUsbApp->glFrameCount++;
        pUsbApp->glPartialBufSize = buffStat.count;
        pUsbApp->glConsCount = pUsbApp->glCons;
        pUsbApp->glProdCount = pUsbApp->glProd;
        pUsbApp->glPrintFlag = 1;
        pUsbApp->glCons = 0;
        pUsbApp->glProd = 0;
        pUsbApp->glfps++;
    }

#if UVC_HEADER_BY_FX
    buffStat.count   = cy_uvc_commitLength;
    buffStat.size    = (cy_uvc_commitLength + 31) & 0x1FFE0;     /* Round size up to a multiple of 32 bytes. */
    buffStat.pBuffer = buffStat.pBuffer - CY_USB_UVC_MAX_HEADER;
#endif /* UVC_HEADER_BY_FX */

    /* Commit the buffer for transfer */
    if (pUsbApp->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
    {
        status = Cy_HBDma_Channel_CommitBuffer(pChHandle, &buffStat);
        if (status != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("HB-DMA CommitBuffer Error\r\n");
        }

        Cy_USB_App_KeepLinkActive(pUsbApp);
    }
    else
    {
        Cy_USB_AppQueueWrite(pUsbApp, pUsbApp->uvcInEpNum, buffStat.pBuffer, cy_uvc_commitLength);
    }
#endif /* INMD_EN */
}

#if LVDS_LB_EN
/*****************************************************************************
* Function Name: Cy_UVC_AppCommitColorbarData(cy_stc_usb_app_ctxt_t  *pUsbApp,
                                        cy_stc_hbdma_channel_t *pChHandle)
******************************************************************************
* Summary:
*  Function that commits loopback data patterns that inject the colorbar
* data pattern into the LVDS ingress socket..
*
* \param pAppCtxt
* application layer context pointer.
*
* \param pChHandle
* Pointer to DMA channel structure.
*
* \return
* None
*
 *******************************************************************************/
static void
Cy_UVC_AppCommitColorbarData (
        cy_stc_usb_app_ctxt_t  *pUsbApp,
        cy_stc_hbdma_channel_t *pChHandle)
{
    cy_stc_hbdma_buff_status_t lbBuffStat;

    if (lvdsLpbkBlocked) {
        DBG_APP_INFO("Skip data commit due to streaming stop\r\n");
        return;
    }

    /* Two buffers of the producer socket are required to fill one buffer of the streaming socket. */
    if (Cy_HBDma_Channel_GetBuffer(pChHandle, &lbBuffStat) != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("GetLpbkBuf 2 failed\r\n");
        return;
    }

    lbBuffStat.count = LOOPBACK_MEM_BUF_SIZE;
    Cy_HBDma_Channel_CommitBuffer(pChHandle, &lbBuffStat);

    if (Cy_HBDma_Channel_GetBuffer(pChHandle, &lbBuffStat) != CY_HBDMA_MGR_SUCCESS) {
        DBG_APP_ERR("GetLpbkBuf 3 failed\r\n");
        return;
    }

    lbBuffStat.count = LOOPBACK_MEM_BUF_SIZE;
    Cy_HBDma_Channel_CommitBuffer(pChHandle, &lbBuffStat);

    Cy_USB_App_KeepLinkActive(pUsbApp);
}
#endif /* LVDS_LB_EN */

/*****************************************************************************
* Function Name: CyUvcAppHandleProduceEvent(cy_stc_usb_app_ctxt_t  *pUsbApp)
******************************************************************************
* Summary:
* Function that handles DMA transfer completion on the USB-HS BULK-IN
* endpoint. This is equivalent to the receipt of a consume event in the USB-SS use
* case and we can discard the active data buffer on the LVDS side.
*
* \param pAppCtxt
* application layer context pointer.
*
* \return
* None
*
 *******************************************************************************/
void
Cy_UVC_AppHandleSendCompletion (
        cy_stc_usb_app_ctxt_t *pUsbApp)
{
#if UVC_INMEM_EN
    /* Call the consume event handler so that next data is queued. */
    Cy_UvcInMem_DmaCallback(pUsbApp->hbBulkInChannel, CY_HBDMA_CB_CONS_EVENT, NULL, (void *)pUsbApp);
#else
    cy_stc_hbdma_buff_status_t buffStat;
    cy_en_hbdma_mgr_status_t   dmaStat;

    /* At least one buffer must be pending. */
    if (pUsbApp->uvcPendingBufCnt == 0)
    {
        DBG_APP_ERR("PendingBufCnt=0 on SendComplete\r\n");
        return;
    }

    /* The buffer which has been sent to the USB host can be discarded. */
    dmaStat = Cy_HBDma_Channel_DiscardBuffer(pUsbApp->hbBulkInChannel, &buffStat);
    if (dmaStat != CY_HBDMA_MGR_SUCCESS)
    {
        DBG_APP_ERR("DiscardBuffer failed with status=%x\r\n", dmaStat);
        return;
    }

#if LVDS_LB_EN
    /* If colorbar data pattern was blocked, enable it to fill one more data buffer. */
    if (pUsbApp->uvcFlowCtrlFlag)
    {
        pUsbApp->uvcFlowCtrlFlag = false;
        Cy_UVC_AppCommitColorbarData(pUsbApp, &lvdsLbPgmChannel);
    }
#endif /* LVDS_LB_EN */

    /* If another DMA buffer has already been filled by the producer, go
     * on and send it to the host controller.
     */
    pUsbApp->uvcPendingBufCnt--;
    if (pUsbApp->uvcPendingBufCnt > 0)
    {
        Cy_UVC_AppHandleProduceEvent(pUsbApp, pUsbApp->hbBulkInChannel);
    }
#endif /* UVC_INMEM_EN */
}

/*****************************************************************************
* Function Name: Cy_UVC_HbDmaCb(cy_stc_hbdma_channel_t *handle, cy_en_hbdma_cb_type_t type,
                              cy_stc_hbdma_buff_status_t *pbufStat, void *userCtx)
******************************************************************************
* Summary:
* HBDMA callback function to add the UVC header to the frame/buffer coming
* from LVDS and commits to USB.
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
void Cy_UVC_HbDmaCb(
        cy_stc_hbdma_channel_t *handle,
        cy_en_hbdma_cb_type_t type,
        cy_stc_hbdma_buff_status_t *pbufStat,
        void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;

    if (type == CY_HBDMA_CB_PROD_EVENT)
    {
        /* Video streamer application enable */
        if ((cy_uvc_IsApplnActive == true) && (cy_uvc_devconfigured == true))
        {
            pAppCtxt->glProd++;
            if (pAppCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
            {
                Cy_UVC_AppHandleProduceEvent(pAppCtxt, handle);
            }
            else
            {
                pAppCtxt->uvcPendingBufCnt++;
                if (pAppCtxt->uvcPendingBufCnt == 1)
                {
                    Cy_UVC_AppHandleProduceEvent(pAppCtxt, handle);
                }
            }
        }
    }

#if LVDS_LB_EN
    if (type == CY_HBDMA_CB_CONS_EVENT)
    {
        /* If the loopback TX socket is in flow control state, commit two more buffers. */
        if (pAppCtxt->uvcFlowCtrlFlag)
        {
            pAppCtxt->uvcFlowCtrlFlag = false;
            Cy_UVC_AppCommitColorbarData(pAppCtxt, &lvdsLbPgmChannel);
        }
    }
#endif /* LVDS_LB_EN */

} /* end of function  */

#if LVDS_LB_EN
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
void Cy_HbDma_LoopbackCb (cy_stc_hbdma_channel_t *handle,
                        cy_en_hbdma_cb_type_t type,
                        cy_stc_hbdma_buff_status_t *pbufStat,
                        void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;

    if ((type == CY_HBDMA_CB_CONS_EVENT) && (!lvdsLpbkBlocked))
    {
        lvdsConsCount++;

        /*
         * If the LVDS producer socket is active, commit the next two buffers.
         * Otherwise, set a flag indicating flow control state.
         */
        if ((lvdsConsCount & 0x01) == 0) {
            if((_FLD2VAL(LVDSSS_LVDS_ADAPTER_DMA_SCK_STATUS_STATE, \
                            (LVDSSS_LVDS->ADAPTER_DMA[0].SCK[0].SCK_STATUS))) == 0x2) {
                Cy_UVC_AppCommitColorbarData(pAppCtxt, &lvdsLbPgmChannel);
            } else {
                pAppCtxt->uvcFlowCtrlFlag = true;
            }
        }
    }
}
#endif /* LVDS_LB_EN */

#if AUDIO_IF_EN
/*****************************************************************************
 * Function Name: Cy_UAC_HbDmaCb
 ******************************************************************************
 * Summary:
 *  Callback function to manage data transfers on the USB Audio Class endpoint
 *  in USB 3.x connections.
 *
 * Parameters:
 *  \param handle
 *  Pointer to the DMA channel associated with the endpoint.
 *
 *  \param type
 *  Type of callback event
 *
 *  \param pbufStat
 *  Event specific data
 *
 *  \param userCtx
 * User context data to be de-referenced to obtain the application context.
 *
 * Return:
 *  void
 *****************************************************************************/
static void Cy_UAC_HbDmaCb (cy_stc_hbdma_channel_t *handle,
                          cy_en_hbdma_cb_type_t type,
                          cy_stc_hbdma_buff_status_t *pbufStat,
                          void *userCtx)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt = (cy_stc_usb_app_ctxt_t *)userCtx;

    /* As this is an IN endpoint, only consume events are expected. */
    if (type == CY_HBDMA_CB_CONS_EVENT)
    {
        /* Data has been consumed on USB side and buffer is free now. If all buffers
         * were full previously, we can queue a read operation using the newly freed
         * buffer.
         */
        if (pAppCtxt->pdmRxFreeBufCount == 0)
        {
            /* Queue read from the PDM RX FIFO into the next DMA buffer. */
            Cy_PDM_AppQueueRead(pAppCtxt, UAC_EP_MAX_PKT_SIZE);
        }

        pAppCtxt->pdmRxFreeBufCount++;
    }
}
#endif /* AUDIO_IF_EN */

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
static void
Cy_USB_AppSetupEndpDmaParamsSs (cy_stc_usb_app_ctxt_t *pUsbApp,
                                uint8_t *pEndpDscr)
{
    cy_stc_hbdma_chn_config_t dmaConfig;
    cy_en_hbdma_mgr_status_t mgrStat;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint8_t *pCompDscr;
    uint8_t burstSize;

    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);
    pCompDscr = Cy_USBD_GetSsEndpCompDscr(pUsbApp->pUsbdCtxt, pEndpDscr);
    Cy_USBD_GetEndpCompnMaxburst(pCompDscr, &burstSize);

    DBG_APP_INFO("AppSetupEndpDmaParamsSs: endpNum:0x%x maxPktSize:0x%x "
                 "dir:0x%x burstSize:0x%x\r\n",
                 endpNumber, maxPktSize,
                 dir, burstSize);

    /* Handling UVC streaming endpoint. */
    if ((endpNumber == UVC_STREAM_ENDPOINT) && (dir != 0)) {
        pUsbApp->uvcInEpNum       = (uint8_t)endpNumber;
        pUsbApp->uvcPendingBufCnt = 0;

        /* If the channel was already created, destroy and create afresh. */
        if (pUsbApp->hbBulkInChannel != NULL) {
#if UVC_INMEM_EN
            Cy_UvcInMem_ClearBufPointers(pUsbApp, pUsbApp->hbBulkInChannel);
#endif /* UVC_INMEM_EN */
            Cy_HBDma_Channel_Disable(pUsbApp->hbBulkInChannel);
            Cy_HBDma_Channel_Destroy(pUsbApp->hbBulkInChannel);
            pUsbApp->hbBulkInChannel = NULL;
        }

        dmaConfig.chType       = CY_HBDMA_TYPE_IP_TO_IP;    /* DMA Channel type: from LVDS to USB EG IP. */
        dmaConfig.consSckCount = 1;                         /* Only one consumer socket per channel. */
        dmaConfig.consSck[0] = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
        dmaConfig.consSck[1] = (cy_hbdma_socket_id_t)0;
        if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS)
        {
            dmaConfig.chType     = CY_HBDMA_TYPE_IP_TO_MEM;
            dmaConfig.consSck[0] = (cy_hbdma_socket_id_t)0;
        }

#if MANUAL_DMA_CHANNEL
#if UVC_HEADER_BY_FX
        dmaConfig.prodHdrSize  = CY_USB_UVC_MAX_HEADER;
#elif (UVC_HEADER_BY_FPGA || INMD_EN)
        dmaConfig.prodHdrSize  = 0;
#endif /* UVC_HEADER_BY_FX */
        dmaConfig.eventEnable  = 0;          /* Manual channel: Disable event signalling between sockets. */
        dmaConfig.intrEnable   = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk |
                                 LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
        dmaConfig.cb           = Cy_UVC_HbDmaCb;   /* HB-DMA callback */
#elif AUTO_DMA_CHANNEL
        dmaConfig.prodHdrSize  = 0;
        dmaConfig.eventEnable  = 1;          /* Auto channel: Enable event signalling between sockets. */
        dmaConfig.intrEnable   = 0;          /* No interrupts required in this case. */
        dmaConfig.cb           = NULL;       /* Call back function is not required for AUTO DMA */
#endif /* MANUAL_DMA_CHANNEL */

        if(pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS)
        {
            /* In HS connection, we have to use a MANUAL channel. */
            dmaConfig.eventEnable = 0;      /* Event signalling cannot be used with USBHS endpoint. */
            dmaConfig.intrEnable  = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk;
            dmaConfig.cb           = Cy_UVC_HbDmaCb;   /* HB-DMA callback */
        }

        dmaConfig.prodBufSize  = FPGA_DMA_BUFFER_SIZE;        /* Receive depends on uvc header adds by FX10/FX20 or FPGA*/
        dmaConfig.size         = DMA_BUFFER_SIZE;             /* DMA Buffer size in bytes */
        dmaConfig.count        = CY_USB_UVC_STREAM_BUF_COUNT; /* DMA Buffer Count */
#if ((FPGA_ENABLE) && (!LVCMOS_EN) && (WL_EN))
        /* If SIP is operating in LVDS WideLink mode and USB is limited to 2.x speeds, reduce the number of DMA
         * buffers used to 1.
         */
        if (pUsbApp->devSpeed < CY_USBD_USB_DEV_SS_GEN1) {
            dmaConfig.count = 1;
        }
#endif /* ((FPGA_ENABLE) && (!LVCMOS_EN) && (WL_EN)) */
        dmaConfig.bufferMode   = true;                        /* DMA buffer mode enabled */
        dmaConfig.userCtx      = (void *)(pUsbApp);           /* Pass the application context as user context. */

#if PORT1_EN
        dmaConfig.prodSckCount = 1; /* No. of producer sockets */
        dmaConfig.prodSck[0] = CY_HBDMA_LVDS_SOCKET_16;
        dmaConfig.prodSck[1] = (cy_hbdma_socket_id_t)0;
#else
#if INTERLEAVE_EN
        dmaConfig.prodSckCount = 2; /* No. of producer sockets */
        dmaConfig.prodSck[0] = CY_HBDMA_LVDS_SOCKET_00;
        dmaConfig.prodSck[1] = CY_HBDMA_LVDS_SOCKET_01;
        LOG_COLOR("Thread Interleaving Enabled \n\r");
#else
        dmaConfig.prodSckCount = 1; /* No. of producer sockets */
        dmaConfig.prodSck[0] = CY_HBDMA_LVDS_SOCKET_00;
        dmaConfig.prodSck[1] = (cy_hbdma_socket_id_t)0; /* Producer Socket ID: None */
#endif /* INTERLEAVE_EN */
#endif /* PORT1_EN */

        LOG_COLOR("DMA Channel Config - Manual Channel enable: %d UVC Header Addition by FPGA/INMD: %d \n\r", MANUAL_DMA_CHANNEL,UVC_HEADER_BY_FPGA);
        LOG_COLOR("DMA Channel Config - DMA buffer Size: %d, Producer buffer size: %d \n\r",dmaConfig.size,dmaConfig.prodBufSize);

#if UVC_INMEM_EN
        LOG_COLOR("Changing DMA configuration for In-Memory streaming\r\n");
        dmaConfig.chType       = CY_HBDMA_TYPE_MEM_TO_IP;
        dmaConfig.size         = UVC_INMEM_LASTBUF_SIZE;         /* These buffers are not used for streaming. */
        dmaConfig.count        = UVC_INMEM_BUF_COUNT;
        dmaConfig.prodHdrSize  = 0;
        dmaConfig.prodBufSize  = UVC_INMEM_LASTBUF_SIZE;
        dmaConfig.prodSckCount = 1;
        dmaConfig.prodSck[0]   = CY_HBDMA_VIRT_SOCKET_WR;
        dmaConfig.prodSck[1]   = (cy_hbdma_socket_id_t)0;
        dmaConfig.consSckCount = 1;
        dmaConfig.consSck[0]   = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
        dmaConfig.consSck[1]   = (cy_hbdma_socket_id_t)0;
        dmaConfig.eventEnable  = 0;
        dmaConfig.intrEnable   = LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_PRODUCE_EVENT_Msk |
            LVDSSS_LVDS_ADAPTER_DMA_SCK_INTR_CONSUME_EVENT_Msk;
        dmaConfig.cb           = Cy_UvcInMem_DmaCallback;
#endif /* UVC_INMEM_EN */

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                &(pUsbApp->endpInDma[endpNumber].hbDmaChannel),
                &dmaConfig);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("BulkIn channel create failed 0x%x\r\n", mgrStat);
            return;
        }

#if LVDS_LB_EN
        /* Use default DMA adapter settings when Link Loopback is being used. */
        Cy_HBDma_Mgr_SetUsbEgressAdapterDelay(pUsbApp->pUsbdCtxt->pHBDmaMgr, 0);
#endif /* LVDS_LB_EN */

        /* Store the DMA channel pointer. */
        pUsbApp->hbBulkInChannel = &(pUsbApp->endpInDma[endpNumber].hbDmaChannel);
    }

#if AUDIO_IF_EN
    /* Handle UAC streaming endpoint. */
    if ((endpNumber == UAC_IN_ENDPOINT) && (dir != 0)) {
        if (pUsbApp->pPDMToUsbChn != NULL) {
            DBG_APP_ERR("ISOC-IN channel already created\r\n");
            return;
        }

        dmaConfig.size         = UAC_XFER_BUF_SIZE;         /* DMA Buffer size in bytes */
        dmaConfig.count        = PDM_APP_BUFFER_CNT;        /* DMA Buffer Count */
        dmaConfig.prodHdrSize  = 0;                         /* No header being added. */
        dmaConfig.prodBufSize  = UAC_XFER_BUF_SIZE;
        dmaConfig.eventEnable  = 0;
        dmaConfig.intrEnable   = 0x3;
        dmaConfig.bufferMode   = false;                     /* DMA buffer mode disabled */
        dmaConfig.chType       = CY_HBDMA_TYPE_MEM_TO_IP;   /* DMA Channel type: from MEM to USB EG IP. */
        dmaConfig.prodSckCount = 1;                         /* No. of producer sockets */
        dmaConfig.prodSck[0]   = CY_HBDMA_VIRT_SOCKET_WR;
        dmaConfig.prodSck[1]   = (cy_hbdma_socket_id_t)0;   /* Producer Socket ID: None */
        dmaConfig.consSckCount = 1;                         /* No. of consumer Sockets */
        dmaConfig.consSck[0]   = (cy_hbdma_socket_id_t)(CY_HBDMA_USBEG_SOCKET_00 + endpNumber);
        dmaConfig.consSck[1]   = (cy_hbdma_socket_id_t)0;   /* Consumer Socket ID: None */
        dmaConfig.cb           = Cy_UAC_HbDmaCb;              /* HB-DMA callback */
        dmaConfig.userCtx      = (void *)(pUsbApp);         /* Pass the application context as user context. */

        mgrStat = Cy_HBDma_Channel_Create(pUsbApp->pUsbdCtxt->pHBDmaMgr,
                &(pUsbApp->endpInDma[endpNumber].hbDmaChannel),
                &dmaConfig);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS) {
            DBG_APP_ERR("ISOCIN channel create failed 0x%x\r\n", mgrStat);
            return;
        }

        /* Store the pointer to the DMA channel. */
        pUsbApp->pPDMToUsbChn = &(pUsbApp->endpInDma[endpNumber].hbDmaChannel);

        /* Get the list of DMA buffers to be used for PDM to USB transfers. */
        mgrStat = Cy_HBDma_Channel_GetBufferInfo(pUsbApp->pPDMToUsbChn, pUsbApp->pPDMRxBuffer, PDM_APP_BUFFER_CNT);
        if (mgrStat != CY_HBDMA_MGR_SUCCESS)
        {
            DBG_APP_ERR("Get Buffer INFO failed 0x%x\r\n", mgrStat);
            return;
        }

        pUsbApp->pdmRxBufIndex      = 0;
        pUsbApp->pdmRxFreeBufCount  = PDM_APP_BUFFER_CNT;
        pUsbApp->nxtAudioTxBufIndex = 0;
        pUsbApp->pdmInXferPending   = false;

        DBG_APP_INFO("HBDMA ISOCIN endpNum:%x ChnCreate status: %x\r\n", endpNumber, mgrStat);

        if (pUsbApp->devSpeed > CY_USBD_USB_DEV_HS) {
            /* Enable the channel for data transfer. */
            mgrStat = Cy_HBDma_Channel_Enable(pUsbApp->pPDMToUsbChn, 0);
            DBG_APP_INFO("HBDMA PDM to USB channel enable status: %x\r\n", mgrStat);
        }
    }
#endif /* AUDIO_IF_EN */
} /* end of function  */

/****************************************************************************
* Function Name: Cy_USB_AppSetupEndpDmaParamsHs(cy_stc_usb_app_ctxt_t *pUsbApp,
                                                uint8_t *pEndpDscr)
******************************************************************************
* Summary:
*  Configure and enable HBW DMA channels.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pEndpDscr
* Endpoint descriptor pointer
*
* \return
* None
*
 *******************************************************************************/
static void
Cy_USB_AppSetupEndpDmaParamsHs(cy_stc_usb_app_ctxt_t *pUsbApp,
                               uint8_t *pEndpDscr)
{
    cy_stc_app_endp_dma_set_t *pEndpDmaSet;
    DW_Type *pDW;
    bool stat;
    uint32_t endpNumber;
    uint16_t maxPktSize = 0x00;
    cy_en_usb_endp_dir_t endpDirection;

    endpNumber = ((*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS)) & 0x7F);
    Cy_USBD_GetEndpMaxPktSize(pEndpDscr, &maxPktSize);

    if (*(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_ADDRESS) & 0x80)
    {
        endpDirection = CY_USB_ENDP_DIR_IN;
        pEndpDmaSet = &(pUsbApp->endpInDma[endpNumber]);
        pDW = pUsbApp->pCpuDw1Base;
    }
    else
    {
        endpDirection = CY_USB_ENDP_DIR_OUT;
        pEndpDmaSet = &(pUsbApp->endpOutDma[endpNumber]);
        pDW = pUsbApp->pCpuDw0Base;
    }

    stat = Cy_USBHS_App_EnableEpDmaSet(pEndpDmaSet, pDW, endpNumber, endpNumber, endpDirection, maxPktSize);
    DBG_APP_INFO("Enable EPDmaSet: endp=%x dir=%x stat=%x\r\n", endpNumber, endpDirection, stat);
} /* end of function  */

/****************************************************************************
* Function Name: Cy_USB_AppSetupEndpDmaParams(cy_stc_usb_app_ctxt_t *pUsbApp,
                                                uint8_t *pEndpDscr)
******************************************************************************
* Summary:
*  Configure and enable DMA channels.
*
* \param pUsbApp
* application layer context pointer.
*
* \param pEndpDscr
* Endpoint descriptor pointer
*
* \return
* None
*
 *******************************************************************************/
void Cy_USB_AppSetupEndpDmaParams (cy_stc_usb_app_ctxt_t *pUsbApp,
                                   uint8_t *pEndpDscr)
{
    /* We always need to do SS setup so that we can get data from the LVDS interface. */
    Cy_USB_AppSetupEndpDmaParamsSs(pUsbApp, pEndpDscr);

    if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS)
    {
        Cy_USB_AppSetupEndpDmaParamsHs(pUsbApp, pEndpDscr);
    }
}

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
void Cy_USB_AppConfigureEndp (cy_stc_usb_usbd_ctxt_t *pUsbdCtxt, uint8_t *pEndpDscr)
{
    cy_stc_usb_endp_config_t endpConfig;
    cy_en_usb_endp_dir_t endpDirection;
    bool valid;
    uint32_t endpType;
    uint32_t endpNumber, dir;
    uint16_t maxPktSize;
    uint32_t isoPkts = 0x00;
    uint8_t burstSize = 0x00;
    uint8_t maxStream = 0x00;
    uint8_t *pCompDscr = NULL;
    uint8_t interval = 0x00;
    cy_en_usbd_ret_code_t usbdRetCode;

    /* If it is not endpoint descriptor then return */
    if (!Cy_USBD_EndpDscrValid(pEndpDscr))
    {
        return;
    }
    Cy_USBD_GetEndpNumMaxPktDir(pEndpDscr, &endpNumber, &maxPktSize, &dir);

    if (dir)
    {
        endpDirection = CY_USB_ENDP_DIR_IN;
    }
    else
    {
        endpDirection = CY_USB_ENDP_DIR_OUT;
    }
    Cy_USBD_GetEndpType(pEndpDscr, &endpType);

    if ((CY_USB_ENDP_TYPE_ISO == endpType) || (CY_USB_ENDP_TYPE_INTR == endpType))
    {
        /* The ISOINPKS setting in the USBHS register is the actual packets per microframe value. */
        isoPkts = ((*((uint8_t *)(pEndpDscr + CY_USB_ENDP_DSCR_OFFSET_MAX_PKT + 1)) & CY_USB_ENDP_ADDL_XN_MASK) >> CY_USB_ENDP_ADDL_XN_POS) + 1;
    }

    valid = 0x01;
    if (pUsbdCtxt->devSpeed > CY_USBD_USB_DEV_HS)
    {
        /* Get companion descriptor and from there get burstSize. */
        pCompDscr = Cy_USBD_GetSsEndpCompDscr(pUsbdCtxt, pEndpDscr);
        Cy_USBD_GetEndpCompnMaxburst(pCompDscr, &burstSize);
        Cy_USBD_GetEndpCompnMaxStream(pCompDscr, &maxStream);
        Cy_USBD_GetEndpInterval(pEndpDscr, &interval);
    }

    /* Prepare endpointConfig parameter. */
    endpConfig.endpType = (cy_en_usb_endp_type_t)endpType;
    endpConfig.endpDirection = endpDirection;
    endpConfig.valid = valid;
    endpConfig.endpNumber = endpNumber;
    endpConfig.maxPktSize = (uint32_t)maxPktSize;
    endpConfig.isoPkts = isoPkts;
    endpConfig.burstSize = burstSize;
    endpConfig.streamID = maxStream;
    endpConfig.allowNakTillDmaRdy = false;
    endpConfig.interval = interval;
    usbdRetCode = Cy_USB_USBD_EndpConfig(pUsbdCtxt, endpConfig);

    /* Print status of the endpoint configuration to help debug. */
    DBG_APP_INFO("#ENDPCFG: %d, %d\r\n", endpNumber, usbdRetCode);
    return;
} /* end of function */

/****************************************************************************
* Function Name: Cy_USB_AppSetCfgCallback(void *pAppCtxt,
                                        cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                        cy_stc_usb_cal_msg_t *pMsg)
******************************************************************************
* Summary:
* Callback function will be invoked by USBD when set configuration is received
*
* \param pAppCtxt
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
void Cy_USB_AppSetCfgCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{

    cy_uvc_devconfigured = true;
    cy_stc_usb_app_ctxt_t *pUsbApp;
    uint8_t *pActiveCfg, *pIntfDscr, *pEndpDscr;
    uint8_t index, numOfIntf, numOfEndp;
    cy_en_usb_speed_t devSpeed;

    DBG_APP_INFO("AppSetCfgCbStart\r\n");

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    devSpeed = pUsbApp->devSpeed;

    /*
     * Print out the contents of the USB event log buffer which has
     * logged event related to enumeration and line training.
     */
    Cy_USB_AppPrintUsbEventLog(pAppCtxt, pUsbdCtxt->pSsCalCtxt);

    /* Disable optional Low Power Mode transitions. */
    Cy_USBD_LpmDisable(pUsbdCtxt);

    /*
     * Based on type of application as well as how data flows,
     * data wire can be used so initialize datawire.
     */
    Cy_DMA_Enable(pUsbApp->pCpuDw0Base);
    Cy_DMA_Enable(pUsbApp->pCpuDw1Base);

    pActiveCfg = Cy_USB_USBD_GetActiveCfgDscr(pUsbdCtxt);
    if (!pActiveCfg)
    {
        /* Set config should be called when active config value > 0x00. */
        return;
    }
    numOfIntf = Cy_USBD_FindNumOfIntf(pActiveCfg);
    if (numOfIntf == 0x00)
    {
        return;
    }

    for (index = 0x00; index < numOfIntf; index++)
    {
        /* During Set Config command always altSetting 0 will be active. */
        pIntfDscr = Cy_USBD_GetIntfDscr(pUsbdCtxt, index, 0x00);
        if (pIntfDscr == NULL)
        {
            DBG_APP_INFO("pIntfDscrNull\r\n");
            return;
        }

        numOfEndp = Cy_USBD_FindNumOfEndp(pIntfDscr);
        if (numOfEndp == 0x00)
        {
            DBG_APP_INFO("numOfEndp 0\r\n");
            continue;
        }

        pEndpDscr = Cy_USBD_GetEndpDscr(pUsbdCtxt, pIntfDscr);
        while (numOfEndp != 0x00)
        {
            Cy_USB_AppConfigureEndp(pUsbdCtxt, pEndpDscr);
            Cy_USB_AppSetupEndpDmaParams(pAppCtxt, pEndpDscr);
            numOfEndp--;

            if (devSpeed > CY_USBD_USB_DEV_HS)
            {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)) + 6);
            }
            else
            {
                pEndpDscr = (pEndpDscr + (*(pEndpDscr + CY_USB_DSCR_OFFSET_LEN)));
            }
        }
    }

    if (devSpeed <= CY_USBD_USB_DEV_HS)
    {
        /* Enable the interrupt for the DataWire channel used for video streaming to USBHS BULK-IN endpoint. */
        Cy_USB_AppInitDmaIntr(pUsbApp->uvcInEpNum, CY_USB_ENDP_DIR_IN, Cy_UVC_DataWire_ISR);
    }

    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_CONFIGURED;
    pUsbApp->devState = CY_USB_DEVICE_STATE_CONFIGURED;

#if UVC_INMEM_EN
    /* Update the DMA channel and make the descriptors point to the buffers with pre-filled colorbar data. */
    Cy_UvcInMem_PrepareBuffers(pUsbApp, pUsbApp->hbBulkInChannel);
#endif /* UVC_INMEM_EN */

#if USB3_LPM_ENABLE
    /* Schedule LPM enable after 1 second. */
    DBG_APP_INFO("Schedule LPM enable\r\n");
    pUsbApp->isLpmEnabled = false;
    pUsbApp->lpmEnableTime = Cy_USBD_GetTimerTick() + 1000;
#endif /* USB3_LPM_ENABLE */

    DBG_APP_INFO("AppSetCfgCbEnd Done\r\n");
    return;
} /* end of function */

/****************************************************************************
* Function Name: Cy_USB_AppBusResetCallback(void *pUserCtxt,cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
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
void Cy_USB_AppBusResetCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    DBG_APP_INFO("AppBusResetCallback\r\n");

    /* Stop and destroy the high bandwidth DMA channel if present. To be done before AppInit is called. */
    if (pUsbApp->hbBulkInChannel != NULL)
    {
        DBG_APP_TRACE("HBDMA destroy\r\n");
#if UVC_INMEM_EN
        Cy_UvcInMem_ClearBufPointers(pUsbApp, pUsbApp->hbBulkInChannel);
#endif /* UVC_INMEM_EN */
        Cy_HBDma_Channel_Disable(pUsbApp->hbBulkInChannel);
        Cy_HBDma_Channel_Destroy(pUsbApp->hbBulkInChannel);
        pUsbApp->hbBulkInChannel = NULL;
    }

    /*
     * USBD layer takes care of reseting its own data structure as well as
     * takes care of calling CAL reset APIs. Application needs to take care
     * of reseting its own data structure as well as "device function".
     */
    Cy_USB_AppInit(pUsbApp, pUsbdCtxt, pUsbApp->pCpuDmacBase,
                   pUsbApp->pCpuDw0Base, pUsbApp->pCpuDw1Base,
                   pUsbApp->pHbDmaMgrCtxt);
    pUsbApp->devState = CY_USB_DEVICE_STATE_RESET;
    pUsbApp->prevDevState = CY_USB_DEVICE_STATE_RESET;

    if (pUsbdCtxt->devSpeed >= CY_USBD_USB_DEV_SS_GEN1)
    {
#if USB3_LPM_ENABLE
        DBG_APP_INFO("Disabling LPM\r\n");
        Cy_USBSS_Cal_LPMDisable(pUsbdCtxt->pSsCalCtxt);
#endif /* USB3_LPM_ENABLE */

        /*
         * Toggle the REG_VBUS and PCLK_EN bits in the PHY.TOP_CTRL_0 register
         * when there is a Warm Reset.
         */
        if ((USB32DEV->USB32DEV_LNK.LNK_LTSSM_STATE & USB32DEV_LNK_LTSSM_STATE_LTSSM_STATE_Msk) ==
            CY_USBSS_LNK_STATE_RXDETECT_RES)
        {
            DBG_APP_TRACE("Skip reset PHY on WarmReset\r\n");
        }
    }
    cy_uvc_devconfigured = false;
    cy_uvc_IsApplnActive = false;
    return;
} /* end of function. */

/****************************************************************************
* Function Name: Cy_USB_AppBusResetDoneCallback
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
void Cy_USB_AppBusResetDoneCallback(void *pAppCtxt,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    DBG_APP_INFO("ppBusResetDoneCallback\r\n");

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->prevDevState = pUsbApp->devState;
    return;
} /* end of function. */

extern uint8_t CyFxUSB20DeviceDscr[];

/****************************************************************************
* Function Name: Cy_USB_AppBusSpeedCallback
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
void Cy_USB_AppBusSpeedCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->devState = CY_USB_DEVICE_STATE_DEFAULT;
    pUsbApp->devSpeed = Cy_USBD_GetDeviceSpeed(pUsbdCtxt);
    Cy_USBD_SetDscr(pUsbApp->pUsbdCtxt, CY_USB_SET_HS_DEVICE_DSCR, 0, (uint8_t *)CyFxUSB20DeviceDscr);
    return;
} /* end of function. */

/****************************************************************************
* Function Name: Cy_USB_AppSetupCallback
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
void Cy_USB_AppSetupCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                             cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    uint8_t bRequest, bReqType;
    uint8_t bType, bTarget, temp;
    uint16_t wValue, wIndex, wLength;
    bool isReqHandled = false;
    cy_en_usbd_ret_code_t retStatus = CY_USBD_STATUS_SUCCESS;
    cy_en_usb_endp_dir_t epDir = CY_USB_ENDP_DIR_INVALID;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t status = 0;

    /* Fast enumeration is used. Only requests addressed to the interface, class,
     * vendor and unknown control requests are received by this function. */

    /* Decode the fields from the setup request. */
    bReqType = pUsbdCtxt->setupReq.bmRequest;
    bType = ((bReqType & CY_USB_CTRL_REQ_TYPE_MASK) >> CY_USB_CTRL_REQ_TYPE_POS);
    bTarget = (bReqType & CY_USB_CTRL_REQ_RECIPENT_OTHERS);
    bRequest = pUsbdCtxt->setupReq.bRequest;
    wValue = pUsbdCtxt->setupReq.wValue;
    wIndex = pUsbdCtxt->setupReq.wIndex;
    wLength = pUsbdCtxt->setupReq.wLength;

    if (bType == CY_USB_CTRL_REQ_STD)
    {
        DBG_APP_INFO("CY_USB_CTRL_REQ_STD\r\n");
        if (bRequest == CY_USB_SC_SET_FEATURE)
        {
            DBG_APP_INFO("CY_USB_SC_SET_FEATURE\r\n");
            /* Function Suspend: ACK the request if the device is in configured state in an USB 3.x connection. */
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (wValue == 0))
            {
                DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_INTF\r\n");
                if ((pUsbApp->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) && (cy_uvc_devconfigured))
                    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                else
                    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);

                isReqHandled = true;
            }

            /* SET-FEATURE(EP-HALT) is only supported to facilitate Chapter 9 compliance tests. */
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (wValue == CY_USB_FEATURE_ENDP_HALT))
            {
                DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_ENDP\r\n");
                epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
                Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL),
                        epDir, true);

                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                    ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE))) {
                /* Set U1/U2 enable */
#if USB3_LPM_ENABLE
                if (pUsbApp->isLpmEnabled == false) {
                DBG_APP_INFO("Enabling LPM\r\n");
                pUsbApp->isLpmEnabled = true;
                Cy_USBD_LpmEnable(pUsbdCtxt);
            }
#endif /* USB3_LPM_ENABLE */
                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }

        if (bRequest == CY_USB_SC_CLEAR_FEATURE)
        {
            DBG_APP_INFO("CY_USB_SC_CLEAR_FEATURE\r\n");
            /* Function Suspend: ACK the request if the device is in configured state in an USB 3.x connection. */
            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (wValue == 0))
            {
                DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_INTF\r\n");
                if ((pUsbApp->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) && (cy_uvc_devconfigured))
                    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                else
                    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, TRUE);

                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_ENDP) && (wValue == CY_USB_FEATURE_ENDP_HALT))
            {
                DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_ENDP && CY_USB_FEATURE_ENDP_HALT\r\n");
                epDir = ((wIndex & 0x80UL) ? (CY_USB_ENDP_DIR_IN) : (CY_USB_ENDP_DIR_OUT));
                if ((epDir == CY_USB_ENDP_DIR_OUT) || ((wIndex & 0x7F) != pUsbApp->uvcInEpNum))
                {
                    /* For any EP other than the UVC streaming endpoint, just clear the STALL bit. */
                    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, ((uint32_t)wIndex & 0x7FUL),
                            epDir, false);
                    Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                }
                else
                {
#if LVDS_LB_EN
                    /*
                     * We need to stop the loopback source channel first so that data does not accumulate on the
                     * ingress thread interface after the streaming channel is reset. We want to ensure that the
                     * loopback source is stopped cleanly at the end of a buffer. Since the transfer of each
                     * loopback DMA buffer takes the order of 68 us, we set a flag indicating that no more data
                     * should be committed and then wait for 150 us (more than 2 * 68 us). By this time, it
                     * is guaranteed that the socket will reach an idle state.
                     */
                    pUsbApp->uvcFlowCtrlFlag = false;

                    lvdsLpbkBlocked = true;
                    Cy_SysLib_DelayUs(150);
                    if (pUsbApp->devSpeed <= CY_USBD_USB_DEV_HS) {
                        Cy_SysLib_DelayUs(500);
                    }

                    Cy_HBDma_Channel_Reset(&lvdsLbPgmChannel);
                    Cy_HBDma_Channel_Enable(&lvdsLbPgmChannel, 0);
                    lvdsConsCount = 0;
#endif /* LVDS_LB_EN */

                    xMsg.type = CY_USB_UVC_VIDEO_STREAM_STOP_EVENT;
                    xMsg.data[0] = wIndex;
                    status = xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                    ASSERT_NON_BLOCK(pdTRUE == status,status);
                    Cy_USBD_SendACkSetupDataStatusStage(pUsbdCtxt);
                }

                isReqHandled = true;
            }

            if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_DEVICE) &&
                    ((wValue == CY_USB_FEATURE_U1_ENABLE) || (wValue == CY_USB_FEATURE_U2_ENABLE))) {
                /* Set U1/U2 disable. */
#if USB3_LPM_ENABLE
                DBG_APP_INFO("Disabling LPM\r\n");
                pUsbApp->isLpmEnabled  = false;
                pUsbApp->lpmEnableTime = 0;
                Cy_USBD_LpmDisable(pUsbdCtxt);
#endif /* USB3_LPM_ENABLE */
                Cy_USBD_SendAckSetupDataStatusStage(pUsbdCtxt);
                isReqHandled = true;
            }
        }
    }

    if (bType == CY_USB_CTRL_REQ_VENDOR)
    {
        /* Add vendor command handling (if any) */
    }

    /* Check for UVC Class Requests */
    if (bType == CY_USB_CTRL_REQ_CLASS)
    {

        /* In USBSS connection, disable LPM entry for the next 100 ms. */
        if (pUsbApp->devSpeed >= CY_USBD_USB_DEV_SS_GEN1) {
            Cy_USB_App_KeepLinkActive(pUsbApp);
        }

        DBG_APP_INFO("UVC Class Requests\r\n");
        /* Handle requests addressed to the Video Control interface. */
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (CY_GET_LSB(wIndex) == CY_USB_UVC_INTERFACE_VC))
        {
            DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_INTF && CY_USB_UVC_INTERFACE_VC\r\n");
            /* Respond to VC_REQUEST_ERROR_CODE_CONTROL and stall every other request as this example does not support
               any of the Video Control features */
            if ((CY_GET_MSB(wIndex) == 0x00) && (wValue == CY_USB_UVC_VC_RQT_ERROR_CODE_CONTROL))
            {
                temp = CY_USB_UVC_RQT_STAT_INVALID_CTRL;
                isReqHandled = true;
                DBG_APP_INFO("UVC_RQT_STAT_INVALID_CTRL\r\n");
                Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, &temp, 0x01);
            }
        }

        /* Handle requests addressed to the Video Streaming interface. */
        if ((bTarget == CY_USB_CTRL_REQ_RECIPENT_INTF) && (CY_GET_LSB(wIndex) == CY_USB_UVC_INTERFACE_VS))
        {
            isReqHandled = true;
            DBG_APP_INFO("CY_USB_CTRL_REQ_RECIPENT_INTF && CY_USB_UVC_INTERFACE_VS\r\n");
            switch (wValue)
            {

            case CY_USB_UVC_VS_PROBE_CONTROL:
            case CY_USB_UVC_VS_COMMIT_CONTROL:
            {
                switch (bRequest)
                {
                case CY_USB_UVC_GET_INFO_REQ:
                    DBG_APP_INFO("UVC_GET_INFO_REQ\r\n");
                    Ep0TestBuffer[0] = 3; /* GET/SET requests are supported. */
                    Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)Ep0TestBuffer, 0x01);
                    break;

                case CY_USB_UVC_GET_LEN_REQ:
                    DBG_APP_INFO("UVC_GET_LEN_REQ\r\n");
                    Ep0TestBuffer[0] = CY_USB_UVC_MAX_PROBE_SETTING;
                    Cy_USB_USBD_SendEndp0Data(pUsbdCtxt, (uint8_t *)Ep0TestBuffer, 0x01);
                    break;

                /* We only have one functional setting. Keep returning the same as current, default
                 * minimum and maximum. */
                case CY_USB_UVC_GET_CUR_REQ:
                case CY_USB_UVC_GET_DEF_REQ:
                case CY_USB_UVC_GET_MIN_REQ:
                case CY_USB_UVC_GET_MAX_REQ:
                    DBG_APP_INFO("UVC_GET_CUR_DEF_MIN_MAX_REQ:%x \r\n", bRequest);

                    /* Handle GET request from task. */
                    xMsg.type    = CY_USB_UVC_DEVICE_GET_CUR_RQT;
                    xMsg.data[0] = wLength;
                    xMsg.data[1] = wValue;
                    xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                    isReqHandled = true;
                    break;

                case CY_USB_UVC_SET_CUR_REQ:
                    /* Since this request has OUT data, it should be handled in task context. */
                    xMsg.type    = CY_USB_UVC_DEVICE_SET_CUR_RQT;
                    xMsg.data[0] = wLength;
                    xMsg.data[1] = wValue;
                    xQueueSendFromISR(pUsbApp->uvcMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
                    isReqHandled = true;
                    break;

                default:
                    isReqHandled = false;
                    break;
                }
            }
            break;

            default:
                isReqHandled = false;
                break;
            }
        }

        /* Don't try to stall the endpoint if we have already attempted data transfer. */
    }

    /* SET_SEL request is supposed to have an OUT data phase of 6 bytes. */
    if ((bRequest == CY_USB_SC_SET_SEL) && (wLength == 6))
    {
        /* SET_SEL request is only received in USBSS case and the Cy_USB_USBD_RecvEndp0Data is blocking. */
        retStatus = Cy_USB_USBD_RecvEndp0Data(pUsbdCtxt, (uint8_t *)SetSelDataBuffer, wLength);
        DBG_APP_INFO("SET_SEL: EP0 recv stat = %d, Data=%x:%x\r\n",
                retStatus, SetSelDataBuffer[0], SetSelDataBuffer[1]);
        isReqHandled = true;
    }

    /*
     * If Request is not handled by the callback, Stall the command.
     */
    if (!isReqHandled)
    {
        Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00,
                                      CY_USB_ENDP_DIR_IN, TRUE);
    }
} /* end of function. */

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
void Cy_USB_AppSuspendCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;
    pUsbApp->prevDevState = pUsbApp->devState;
    pUsbApp->devState = CY_USB_DEVICE_STATE_SUSPEND;
} /* end of function. */

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
void Cy_USB_AppResumeCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                              cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_app_ctxt_t *pUsbApp;
    cy_en_usb_device_state_t tempState;

    pUsbApp = (cy_stc_usb_app_ctxt_t *)pAppCtxt;

    tempState = pUsbApp->devState;
    pUsbApp->devState = pUsbApp->prevDevState;
    pUsbApp->prevDevState = tempState;
    return;
} /* end of function. */

/*******************************************************************************
* Function name: Cy_USB_AppSetIntfCallback(void *pUserCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                       cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* Callback function will be invoked by USBD when SET_INTERFACE is  received
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
void Cy_USB_AppSetIntfCallback(void *pAppCtxt, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    cy_stc_usb_setup_req_t *pSetupReq;
    uint8_t intfNum, altSetting;

    DBG_APP_INFO("AppSetIntfCallback Start\r\n");
    pSetupReq  = &(pUsbdCtxt->setupReq);
    intfNum    = pSetupReq->wIndex;
    altSetting = pSetupReq->wValue;

#if AUDIO_IF_EN
    if (intfNum == UAC_STREAM_INTF_NUM) {
        Cy_UAC_AppSetIntfHandler((cy_stc_usb_app_ctxt_t *)pAppCtxt, altSetting);
        return;
    }
#endif /* AUDIO_IF_EN */

    /* Interface does not support multiple alternate settings: Stall the SET_INTERFACE request. */
    DBG_APP_INFO("SetIntf(%d) on interface %d: Stalled\r\n", altSetting, intfNum);
    Cy_USB_USBD_EndpSetClearStall(pUsbdCtxt, 0x00, CY_USB_ENDP_DIR_IN, true);
    DBG_APP_INFO("AppSetIntfCallback done\r\n");
} /* end of function. */

/*******************************************************************************
* Function name: Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when ZLP message comes
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
void Cy_USB_AppZlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppZlpCb\r\n");

    return;
} /* end of function. */

/*******************************************************************************
* Function name: Cy_USB_AppL1SleepCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when L1 Sleep message comes.
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
void Cy_USB_AppL1SleepCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                               cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppL1SleepCb\r\n");
    return;
} /* end of function. */

/*******************************************************************************
* Function name: Cy_USB_AppL1ResumeCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
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
void Cy_USB_AppL1ResumeCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppL1ResumeCb\r\n");
    return;
} /* end of function. */

/*******************************************************************************
* Function name: Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when SLP message comes.
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
void Cy_USB_AppSlpCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppSlpCb\r\n");
    return;
} /* end of function. */

/*******************************************************************************
* Function name: Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when set feature message comes.
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
void Cy_USB_AppSetFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                  cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppSetFeatureCb\r\n");
    return;
} /* end of function. */

/*******************************************************************************
* Function name: Cy_USB_AppClearFeatureCallback(void *pUsbApp, cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                           cy_stc_usb_cal_msg_t *pMsg)
****************************************************************************//**
*
* This Function will be called by USBD layer when clear feature message comes.
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
void Cy_USB_AppClearFeatureCallback(void *pUsbApp,
                                    cy_stc_usb_usbd_ctxt_t *pUsbdCtxt,
                                    cy_stc_usb_cal_msg_t *pMsg)
{
    DBG_APP_INFO("AppClearFeatureCb\r\n");
    return;
} /* end of function. */

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
void Cy_USB_AppQueueWrite(cy_stc_usb_app_ctxt_t *pAppCtxt, uint8_t endpNumber,
                          uint8_t *pBuffer, uint32_t dataSize)
{
    cy_stc_app_endp_dma_set_t *dmaset_p;

    /* Null pointer checks. */
    if ((pAppCtxt == NULL) || (pAppCtxt->pUsbdCtxt == NULL) ||
            (pAppCtxt->pCpuDw1Base == NULL) || (pBuffer == NULL)) {
        DBG_APP_ERR("QueueWrite Err0\r\n");
        return;
    }

    /*
     * Verify that the selected endpoint is valid and the dataSize
     * is non-zero.
     */
    dmaset_p = &(pAppCtxt->endpInDma[endpNumber]);
    if ((dmaset_p->valid == 0) || (dataSize == 0)) {
        DBG_APP_ERR("QueueWrite Err1\r\n");
        return;
    }

    Cy_USBHS_App_QueueWrite(dmaset_p, pBuffer, dataSize);
} /* end of function */

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
void Cy_USB_AppInitDmaIntr(uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection,
                           cy_israddress userIsr)
{
    cy_stc_sysint_t intrCfg;
    if ((endpNumber > 0) && (endpNumber < CY_USB_MAX_ENDP_NUMBER))
    {
#if (!CY_CPU_CORTEX_M4)
        intrCfg.intrPriority = 3;
        intrCfg.intrSrc = NvicMux7_IRQn;
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.cm0pSrc = (cy_en_intr_t)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }
#else
        intrCfg.intrPriority = 5;
        if (endpDirection == CY_USB_ENDP_DIR_IN)
        {
            /* DW1 channels 0 onwards are used for IN endpoints. */
            intrCfg.intrSrc =
                (IRQn_Type)(cpuss_interrupts_dw1_0_IRQn + endpNumber);
        }
        else
        {
            /* DW0 channels 0 onwards are used for OUT endpoints. */
            intrCfg.intrSrc =
                (IRQn_Type)(cpuss_interrupts_dw0_0_IRQn + endpNumber);
        }
#endif /* (!CY_CPU_CORTEX_M4) */

        if (userIsr != NULL)
        {
            /* If an ISR is provided, register it and enable the interrupt. */
            Cy_SysInt_Init(&intrCfg, userIsr);
            NVIC_EnableIRQ(intrCfg.intrSrc);
        }
        else
        {
            /* ISR is NULL. Disable the interrupt. */
            NVIC_DisableIRQ(intrCfg.intrSrc);
        }
    }
} /* end of function. */

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
void Cy_USB_AppClearDmaInterrupt(cy_stc_usb_app_ctxt_t *pAppCtxt,
                                 uint32_t endpNumber, cy_en_usb_endp_dir_t endpDirection)
{
    if ((pAppCtxt != NULL) && (endpNumber > 0) &&
            (endpNumber < CY_USB_MAX_ENDP_NUMBER)) {
        if (endpDirection == CY_USB_ENDP_DIR_IN) {
            Cy_USBHS_App_ClearDmaInterrupt(&(pAppCtxt->endpInDma[endpNumber]));
        } else  {
            Cy_USBHS_App_ClearDmaInterrupt(&(pAppCtxt->endpOutDma[endpNumber]));
        }
    }
} /* end of function. */

/*******************************************************************************
* Function name: Cy_CheckStatus
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
void Cy_CheckStatus(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking)
{
    if (!condition)
    {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, COLOR_RESET);
        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

/*******************************************************************************
* Function name: Cy_CheckStatusHandleFailure
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
void Cy_CheckStatusAndHandleFailure(const char *function, uint32_t line, uint8_t condition, uint32_t value, uint8_t isBlocking, void (*failureHandler)(void))
{
    if (!condition)
    {
        /* Application failed with the error code status */
        Cy_Debug_AddToLog(1, RED);
        Cy_Debug_AddToLog(1, "Function %s failed at line %d with status = 0x%x\r\n", function, line, value);
        Cy_Debug_AddToLog(1, COLOR_RESET);

        if(failureHandler != NULL)
        {
            (*failureHandler)();
        }
        if (isBlocking)
        {
            /* Loop indefinitely */
            for (;;)
            {
            }
        }
    }
}

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
void Cy_FailHandler(void)
{
    DBG_APP_ERR("Reset Done\r\n");
}

/*[]*/

