// SPDX-License-Identifier: GPL-2.0-or-later

/***************************************************************************
 ***************************************************************************/

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "imp.h"
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/cortex_m.h>

/* flash bank algo <base> <size> 0 0 <target#>
*/
FLASH_BANK_COMMAND_HANDLER(algo_flash_bank_command)
{
    if (CMD_ARGC < 6)
        return ERROR_COMMAND_SYNTAX_ERROR;

    return ERROR_OK;
}

static int algo_protect_check(struct flash_bank *bank)
{
    struct algo_flash_bank *algo_info = bank->driver_priv;
    return ERROR_OK;
}

static int algo_erase(struct flash_bank *bank, unsigned int first,
        unsigned int last)
{
    struct target *target = bank->target;
    return ERROR_OK;
}

static int algo_protect(struct flash_bank *bank, int set, unsigned int first,
        unsigned int last)
{
    struct target *target = bank->target;
    return ERROR_OK;
}

static int algo_write(struct flash_bank *bank, const uint8_t *buffer,
        uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;
    return ERROR_OK;
}

static int algo_probe(struct flash_bank *bank)
{
    struct target *target = bank->target;
    return ERROR_OK;
}

static int algo_auto_probe(struct flash_bank *bank)
{
    return algo_probe(bank);
}

static int get_algo_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    return ERROR_OK;
}

static const struct command_registration algo_exec_command_handlers[] = {
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
