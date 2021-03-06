/*-----------------------------------------*\
|  CorsairPeripheralController.cpp          |
|                                           |
|  Driver for Corsair RGB keyboard, mouse,  |
|  and mousemat lighting controller         |
|                                           |
|  Adam Honse (CalcProgrammer1) 1/9/2020    |
\*-----------------------------------------*/

#include "CorsairPeripheralController.h"

#include <cstring>

using namespace std::chrono_literals;

static unsigned int keys[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0C, 0x0D, 0x0E, 0x0F, 0x11, 0x12,
                              0x14, 0x15, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F, 0x20, 0x21, 0x24, 0x25, 0x26,
                              0x27, 0x28, 0x2A, 0x2B, 0x2C, 0x30, 0x31, 0x32, 0x33, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
                              0x3C, 0x3D, 0x3E, 0x3F, 0x40, 0x42, 0x43, 0x44, 0x45, 0x48, 73,   74,   75,   76,   78,
                              79,   80,   81,   84,   85,   86,   87,   88,   89,   90,   91,   92,   93,   96,   97,
                              98,   99,   100,  101,  102,  103,  104,  105,  108,  109,  110,  111,  112,  113,  115,
                              116,  117,  120,  121,  122,  123,  124,  126,  127,  128,  129,  132,  133,  134,  135,
                              136,  137,  139,  140,  141};

static unsigned int st100[] = { 0x00, 0x01, 0x02, 0x03, 0x05, 0x06, 0x07, 0x08, 0x04 };

static void send_usb_msg(hid_device* dev, char * data_pkt)
{
    char usb_pkt[65];
    usb_pkt[0] = 0x00;
    for(int i = 1; i < 65; i++)
    {
        usb_pkt[i] = data_pkt[i-1];
    }
    int bytes = hid_send_feature_report(dev, (unsigned char *)usb_pkt, 65);
    bytes++;

    std::this_thread::sleep_for(1ms);
}

CorsairPeripheralController::CorsairPeripheralController(hid_device* dev_handle)
{
    dev = dev_handle;

    ReadFirmwareInfo();

    SpecialFunctionControl();
    LightingControl();
}

CorsairPeripheralController::~CorsairPeripheralController()
{

}

device_type CorsairPeripheralController::GetDeviceType()
{
    return type;
}

std::string CorsairPeripheralController::GetFirmwareString()
{
    return firmware_version;
}

void CorsairPeripheralController::SetLEDs(std::vector<RGBColor>colors)
{
    switch(type)
    {
        case DEVICE_TYPE_KEYBOARD:
            SetLEDsKeyboardFull(colors);
            break;

        case DEVICE_TYPE_MOUSE:
            SetLEDsMouse(colors);
            break;

        case DEVICE_TYPE_MOUSEMAT:
            SetLEDsMousemat(colors);
            break;

        case DEVICE_TYPE_HEADSET_STAND:
            /*-----------------------------------------------------*\
            | The logo zone of the ST100 is in the middle of the    |
            | base LED strip, so remap the colors so that the logo  |
            | is the last LED in the sequence.                      |
            \*-----------------------------------------------------*/
            std::vector<RGBColor> remap_colors;
            remap_colors.resize(colors.size());

            for(int i = 0; i < 9; i++)
            {
                remap_colors[st100[i]] = colors[i];
            }

            /*-----------------------------------------------------*\
            | The ST100 uses the mousemat protocol                  |
            \*-----------------------------------------------------*/
            SetLEDsMousemat(remap_colors);
            break;
    }
}

