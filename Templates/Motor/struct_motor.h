/******************************************************************************
 * @file Struct motor
 * @brief motor data structure definition
 * WARING : This h file should be only included by class_*.h or template_*.h codes
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/
#ifndef TEMPLATE_STRUCT_MOTOR_H_
#define TEMPLATE_STRUCT_MOTOR_H_

#include <stdbool.h>
#include "luos.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/
/*
 * motor mode structure
 */
typedef struct __attribute__((__packed__))
{
    union
    {
        struct __attribute__((__packed__))
        {
            // drive modes
            uint8_t mode_compliant : 1;

            // measures modes
            uint8_t temperature : 1;
            uint8_t current : 1;
        };
        uint8_t unmap[1];
    };
} motor_mode_t;
/*
 * motor data structure
 */
typedef struct
{
    // drive
    motor_mode_t mode;
    ratio_t power;

    // limits
    ratio_t limit_ratio;
    current_t limit_current;

    // measures
    temperature_t temperature;
    current_t current;
} class_motor_t;

#endif /* TEMPLATE_STRUCT_MOTOR_H_ */
