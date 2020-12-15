/******************************************************************************
 * @file Struct servo motor
 * @brief servo motor data structure definition
 * WARING : This h file should be only included by class_*.h or template_*.h codes
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/
#ifndef TEMPLATE_STRUCT_SERVO_MOTOR_H_
#define TEMPLATE_STRUCT_SERVO_MOTOR_H_

#include <stdbool.h>
#include "luos.h"
#include "struct_motor.h"

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
            uint16_t mode_compliant : 1;
            uint16_t mode_power : 1;
            uint16_t mode_torque : 1;
            uint16_t mode_angular_speed : 1;
            uint16_t mode_angular_position : 1;
            uint16_t mode_linear_speed : 1;
            uint16_t mode_linear_position : 1;

            // measures modes
            uint16_t angular_position : 1;
            uint16_t angular_speed : 1;
            uint16_t linear_position : 1;
            uint16_t linear_speed : 1;
            uint16_t current : 1;
            uint16_t temperature : 1;
        };
        uint8_t unmap[2];
    };
} servo_motor_mode_t;
/*
 * motor data structure
 */
typedef struct __attribute__((__packed__))
{
    // H bridge and motor object
    class_motor_t motor;

    // drive
    servo_motor_mode_t mode;
    angular_position_t target_angular_position;
    angular_speed_t target_angular_speed;

    // limits
    angular_position_t limit_angular_position[2];

    // measures
    angular_position_t angular_position;
    angular_speed_t angular_speed;
    linear_position_t linear_position;
    linear_speed_t linear_speed;
    current_t current;
    temperature_t temperature;

    //configs
    float motor_reduction;
    float resolution;
    linear_position_t wheel_diameter;
    asserv_pid_t position;
    asserv_pid_t speed;

    //streaming
    control_t control;
    streaming_channel_t trajectory;
    streaming_channel_t measurement;
    time_luos_t sampling_period;
} class_servo_motor_t;

#endif /* TEMPLATE_STRUCT_SERVO_MOTOR_H_ */
