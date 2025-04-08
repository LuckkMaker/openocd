// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "imp.h"
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/cortex_m.h>

#include <string.h>
#include <stdlib.h>

typedef struct algo_image {
    uint32_t bankid;
    uint32_t addr;
    uint32_t size;
    uint32_t offset;
    struct flash_bank *bank;
    struct algo_image *next;
    uint8_t *buffer;
} algo_image;

algo_image *algo_image_list_head = NULL;
algo_image *algo_image_list_end = NULL;
algo_image *algo_current_image = NULL;

uint32_t algo_image_page = 0;

uint32_t algo_data_size;
uint32_t algo_data_base;

/* flash bank algo <base> <size> 0 0 <target#> <FLM_data_base> <FLM_data_size>
*/
FLASH_BANK_COMMAND_HANDLER(algo_flash_bank_command)
{
    if (CMD_ARGC < 6)
        return ERROR_COMMAND_SYNTAX_ERROR;
    COMMAND_PARSE_NUMBER(u32, CMD_ARGV[6], algo_data_base);
    COMMAND_PARSE_NUMBER(u32, CMD_ARGV[7], algo_data_size);
    return ERROR_OK;
}

static int algo_init(void)
{
    if(algo_image_list_head == NULL) {
        algo_image_list_head = (algo_image*)malloc(sizeof(algo_image));
        if(algo_image_list_head != NULL) {
            algo_image_list_end = algo_image_list_head;
            algo_current_image = algo_image_list_head;
            algo_image_list_head->next = NULL;
            return ERROR_OK;
        }
        else {
            return ERROR_FAIL;
        }
    }
    else {
        return ERROR_OK;
    }
}

static int algo_protect_check(struct flash_bank *bank)
{
    LOG_USER("algo: enter protect check");
    return ERROR_OK;
}

static int algo_erase(struct flash_bank *bank, unsigned int first,
    unsigned int last)
{
    LOG_USER("algo: enter erase");
    return ERROR_OK;
}

static int algo_protect(struct flash_bank *bank, int set, unsigned int first,
        unsigned int last)
{
    LOG_USER("algo: enter protect");
    return ERROR_OK;
}

/* push the buffer, offset, count to a link list */
static int algo_write(struct flash_bank *bank, const uint8_t *buffer,
        uint32_t offset, uint32_t count)
{
    LOG_USER("algo: enter write");
    LOG_USER("bank base = %08x", bank->base);

    if (count == 0) {
        LOG_WARNING("algo: write size is zero");
        return ERROR_FAIL;
    }

    if (buffer == NULL) {
        LOG_WARNING("algo: buffer is NULL");
        return ERROR_FAIL;
    }

    if (algo_image_list_end == NULL) {
        LOG_WARNING("algo: algo_image_list_end is NULL");
        return ERROR_FAIL;
    }

    algo_image *algo_image_node = NULL;
    uint32_t size, offset2;
    for (offset2 = 0; count != 0; offset2 += algo_data_size, algo_image_node = NULL) {
        if (count > algo_data_size) {
            size = algo_data_size;
            count -= algo_data_size;
        }
        else {
            size = count;
            count = 0;
        }
        algo_image_node = (algo_image *)malloc(sizeof(algo_image));
        algo_image_node->addr = bank->base + offset2;
        algo_image_node->size = size;
        algo_image_node->offset = offset2;
        algo_image_node->bankid = bank->bank_number;
        algo_image_node->bank = bank;
        algo_image_node->buffer = (uint8_t *)malloc(size * sizeof(uint8_t));
        algo_image_node->next = NULL;
        memcpy(algo_image_node->buffer, (buffer + offset2), size);

        algo_image_list_end->next = algo_image_node;
        algo_image_list_end = algo_image_list_end->next;
        /* DEBUG */
        LOG_USER("algo: offset %0.8x, count %d", algo_image_list_end->offset, algo_image_list_end->size);
        algo_image_page++;
    }

    return ERROR_OK;
}

static int algo_probe(struct flash_bank *bank)
{
    LOG_USER("algo: enter probe");

    if (algo_init() == ERROR_FAIL) {
        LOG_WARNING("algo: algo_init fail");
        return ERROR_FAIL;
    }

    return ERROR_OK;
}

static int algo_auto_probe(struct flash_bank *bank)
{
    LOG_USER("algo: enter auto probe");
    return algo_probe(bank);
}

