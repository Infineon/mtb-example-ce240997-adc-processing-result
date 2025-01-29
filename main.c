/******************************************************************************
* File Name:   main.c
*
* Description: This is the source code for the PROT PPU ECC Injection Example
*              for ModusToolbox.
*
* Related Document: See README.md
*
*
*******************************************************************************
* Copyright 2024-2025, Cypress Semiconductor Corporation (an Infineon company) or
* an affiliate of Cypress Semiconductor Corporation.  All rights reserved.
*
* This software, including source code, documentation and related
* materials ("Software") is owned by Cypress Semiconductor Corporation
* or one of its affiliates ("Cypress") and is protected by and subject to
* worldwide patent protection (United States and foreign),
* United States copyright laws and international treaty provisions.
* Therefore, you may use this Software only as provided in the license
* agreement accompanying the software package from which you
* obtained this Software ("EULA").
* If no EULA applies, Cypress hereby grants you a personal, non-exclusive,
* non-transferable license to copy, modify, and compile the Software
* source code solely for use in connection with Cypress's
* integrated circuit products.  Any reproduction, modification, translation,
* compilation, or representation of this Software except as specified
* above is prohibited without the express written permission of Cypress.
*
* Disclaimer: THIS SOFTWARE IS PROVIDED AS-IS, WITH NO WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING, BUT NOT LIMITED TO, NONINFRINGEMENT, IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. Cypress
* reserves the right to make changes to the Software without notice. Cypress
* does not assume any liability arising out of the application or use of the
* Software or any product or circuit described in the Software. Cypress does
* not authorize its products for use in any products where a malfunction or
* failure of the Cypress product may reasonably be expected to result in
* significant property damage, injury or death ("High Risk Product"). By
* including Cypress's product in a High Risk Product, the manufacturer
* of such system or application assumes all risk of such use and in doing
* so agrees to indemnify Cypress against all liability.
*******************************************************************************/
#include "cy_pdl.h"
#include "cybsp.h"
#include "cy_retarget_io.h"
#include <inttypes.h>

/*******************************************************************************
* Macros
*******************************************************************************/
/* Result output format  */
enum OutputFmt
{
    UNSIGNED_RIGHT_ALIGNED,
    SIGNED_RIGHT_ALIGNED,
    LEFT_ALIGNED,
    FORMAT_NUM
};

/* Lower level of average count  */
#define AVERAGE_COUNT_MIN (1u)

/* Upper level of average count  */
#define AVERAGE_COUNT_MAX (256u)

/* Internal band gap reference voltage */
#define BAND_GAP_MV (900u)

/*******************************************************************************
* Global Variables
*******************************************************************************/
/* Interrupt configuration */
const cy_stc_sysint_t IRQ_CFG =
{
    .intrSrc = ((NvicMux3_IRQn << CY_SYSINT_INTRSRC_MUXIRQ_SHIFT) | CE_SAR2_CH1_IRQ),
    .intrPriority = 2UL
};

int32_t g_nextOutputFormat = UNSIGNED_RIGHT_ALIGNED;
int32_t g_nextAverageCount = 1;
int32_t g_outputFormat = -1;
int32_t g_averageCount = -1;

const char *OUTPUT_FORMAT_STR[FORMAT_NUM] =
{
    "Unsigned/Right Aligned",
    "Signed/Right Aligned  ",
    "Left Aligned          "
};

/*******************************************************************************
* Function Prototypes
*******************************************************************************/
void handle_SAR_ADC_IRQ(void);
void configure_SAR_ADC(int32_t outputFormat, int32_t averageCount);

