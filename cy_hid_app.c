/***************************************************************************//**
* \file cy_hid_app.c
* \version 1.0
*
* Implements a generic vendor-defined HID device that periodically sends data
* from CPU SRAM over a USB HID interrupt IN endpoint.
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

#include "cy_pdl.h"
#include "FreeRTOS.h"
#include "queue.h"
#include "task.h"
#include "timers.h"
#include "cy_usb_common.h"
#include "cy_usb_usbd.h"
#include "cy_usb_uvc_device.h"
#include "cy_usb_app.h"
#include "cy_debug.h"

#if HID_EN

/******************************************************************************
 * HID Report Descriptor
 ******************************************************************************
 * Defines a generic vendor-defined HID input report.
 * Usage Page: 0xFF00 (Vendor-Defined)
 * Report: 64 bytes of raw data (Input, Data, Variable, Absolute)
 *****************************************************************************/
const uint8_t cy_hid_report_descriptor[] = {
    0x06, 0x00, 0xFF,  /* Usage Page (Vendor-Defined 0xFF00) */
    0x09, 0x01,        /* Usage (Vendor-Defined 0x01) */
    0xA1, 0x01,        /* Collection (Application) */
    0x75, 0x08,        /*   Report Size (8 bits) */
    0x95, HID_REPORT_SIZE, /* Report Count */
    0x81, 0x02,        /*   Input (Data, Variable, Absolute) */
    0xC0,              /* End Collection */
};

/* Reference to the global application context. */
extern cy_stc_usb_app_ctxt_t appCtxt;

/*****************************************************************************
 * Function Name: Cy_HID_FillReport
 *****************************************************************************
 * Summary:
 *  Fills the HID report buffer with data from CPU SRAM. This is where
 *  the application populates the data to be sent to the USB host.
 *
 *  Modify this function to fill the buffer with whatever data you need.
 *
 * Parameters:
 *  \param pAppCtxt
 *  Pointer to the application context structure.
 *
 * Return:
 *  void
 *****************************************************************************/
static void Cy_HID_FillReport(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    uint8_t i;

    /*
     * Fill the HID report buffer with data.
     * This example fills with an incrementing counter in the first byte
     * and places the remaining bytes with a fixed pattern.
     */
    for (i = 0; i < HID_REPORT_SIZE; i++)
    {
        pAppCtxt->hidReportBuffer[i] = (uint8_t)(i & 0xFF);
    }
}

/*****************************************************************************
 * Function Name: Cy_HID_TimerCb
 *****************************************************************************
 * Summary:
 *  FreeRTOS timer callback that triggers the HID report transmission.
 *  Runs in the timer daemon task context (priority 14). Sends a message
 *  to the HID task to perform the actual USB write.
 *
 * Parameters:
 *  \param xTimer
 *  Timer handle (contains the application context as the timer ID).
 *
 * Return:
 *  void
 *****************************************************************************/
static void Cy_HID_TimerCb(TimerHandle_t xTimer)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    cy_stc_usbd_app_msg_t xMsg;
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pvTimerGetTimerID(xTimer);

    if (pAppCtxt->devState == CY_USB_DEVICE_STATE_CONFIGURED)
    {
        xMsg.type = CY_USB_HID_SEND_REPORT;
        xMsg.data[0] = 0;
        xMsg.data[1] = 0;
        xQueueSendFromISR(pAppCtxt->hidMsgQueue, &(xMsg), &(xHigherPriorityTaskWoken));
    }
}

/*****************************************************************************
 * Function Name: Cy_HID_AppTaskHandler
 *****************************************************************************
 * Summary:
 *  Task handler for the HID device. Waits on the HID message queue for
 *  timer events and sends HID reports on the USB IN endpoint.
 *
 * Parameters:
 *  \param pTaskParam
 *  Opaque pointer to the application context structure.
 *
 * Return:
 *  void
 *****************************************************************************/
