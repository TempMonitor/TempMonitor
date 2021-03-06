/*--------------------------------------------------------------------------
Copyright (C) 2016  Temp monitor UF

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY W   ARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License along
with this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.

The base usb framework for this software is based of the V-USB example
project hid-data written by Christian Starkjohann and licensed with GNU GPL v2.
--------------------------------------------------------------------------*/

#include "main.h"


/* USB interface */

#define HIDSERIAL_INBUFFER_SIZE 32

PROGMEM const char usbHidReportDescriptor[USB_CFG_HID_REPORT_DESCRIPTOR_LENGTH] = {    /* USB report descriptor */
    0x06, 0x00, 0xff,              // USAGE_PAGE (Generic Desktop)
    0x09, 0x01,                    // USAGE (Vendor Usage 1)
    0xa1, 0x01,                    // COLLECTION (Application)
    0x15, 0x00,                    //   LOGICAL_MINIMUM (0)
    0x26, 0xff, 0x00,              //   LOGICAL_MAXIMUM (255)
    0x75, 0x08,                    //   REPORT_SIZE (8)
    0x95, 0x08,                    //   REPORT_COUNT (8)
    0x09, 0x00,                    //   USAGE (Undefined)
    0x82, 0x02, 0x01,              //   INPUT (Data,Var,Abs,Buf)
    0x95, HIDSERIAL_INBUFFER_SIZE, //   REPORT_COUNT (32)
    0x09, 0x00,                    //   USAGE (Undefined)
    0xb2, 0x02, 0x01,              //   FEATURE (Data,Var,Abs,Buf)
    0xc0                           // END_COLLECTION
};

static uchar received = 0;
static uchar inBuffer[HIDSERIAL_INBUFFER_SIZE];
static uchar reportId = 0;
static uchar bytesRemaining;
static uchar* pos;

static uchar    bytesRemaining;

/* Called when host requests data */
uchar   usbFunctionRead(uchar *data, uchar len)
{
    return 0;
}

/* Called when host sends data */
uchar   usbFunctionWrite(uchar *data, uchar len)
{
    if (reportId == 0) {
        int i;
        if(len > bytesRemaining)
            len = bytesRemaining;
        bytesRemaining -= len;
        for(i=0;i<len;i++) {
            if (data[i]!=0) {
                *pos++ = data[i];
             }
        }
        if (bytesRemaining == 0) {
            received = 1;
            *pos++ = 0;
            return 1;
        } else {
            return 0;
        }
    } else {
        return 1;
    }
}

usbMsgLen_t usbFunctionSetup(uchar data[8])
{
    usbRequest_t    *rq = (usbRequest_t *)data;
    reportId = rq->wValue.bytes[0];
    if((rq->bmRequestType & USBRQ_TYPE_MASK) == USBRQ_TYPE_CLASS){    /* HID class request */
        if(rq->bRequest == USBRQ_HID_GET_REPORT){
          /* wValue: ReportType (highbyte), ReportID (lowbyte) */
            /* since we have only one report type, we can ignore the report-ID */
            return USB_NO_MSG;  /* use usbFunctionRead() to obtain data */
        }else if(rq->bRequest == USBRQ_HID_SET_REPORT){
            /* since we have only one report type, we can ignore the report-ID */
            pos = inBuffer;
            bytesRemaining = rq->wLength.word;
            if(bytesRemaining > sizeof(inBuffer))
                bytesRemaining = sizeof(inBuffer);
            return USB_NO_MSG;  /* use usbFunctionWrite() to receive data from host */
        }
    }else{
        /* ignore vendor type requests, we don't use any */
    }
    return 0;
}

void USBDisconnect()
{
    uchar   i;
    usbDeviceDisconnect();  /* enforce re-enumeration, do this while interrupts are disabled! */
    i = 0;
    while(--i){             /* fake USB disconnect for > 250 ms */
        wdt_reset();
        _delay_ms(1);
    }
}

#define abs(x) ((x) > 0 ? (x) : (-x))

// Called by V-USB after device reset
void hadUsbReset() {
    int frameLength, targetLength = (unsigned)(1499 * (double)F_CPU / 10.5e6 + 0.5);
    int bestDeviation = 9999;
    uchar trialCal, bestCal, step, region;

    // do a binary search in regions 0-127 and 128-255 to get optimum OSCCAL
    for(region = 0; region <= 1; region++) {
        frameLength = 0;
        trialCal = (region == 0) ? 0 : 128;

        for(step = 64; step > 0; step >>= 1) {
            if(frameLength < targetLength) // true for initial iteration
                trialCal += step; // frequency too low
            else
                trialCal -= step; // frequency too high

            OSCCAL = trialCal;
            frameLength = usbMeasureFrameLength();

            if(abs(frameLength-targetLength) < bestDeviation) {
                bestCal = trialCal; // new optimum found
                bestDeviation = abs(frameLength -targetLength);
            }
        }
    }

    OSCCAL = bestCal;
}



int main(void)
{
    //wdt_enable(WDTO_1S);
    wdt_disable();

    //Init IO
	SHIFT_DDR |= 1 << CLOCK_PIN | 1 << DATA_PIN | 1 << LATCH_PIN;

    usbInit();

    USBDisconnect();

    usbDeviceConnect();
    sei();

    DisplayInit();

    DisplayWrite(0,123);
    DisplayWrite(1,456);
    DisplayWrite(2,789);
    DisplayWrite(3,012);
    DisplayBrightness(250);

	received = 0;

	int connectionTimeout = 30000;

    for(;;){
        wdt_reset();

        if(received != 0)
        {
			//Reset the connection timeout counting
			connectionTimeout = 30000;

			//Convert 255 into zeros (Computer can't send zeros)
			for(uchar x = 0; x != 8; x++)
			{
				if(inBuffer[x] == 255)
					inBuffer[x] = 0;
			}

			DisplayBrightness(inBuffer[1]);
            DisplayWrite(0, inBuffer[2]);
            DisplayWrite(1, inBuffer[3]);
            DisplayWrite(2, inBuffer[4]);
            DisplayWrite(3, inBuffer[5]);

            received = 0;
        }

        //Turn of display on connection timeout
        if(connectionTimeout == 0)
        {
            DisplayBrightness(0);
        }
        else
        {
            connectionTimeout--;
        }
        _delay_us(50);
        usbPoll();
    }
    return 0;
}
