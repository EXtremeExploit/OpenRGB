/*---------------------------------------------------------*\
|  Definitions for Generic LED Strip Interface              |
|                                                           |
|  Adam Honse (calcprogrammer1@gmail.com), 12/11/2016       |
\*---------------------------------------------------------*/

#ifndef LED_STRIP_H
#define LED_STRIP_H

#include "RGBController.h"
#include "serial_port.h"
#include "net_port.h"
#include <vector>


#ifndef TRUE
#define TRUE true
#define FALSE false
#endif

#ifndef WIN32
#define LPSTR           char *
#define strtok_s        strtok_r
#endif

class LEDStripController
{
public:
    LEDStripController();
    ~LEDStripController();

    void Initialize(char* ledstring);
    void InitializeSerial(char* portname, int baud);
    void InitializeUDP(char* clientname, char* port);
    char* GetLEDString();
    void SetLEDs(std::vector<RGBColor> colors);

    int num_leds;

private:
    int baud_rate;

    char led_string[1024];
    char port_name[128];
    char client_name[1024];
    serial_port *serialport;
    net_port *udpport;
};

#endif
