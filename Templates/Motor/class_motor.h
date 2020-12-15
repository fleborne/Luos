/******************************************************************************
 * @file Class motor
 * @brief motor object managing a true false API
 * WARING : This h file should be only included by class_*.c codes
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/
#ifndef TEMPLATE_CLASS_MOTOR_H_
#define TEMPLATE_CLASS_MOTOR_H_

#include "struct_motor.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Function
 ******************************************************************************/

void ClassMotor_Handler(container_t *container, msg_t *msg, class_motor_t *motor_class);

#endif /* TEMPLATE_CLASS_MOTOR_H_ */
