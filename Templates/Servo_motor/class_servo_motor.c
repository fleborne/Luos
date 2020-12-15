#include "template_servo_motor.h"
#include "class_servo_motor.h"
#include "luos_hal.h"
/******************************************************************************
 * @brief function converting Luos messages innto data and reverse.
 * @param container the target container
 * @param msg the received message
 * @param class_servo_motor the data struct to update
 * @return None
 ******************************************************************************/
static void ServoMotorConfig_Handler(container_t *container, msg_t *msg, class_servo_motor_t *class_servo_motor)
{
    if (msg->header.cmd == PARAMETERS)
    {
        // check the message size
        if (msg->header.size == sizeof(servo_motor_mode_t))
        {
            // fill the message infos
            memcpy((void *)&class_servo_motor->mode, msg->data, msg->header.size);

            // copy motor specific configuration into motor object
            class_servo_motor->motor.mode.current = class_servo_motor->mode.current;
            class_servo_motor->motor.mode.mode_compliant = class_servo_motor->mode.mode_compliant;
            class_servo_motor->motor.mode.temperature = class_servo_motor->mode.temperature;

            // manage specific configuration operations
            if (class_servo_motor->mode.mode_compliant == 0)
            {
                class_servo_motor->target_angular_position = class_servo_motor->angular_position;
            }
        }
    }
}
/******************************************************************************
 * @brief Msg Handler call backed by Luos when a msg receive for this container
 * @param Container destination
 * @param Msg receive
 * @return None
 ******************************************************************************/
static void TemplateServoMotor_MsgHandler(container_t *container, msg_t *msg)
{
    template_servo_motor_t *servo_motor_template = (template_servo_motor_t *)container->template_context;
    ClassServoMotor_Handler(container, msg, &servo_motor_template->class);
    ServoMotorConfig_Handler(container, msg, &servo_motor_template->class);
    if (servo_motor_template->self != 0)
    {
        servo_motor_template->self(container, msg);
    }
}
/******************************************************************************
 * @brief Container creation following the template
 * @param cont_cb is an optional user callback called on every massage for this container
 * @param servo_motor_template template object pointer
 * @param alias for the container string (15 caracters max).
 * @param revision FW for the container (tab[MajorVersion,MinorVersion,Patch])
 * @return None
 ******************************************************************************/
container_t *TemplateServoMotor_CreateContainer(CONT_CB cont_cb, template_servo_motor_t *servo_motor_template, const char *alias, revision_t revision)
{
    servo_motor_template->self = cont_cb;
    // copy motor specific configuration into motor object
    servo_motor_template->class.motor.mode.current = servo_motor_template->class.mode.current;
    servo_motor_template->class.motor.mode.mode_compliant = servo_motor_template->class.mode.mode_compliant;
    servo_motor_template->class.motor.mode.temperature = servo_motor_template->class.mode.temperature;

    container_t *container = Luos_CreateContainer(TemplateServoMotor_MsgHandler, SERVO_MOTOR_TYPE, alias, revision);
    container->template_context = (void *)servo_motor_template;
    return container;
}
/******************************************************************************
 * @brief function converting Luos messages innto data and reverse.
 * @param container the target container
 * @param msg the received message
 * @param servo_motor_class the data struct to update
 * @return None
 ******************************************************************************/
