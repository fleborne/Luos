/******************************************************************************
 * @file Template voltage
 * @brief voltage container template
 * WARING : This h file should be only included by user code or class_*.h codes
 * @author Luos
 * @version 0.0.0
 ******************************************************************************/
#ifndef TEMPLATE_TEMPLATE_VOLTAGE_H_
#define TEMPLATE_TEMPLATE_VOLTAGE_H_

#include <stdbool.h>
#include "luos.h"
#include "struct_voltage.h"

/*******************************************************************************
 * Definitions
 ******************************************************************************/

/*
 * voltage object
 */
typedef struct
{
    CONT_CB self;
    class_voltage_t class;
} template_voltage_t;

/*******************************************************************************
 * Variables
 ******************************************************************************/

/*******************************************************************************
 * Function
 ******************************************************************************/

container_t *TemplateVoltage_CreateContainer(CONT_CB cont_cb, template_voltage_t *var, const char *alias, revision_t revision);

#endif /* TEMPLATE_TEMPLATE_VOLTAGE_H_ */
