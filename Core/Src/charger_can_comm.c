/*
 * can_comm.c
 *
 *  Created on: May 24, 2024
 *      Author: Brij Zaveri
 */
/* this part of code will the send data to charger via CAN which are collected from battery via RS485*/
/* Battery(rs485) >> MCU >> Charger(CAN)*/

#include <charger_can_comm.h>
#include "main.h"
#include <stdbool.h> // Include standard boolean type definitions

// Define constants for maximum output voltages
#define MAX_VOLTAGE_23S 8400
#define MAX_VOLTAGE_19S 6935
#define MAX_VOLTAGE_16S 5840
#define MAX_VOLTAGE_15S 5475

// Function prototypes
void setLEDIndicationBasedOnSOC(uint8_t SoC);
bool hasChargingExceededMaxTime();
void indicateCANCommunicationError();
void configureMaxOutputVoltage();
uint8_t getBatteryType(); // Add this function prototype

void transmitDataOverCAN(void)
{
    int totalCellVolt = (bmsResponse_1[25] * 3.65) * 100;
    uint16_t scaled_Volt_Value = totalCellVolt / 10;
    uint8_t scaled_Volt_HighByte = (scaled_Volt_Value >> 8) & 0xFF;
    uint8_t scaled_Volt_LowByte = scaled_Volt_Value & 0xFF;
    uint32_t msgId = 0x200;

    uint8_t broadcastAdd = 0xFF;       // byte 0
    uint8_t relayPowerON = 3;          // for bit 1 in byte 1
    uint8_t min_current = 0x10;        // 5 amps
    uint8_t max_current = 0x20;        // 10 amps
    uint8_t derated_current = 0x04;    // 4 amps

    TxHeader.Identifier = msgId;
    TxHeader.IdType = FDCAN_STANDARD_ID;
    TxHeader.TxFrameType = FDCAN_DATA_FRAME;
    TxHeader.DataLength = FDCAN_DLC_BYTES_8;
    TxHeader.ErrorStateIndicator = FDCAN_ESI_ACTIVE;
    TxHeader.BitRateSwitch = FDCAN_BRS_OFF;
    TxHeader.FDFormat = FDCAN_CLASSIC_CAN;
    TxHeader.TxEventFifoControl = FDCAN_NO_TX_EVENTS;
    TxHeader.MessageMarker = 0;

    bool CellVoltLessThan3000 = false;
    for (int i = 0; i < 23; i++)
    {
        if (Cell_String[i] <= 3000)
        {
            CellVoltLessThan3000 = true;
            break;
        }
    }

    if (CellVoltLessThan3000)
    {
        TxData[0] = broadcastAdd;
        TxData[1] = relayPowerON;
        TxData[2] = scaled_Volt_HighByte;
        TxData[3] = scaled_Volt_LowByte;
        TxData[4] = 0x00;
        TxData[5] = 0x02;  // 2 amps
        canConnected = 1;  // Indicate CAN connection is established
        printf("Charging with 2.0A as one of the cell voltage is less than 3000 mV\r\n");
    }
    else
    {
        TxData[0] = broadcastAdd;
        TxData[1] = relayPowerON;
        TxData[2] = scaled_Volt_HighByte;
        TxData[3] = scaled_Volt_LowByte;
        TxData[4] = 0x00;
        TxData[5] = 0x0A;  // 10 amps
        canConnected = 1;  // Indicate CAN connection is established
        printf("Charging with 10.0A \r\n");
    }

    if (!canConnected) {
        TxData[0] = broadcastAdd;
        TxData[1] = relayPowerON;
        TxData[2] = 0x02;  // 02DF = 735 Volt
        TxData[3] = 0xDF;  // 02DF = 735 Volt
        TxData[4] = 0x00;
        TxData[5] = 0x05;  // 5 amps
        printf("Charging with 5.0A without CAN communication\r\n");
    }

    if(voltage >= 7305){
        TxData[0] = broadcastAdd;
        TxData[1] = 0x00;
        TxData[2] = 0x00;
        TxData[3] = 0x00;
        TxData[4] = 0x00;
        TxData[5] = 0x00;  // Stop charging
        printf("Stopped charging as voltage is above 73.5V without CAN\r\n");
    }

    if (SoC > 90)  // start CV mode
    {
        TxData[0] = broadcastAdd;
        TxData[1] = relayPowerON;
        TxData[2] = scaled_Volt_HighByte;
        TxData[3] = scaled_Volt_LowByte;
        TxData[4] = 0x00;
        TxData[5] = min_current;  // 5 amps
        printf("CV mode started, SoC = %d\r\n", SoC);
    }
    else
    {
        TxData[0] = broadcastAdd;
        TxData[1] = relayPowerON;
        TxData[2] = scaled_Volt_HighByte;
        TxData[3] = scaled_Volt_LowByte;
        TxData[4] = 0x00;
        TxData[5] = max_current;  // 10 amps
        printf("Charging with 10 amps, SoC = %d\r\n", SoC);
    }

    bool cellTempGreaterThan500 = false;
    bool cellTempGreaterThan600 = false;
    for (int c = 0; c < 5; c++)
    {
        if (Cell_temp[c] >= 600)
        {
            cellTempGreaterThan600 = true;
            break;
        }
        if (Cell_temp[c] >= 500)
        {
            cellTempGreaterThan500 = true;
        }
    }
    if (cellTempGreaterThan600)
    {
        TxData[0] = broadcastAdd;
        TxData[1] = 0x00;
        TxData[2] = 0x00;
        TxData[3] = 0x00;
        TxData[4] = 0x00;
        TxData[5] = 0x00;  // Stop charging
        printf("Stopped charging as cell temperature is above 60°C\r\n");
    }
    else if(cellTempGreaterThan500)
    {
        TxData[0] = broadcastAdd;
        TxData[1] = relayPowerON;
        TxData[2] = scaled_Volt_HighByte;
        TxData[3] = scaled_Volt_LowByte;
        TxData[4] = 0x00;
        TxData[5] = derated_current;  // 4 amps
        printf("Temperature exceeds limit, charging with 4 amps\r\n");
    }

    // Implement LED indication based on SOC
    setLEDIndicationBasedOnSOC(SoC);

    // Implement charging time cut-off
    if (hasChargingExceededMaxTime())
    {
        TxData[0] = broadcastAdd;
        TxData[1] = 0x00;
        TxData[2] = 0x00;
        TxData[3] = 0x00;
        TxData[4] = 0x00;
        TxData[5] = 0x00;  // Stop charging
        printf("Stopped charging as it exceeded 7 hours\r\n");
    }

    // Implement LED fault indications
    if (!canConnected)
    {
        indicateCANCommunicationError();
    }
    // Other fault indications can be added here

    // Configure maximum output voltage based on battery type
    configureMaxOutputVoltage();

    // Transmit CAN frame
    if (HAL_FDCAN_AddMessageToTxFifoQ(&hfdcan1, &TxHeader, TxData) == HAL_OK) {
        printf("Data sent over CAN successfully.\r\n");
        canConnected = 1;
    } else {
        printf("CAN transmission error.\r\n");
        canConnected = 0;  // Indicate CAN connection error
    }
    printf("\n");
}

