# EZ-USB&trade; FX20: USB Video Class (UVC) application

This code example demonstrates the implementation of a USB Video Class (UVC) 1.1-compliant camera application using Infineon's EZ-USB&trade; FX20 device. An integrated USB Audio Class (UAC) interface is provided to stream the audio data received from a PDM microphone. This example illustrates the configuration and usage of the sensor interface port (SIP) on the EZ-USB&trade; FX20 device to implement the synchronous slave FIFO IN protocol.

> **Note:** This code example is applicable to EZ-USB&trade; FX20, EZ-USB&trade; FX10, and EZ-USB&trade; FX5 devices.

[View this README on GitHub.](https://github.com/Infineon/mtb-example-fx20-uvc-uac)

[Provide feedback on this code example.](https://cypress.co1.qualtrics.com/jfe/form/SV_1NTns53sK2yiljn?Q_EED=eyJVbmlxdWUgRG9jIElkIjoiQ0UyNDA4NjUiLCJTcGVjIE51bWJlciI6IjAwMi00MDg2NSIsIkRvYyBUaXRsZSI6IkVaLVVTQiZ0cmFkZTsgRlgyMDogVVNCIFZpZGVvIENsYXNzIChVVkMpIGFwcGxpY2F0aW9uIiwicmlkIjoic3VrdSIsIkRvYyB2ZXJzaW9uIjoiMS4wLjMiLCJEb2MgTGFuZ3VhZ2UiOiJFbmdsaXNoIiwiRG9jIERpdmlzaW9uIjoiTUNEIiwiRG9jIEJVIjoiV0lSRUQiLCJEb2MgRmFtaWx5IjoiU1NfVVNCIn0=)


## Requirements

- [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) v3.5 or later (tested with v3.5)
- Board support package (BSP) minimum required version: 4.3.3
- Programming language: C


## Supported toolchains (make variable 'TOOLCHAIN')

- GNU Arm&reg; Embedded Compiler v11.3.1 (`GCC_ARM`) – Default value of `TOOLCHAIN`
- Arm&reg; Compiler v6.22 (`ARM`)


## Supported kits (make variable 'TARGET')

- [EZ-USB&trade; FX20 DVK](https://www.infineon.com/fx20) (`KIT_FX20_FMC_001`) – Default value of `TARGET`


## Hardware setup

This example uses the board's default configuration. See the kit user guide to ensure that the board is configured correctly.

This example demonstrates the implementation of a UVC-compliant camera application with the [Titanium Ti180 J484 Development Kit](https://www.efinixinc.com/products-devkits-titaniumti180j484.html) as the video source.

> **Note:** The Titanium Ti180 J484 Development Kit is used for demonstration purposes only. The Ti180 FPGA bitfiles listed in **Table 5** of the [FPGA bitfile information](#fpga-bitfile-information) Section supports up to 4K (3840 x 2160) colorbar video streaming.


## Software setup

See the [ModusToolbox&trade; tools package installation guide](https://www.infineon.com/ModusToolboxInstallguide) for information about installing and configuring the tools package.

Install a terminal emulator if you do not have one. Instructions in this document use [Tera Term](https://teratermproject.github.io/index-en.html).

Install the [EZ-USB&trade; FX Control Center](https://softwaretools.infineon.com/tools/com.ifx.tb.tool.ezusbfxcontrolcenter) application for assistance with device programming and testing.

Install the [Efinity Software](https://www.efinixinc.com/products-efinity.html) application to program the bit file to the FPGA in the Titanium Ti180 J484 Development Kit.


## Using the code example


### Create the project

The ModusToolbox&trade; tools package provides the Project Creator as both a GUI tool and a command line tool.

<details><summary><b>Use Project Creator GUI</b></summary>

1. Open the Project Creator GUI tool

   There are several ways to do this, including launching it from the dashboard or from inside the Eclipse IDE. For more details, see the [Project Creator user guide](https://www.infineon.com/ModusToolboxProjectCreator) (locally available at *{ModusToolbox&trade; install directory}/tools_{version}/project-creator/docs/project-creator.pdf*)

2. On the **Choose Board Support Package (BSP)** page, select a kit supported by this code example. See [Supported kits](#supported-kits-make-variable-target)

   > **Note:** To use this code example for a kit not listed here, you may need to update the source files. If the kit does not have the required resources, the application may not work

3. On the **Select Application** page:

   a. Select the **Applications(s) Root Path** and the **Target IDE**

      > **Note:** Depending on how you open the Project Creator tool, these fields may be pre-selected for you

   b. Select this code example from the list by enabling its check box

      > **Note:** You can narrow the list of displayed examples by typing in the filter box

   c. (Optional) Change the suggested **New Application Name** and **New BSP Name**

   d. Click **Create** to complete the application creation process

</details>


<details><summary><b>Use Project Creator CLI</b></summary>

The 'project-creator-cli' tool can be used to create applications from a CLI terminal or from within batch files or shell scripts. This tool is available in the *{ModusToolbox&trade; install directory}/tools_{version}/project-creator/* directory.

Use a CLI terminal to invoke the 'project-creator-cli' tool. On Windows, use the command-line 'modus-shell' program provided in the ModusToolbox&trade; installation instead of a standard Windows command-line application. This shell provides access to all ModusToolbox&trade; tools. You can access it by typing "modus-shell" in the search box in the Windows menu. In Linux and macOS, you can use any terminal application.

The following example clones the "[mtb-example-fx20-uvc-uac](https://github.com/Infineon/mtb-example-fx20-uvc-uac)" application with the desired name "UVC_UAC" configured for the *KIT_FX20_FMC_001* BSP into the specified working directory, *C:/mtb_projects*:

   ```
   project-creator-cli --board-id KIT_FX20_FMC_001 --app-id mtb-example-fx20-uvc-uac --user-app-name UVC_UAC --target-dir "C:/mtb_projects"
   ```

The 'project-creator-cli' tool has the following arguments:

Argument | Description | Required/optional
---------|-------------|-----------
`--board-id` | Defined in the <id> field of the [BSP](https://github.com/Infineon?q=bsp-manifest&type=&language=&sort=) manifest | Required
`--app-id`   | Defined in the <id> field of the [CE](https://github.com/Infineon?q=ce-manifest&type=&language=&sort=) manifest | Required
`--target-dir`| Specify the directory in which the application is to be created if you prefer not to use the default current working directory | Optional
`--user-app-name`| Specify the name of the application if you prefer to have a name other than the example's default name | Optional

<br>

> **Note:** The project-creator-cli tool uses the `git clone` and `make getlibs` commands to fetch the repository and import the required libraries. For details, see the "Project creator tools" section of the [ModusToolbox&trade; tools package user guide](https://www.infineon.com/ModusToolboxUserGuide) (locally available at {ModusToolbox&trade; install directory}/docs_{version}/mtb_user_guide.pdf).

</details>


### Open the project

After the project has been created, you can open it in your preferred development environment.


<details><summary><b>Eclipse IDE</b></summary>

If you opened the Project Creator tool from the included Eclipse IDE, the project will open in Eclipse automatically.

For more details, see the [Eclipse IDE for ModusToolbox&trade; user guide](https://www.infineon.com/MTBEclipseIDEUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_ide_user_guide.pdf*).

</details>


<details><summary><b>Visual Studio (VS) Code</b></summary>

Launch VS Code manually, and then open the generated *{project-name}.code-workspace* file located in the project directory.

For more details, see the [Visual Studio Code for ModusToolbox&trade; user guide](https://www.infineon.com/MTBVSCodeUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mt_vscode_user_guide.pdf*).

</details>


<details><summary><b>Command line</b></summary>

If you prefer to use the CLI, open the appropriate terminal, and navigate to the project directory. On Windows, use the command-line 'modus-shell' program; on Linux and macOS, you can use any terminal application. From there, you can run various `make` commands.

For more details, see the [ModusToolbox&trade; tools package user guide](https://www.infineon.com/ModusToolboxUserGuide) (locally available at *{ModusToolbox&trade; install directory}/docs_{version}/mtb_user_guide.pdf*).

</details>


### Using this code example with specific manufacturer part numbers (MPNs)

By default, the code example build is targeted for the CYUSB4024-BZXI MPN, which has 512 KB of flash memory and 1024 KB of buffer RAM. Use the **BSP Assistant** tool to modify the application to target different EZ-USB&trade; FX20, EZ-USB&trade; FX10, EZ-USB&trade; FX5N, or EZ-USB&trade; FX5 family MPNs as shown in **Table 1**.

> **Note:** This application utilizes the Quad SPI interface on the EZ-USB&trade; FX device and hence is not supported on CYUSB4022-FCAXI, CYUSB4012-FCAXI, CYUSB3282-FCAXI, and CYUSB3082-FCAXI devices. It also requires more than 512 KB of DMA buffer RAM and hence is not supported on the CYUSB3081-FCAXI device.

**Table 1. MPNs supported by this code example**

Part Number       | Family | Flash size (KB) | Buffer RAM size (KB)
:---------------- | :----- | :-------------- | :-------------------
CYUSB4024-BZXI    | FX20   | 512             | 1024
CYUSB4021-FCAXI   | FX20   | 512             | 1024
CYUSB4014-FCAXI   | FX10   | 512             | 1024
CYUSB4013-FCAXI   | FX10   | 512             | 1024
CYUSB4011-FCAXI   | FX10   | 512             | 1024
CYUSB3284-FCAXI   | FX5N   | 512             | 1024
CYUSB3084-FCAXI   | FX5    | 512             | 1024
CYUSB3083-FCAXI   | FX5    | 512             | 1024


#### Setup for a different MPN

Perform the following steps to modify the code example to work on a different MPN, as listed in **Table 1**:

1. Launch the BSP Assistant tool:

   a. **Eclipse IDE:** Launch the BSP Assistant tool by navigating to **Quick Panel** > **Tools**

   b. **Visual Studio Code:** Select the ModusToolbox&trade; extension from the left menu bar and launch the BSP Assistant tool, which is available in the **Application** menu of the **MODUSTOOLBOX TOOLS** section

2. In **BSP Assistant**, select **Devices** from the tree view on the left

3. Choose the desired part from the dropdown menu on the right

4. Click "Save" to close the BSP Assistant tool

5. Build the application and proceed with programming


## Operation

1. Connect the board (J2) to your PC using the provided USB cable

2. Connect the USB FS port (J3) on the board to the PC for debug logs

3. Open a terminal program and select the serial COM port. Set the serial port parameters to 8N1 and 921600 baud

4. Browse the *\<CE Title>/BitFiles/* folder for Ti180 FPGA binary and program the FPGA. For more details, see the [J484 kit user guide](https://www.efinixinc.com/products-devkits-titaniumti180j484.html)

5. After successful programming, switch off the FPGA DVK power, connect the FPGA (Titanium Ti180 J484 Development Kit) DVK to the FMC (J8) connector of the KIT_FX20_FMC_001 board, and switch on the FPGA DVK power

6. Perform the following steps to program the board using the EZ-USB&trade; FX Control Center (Alpha) application

   1. To enter Bootloader mode:

         a. Press and hold the PMODE (SW2) switch

         b. Press and release the RESET (SW3) switch

         c. Release the PMODE switch

   2. Open the EZ-USB&trade; FX Control Center application. Observe the EZ-USB&trade; FX20 device displayed as **EZ-USB&trade; FX Bootloader**

   3. Select the **EZ-USB&trade; FX Bootloader** device in EZ-USB&trade; FX Control Center

   4. Click **Program** > **Internal Flash**

   5. Navigate to the *\<CE Title>/build/APP_KIT_FX20_FMC_001/Release* folder within the CE directory and locate the *.hex* file and program. Confirm if the programming is successful in the log window of the EZ-USB&trade; FX Control Center application

7. After programming, the application starts automatically. Confirm the following title is displayed on the UART terminal

   **Figure 1. Terminal output on program startup**

   ![](images/terminal-fx20-uvc-uac.png)

7. Open any third-party camera application and select the **EZ-USB&trade; FX20** device and video resolution to stream video. By default, the device streams 4K (3840 x 2160 ~60 fps) video data


## Debugging


### Using the Arm&reg; debug port

If you have access to a MiniProg or KitProg3 device, you can debug the example to step through the code.


<details><summary><b>In Eclipse IDE</b></summary>

Use the **\<Application Name> Debug (KitProg3_MiniProg4)** configuration in the **Quick Panel**. For details, see the "Program and debug" section in the [Eclipse IDE for ModusToolbox&trade; user guide](https://www.infineon.com/MTBEclipseIDEUserGuide).

</details>


<details><summary><b>In other IDEs</b></summary>

Follow the instructions in your preferred IDE.

</details>


### Log messages

The code example and the EZ-USB&trade; FX20 stack output debug log messages indicate any unexpected conditions and highlight the performed operations.

By default, the USB FS port is enabled for debug logs. To enable debug logs on UART, set the `USBFS_LOGS_ENABLE` compiler flag to '0u' in the *Makefile* file. SCB1 of the EZ-USB&trade; FX20 device is used as UART with a baud rate of 921600 to send out log messages through the P8.1 pin.

The verbosity of the debug log output can be modified by setting the `DEBUG_LEVEL` macro in the *main.c* file with the values shown in **Table 2**.

**Table 2. Debug values**

Macro value | Description
:--------   | :------------
1u          | Enable only error messages
2u          | Enable error and warning messages
3u          | Enable error, warning, and info messages
4u          | Enable all message types


## Design and implementation

This code example demonstrates the implementation of the UVC and UAC specifications, enabling the EZ-USB&trade; FX20 device to function as a UVC- and UAC-compliant composite device. This allows the video and audio data to be streamed over USB 3.2. This application uses various low-performance peripherals to interface with the system, such as:

- I2C master to configure the video source
- PDM receiver interface to connect audio source
- Enable debug prints over CDC using the USBFS block on the EZ-USB&trade; FX20 device


### Features

- **USB specification:** USB 2.0 (High-Speed only) and USB 3.2 Gen2/Gen1

   > **Note:** UVC and UAC functions are not supported in USB Full-Speed connections

- **Video format:** Uncompressed YUY2 (YUYV)

- **Video resolutions:**
   - **USB3.2 (Gen1/Gen2):** 3840 x 2160 (4K), 1280 x 720 (720p), and 640 x 480 (VGA)
   - **USB2.0 (HS):** 640 x 480 (VGA)

- FPGA configuration using SMIF block. The application supports FPGA configuration in passive serial (x4) mode only

> **Note:** This example does not currently implement any UVC control functions such as brightness, contrast, or exposure.

This code example demonstrates the following:

- Using EZ-USB&trade; FX20 APIs to implement a standard UVC device. Handling of class-specific USB control requests at the application level

- Streaming video data at USB 3.2 Gen2 speeds (4K video stream at ~60 fps) from the SIP to USB endpoint. A Bulk endpoint is used for video streaming to enable the maximum throughput, which can exceed the maximum allowed by the USB specification over isochronous endpoints

- FPGA configuration using SMIF Block of the EZ-USB&trade; FX20 device

- I2C register writes to configure FPGA register


### Video/audio streaming data path


#### Audio streaming

- The application enables EP 3-In as an isochronous endpoint with a maximum packet size of 192 bytes
- The device receives the audio data through the PDM receiver and sends it on EP 3-In
- Four DMA buffers of size 192 bytes are used to hold the data while it is being forwarded to the USB


#### Video streaming

- The application enables EP 1-In as a bulk endpoint with a maximum packet size of 1024 bytes. In case of USB3.2 Gen1/Gen2, the endpoint is configured to support a maximum burst of 16 packets
- The device receives the video data through the LVDS/LVCMOS Adapter 0 Socket 0 and sends it to EP 1-In
- Four DMA buffers of 64512 bytes size are used to hold the data while it is being forwarded to USB


#### UVC header addition modes

A 32-byte UVC header is added to each buffer, containing 64480 bytes of video data, and the resulting 64512 bytes are sent to the USB host. By default, the header addition is performed by a firmware task. 
If the FPGA, which drives the SIP interface of EZ-USB&trade; FX20 supports the header addition, the application can be configured to enable FPGA to add the UVC header, speeding up the streaming operation.

- The firmware adds 32 bytes of UVC header in the data received from the LVCMOS/LVDS side before the data is sent to the USB side on UVC Bulk endpoint 1-In
- UVC Header is inserted by the FPGA itself. To enable this feature, enable the `FPGA_ADDS_HEADER` compiler flag in *Makefile*
- This example supports UVC header insertion using the LVDS IP. This option can only be used if the FPGA which provides the video data supports `enhanced mode` commands to trigger the UVC header (metadata) addition. Set `INMD_EN` in *Makefile* to enable this mode

> **Note:** This example does not currently implement LVCMOS enhanced mode to add UVC header.


#### Internal colorbar generation

The application supports two methods of internal colorbar generation so the USB video stream can be observed and its speed measured in the absence of an FPGA data source. These modes can be tested and used with only the EZ-USB&trade; DX20 DVK.

- The first mode uses the 'Link loopback' feature of the FX20 SIP, which allows a data pattern to be generated and transmitted through one port and received through the other port. In this case, Port 1 is configured in transmit mode and sends data corresponding to a colorbar pattern with 8 colours. Port 0 is configured and operates in the same way as it would be to receive data from an FPGA. It receives the video data, adds the UVC headers through the DMA callbacks, and streams to the USB host. The maximum data rate achievable in this mode of operation is about 6.4 Gbps due to limitations of the clock frequency supported in loopback mode

- The second mode uses a firmware-driven mechanism to stream video data that is prefilled in a set of RAM buffers. The firmware uses the DMA support of the FX20 device to queue transfers from the RAM buffers in the correct order based on the UVC specifications. See the *cy_video_inmem.c* file for implementation details


### Application workflow

The application flow involves three main steps:

- [Initialization](#initialization)
- [USB device enumeration](#usb-device-enumeration)
- [UVC data transfer](#uvc-data-transfer)


#### Initialization

During initialization, the following steps are performed:

1. All the required data structures are initialized

2. USBD and USB driver (CAL) layers are initialized

3. The application registers all descriptors supported by function/application with the USBD layer

4. The application registers callback functions for different events, such as `RESET`, `SUSPEND`, `RESUME`, `SET_CONFIGURATION`, `SET_INTERFACE`, `SET_FEATURE`, and `CLEAR_FEATURE`. USBD will call the respective callback function when the corresponding events are detected

5. The data transfer state machines are initialized

6. The application registers handlers for all relevant interrupts

7. The application makes the USB device visible to the host by calling the Connect API

8. If FPGA configuration using FX is enabled, FPGA is configured using the SMIF (in x4 or Quad mode) block to read the bit file stored in the external flash. FPGA sees the data on the bus and gets configured

9. The FPGA is initialized using I2C writes to FPGA registers

10. The application initializes the SIP block on the EZ-USB&trade; FX20 as required by the selected LVCMOS operating mode


#### USB device enumeration

1. During USB device enumeration, the host requests for descriptors, which are already registered with the USBD layer during the initialization phase
2. The host sends `SET_CONFIGURATION` and `SET_INTERFACE` commands to activate the required function in the device
3. After the `SET_CONFIGURATION` and `SET_INTERFACE` commands, the application task takes control and enables the endpoints for data transfer


#### UVC data transfer

**Loopback mode (LVDS interface):**

1. Loopback program (video data and control bytes) is stored in HBWSS SRAM
2. Data is moved by Thread 3 to Port 1 through DMA and GPIF
3. Link level loopback is enabled so the data generated by Port 1 is received by Port 0
4. Data moves from Port 0 to USB 3.x Bulk endpoint-In through GPIF and DMA
5. Data moves from device Bulk endpoint-In to the host UVC application


**Firmware generated (in-memory) mode:**

1. A set of memory buffers are allocated in the high-bandWidth RAM region and prefilled with UVC headers as well as colorbar data
2. A DMA channel of the `memory-to-IP` type is created to send data from these memory buffers to the USB host
3. When the `SET_CUR` request to start video streaming is received, video streaming is started by queuing requests to send data from these buffers
4. Data moves from device Bulk endpoint-In to the host UVC application
5. When DMA consume events indicating the completion of transfer from each buffer is received, firmware queues additional transfer requests on the endpoint to maintain a continuous data flow


**Normal mode:**

1. Depending on the compile-time options, LVDS/LVCMOS interface, Port 1/Port 0, and Narrow Link/Wide Link are selected
2. After the streaming DMA channel is enabled, the DMA ready flag on the SIP interface asserts and the FPGA data source starts streaming the video data to the EZ-USB&trade; FX20 device
3. Video data moves from the LVDS/LVCMOS subsystem to the SRAM through high-bandwidth DMA
4. The data is forwarded on the USBSS or USBHS EP 1-In, based on the active USB connection speed. DataWire DMA channels are used for USBHS transfers and high-bandwidth DMA channels are used for USBSS transfers
5. Video data moves from USB device Bulk endpoint-In to the host UVC application


#### Integrated UAC data transfer

In addition to the UVC interface, this application also implements a UAC interface, which carries the data received from a PDM microphone.

- P4.1 and P4.2 pins of the EZ-USB&trade; FX20 device are used to interface to the PDM microphone
- P4.1 is the clock output from the EZ-USB&trade; FX20 device, which is generated at 3.072 MHz
- P4.2 is the data input from the PDM microphones to the EZ-USB&trade; FX20 device

A pair of microphones in stereo configuration can be connected. By default, the interface is configured for Stereo operation.


#### Slave FIFO interface

The code example uses a synchronous slave FIFO interface when configured in LVCMOS mode. The slave FIFO interface connections are:

**Table 3. Control signal usage in LVCMOS slave FIFO state machine for Port 0**

EZ-USB&trade; FX pin            | Function            | Description
:-------------    | :------------       | :--------------
P0CLK             | PCLK                | LVCMOS clock
P0CTL0            | SLCS#               | Active low chip select signal. Asserted (low) by the master/FPGA when communicating with the EZ-USB&trade; FX device
P0CTL1            | SLWR#               | Active low write enable signal. Asserted (low) by the master/FPGA when sending any data to the EZ-USB&trade; FX device
P0CTL2            | SLOE#               | Active low output enable signal
P0CTL3            | SLRD#               | Active low read enable signal. Not used in this application as data is only being received by the EZ-USB&trade; FX device
P0CTL5            | FlagA               | Active low DMA ready indication for currently addressed/active thread
P0CTL6            | Link Ready          | Active high Link ready indication for respective link/port
P0CTL7            | PKTEND#             | Active low packet end signal. Asserted (low) when the FPGA/master wants to terminate the ongoing DMA transfer
P0CTL9            | A0                  | LS bit of 2-bit address bus used to select thread (applicable for Port 0 Narrow Link)
P0CTL8            | A1                  | MS bit of 2-bit address bus used to select thread (applicable for Port 0 Narrow Link)
P1CTL9            | A0                  | LS bit of 2-bit address bus used to select thread (applicable for Port 0 WideLink)
P1CTL8            | A1                  | MS bit of 2-bit address bus used to select thread (Applicable for Port 0 WideLink)

> **Note:** Port 1 Narrow Link has the same address lines as that of WideLink.


## Compile-time configurations

This application's functionality can be customized by setting variables in *Makefile* or by configuring them through `make` CLI arguments.

- Run the `make build` command or build the project in your IDE to compile the application and generate a USB bootloader-compatible binary. This binary can be programmed onto the EZ-USB&trade; FX20 device using the EZ-USB&trade; Control Center application

- Run the `make build BLENABLE=no` command or set the variable in *Makefile* to compile the application and generate the standalone binary. This binary can be programmed onto the EZ-USB&trade; FX20 device through the SWD interface using the OpenOCD tool. For more details, see the [EZ-USB&trade; FX20 SDK user guide](https://www.infineon.com/fx20)

- Choose between the **Arm&reg; Compiler** or the **GNU Arm&reg; Embedded Compiler** build toolchains by setting the `TOOLCHAIN` variable in *Makefile* to `ARM` or `GCC_ARM` respectively. If you set it to `ARM`, ensure to set `CY_ARM_COMPILER_DIR` as a make variable or environment variable, pointing to the path of the compiler's root directory

- Run the `make build LPBK_EN=yes` command or set the variable in *Makefile* to make the application use the LVDS link loopback feature to send colorbar data to sensor interface Port 0. This configuration can be used on the EZ-USB&trade; FX20 DVK without requiring any additional boards or connections

   > **Note:** The internal data path used for this function has throughput limitations and can only support about 48 frames per second of a 4K video stream. Using an external video source is required to achieve the data rates enabled by the USB 3.2 Gen2 data connection.

- Run the `make build FWGEN=yes` command or set the variable in *Makefile* to make the application stream data from the prefilled RAM buffers based on a firmware sequence. This configuration can also be used on the EZ-USB&trade; FX20 DVK without requiring any additional boards or connections

By default, the application is configured to receive data from a 32-bit wide LVCMOS interface in DDR mode and make a USB 3.2 Gen2x2 (20 Gbps) data connection. Additional settings can be configured through macros specified by the `DEFINES` variable in *Makefile*:

**Table 4. Macro description**

Macro name           |    Description                           | Allowed values
:-------------       | :------------                            | :--------------
`USB_CONN_TYPE`      | Choose USB connection speed from a set of options | `CY_USBD_USB_DEV_SS_GEN2X2` for USB 3.2 Gen2x2 <br> `CY_USBD_USB_DEV_SS_GEN2` for USB 3.2 Gen2x1 <br> `CY_USBD_USB_DEV_SS_GEN1X2` for USB 3.2 Gen1x2 <br> `CY_USBD_USB_DEV_SS_GEN1` for USB 3.2 Gen1x1 <br> `CY_USBD_USB_DEV_HS` for USB 2.0 HS<br> `CY_USBD_USB_DEV_FS` for USB 1.1 FS
`LVDS_LB_EN`         | Enable link loopback                     | '1u' to enable link loopback <br> '0u' for disable
`LVCMOS_EN`          | Select the LVCMOS/LVDS                   | '1u' for LVCMOS <br> '0u' for LVDS
`LVCMOS_DDR_EN`      | Select LVCMOS clock configuration        | '1u' for LVCMOS DDR clock <br> '0u' to select LVCMOS SDR
`WL_EN`              | Select the WideLink or Narrow Link       | '1u' for 32-bit/WideLink <br> '0u' for Narrow Link
`PORT1_EN`           | Select LVCMOS/LVDS port                  | '1u' to selecting Port 1 <br> '0u' to select Port 0
`INTERLEAVE_EN`      | Enable thread interleave Port 0          | '1u' to enable thread interleaving on Port 0 <br> '0u' to select single thread
`FPGA_ENABLE`        | Select FPGA as data source               | '1u' if FPGA is interfaced to the EZ-USB&trade; FX20 <br> '0u' for to disable FPGA interface
`FPGA_ADDS_HEADER`   | UVC header addition                      | '1u' for FPGA added header <br> '0u' for EZ-USB&trade; FX20 added header
`INMD_EN`            | Insert metadata                          | '1u' to enable insert metadata <br> '0u' to disable insert metadata
`AUDIO_IF_EN`        | Enable integrated UAC function           | '1u' to enable <br> '0u' to disable
`USBFS_LOGS_ENABLE`  | Enable debug logs through USBFS port     | '1u' for debug logs over USBFS <br> '0u' for debug logs over UART (SCB1)
`USB3_LPM_ENABLE`    | Enable USB LPM handling                  | '1u' to enable USB LPM handling <br> '0u' to disable LPM
`FPGA_CONFIG_EN`     | Enable FPGA configuration                | '1u' to enable FPGA configuration by FX <br> '0u' to disable FPGA configuration using FX device


## FPGA configuration

### FPGA configuration using the Efinix tool

Ti180 FPGA can be configured using the Efinity tool. For more details, see the [J484 kit user guide](https://www.efinixinc.com/products-devkits-titaniumti180j484.html). 

On every bootup, EZ-USB&trade; FX deasserts the INT_RESET pin.

**Table 5. GPIO for checking FPGA configuration on KIT_FX20_FMC_001 DVK**

EZ-USB&trade; FX20 pin          | Function            | Description
:-------------                  | :------------       | :--------------
P4_3                            | INT_RESET#          | Active low signal. EZ-USB&trade; FX device asserts to reset the FPGA
P4_4                            | CDONE#              | Active high signal. FPGA asserts when FPGA configuration is complete


### FPGA configuration using the EZ-USB&trade; FX device

The code example supports FPGA configuration. To enable FPGA configuration using EZ-USB&trade; FX, set `FPGA_CONFIG_EN` in **Makefile** to '1u'. 

The FPGA binary in the *BitFile* folder of the project can be programmed to the external flash on the EZ-USB&trade; FX20 DVK using the EZ-USB&trade; FX20 USB Bootloader. Perform the following steps to program the binary file to the external flash:

> **Note:** The following steps are assuming the device is pre-programmed with the EZ-USB&trade; FX20 USB Bootloader.

1. Open the EZ-USB&trade; FX Control Center application
2. Navigate to **Program** > **External Flash** and browse the FPGA binary file
3. Check the programming status

FPGA configuration can be performed using the SMIF block of FX. The SMIF (in x4 or Quad mode) interface is used for downloading the FPGA configuration binary on every bootup. FPGA binary files support passive serial x4 configuration mode.

Steps to configure FPGA (in passive serial x4 mode):

1. EZ-USB&trade; FX deasserts the INT_RESET pin
2. EZ-USB&trade; FX starts sending dummy SMIF (in x4 or Quad mode) clock to read the FPGA bitfile from the SPI flash
3. FPGA listens to the data on the SMIF lines and configures itself
4. FPGA asserts CDONE# when the configuration is complete

**Table 6. GPIOs for configuring FPGA on KIT_FX20_FMC_001 DVK**

EZ-USB&trade; FX20 pin | Function      | Description
:-------------         | :------------ | :--------------
P4_4                   | CDONE#        | Active high signal; FPGA asserts when FPGA configuration is completed
P4_3                   | INT_RESET#    | Active low signal; EZ-USB&trade; FX asserts to reset the FPGA
P6_4                   | PROG#         | Active low FPGA program signal


### FPGA bitfile information

**Table 7. Bitfile description**
Bitfile                                                   | Description                   | Supported features
:-------------                                            | :------------                 | :------------  
*fxn_ti180_dvk_multi_cam_ref_des_lvcmos_ddr_nl_p1.hex*    | LVCMOS Port 1 Narrow Link DDR | Port 1 Single Thread, FPGA added header
*fxn_ti180_dvk_multi_cam_ref_des_lvcmos_sdr_nl_p1.hex*    | LVCMOS Port 1 Narrow Link SDR | Port 1 Single Thread
*fxn_ti180_dvk_multi_cam_ref_des_lvcmos_ddr_nl_p0.hex*    | LVCMOS Port 0 Narrow Link DDR | Port 0 Single Thread, PORT 0 Thread Interleave, FPGA added header
*fxn_ti180_dvk_multi_cam_ref_des_lvcmos_ddr_wl.hex*       | LVCMOS WideLink DDR           | Port 0 Single Thread, FPGA added header
*fxn_ti180_dvk_multi_cam_ref_des_lvcmos_sdr_wl*           | LVCMOS WideLink SDR           | Port 0 Single Thread, PORT 0 Thread Interleave, FPGA added header
*fxn_ti180_dvk_multi_cam_ref_des_lvds_wl_148m_inmd*       | LVDS WideLink with INMD       | Port 0 Single Thread, INMD added header
*fxn_ti180_dvk_multi_cam_ref_des_lvds_nl_p0_148m*         | LVDS Port 0 Narrow Link       | Port 0 Single Thread, FPGA added header



## Application files

**Table 8. Application file description**

File                                              |    Description
:-------------                                    | :------------
*gpif_header_lvcmos.h*                            | Generated header file for GPIF state configuration for LVCMOS interface
*gpif_header_lvds.h*                              | Generated header file for GPIF state configuration for LVDS interface
*cy_usb_app.c*                                    | C source file implementing UVC 1.1 application logic
*cy_usb_app.h*                                    | Header file for application data structures and functions declaration
*cy_usb_uvc_device.h*                             | Header file with UVC application constants and the video frame configurations
*cy_usb_descriptors.c*                            | C source file containing the USB descriptors
*main.c*                                          | Source file for device initialization, ISRs, and LVCMOS/LVDS interface initialization, etc.
*cy_uac_app.c*                                    | C source file with UAC interface handlers
*cy_usb_i2c.c*                                    | C source file with I2C handlers
*cy_usb_i2c.h*                                    | Header file with I2C application constants and the function definitions
*cy_usb_qspi.c*                                   | C source file with SMIF handlers and FPGA configuration functions
*cy_usb_qspi.h*                                   | Header file with SMIF application constants and the function definitions
*cy_video_inmem.c*                                | C source file which implements the logic for streaming video from memory
*cm0_code.c*                                      | CM0 initialization code
*Makefile*                                        | GNU make compliant build script for compiling this example

<br>


## Related resources

Resources  | Links
-----------|----------------------------------
Application notes  | [AN237841](https://www.infineon.com/dgdl/Infineon-Getting_started_with_EZ_USB_FX20_FX10_FX5N_FX5-ApplicationNotes-v01_00-EN.pdf?fileId=8ac78c8c956a0a470195a515c54916e1) – Getting started with EZ-USB&trade; FX20/FX10/FX5N/FX5
Code examples  | [Using ModusToolbox&trade;](https://github.com/Infineon/Code-Examples-for-ModusToolbox-Software) on GitHub
Device documentation | [EZ-USB&trade; FX20 datasheets](https://www.infineon.com/fx20)
Development kits | Select your kits from the [Evaluation board finder](https://www.infineon.com/cms/en/design-support/finder-selection-tools/product-finder/evaluation-board)
Libraries on GitHub  | [mtb-pdl-cat1](https://github.com/Infineon/mtb-pdl-cat1) – Peripheral Driver Library (PDL)
Middleware on GitHub  | [usbfxstack](https://github.com/Infineon/usbfxstack) – USBFX Stack middleware library and documents
Tools  | [ModusToolbox&trade;](https://www.infineon.com/modustoolbox) – ModusToolbox&trade; software is a collection of easy-to-use libraries and tools enabling rapid development with Infineon MCUs for applications ranging from wireless and cloud-connected systems, edge AI/ML, embedded sense and control, to wired USB connectivity using PSOC&trade; Industrial/IoT MCUs, AIROC&trade; Wi-Fi and Bluetooth&reg; connectivity devices, XMC&trade; Industrial MCUs, and EZ-USB&trade;/EZ-PD&trade; wired connectivity controllers. ModusToolbox&trade; incorporates a comprehensive set of BSPs, HAL, libraries, configuration tools, and provides support for industry-standard IDEs to fast-track your embedded application development

<br>


## Other resources

Infineon provides a wealth of data at [www.infineon.com](https://www.infineon.com) to help you select the right device, and quickly and effectively integrate it into your design.


## Document history

Document title: *CE240865* – *EZ-USB&trade; FX20: USB Video Class (UVC) application*

 Version | Description of change
 ------- | ---------------------
 1.0.0   | New code example
 1.0.1   | Updated to use USBFXStack v1.1 from GitHub
 1.0.2   | Updated to use USBFXStack v1.2 from GitHub
 1.0.3   | Updated to use USBFXStack v1.3 from GitHub <br> Added capability to stream data from RAM buffers <br> Enabled porting of application to other EZ-USB&trade; FX devices

<br>


All referenced product or service names and trademarks are the property of their respective owners.

The Bluetooth&reg; word mark and logos are registered trademarks owned by Bluetooth SIG, Inc., and any use of such marks by Infineon is under license.

PSOC&trade;, formerly known as PSoC&trade;, is a trademark of Infineon Technologies. Any references to PSoC&trade; in this document or others shall be deemed to refer to PSOC&trade;.

---------------------------------------------------------

© Cypress Semiconductor Corporation, 2024-2025. This document is the property of Cypress Semiconductor Corporation, an Infineon Technologies company, and its affiliates ("Cypress").  This document, including any software or firmware included or referenced in this document ("Software"), is owned by Cypress under the intellectual property laws and treaties of the United States and other countries worldwide.  Cypress reserves all rights under such laws and treaties and does not, except as specifically stated in this paragraph, grant any license under its patents, copyrights, trademarks, or other intellectual property rights.  If the Software is not accompanied by a license agreement and you do not otherwise have a written agreement with Cypress governing the use of the Software, then Cypress hereby grants you a personal, non-exclusive, nontransferable license (without the right to sublicense) (1) under its copyright rights in the Software (a) for Software provided in source code form, to modify and reproduce the Software solely for use with Cypress hardware products, only internally within your organization, and (b) to distribute the Software in binary code form externally to end users (either directly or indirectly through resellers and distributors), solely for use on Cypress hardware product units, and (2) under those claims of Cypress's patents that are infringed by the Software (as provided by Cypress, unmodified) to make, use, distribute, and import the Software solely for use with Cypress hardware products.  Any other use, reproduction, modification, translation, or compilation of the Software is prohibited.
<br>
TO THE EXTENT PERMITTED BY APPLICABLE LAW, CYPRESS MAKES NO WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, WITH REGARD TO THIS DOCUMENT OR ANY SOFTWARE OR ACCOMPANYING HARDWARE, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.  No computing device can be absolutely secure.  Therefore, despite security measures implemented in Cypress hardware or software products, Cypress shall have no liability arising out of any security breach, such as unauthorized access to or use of a Cypress product. CYPRESS DOES NOT REPRESENT, WARRANT, OR GUARANTEE THAT CYPRESS PRODUCTS, OR SYSTEMS CREATED USING CYPRESS PRODUCTS, WILL BE FREE FROM CORRUPTION, ATTACK, VIRUSES, INTERFERENCE, HACKING, DATA LOSS OR THEFT, OR OTHER SECURITY INTRUSION (collectively, "Security Breach").  Cypress disclaims any liability relating to any Security Breach, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any Security Breach.  In addition, the products described in these materials may contain design defects or errors known as errata which may cause the product to deviate from published specifications. To the extent permitted by applicable law, Cypress reserves the right to make changes to this document without further notice. Cypress does not assume any liability arising out of the application or use of any product or circuit described in this document. Any information provided in this document, including any sample design information or programming code, is provided only for reference purposes.  It is the responsibility of the user of this document to properly design, program, and test the functionality and safety of any application made of this information and any resulting product.  "High-Risk Device" means any device or system whose failure could cause personal injury, death, or property damage.  Examples of High-Risk Devices are weapons, nuclear installations, surgical implants, and other medical devices.  "Critical Component" means any component of a High-Risk Device whose failure to perform can be reasonably expected to cause, directly or indirectly, the failure of the High-Risk Device, or to affect its safety or effectiveness.  Cypress is not liable, in whole or in part, and you shall and hereby do release Cypress from any claim, damage, or other liability arising from any use of a Cypress product as a Critical Component in a High-Risk Device. You shall indemnify and hold Cypress, including its affiliates, and its directors, officers, employees, agents, distributors, and assigns harmless from and against all claims, costs, damages, and expenses, arising out of any claim, including claims for product liability, personal injury or death, or property damage arising from any use of a Cypress product as a Critical Component in a High-Risk Device. Cypress products are not intended or authorized for use as a Critical Component in any High-Risk Device except to the limited extent that (i) Cypress's published data sheet for the product explicitly states Cypress has qualified the product for use in a specific High-Risk Device, or (ii) Cypress has given you advance written authorization to use the product as a Critical Component in the specific High-Risk Device and you have signed a separate indemnification agreement.
<br>
Cypress, the Cypress logo, and combinations thereof, ModusToolbox, PSoC, CAPSENSE, EZ-USB, F-RAM, and TRAVEO are trademarks or registered trademarks of Cypress or a subsidiary of Cypress in the United States or in other countries. For a more complete list of Cypress trademarks, visit www.infineon.com. Other names and brands may be claimed as property of their respective owners.