static void Cy_HID_AppTaskHandler(void *pTaskParam)
{
    cy_stc_usb_app_ctxt_t *pAppCtxt;
    cy_stc_usbd_app_msg_t queueMsg;
    BaseType_t status;

    pAppCtxt = (cy_stc_usb_app_ctxt_t *)pTaskParam;

    while (1)
    {
        /* Wait for a message from the timer callback. */
        status = xQueueReceive(pAppCtxt->hidMsgQueue, &queueMsg, portMAX_DELAY);
        if (status != pdPASS)
        {
            continue;
        }

        switch (queueMsg.type)
        {
        case CY_USB_HID_SEND_REPORT:
            /*
             * Only queue a new write if the previous one has completed.
             * The DataWire DMA completion ISR clears hidReportPending.
             */
            if (!pAppCtxt->hidReportPending)
            {
                /* Fill the report buffer with data from CPU SRAM. */
                Cy_HID_FillReport(pAppCtxt);

                pAppCtxt->hidReportPending = true;
                Cy_USB_AppQueueWrite(pAppCtxt,
                                     HID_IN_ENDPOINT,
                                     pAppCtxt->hidReportBuffer,
                                     HID_REPORT_SIZE);
            }
            break;

        default:
            break;
        }
    }
}

/*****************************************************************************
 * Function Name: Cy_HID_InEpDma_ISR
 *****************************************************************************
 * Summary:
 *  Interrupt service routine for the DataWire DMA channel associated
 *  with the HID IN endpoint. Clears the DMA interrupt and marks the
 *  endpoint as ready for the next transfer.
 *
 * Parameters:
 *  void
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_HID_InEpDma_ISR(void)
{
    Cy_USB_AppClearDmaInterrupt(&appCtxt, HID_IN_ENDPOINT, CY_USB_ENDP_DIR_IN);
    appCtxt.hidReportPending = false;
    portYIELD_FROM_ISR(true);
}

/*****************************************************************************
 * Function Name: Cy_HID_AppInit
 *****************************************************************************
 * Summary:
 *  Initializes the HID application: creates the message queue, task,
 *  and periodic timer.
 *
 * Parameters:
 *  \param pAppCtxt
 *  Pointer to the application context structure.
 *
 * Return:
 *  void
 *****************************************************************************/
void Cy_HID_AppInit(cy_stc_usb_app_ctxt_t *pAppCtxt)
{
    BaseType_t status;

    if ((!pAppCtxt) || (pAppCtxt->firstInitDone))
    {
        return;
    }

    pAppCtxt->hidInEpNum = HID_IN_ENDPOINT;
    pAppCtxt->hidReportPending = false;

    /* Create message queue for the HID task. */
    pAppCtxt->hidMsgQueue = xQueueCreate(4, CY_USB_UVC_DEVICE_MSG_SIZE);
    if (pAppCtxt->hidMsgQueue == NULL)
    {
        DBG_APP_ERR("HID MsgQueue create failed\r\n");
        return;
    }

    DBG_APP_INFO("Created HID Queue\r\n");
    vQueueAddToRegistry(pAppCtxt->hidMsgQueue, "HIDMsgQueue");

    /* Create the HID task. */
    status = xTaskCreate(Cy_HID_AppTaskHandler, "HidTask", 512,
                         (void *)pAppCtxt, 5, &(pAppCtxt->hidTaskHandle));
    if (status != pdPASS)
    {
        DBG_APP_ERR("HID TaskCreate failed\r\n");
        return;
    }

    /* Create an auto-reload timer for periodic HID reports. */
    pAppCtxt->hidTimer = xTimerCreate("HidTimer",
                                      pdMS_TO_TICKS(HID_POLLING_INTERVAL_MS),
                                      pdTRUE,
                                      (void *)pAppCtxt,
                                      Cy_HID_TimerCb);
    if (pAppCtxt->hidTimer == NULL)
    {
        DBG_APP_ERR("HID TimerCreate failed\r\n");
        return;
    }

    /* Start the periodic timer. */
    xTimerStart(pAppCtxt->hidTimer, 0);

    DBG_APP_INFO("HID AppInit done\r\n");
}

#endif /* HID_EN */

/*[]*/