void CorsairPeripheralController::SetLEDsKeyboardFull(std::vector<RGBColor> colors)
{
    unsigned char red_val[144];
    unsigned char grn_val[144];
    unsigned char blu_val[144];

    /*-----------------------------------------------------*\
    | Zero out buffers                                      |
    \*-----------------------------------------------------*/
    memset(red_val, 0x00, sizeof( red_val ));
    memset(grn_val, 0x00, sizeof( grn_val ));
    memset(blu_val, 0x00, sizeof( blu_val ));

    /*-----------------------------------------------------*\
    | Copy red, green, and blue components into buffers     |
    \*-----------------------------------------------------*/
    for(std::size_t color_idx = 0; color_idx < colors.size(); color_idx++)
    {
        RGBColor           color = colors[color_idx];
        red_val[keys[color_idx]] = RGBGetRValue(color);
        grn_val[keys[color_idx]] = RGBGetGValue(color);
        blu_val[keys[color_idx]] = RGBGetBValue(color);
    }

    /*-----------------------------------------------------*\
    | Send red bytes                                        |
    \*-----------------------------------------------------*/
    StreamPacket(1, 60, &red_val[0]);
    StreamPacket(2, 60, &red_val[60]);
    StreamPacket(3, 24, &red_val[120]);
    SubmitKeyboardFullColors(1, 3, 1);

    /*-----------------------------------------------------*\
    | Send green bytes                                      |
    \*-----------------------------------------------------*/
    StreamPacket(1, 60, &grn_val[0]);
    StreamPacket(2, 60, &grn_val[60]);
    StreamPacket(3, 24, &grn_val[120]);
    SubmitKeyboardFullColors(2, 3, 1);

    /*-----------------------------------------------------*\
    | Send blue bytes                                       |
    \*-----------------------------------------------------*/
    StreamPacket(1, 60, &blu_val[0]);
    StreamPacket(2, 60, &blu_val[60]);
    StreamPacket(3, 24, &blu_val[120]);
    SubmitKeyboardFullColors(3, 3, 2);
}

void CorsairPeripheralController::SetLEDsMouse(std::vector<RGBColor> colors)
{
    SubmitMouseColors(colors.size(), &colors[0]);
}

void CorsairPeripheralController::SetLEDsMousemat(std::vector<RGBColor> colors)
{
    SubmitMousematColors(colors.size(), &colors[0]);
}

void CorsairPeripheralController::SetLEDsKeyboardLimited(std::vector<RGBColor> colors)
{
    unsigned char data_pkt[216];
    unsigned char red_val[144];
    unsigned char grn_val[144];
    unsigned char blu_val[144];

    /*-----------------------------------------------------*\
    | Zero out buffers                                      |
    \*-----------------------------------------------------*/
    memset(data_pkt, 0x00, sizeof( data_pkt ));
    memset(red_val, 0x00, sizeof( red_val ));
    memset(grn_val, 0x00, sizeof( grn_val ));
    memset(blu_val, 0x00, sizeof( blu_val ));

    /*-----------------------------------------------------*\
    | Scale color values to 9-bit                           |
    \*-----------------------------------------------------*/
    for(std::size_t color_idx = 0; color_idx < colors.size(); color_idx++)
    {
        RGBColor      color = colors[color_idx];
        unsigned char red   = RGBGetRValue(color);
        unsigned char grn   = RGBGetGValue(color);
        unsigned char blu   = RGBGetBValue(color);

        if( red > 7 ) red = 7;
        if( grn > 7 ) grn = 7;
        if( blu > 7 ) blu = 7;

        red = 7 - red;
        grn = 7 - grn;
        blu = 7 - blu;

        red_val[keys[color_idx]] = red;
        grn_val[keys[color_idx]] = grn;
        blu_val[keys[color_idx]] = blu;
    }

    /*-----------------------------------------------------*\
    | Pack the color values, 2 values per byte              |
    \*-----------------------------------------------------*/
    for(int red_idx = 0; red_idx < 72; red_idx++)
    {
        data_pkt[red_idx] = red_val[(red_idx * 2) + 1] << 4 | red_val[red_idx * 2];
    }

    for(int grn_idx = 0; grn_idx < 72; grn_idx++)
    {
        data_pkt[grn_idx + 72] = grn_val[(grn_idx * 2) + 1] << 4 | grn_val[grn_idx * 2];
    }

    for(int blu_idx = 0; blu_idx < 72; blu_idx++)
    {
        data_pkt[blu_idx + 144] = blu_val[(blu_idx * 2) + 1] << 4 | blu_val[blu_idx * 2];
    }

    /*-----------------------------------------------------*\
    | Send the packets                                      |
    \*-----------------------------------------------------*/
    StreamPacket(1, 60, &data_pkt[0]);
    StreamPacket(2, 60, &data_pkt[60]);
    StreamPacket(3, 60, &data_pkt[120]);
    StreamPacket(4, 36, &data_pkt[180]);
    
    SubmitKeyboardLimitedColors(216);
}

/*-------------------------------------------------------------------------------------------------*\
| Private packet sending functions.                                                                 |
\*-------------------------------------------------------------------------------------------------*/

