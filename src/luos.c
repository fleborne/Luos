#include "luos.h"
#include "luos_board.h"
#include <stdio.h>
#include "message_mngr.h"


#define STRINGIFY(s) STRINGIFY1(s)
#define STRINGIFY1(s) #s

static module_t *luos_module_pointer;
static volatile msg_t luos_pub_msg;
static volatile int luos_pub = LUOS_PROTOCOL_NB;
module_t module_table[MAX_VM_NUMBER];
unsigned char module_number;
volatile route_table_t *route_table_pt;

//**************** Private functions*********************
static int luos_msg_handler(module_t *module, msg_t *input, msg_t *output)
{
    if (input->header.cmd == IDENTIFY_CMD)
    {
        // someone request a local route table
        // Just create an empty message, it will be filled in luos_loop.
        output->header.cmd = INTRODUCTION_CMD;
        output->header.target_mode = IDACK;
        output->header.target = input->header.source;
        luos_pub = IDENTIFY_CMD;
        return 1;
    }
    if (input->header.cmd == INTRODUCTION_CMD)
    {
        volatile route_table_t *route_tab = &get_route_table()[get_last_entry()];
        if (luos_receive_data(module, input, (void *)route_tab))
        {
            // route table of this board is finish
            compute_route_table_entry_nb();
        }
        return 1;
    }
    if ((input->header.cmd == REVISION) & (input->header.size == 0))
    {
        output->header.cmd = REVISION;
        output->header.target_mode = ID;
        sprintf((char *)output->data, "%s", module->firm_version);
        memcpy(output->data, module->firm_version, sizeof(output->data));
        output->header.size = strlen((char *)output->data);
        output->header.target = input->header.source;
        luos_pub = REVISION;
        return 1;
    }
    if ((input->header.cmd == LUOS_REVISION) & (input->header.size == 0))
    {
        output->header.cmd = LUOS_REVISION;
        output->header.target_mode = ID;
        const char *luos_version = STRINGIFY(VERSION);
        sprintf((char *)output->data, "%s", luos_version);
        memcpy(output->data, luos_version, sizeof(output->data));
        output->header.size = strlen((char *)output->data);
        output->header.target = input->header.source;
        luos_pub = LUOS_REVISION;
        return 1;
    }
    if ((input->header.cmd == ROBUS_REVISION) & (input->header.size == 0))
    {
        output->header.cmd = ROBUS_REVISION;
        output->header.target_mode = ID;
        sprintf((char *)output->data, "%s", ROBUS_VERSION);
        memcpy(output->data, ROBUS_VERSION, sizeof(output->data));
        output->header.size = strlen((char *)output->data);
        output->header.target = input->header.source;
        luos_pub = ROBUS_REVISION;
        return 1;
    }
    if ((input->header.cmd == NODE_UUID) & (input->header.size == 0))
    {
        output->header.cmd = NODE_UUID;
        output->header.target_mode = ID;
        output->header.size = sizeof(luos_uuid_t);
        output->header.target = input->header.source;
        luos_uuid_t uuid;
        uuid.uuid[0] = LUOS_UUID[0];
        uuid.uuid[1] = LUOS_UUID[1];
        uuid.uuid[2] = LUOS_UUID[2];
        memcpy(output->data, &uuid.unmap, sizeof(luos_uuid_t));
        luos_pub = NODE_UUID;
        return 1;
    }
    if (input->header.cmd == WRITE_ALIAS)
    {
        // Make a clean copy with full \0 at the end.
        memset(module->alias, '\0', sizeof(module->alias));
        if (input->header.size > 16)
            input->header.size = 16;
        if ((((input->data[0] >= 'A') & (input->data[0] <= 'Z')) | ((input->data[0] >= 'a') & (input->data[0] <= 'z')) | (input->data[0] == '\0')) & (input->header.size != 0))
        {
            memcpy(module->alias, input->data, input->header.size);
            luos_save_alias(module, module->alias);
        }
        else
        {
            // This is an alias erase instruction, get back to default one
            luos_save_alias(module, '\0');
            memcpy(module->alias, module->default_alias, MAX_ALIAS_SIZE);
        }
        return 1;
    }
    if (input->header.cmd == UPDATE_PUB)
    {
        // this module need to be auto updated
        time_luos_t time;
        time_from_msg(&time, input);
        module->auto_refresh.target = input->header.source;
        module->auto_refresh.time_ms = (uint16_t)time_to_ms(time);
        module->auto_refresh.last_update = node_get_systick();
        return 1;
    }

    return 0;
}

