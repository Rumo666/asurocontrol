/***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   any later version.                                                    *
 ***************************************************************************/

#include "../shared/shared.h"
#include "asuro.h"

#define MODE_IDLE        0 /* Doing nothing */
#define MODE_SLEEP       1 /* Set when waiting no status update for a while */
#define MODE_DRIVE       2 /* Driving */

static struct
{
    unsigned long lastUpdate;
    int8_t motorPower[2];
    int8_t mode;
    int8_t LEDsEnabled;
} asuroInfo;

void setBackLEDs(void)
{
    BackLED((asuroInfo.LEDsEnabled & (1<<LED_BACK_LEFT)) ? ON : OFF,
            (asuroInfo.LEDsEnabled & (1<<LED_BACK_RIGHT)) ? ON : OFF);
}

// Own version of SetMotorPower: FREE instead of BREAK at 0 speed
void setMotorSpeed(int8_t left, int8_t right)
{
    int8_t ldir, rdir;
    
    if (left == 0)
        ldir = FREE;
    else if (left < 0)
    {
        ldir = RWD;
        left = -left;
    }
    else
        ldir = FWD;
    
    if (right == 0)
        rdir = FREE;
    else if (right < 0)
    {
        right = -right;
        rdir = RWD;
    }
    else
        rdir = FWD;
    
    MotorDir(ldir, rdir);
    MotorSpeed(left * 2, right * 2);
    
    if (ldir == RWD)
        asuroInfo.LEDsEnabled |= (1<<LED_BACK_LEFT);
    else
        asuroInfo.LEDsEnabled &= ~(1<<LED_BACK_LEFT);
    
    if (rdir == RWD)
        asuroInfo.LEDsEnabled |= (1<<LED_BACK_RIGHT);
    else
        asuroInfo.LEDsEnabled &= ~(1<<LED_BACK_RIGHT);
    
    setBackLEDs();
}

void setIdle(void)
{
    setMotorSpeed(0, 0);
    asuroInfo.mode = MODE_IDLE;
}

void setSleep(void)
{
    setMotorSpeed(0, 0);
    asuroInfo.mode = MODE_SLEEP;
}

void startDrive(int8_t left, int8_t right)
{
    setMotorSpeed(left, right);
    asuroInfo.mode = MODE_DRIVE;
}

void sendIRByte(unsigned char byte)
{
    /* 1: Start
    * 2: Stop
    * 4: One
    * 5: Zero
    * This is used to have a 'compatible' way of communicating with the e51
    */

    int8_t i;
    
    UartPutc('1');
    
    for (i=0; i<8; i++)
    {
        if (byte & (1<<i))
            UartPutc('4');
        else
            UartPutc('5');
    }
    
    UartPutc('2');
}

void sendSensors(void)
{
    // Switch
    sendIRByte('S'); // S == msg for switch
    sendIRByte(PollSwitch() & PollSwitch()); // Use pollswitch twice
    
    // All sensor data is converted from 0..1023 to 0..255 to lessen IR IO
    // long: conversion may give int overflows
    const unsigned long adcmax = 1023, sendmax = 255;
    unsigned int adc[2];
    
    // Line sensors
    LineData(adc);
    sendIRByte('L'); // Line
    sendIRByte(adc[0]*sendmax/adcmax);
    sendIRByte(adc[1]*sendmax/adcmax);
    
    // Odo
    OdometryData(adc);
    sendIRByte('O'); // Odo
    sendIRByte(adc[0]*sendmax/adcmax);
    sendIRByte(adc[1]*sendmax/adcmax);
    setBackLEDs(); // Restore backleds (odo read switches them off)
    
    // Battery
    sendIRByte('B'); // Battery
    sendIRByte(Battery()*sendmax/adcmax);
}

void parseIR(char cmd, char val)
{
    if (cmd == CMD_UPDATE)
    {
        StatusLED(GREEN);
        asuroInfo.lastUpdate = Gettime();
        sendSensors();
//         SerPrint("1234567890");
        StatusLED(RED);
        
        if (asuroInfo.mode == MODE_SLEEP)
            startDrive(asuroInfo.motorPower[0], asuroInfo.motorPower[1]);
    }
    else if (cmd == CMD_SPEEDL)
    {
        asuroInfo.motorPower[0] = (int8_t)val;
        startDrive(asuroInfo.motorPower[0], asuroInfo.motorPower[1]);
    }
    else if (cmd == CMD_SPEEDR)
    {
        asuroInfo.motorPower[1] = (int8_t)val;
        startDrive(asuroInfo.motorPower[0], asuroInfo.motorPower[1]);
    }
    else if (cmd == CMD_TOGGLELED)
    {
        if (val == LED_FRONT)
        {
            asuroInfo.LEDsEnabled ^= (1<<LED_FRONT);
            FrontLED(asuroInfo.LEDsEnabled & LED_FRONT);
        }        
    }
}

void readIR(void)
{
    if (PIND & (1<<PD0))
        return;
    
    int8_t i;
    unsigned char datastr[13], irstat, cmd = 0, data = 0;
    unsigned long time;
    
    /* Read manchester data:
     * First 5 bits: cmd type
     * Remaining 8 bits: value
     */
   
    for (i=0; i<13; i++)
    {
        // Longer timing than regular RC5: Symbian doesn't seem to provide enough accuracy
        Msleep(7);
        
        irstat = (PIND & (1<<PD0));
        if (irstat)
        {
            datastr[i] = '1';
            if (i < 5)
                cmd |= (1<<(4-i));
            else
                data |= (1<<(7-(i-5)));
        }
        else
            datastr[i] = '0';
        
        time = Gettime();
        while (irstat == (PIND & (1<<PD0)))
        {
            if ((Gettime()-time) > 30) // Timeout! --> abort
                return;
            Sleep(10);
        }
    }
    
//     SerPrint("Received IR data: ");
//     SerWrite(datastr, 13);
//     SerPrint("\ncmd: ");
//     PrintInt(cmd);
//     SerPrint("\ndata: ");
//     PrintInt(data);
//     UartPutc('\n');

    Msleep(25);
    
    parseIR(cmd, data);    
}

int main(void)
{   
    unsigned long timediff;
    
    Init();
 
    StatusLED(RED);
    BackLED(OFF, OFF);
    
    asuroInfo.lastUpdate = 0;
    asuroInfo.motorPower[0] = asuroInfo.motorPower[1] = 0;
    asuroInfo.mode = MODE_IDLE;
    asuroInfo.LEDsEnabled = 0;
    
    while (1)
    {
        readIR();
        
        switch (asuroInfo.mode)
        {
            case MODE_IDLE:
            case MODE_SLEEP:
                break;
            case MODE_DRIVE:
                timediff = (Gettime() - asuroInfo.lastUpdate);
                
                if (timediff > 40000) // No contact for over 40s
                    setIdle();
                else if (timediff > 5000)
                {
                    setSleep();
                    Msleep(500); // Wait a little to regain power
                }                
                break;
        }
    }
    
    return 0;
}