void CorsairPeripheralController::LightingControl()
{
    char usb_buf[64];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up Lighting Control packet                        |
    \*-----------------------------------------------------*/
    usb_buf[0x00]           = CORSAIR_COMMAND_WRITE;
    usb_buf[0x01]           = CORSAIR_PROPERTY_LIGHTING_CONTROL;
    usb_buf[0x02]           = CORSAIR_LIGHTING_CONTROL_SOFTWARE;

    /*-----------------------------------------------------*\
    | Lighting control byte needs to be 3 for keyboards and |
    | headset stand, 1 for mice and mousepads               |
    \*-----------------------------------------------------*/
    switch(type)
    {
        default:
        case DEVICE_TYPE_KEYBOARD:
            usb_buf[0x04]   = 0x03;
            break;

        case DEVICE_TYPE_MOUSE:
            usb_buf[0x04]   = 0x01;
            break;

        case DEVICE_TYPE_MOUSEMAT:
            usb_buf[0x04]   = 0x04;
            break;

        case DEVICE_TYPE_HEADSET_STAND:
            usb_buf[0x04]   = 0x03;
            break;
    }
    
    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    send_usb_msg(dev, usb_buf);
}

void CorsairPeripheralController::SpecialFunctionControl()
{
    char usb_buf[64];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up Lighting Control packet                        |
    \*-----------------------------------------------------*/
    usb_buf[0x00]           = CORSAIR_COMMAND_WRITE;
    usb_buf[0x01]           = CORSAIR_PROPERTY_SPECIAL_FUNCTION;
    usb_buf[0x02]           = CORSAIR_LIGHTING_CONTROL_SOFTWARE;
    
    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    send_usb_msg(dev, usb_buf);
}

void CorsairPeripheralController::ReadFirmwareInfo()
{
    int  actual;
    char usb_buf[64];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up Read Firmware Info packet                      |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = CORSAIR_COMMAND_READ;
    usb_buf[0x01]   = CORSAIR_PROPERTY_FIRMWARE_INFO;

    /*-----------------------------------------------------*\
    | Send packet and try reading it using an HID read      |
    | If that fails, repeat the send and read the reply as  |
    | a feature report.                                     |
    \*-----------------------------------------------------*/
    hid_write(dev, (unsigned char*)usb_buf, 64);
    actual = hid_read_timeout(dev, (unsigned char*)usb_buf, 64, 1000);

    if(actual == 0)
    {
        /*-------------------------------------------------*\
        | Zero out buffer                                   |
        \*-------------------------------------------------*/
        memset(usb_buf, 0x00, sizeof(usb_buf));

        /*-------------------------------------------------*\
        | Set up Read Firmware Info packet                  |
        \*-------------------------------------------------*/
        usb_buf[0x00]   = CORSAIR_COMMAND_READ;
        usb_buf[0x01]   = CORSAIR_PROPERTY_FIRMWARE_INFO;

        hid_write(dev, (unsigned char*)usb_buf, 64);
        actual = hid_get_feature_report(dev, (unsigned char*)usb_buf, 64);
    }

    /*-----------------------------------------------------*\
    | Get device type                                       |
    |   0xC0    Device is a keyboard                        |
    |   0xC1    Device is a mouse                           |
    |   0xC2    Device is a mousepad or headset stand       |
    \*-----------------------------------------------------*/
    switch((unsigned char)usb_buf[0x14])
    {
        case 0xC0:
            type = DEVICE_TYPE_KEYBOARD;
            break;

        case 0xC1:
            type = DEVICE_TYPE_MOUSE;
            break;

        case 0xC2:
            {
                unsigned short pid = (unsigned short)(usb_buf[0x0F] << 8) + (unsigned char)(usb_buf[0x0E]);

                switch(pid)
                {
                    case 0x0A34:
                        type = DEVICE_TYPE_HEADSET_STAND;
                        break;
                    
                    default:
                        type = DEVICE_TYPE_MOUSEMAT;
                        break;
                }
            }
            break;

        default:
            type = DEVICE_TYPE_UNKNOWN;
            break;
    }

    /*-----------------------------------------------------*\
    | Format firmware version string if device type is valid|
    \*-----------------------------------------------------*/
    if(type != DEVICE_TYPE_UNKNOWN)
    {
        firmware_version = std::to_string(usb_buf[0x09]) + "." + std::to_string(usb_buf[0x08]);
    }
}