module_t *get_module(vm_t *vm)
{
    for (int i = 0; i < module_number; i++)
    {
        if (vm == module_table[i].vm)
        {
            return &module_table[i];
        }
    }
    return 0;
}

int get_module_index(module_t *module)
{
    for (int i = 0; i < module_number; i++)
    {
        if (module == &module_table[i])
        {
            return i;
        }
    }
    return -1;
}

void luos_cb(vm_t *vm, msg_t *msg)
{
    // Luos message management
    volatile module_t *module = get_module(vm);
    if (module == 0)
    {
        // module overwrited, it seem to be possible if ring_buffer overflow.
        while (1);
    }
    if (luos_msg_handler((module_t *)module, msg, (msg_t *)&luos_pub_msg))
    {
        luos_module_pointer = (module_t *)module;
        return;
    }
    // L0 message management
    int pub_type = node_msg_handler((module_t *)module, msg, (msg_t *)&luos_pub_msg);
    if (pub_type == NODE_LED)
    {
        return;
    }
    if (pub_type != LUOS_PROTOCOL_NB)
    {
        luos_module_pointer = (module_t *)module;
        luos_pub = pub_type;
        return;
    }
    if ((module->rt >= 1) & (module->mod_cb != 0))
    {
        module->mod_cb((module_t *)module, msg);
    }
    else
    {
        //store module and msg pointer
        mngr_set((module_t *)module, msg);
    }
}

void transmit_local_route_table(void)
{
    // We receive this command because someone creating a new route table
    // Reset the actual route table
    flush_route_table();
    volatile int entry_nb = 0;
    volatile route_table_t local_route_table[module_number + 1];
    //start by saving board entry
    luos_uuid_t uuid;
    uuid.uuid[0] = LUOS_UUID[0];
    uuid.uuid[1] = LUOS_UUID[1];
    uuid.uuid[2] = LUOS_UUID[2];
    unsigned char table_size;
    unsigned short *detection_branches = robus_get_node_branches(&table_size);
    convert_board_to_route_table((route_table_t *)&local_route_table[entry_nb++], uuid, detection_branches, table_size);
    // save modules entry
    for (int i = 0; i < module_number; i++)
    {
        convert_module_to_route_table((route_table_t *)&local_route_table[entry_nb++], &module_table[i]);
    }
    luos_send_data(luos_module_pointer, (msg_t *)&luos_pub_msg, (void *)local_route_table, (entry_nb * sizeof(route_table_t)));
}

void auto_update_manager(void)
{
    // check all modules timed_update_t contexts
    for (int i = 0; i < module_number; i++)
    {
        // check if modules have an actual ID. If not, we are in detection mode and should reset the auto refresh
        if (module_table[i].vm->id == DEFAULTID)
        {
            // this module have not been detected or is in detection mode. remove auto_refresh parameters
            module_table[i].auto_refresh.target = 0;
            module_table[i].auto_refresh.time_ms = 0;
            module_table[i].auto_refresh.last_update = 0;
        }
        else
        {
            // check if there is a timed update setted and if it's time to update it.
            if (module_table[i].auto_refresh.time_ms)
            {
                if ((node_get_systick() - module_table[i].auto_refresh.last_update) >= module_table[i].auto_refresh.time_ms)
                {
                    // This module need to send an update
                    // Create a fake message for it from the module asking for update
                    msg_t updt_msg;
                    updt_msg.header.target = module_table[i].vm->id;
                    updt_msg.header.source = module_table[i].auto_refresh.target;
                    updt_msg.header.target_mode = IDACK;
                    updt_msg.header.cmd = ASK_PUB_CMD;
                    updt_msg.header.size = 0;
                    if ((module_table[i].mod_cb != 0))
                    {
                        module_table[i].mod_cb(&module_table[i], &updt_msg);
                    }
                    else
                    {
                        //store module and msg pointer
                        // todo this can't work for now because this message is not permanent.
                        //mngr_set(&module_table[i], &updt_msg);
                    }
                    module_table[i].auto_refresh.last_update = node_get_systick();
                }
            }
        }
    }
}