static int get_algo_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    LOG_USER("algo: enter get info");
    return ERROR_OK;
}

// algo init
COMMAND_HANDLER(algo_handle_init_command)
{
    if (algo_init() == ERROR_FAIL) {
        LOG_WARNING("algo: algo_init fail");
        return ERROR_FAIL;
    }

    return ERROR_OK;
}

// algo load
/* load all the data in link list to the target address */
COMMAND_HANDLER(algo_handle_load_data_command)
{
	struct flash_bank *bank;
    struct target *target = NULL;
    uint32_t size = 0, addr = 0, bankid = 0;
    int retval = 0;

    if (algo_current_image->next == NULL) {
        command_print(cmd, "%d %d %d", addr, size, bankid);
        return ERROR_OK;
    }

    algo_current_image = algo_current_image->next;

    bankid = algo_current_image->bankid;
    size = algo_current_image->size;
    addr = algo_current_image->addr;

    if (algo_current_image->bank != NULL) {
        LOG_USER("bank not null");
        target = algo_current_image->bank->target;
    }
    else {
        LOG_USER("bank null");
        retval = CALL_COMMAND_HANDLER(flash_command_get_bank, bankid, &bank);
        if (retval != ERROR_OK)
            return retval;

        target = bank->target;
    }

    if (size % 4 == 0)
    {
        retval = target_write_memory(target, algo_data_base, 4, (size / 4), (algo_current_image->buffer));
    } else if (size % 2 == 0) {
        retval = target_write_memory(target, algo_data_base, 2, (size / 2), (algo_current_image->buffer));
    } else {
        retval = target_write_memory(target, algo_data_base, 1, size, (algo_current_image->buffer));
    }
    if (retval != ERROR_OK){
        LOG_USER("write memory error")
    }
    command_print(cmd, "%d %d %d", addr, size, bankid);
    return ERROR_OK;
}

COMMAND_HANDLER(algo_handle_done_data_command)
{
    algo_image *p;
    algo_image_page = 0;
    algo_current_image = NULL;
    algo_image_list_end = NULL;
    for (p = algo_image_list_head->next; p != NULL; p = p->next) {
        free(algo_image_list_head);
        algo_image_list_head = p;
    }
    if (algo_image_list_head != NULL) {
        free(algo_image_list_head);
        algo_image_list_head = NULL;
    }
    return ERROR_OK;
}

COMMAND_HANDLER(algo_handle_test_command)
{
    algo_image *p;

    if (algo_image_list_head == NULL) {
        return ERROR_OK;
    }

    for (p = algo_image_list_head->next; p != NULL; p = p->next) {
        LOG_USER("bankid = %d, size = %d, offset = %d", p->bankid, p->size, p->offset);
    }

    return ERROR_OK;
}

static const struct command_registration algo_exec_command_handlers[] = {
    {
		.name = "init",
		.handler = algo_handle_init_command,
		.mode = COMMAND_EXEC,
		.usage = "init",
		.help = "init.",
	},
    {
		.name = "load",
		.handler = algo_handle_load_data_command,
		.mode = COMMAND_EXEC,
		.usage = "load",
		.help = "load.",
	},
    {
		.name = "done",
		.handler = algo_handle_done_data_command,
		.mode = COMMAND_EXEC,
		.usage = "done",
		.help = "done.",
	},
    {
		.name = "test",
		.handler = algo_handle_test_command,
		.mode = COMMAND_EXEC,
		.usage = "test",
		.help = "test.",
	},
    COMMAND_REGISTRATION_DONE
};

static const struct command_registration algo_command_handlers[] = {
    {
        .name = "algo",
        .mode = COMMAND_ANY,
        .help = "algo flash command group",
        .usage = "",
        .chain = algo_exec_command_handlers,
    },
    COMMAND_REGISTRATION_DONE
};

const struct flash_driver algo_flash = {
    .name = "algo",
    .commands = algo_command_handlers,
    .flash_bank_command = algo_flash_bank_command,
    .erase = algo_erase,
    .protect = algo_protect,
    .write = algo_write,
    .read = default_flash_read,
    .probe = algo_probe,
    .auto_probe = algo_auto_probe,
    .erase_check = default_flash_blank_check,
    .protect_check = algo_protect_check,
    .info = get_algo_info,
    .free_driver_priv = default_flash_free_driver_priv,
};