void ClassServoMotor_Handler(container_t *container, msg_t *msg, class_servo_motor_t *servo_motor_class)
{
    if (msg->header.cmd == ASK_PUB_CMD)
    {
        // Report management
        msg_t pub_msg;
        pub_msg.header.target_mode = ID;
        pub_msg.header.target = msg->header.source;
        if (servo_motor_class->mode.angular_position)
        {
            if (servo_motor_class->control.rec)
            {
                LUOS_ASSERT(servo_motor_class->measurement.data_ptr != 0);
                // send back a record stream
                pub_msg.header.cmd = ANGULAR_POSITION;
                Luos_SendStreaming(container, &pub_msg, &servo_motor_class->measurement);
            }
            else
            {
                LuosHAL_SetIrqState(false);
                AngularOD_PositionToMsg((angular_position_t *)&servo_motor_class->angular_position, &pub_msg);
                LuosHAL_SetIrqState(true);
                Luos_SendMsg(container, &pub_msg);
            }
        }
        if (servo_motor_class->mode.angular_speed)
        {
            AngularOD_SpeedToMsg((angular_speed_t *)&servo_motor_class->angular_speed, &pub_msg);
            Luos_SendMsg(container, &pub_msg);
        }
        if (servo_motor_class->mode.linear_position)
        {
            LinearOD_PositionToMsg((linear_position_t *)&servo_motor_class->linear_position, &pub_msg);
            Luos_SendMsg(container, &pub_msg);
        }
        if (servo_motor_class->mode.linear_speed)
        {
            LinearOD_SpeedToMsg((linear_speed_t *)&servo_motor_class->linear_speed, &pub_msg);
            Luos_SendMsg(container, &pub_msg);
        }
        return;
    }
    if (msg->header.cmd == PID)
    {
        // fill the message infos
        if ((servo_motor_class->mode.mode_angular_position || servo_motor_class->mode.mode_linear_position) &&
            !(servo_motor_class->mode.mode_angular_speed || servo_motor_class->mode.mode_linear_speed))
        {
            // only position control is enable, we can save PID for positioning
            PidOD_PidFromMsg(&servo_motor_class->position, msg);
        }
        if ((servo_motor_class->mode.mode_angular_speed || servo_motor_class->mode.mode_linear_speed) &&
            !(servo_motor_class->mode.mode_angular_position || servo_motor_class->mode.mode_linear_position))
        {
            // only speed control is enable, we can save PID for speed
            PidOD_PidFromMsg(&servo_motor_class->speed, msg);
        }
        return;
    }
    if (msg->header.cmd == CONTROL)
    {
        ControlOD_ControlFromMsg(&servo_motor_class->control, msg);
        return;
    }
    if (msg->header.cmd == RESOLUTION)
    {
        // set the encoder resolution
        memcpy((void *)&servo_motor_class->resolution, msg->data, sizeof(float));
        return;
    }
    if (msg->header.cmd == REDUCTION)
    {
        // set the motor reduction
        memcpy((void *)&servo_motor_class->motor_reduction, msg->data, sizeof(float));
        return;
    }
    if (msg->header.cmd == REINIT)
    {
        // set state to 0
        __disable_irq();
        servo_motor_class->angular_position = 0.0;
        servo_motor_class->target_angular_position = 0.0;
        __enable_irq();
        return;
    }
    if (msg->header.cmd == DIMENSION)
    {
        // set the wheel diameter m
        LinearOD_PositionFromMsg((linear_position_t *)&servo_motor_class->wheel_diameter, msg);
        return;
    }
    if (msg->header.cmd == ANGULAR_POSITION)
    {
        if (servo_motor_class->mode.mode_angular_position)
        {
            // Check message size
            if (msg->header.size == sizeof(angular_position_t))
            {
                // set the motor target angular position
                __disable_irq();
                AngularOD_PositionFromMsg((angular_position_t *)&servo_motor_class->target_angular_position, msg);
                __enable_irq();
            }
            else
            {
                LUOS_ASSERT(servo_motor_class->trajectory.data_ptr != 0);
                // this is a trajectory, save it into streaming channel.
                Luos_ReceiveStreaming(container, msg, &servo_motor_class->trajectory);
            }
        }
        return;
    }
    if (msg->header.cmd == ANGULAR_SPEED)
    {
        // set the motor target angular position
        if (servo_motor_class->mode.mode_angular_speed)
        {
            AngularOD_SpeedFromMsg((angular_speed_t *)&servo_motor_class->target_angular_speed, msg);
        }
        return;
    }
    if (msg->header.cmd == LINEAR_POSITION)
    {
        // set the motor target linear position
        // Check message size
        if (msg->header.size == sizeof(linear_position_t))
        {
            // Get the position
            linear_position_t linear_position = 0.0;
            LinearOD_PositionFromMsg(&linear_position, msg);
            // Convert it directly into angle
            servo_motor_class->target_angular_position = (linear_position * 360.0) / (3.141592653589793 * servo_motor_class->wheel_diameter);
        }
        else
        {
            LUOS_ASSERT(servo_motor_class->trajectory.data_ptr != 0);
            // this is a trajectory, save it into ring buffer.
            Luos_ReceiveStreaming(container, msg, &servo_motor_class->trajectory);
            // values will be converted one by one during trajectory management.
        }
        return;
    }
    if (msg->header.cmd == LINEAR_SPEED)
    {
        // set the motor target linear speed
        if (servo_motor_class->wheel_diameter > 0.0)
        {
            // Get the linear speed
            linear_speed_t linear_speed = 0.0;
            LinearOD_SpeedFromMsg(&linear_speed, msg);
            // Convert it directly
            servo_motor_class->target_angular_speed = (linear_speed * 360.0) / (3.141592653589793 * servo_motor_class->wheel_diameter);
        }
        return;
    }
    if (msg->header.cmd == ANGULAR_POSITION_LIMIT)
    {
        // set the motor limit anglular position

        AngularOD_PositionFromMsg((angular_position_t *)&servo_motor_class->limit_angular_position, msg);
        return;
    }
    if (msg->header.cmd == LINEAR_POSITION_LIMIT)
    {
        // set the motor target linear position
        if (servo_motor_class->mode.mode_linear_position & (servo_motor_class->wheel_diameter != 0))
        {
            // Get the linear positions limit
            linear_position_t linear_position[2] = {0.0, 0.0};
            LinearOD_PositionFromMsg((linear_position_t *)&linear_position[0], msg);
            // Directly convert it
            servo_motor_class->limit_angular_position[0] = (linear_position[0] * 360.0) / (3.141592653589793 * servo_motor_class->wheel_diameter);
            servo_motor_class->limit_angular_position[1] = (linear_position[1] * 360.0) / (3.141592653589793 * servo_motor_class->wheel_diameter);
        }
        return;
    }
    if (msg->header.cmd == TIME)
    {
        // save time in ms
        TimeOD_TimeFromMsg((time_luos_t *)&servo_motor_class->sampling_period, msg);
        return;
    }
}