/*******************************************************************************
* Function Name: main
********************************************************************************
* Summary:
*  This is the main function.
*  It sets up SAR ADC with default setting then inputs software trigger to start 
*  AD conversion.
*  The main while loop captures the command from terminal and stores the value 
*  to global variable.
*
* Parameters:
*  none
*
* Return:
*  int
*
*******************************************************************************/
int main(void)
{
    cy_rslt_t result;

    /* Initialize the device and board peripherals */
    result = cybsp_init();

    /* Board init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* Enable global interrupts */
    __enable_irq();

    /* Initialize retarget-io to use the debug UART port */
    Cy_SCB_UART_Init(UART_HW, &UART_config, NULL);
    Cy_SCB_UART_Enable(UART_HW);
    result = cy_retarget_io_init(UART_HW);

    /* retarget-io init failed. Stop program execution */
    if (result != CY_RSLT_SUCCESS)
    {
        CY_ASSERT(0);
    }

    /* \x1b[2J\x1b[;H - ANSI ESC sequence for clear screen */
    printf("\x1b[2J\x1b[;H");

    printf("****************** Code Example: SAR ADC Various Processing of Conversion Result ******************\r\n");
    printf("Press 'a' key to decrease the average count:\r\n"
           "    [256 -> 128 -> 64 -> 32 -> 16 -> 8 -> 4 -> 2 -> 1]\r\n"
           "Press 'd' key to increase the average count:\r\n"
           "    [1 -> 2 -> 4 -> 8 -> 16 -> 32 -> 64 -> 128 -> 256]\r\n"
           "Press 's' key to change the output format:\r\n"
           "    [(Unsigned/Right Aligned) -> (Signed/Right Aligned) -> (Left Aligned) -> (Unsigned/Right Aligned)...]\r\n\n");

    /* \x1b[?25l - ESC sequence for clear cursor (not a pure VT100 escape sequence, but it works in TeraTerm) */
    printf("\x1b[?25l");

    /* Configure SAR-ADC */
    configure_SAR_ADC(g_nextOutputFormat, g_nextAverageCount);

    for (;;)
    {
        uint8_t uartReadValue;

        /* Check if any of valid keys was pressed */
        uartReadValue = Cy_SCB_UART_Get(UART_HW);
        if ((uartReadValue == 'a') || (uartReadValue == 'd'))
        {
            /* Check for limits and increment/decrement accordingly */
            if ((uartReadValue == 'a') && (g_nextAverageCount != AVERAGE_COUNT_MIN))
            {
                g_nextAverageCount >>= 1;
            }
            else if ((uartReadValue == 'd') && (g_nextAverageCount != AVERAGE_COUNT_MAX))
            {
                g_nextAverageCount <<= 1;
            }
        }
        else if (uartReadValue == 's')
        {
            /* change the output format to next one */
            if (++g_nextOutputFormat > LEFT_ALIGNED)
            {
                g_nextOutputFormat = UNSIGNED_RIGHT_ALIGNED;
            }
        }
    }
}

/*******************************************************************************
* Function Name: handle_SAR_ADC_IRQ
********************************************************************************
* Summary:
*  SAR ADC interrupt handler function, will display the potentiometer voltage 
*  in milli volt.
*  Then it reconfigures SAR ADC according to the value of global variable 
*  if changes are there.
*
* Parameters:
*  none
*
* Return:
*  none
*
*******************************************************************************/
void handle_SAR_ADC_IRQ(void)
{
    /* Get interrupt source */
    uint32_t intr = Cy_SAR2_Channel_GetInterruptStatus(PASS0_SAR0, CE_SAR2_AN0_IDX);

    /* Clear interrupt source */
    Cy_SAR2_Channel_ClearInterrupt(PASS0_SAR0, CE_SAR2_AN0_IDX, intr);

    /* if the interrupt is group-done */
    if (intr == CY_SAR2_INT_GRP_DONE)
    {
        /* Get conversion results in counts, do not obtain or analyze status here */
        uint16_t resultVBG = Cy_SAR2_Channel_GetResult(PASS0_SAR0, CE_SAR2_VBG_IDX, NULL);
        uint16_t resultAN0_raw = Cy_SAR2_Channel_GetResult(PASS0_SAR0, CE_SAR2_AN0_IDX, NULL);
        uint16_t resultAN0 = resultAN0_raw;

        if (g_outputFormat == SIGNED_RIGHT_ALIGNED)
        {
            resultAN0 = (resultAN0_raw & 0xFFF);

            /* The 12-bit code for a signal at VREFH/2 is 0x800.
             * This means 0x800 is considered 0, any value below 0x800 is on considered negative,
             * and values above 0x800 are considered positive
             */
            if ((resultAN0 & 0x800) != 0)
            {
                resultAN0 -= 0x800;
            }
            else
            {
                resultAN0 += 0x800;
            }
        }
        else if (g_outputFormat == LEFT_ALIGNED)
        {
            resultAN0 = (resultAN0_raw >> 4) & 0xFFF;
        }

        /* Update the current configuration and the conversion result then move cursor to previous line */
        printf("Output format: %s\r\nAverage count: %" PRId32 "\r\n", OUTPUT_FORMAT_STR[g_outputFormat], g_averageCount);
        printf("Conversion result raw value: 0x%" PRIu16 "\r\n", resultAN0_raw);
        printf("Potentiometer voltage: %" PRIu32 "mV\r\n", ((uint32_t)resultAN0 * BAND_GAP_MV) / (uint32_t)resultVBG);
        printf("\x1b[4F");

        configure_SAR_ADC(g_nextOutputFormat, g_nextAverageCount);
    }
}