void setLEDIndicationBasedOnSOC(uint8_t SoC)
{
    // Implementation of LED indication logic based on SOC
    // Example:
    // if (SoC < 25) {
    //     turnOnLED(LED_25);
    // } else if (SoC < 50) {
    //     turnOnLED(LED_50);
    // } else if (SoC < 75) {
    //     turnOnLED(LED_75);
    // } else {
    //     turnOnLED(LED_100);
    // }
}

bool hasChargingExceededMaxTime()
{
    // Implementation to check if charging has exceeded 7 hours
    // Example:
    // return (current_time - start_time) >= 7 * 3600;
    return false; // Placeholder return statement
}

void indicateCANCommunicationError()
{
    // Implementation of CAN communication error indication
    // Example:
    // blinkLED(LED_75, 1);
    // blinkLED(LED_100, 1);
}

void configureMaxOutputVoltage()
{
    // Configure maximum output voltage based on battery type
    uint16_t maxOutputVoltage = MAX_VOLTAGE_23S; // Default to 23S battery
    uint8_t batteryType = getBatteryType(); // Function to get battery type

    switch (batteryType)
    {
        case 19:
            maxOutputVoltage = MAX_VOLTAGE_19S;
            break;
        case 16:
            maxOutputVoltage = MAX_VOLTAGE_16S;
            break;
        case 15:
            maxOutputVoltage = MAX_VOLTAGE_15S;
            break;
        default:
            maxOutputVoltage = MAX_VOLTAGE_23S;
            break;
    }

    // Ensure voltage does not exceed maximum for battery type
    if (voltage > maxOutputVoltage)
    {
        uint8_t broadcastAdd = 0xFF;  // Ensure broadcastAdd is declared here
        TxData[0] = broadcastAdd;
        TxData[1] = 0x00;
        TxData[2] = 0x00;
        TxData[3] = 0x00;
        TxData[4] = 0x00;
        TxData[5] = 0x00;  // Stop charging
        printf("Stopped charging as voltage exceeds max output voltage for battery type\r\n");
    }
}

uint8_t getBatteryType()
{
    // Placeholder function to get the battery type
    // Replace this with actual logic to determine the battery type
    return 23; // Default to 23S battery
}