void CorsairPeripheralController::StreamPacket
    (
    unsigned char   packet_id,
    unsigned char   data_sz,
    unsigned char*  data_ptr
    )
{
    char usb_buf[64];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up Stream packet                                  |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = CORSAIR_COMMAND_STREAM;
    usb_buf[0x01]   = packet_id;
    usb_buf[0x02]   = data_sz;

    /*-----------------------------------------------------*\
    | Copy in data bytes                                    |
    \*-----------------------------------------------------*/
    memcpy(&usb_buf[0x04], data_ptr, data_sz);

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, (unsigned char *)usb_buf, 64);
}

void CorsairPeripheralController::SubmitKeyboardFullColors
    (
    unsigned char   color_channel,
    unsigned char   packet_count,
    unsigned char   finish_val
    )
{
    char usb_buf[64];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up Submit Keyboard 24-Bit Colors packet           |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = CORSAIR_COMMAND_WRITE;
    usb_buf[0x01]   = CORSAIR_PROPERTY_SUBMIT_KEYBOARD_COLOR_24;
    usb_buf[0x02]   = color_channel;
    usb_buf[0x03]   = packet_count;
    usb_buf[0x04]   = finish_val;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, (unsigned char *)usb_buf, 64);
}

void CorsairPeripheralController::SubmitKeyboardLimitedColors
    (
    unsigned char   byte_count
    )
{
    char usb_buf[64];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up Submit Keyboard 9-Bit Colors packet            |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = CORSAIR_COMMAND_WRITE;
    usb_buf[0x01]   = CORSAIR_PROPERTY_SUBMIT_KEYBOARD_COLOR_9;
    usb_buf[0x04]   = byte_count;

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, (unsigned char *)usb_buf, 64);
}

void CorsairPeripheralController::SubmitMouseColors
    (
    unsigned char   num_zones,
    RGBColor *      color_data
    )
{
    char usb_buf[64];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up Submit Mouse Colors packet                     |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = CORSAIR_COMMAND_WRITE;
    usb_buf[0x01]   = CORSAIR_PROPERTY_SUBMIT_MOUSE_COLOR;
    usb_buf[0x02]   = num_zones;
    usb_buf[0x03]   = 0x00;

    /*-----------------------------------------------------*\
    | Copy in colors in <ZONE> <RED> <GREEN> <BLUE> order   |
    \*-----------------------------------------------------*/
    for(unsigned int zone_idx = 0; zone_idx < num_zones; zone_idx++)
    {
        usb_buf[(zone_idx * 4) + 4] = zone_idx;
        usb_buf[(zone_idx * 4) + 5] = RGBGetRValue(color_data[zone_idx]);
        usb_buf[(zone_idx * 4) + 6] = RGBGetGValue(color_data[zone_idx]);
        usb_buf[(zone_idx * 4) + 7] = RGBGetBValue(color_data[zone_idx]);
    }

    /*-----------------------------------------------------*\
    | Send packet                                           |
    \*-----------------------------------------------------*/
    hid_write(dev, (unsigned char *)usb_buf, 64);
}

void CorsairPeripheralController::SubmitMousematColors
    (
    unsigned char   num_zones,
    RGBColor *      color_data
    )
{
    char usb_buf[64];

    /*-----------------------------------------------------*\
    | Zero out buffer                                       |
    \*-----------------------------------------------------*/
    memset(usb_buf, 0x00, sizeof(usb_buf));

    /*-----------------------------------------------------*\
    | Set up Submit Mouse Colors packet                     |
    \*-----------------------------------------------------*/
    usb_buf[0x00]   = CORSAIR_COMMAND_WRITE;
    usb_buf[0x01]   = CORSAIR_PROPERTY_SUBMIT_MOUSE_COLOR;
    usb_buf[0x02]   = num_zones;
    usb_buf[0x03]   = 0x00;

    /*-----------------------------------------------------*\
    | Copy in colors in <RED> <GREEN> <BLUE> order          |
    \*-----------------------------------------------------*/
    for(unsigned int zone_idx = 0; zone_idx < num_zones; zone_idx++)
    {
        usb_buf[(zone_idx * 3) + 4] = RGBGetRValue(color_data[zone_idx]);
        usb_buf[(zone_idx * 3) + 5] = RGBGetGValue(color_data[zone_idx]);
        usb_buf[(zone_idx * 3) + 6] = RGBGetBValue(color_data[zone_idx]);
    }

    /*-----------------------------------------------------*\
    | Send packet using feature reports, as headset stand   |
    | seems to not update completely using HID writes       |
    \*-----------------------------------------------------*/
    send_usb_msg(dev, usb_buf);
}