/*******************************************************************************
* Function Name: configure_SAR_ADC
********************************************************************************
* Summary:
*  This function configures SAR ADC with specified setting.
*
* Parameters:
*  int32_t outputFormat - The received output format from user input
*  int32_t averageCount - The received average count from user input
*
* Return:
*  none
*
*******************************************************************************/
void configure_SAR_ADC(int32_t outputFormat, int32_t averageCount)
{
    if ((g_outputFormat != outputFormat) || (g_averageCount != averageCount))
    {
        /* De-initialize the SAR2 module */
        Cy_SAR2_DeInit(PASS0_SAR0);

        /* Reflect specified configuration into the structure value */
        CE_SAR2_AN0_config.rightShift = (uint8_t)(log(averageCount) / log(2));
        CE_SAR2_AN0_config.averageCount = (uint16_t)averageCount;

        if (outputFormat == UNSIGNED_RIGHT_ALIGNED)
        {
            CE_SAR2_AN0_config.resultAlignment = CY_SAR2_RESULT_ALIGNMENT_RIGHT;
            CE_SAR2_AN0_config.signExtention = CY_SAR2_SIGN_EXTENTION_UNSIGNED;
        }
        else if (outputFormat == SIGNED_RIGHT_ALIGNED)
        {
            CE_SAR2_AN0_config.resultAlignment = CY_SAR2_RESULT_ALIGNMENT_RIGHT;
            CE_SAR2_AN0_config.signExtention = CY_SAR2_SIGN_EXTENTION_SIGNED;
        }
        else
        {
            CE_SAR2_AN0_config.resultAlignment = CY_SAR2_RESULT_ALIGNMENT_LEFT;
            CE_SAR2_AN0_config.signExtention = CY_SAR2_SIGN_EXTENTION_UNSIGNED;
        }

        /* Initialize the SAR2 module */
        Cy_SAR2_Init(PASS0_SAR0, &CE_SAR2_config);

        /* Set ePASS MMIO reference buffer mode for bangap voltage */
        Cy_SAR2_SetReferenceBufferMode(PASS0_EPASS_MMIO, CY_SAR2_REF_BUF_MODE_ON);

        /* Interrupt settings */
        Cy_SAR2_Channel_SetInterruptMask(PASS0_SAR0, CE_SAR2_AN0_IDX, CY_SAR2_INT_GRP_DONE);
        Cy_SysInt_Init(&IRQ_CFG, &handle_SAR_ADC_IRQ);
        NVIC_SetPriority((IRQn_Type) NvicMux3_IRQn, 2UL);
        NVIC_EnableIRQ((IRQn_Type) NvicMux3_IRQn);
    }

    /* Update current configuration */
    g_outputFormat = outputFormat;
    g_averageCount = averageCount;

    /* Scenario: Obtaining conversion results in counts */
    Cy_SAR2_Channel_SoftwareTrigger(PASS0_SAR0, CE_SAR2_VBG_IDX);
}
/* [] END OF FILE */