//************* Public functions *********************

void luos_init(void)
{

    module_number = 0;
    node_init();
    robus_init(luos_cb);

    /*pinMode(10,OUTPUT);
    digitalWrite(10,HIGH);
    delay(1000);
    digitalWrite(10,LOW);*/
}

void luos_loop(void)
{
    mngr_t chunk;
    if (luos_pub != LUOS_PROTOCOL_NB)
    {
        if (luos_pub == IDENTIFY_CMD)
        {
            transmit_local_route_table();
        }
        else
        {
            luos_send(luos_module_pointer, (msg_t *)&luos_pub_msg);
        }
        luos_pub = LUOS_PROTOCOL_NB;
    }
    // filter stacked module with callback
    int i = get_next_cb_id();
    while (i >= 0)
    {
        mngr_get(i, &chunk);
        chunk.module->mod_cb(chunk.module, chunk.msg);
        i = get_next_cb_id();
    }
    // manage timed auto update
    auto_update_manager();
    node_loop();
}

void luos_modules_clear(void)
{
    module_number = 0;
    robus_modules_clear();
}

module_t *luos_module_create(MOD_CB mod_cb, unsigned char type, const char *alias, char *firm_revision)
{
    unsigned char i = 0;
    module_t *module = &module_table[module_number];
    module->vm = robus_module_create(type);
    module->rt = 0;
    module->message_available = 0;

    // Link the module to his callback
    module->mod_cb = mod_cb;
    // Save default alias
    for (i = 0; i < MAX_ALIAS_SIZE - 1; i++)
    {
        module->default_alias[i] = alias[i];
        if (module->default_alias[i] == '\0')
            break;
    }
    module->default_alias[i] = '\0';
    // Initialise the module alias to 0
    memset((void *)module->alias, 0, sizeof(module->alias));
    if (!read_alias(module_number, (char *)module->alias))
    {
        // if no alias saved keep the default one
        for (i = 0; i < MAX_ALIAS_SIZE - 1; i++)
        {
            module->alias[i] = alias[i];
            if (module->alias[i] == '\0')
                break;
        }
        module->alias[i] = '\0';
    }

    //Initialise the module firm_version to 0
    memset((void *)module->firm_version, 0, sizeof(module->firm_version));
    // Save firmware version
    for (i = 0; i < 20; i++)
    {
        module->firm_version[i] = firm_revision[i];
        if (module->firm_version[i] == '\0')
            break;
    }

    module_number++;
    return module;
}

void luos_module_enable_rt(module_t *module)
{
    module->rt = 1;
}

unsigned char luos_send(module_t *module, msg_t *msg)
{
    return robus_send(module->vm, msg);
}

unsigned char luos_send_data(module_t *module, msg_t *msg, void *bin_data, unsigned short size)
{
    // Compute number of message needed to send this data
    int msg_number = 1;
    int sent_size = 0;
    if (size > MAX_DATA_MSG_SIZE)
    {
        msg_number = (size / MAX_DATA_MSG_SIZE);
        msg_number += (msg_number * MAX_DATA_MSG_SIZE < size);
    }

    // Send messages one by one
    for (volatile int chunk = 0; chunk < msg_number; chunk++)
    {
        // Compute chunk size
        int chunk_size = 0;
        if ((size - sent_size) > MAX_DATA_MSG_SIZE)
            chunk_size = MAX_DATA_MSG_SIZE;
        else
            chunk_size = size - sent_size;

        // Copy data into message
        memcpy(msg->data, (char *)bin_data + sent_size, chunk_size);
        msg->header.size = size - sent_size;

        // Send message
        if (luos_send(module, msg))
        {
            // This message fail stop transmission and return an error
            return 1;
        }

        // Save current state
        sent_size = sent_size + chunk_size;
    }
    return 0;
}

unsigned char luos_send_streaming(module_t *module, msg_t *msg, streaming_channel_t *stream)
{
    // Compute number of message needed to send available datas on ring buffer
    int msg_number = 1;
    int data_size = get_nb_available_samples(stream);
    const int max_data_msg_size = (MAX_DATA_MSG_SIZE / stream->data_size);
    if (data_size > max_data_msg_size)
    {
        msg_number = (data_size / max_data_msg_size);
        msg_number += ((msg_number * max_data_msg_size) < data_size);
    }

    // Send messages one by one
    for (volatile int chunk = 0; chunk < msg_number; chunk++)
    {
        // compute chunk size
        int chunk_size = 0;
        if (data_size > max_data_msg_size)
            chunk_size = max_data_msg_size;
        else
            chunk_size = data_size;

        // Copy data into message
        get_samples(stream, msg->data, chunk_size);

        // Send message
        if (luos_send(module, msg))
        {
            // this message fail stop transmission, retrieve datas and return an error
            stream->sample_ptr = stream->sample_ptr - (chunk_size * stream->data_size);
            if (stream->sample_ptr < stream->ring_buffer)
            {
                stream->sample_ptr = stream->end_ring_buffer - (stream->ring_buffer - stream->sample_ptr);
            }
            return 1;
        }

        // check end of data
        if (data_size > max_data_msg_size)
        {
            data_size -= max_data_msg_size;
        }
        else
        {
            data_size = 0;
        }
    }
    return 0;
}

unsigned char luos_receive_data(module_t *module, msg_t *msg, void *bin_data)
{
    // Manage buffer session (one per module)
    static uint32_t data_size[MAX_VM_NUMBER] = {0};
    static int last_msg_size = 0;
    int id = get_module_index(module);

    // check message integrity
    if ((last_msg_size > 0) && (last_msg_size - MAX_DATA_MSG_SIZE > msg->header.size))
    {
        // we miss a message (a part of the data),
        // reset session and return an error.
        data_size[id] = 0;
        last_msg_size = 0;
        return 2;
    }

    // Get chunk size
    unsigned short chunk_size = 0;
    if (msg->header.size > MAX_DATA_MSG_SIZE)
        chunk_size = MAX_DATA_MSG_SIZE;
    else
        chunk_size = msg->header.size;

    // Copy data into buffer
    memcpy((char *)bin_data + data_size[id], msg->data, chunk_size);

    // Save buffer session
    data_size[id] = data_size[id] + chunk_size;
    last_msg_size = msg->header.size;

    // Check end of data
    if (!(msg->header.size > MAX_DATA_MSG_SIZE))
    {
        // Data collection finished, reset buffer session state
        data_size[id] = 0;
        last_msg_size = 0;
        return 1;
    }
    return 0;
}

unsigned char luos_receive_streaming(module_t *module, msg_t *msg, streaming_channel_t *stream)
{
    // Get chunk size
    unsigned short chunk_size = 0;
    if (msg->header.size > MAX_DATA_MSG_SIZE)
        chunk_size = MAX_DATA_MSG_SIZE;
    else
        chunk_size = msg->header.size;

    // Copy data into buffer
    set_samples(stream, msg->data, (chunk_size / stream->data_size));

    // Check end of data
    if ((msg->header.size <= MAX_DATA_MSG_SIZE))
    {
        // Chunk collection finished
        return 1;
    }
    return 0;
}

msg_t *luos_read(module_t *module)
{
    if (module->message_available > MSG_BUFFER_SIZE)
    {
        // msg read too slow
    }
    if (module->message_available)
    {
        // check if there is a message for this module
        int i = get_next_module_id(module);
        if (i >= 0)
        {
            // this module have a message, get it
            mngr_t chunk;
            mngr_get(i, &chunk);
            return chunk.msg;
        }
    }
    return 0;
}

msg_t *luos_read_from(module_t *module, short id)
{
    if (module->message_available)
    {
        // Get the next message manager id containing something for this module
        int mngr_module_id = get_next_module_id(module);
        if (mngr_module_id >= 0)
        {
            // check if there is a message from this id in this module
            int mngr_msg_id = get_next_msg_id(mngr_module_id, id);
            if (mngr_msg_id >= 0)
            {
                mngr_t chunk;
                mngr_get_msg(mngr_module_id, mngr_msg_id, &chunk);
                return chunk.msg;
            }
        }
    }
    return 0;
}

char luos_message_available(void)
{
    return mngr_message_available();
}

void luos_save_alias(module_t *module, char *alias)
{
    // Get module index
    int i = get_module_index(module);
    if (i >= 0)
    {
        write_alias(i, alias);
    }
}

void luos_set_baudrate(module_t *module, uint32_t baudrate)
{
    robus_set_baudrate(module->vm, baudrate);
}
