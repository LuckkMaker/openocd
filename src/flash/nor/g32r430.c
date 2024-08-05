#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <string.h>

#include "imp.h"
#include <helper/binarybuffer.h>
#include <target/algorithm.h>
#include <target/cortex_m.h>

/* g32r430 register locations */
#define FLASH_REG_BASE 0x40020800

#define G32_FLASH_KEY 0x00
#define G32_FLASH_OPTKEY 0x04
#define G32_FLASH_SR 0x08
#define G32_FLASH_CR0 0x0C
#define G32_FLASH_CR1 0x10
#define G32_FLASH_TMCR 0x18
#define G32_FLASH_IPCR 0x1C
#define G32_FLASH_OBSR0 0x20
#define G32_FLASH_OBSR1 0x24
#define G32_FLASH_OBCR0 0x28
#define G32_FLASH_OBCR1 0x2C

/* FLASH_SR register bits */
#define G32_FLASH_SR_ERD (1 << 0)
#define G32_FLASH_SR_PRD (1 << 1)
#define G32_FLASH_SR_BSY (1 << 2)
#define G32_FLASH_SR_KEYERR (1 << 3)
#define G32_FLASH_SR_WRPRTERR (1 << 4)
#define G32_FLASH_SR_WADRERR (1 << 5)
#define G32_FLASH_SR_MAINUNLOCK (1 << 17)

#define FLASH_STATUS_ERROR (G32_FLASH_SR_WRPRTERR | G32_FLASH_SR_WADRERR | G32_FLASH_SR_KEYERR)

// Flash Control0 Register definitions
#define G32_FLASH_CR0_EREQ ((unsigned int)0x00000001)
#define G32_FLASH_CR0_PREQ ((unsigned int)0x00000002)
#define G32_FLASH_CR0_ERTYPE ((unsigned int)0x0000000C)
#define G32_FLASH_CR0_EOPIE ((unsigned int)0x00000010)
#define G32_FLASH_CR0_ERRIE ((unsigned int)0x00000020)
#define G32_FLASH_CR0_OPRELOAD ((unsigned int)0x00008000)

/* timeout values */

#define FLASH_WRITE_TIMEOUT 10
#define FLASH_ERASE_TIMEOUT 1000

#define KEY1 0x96969696
#define KEY2 0x3C3C3C3C

#define OPT_BASE_REG 0x1FFF0000
#define OPT_BIT_WLOCK 0x04

struct g32r430xx_options
{
    uint8_t rdp;
    uint8_t user;
    uint16_t data;
    uint32_t protection;
};

struct g32r430xx_flash_bank
{
    struct g32r430xx_options option_bytes;
    int ppage_size;
    bool probed;

    uint32_t register_base;
    uint8_t default_rdp;
    int rdp_length;
    int watchdog_offset;
    int reset_stop_offset;
    int reset_stdb_offset;
    int user_data_offset;
    int option_offset;
    uint32_t user_bank_size;
};

static int g32r430xx_mass_erase(struct flash_bank *bank);

/* flash bank g32r430xx <base> <size> 0 0 <target#>
 */
FLASH_BANK_COMMAND_HANDLER(g32r430xx_flash_bank_command)
{
    struct g32r430xx_flash_bank *g32r430xx_info;

    if (CMD_ARGC < 6)
        return ERROR_COMMAND_SYNTAX_ERROR;

    g32r430xx_info = malloc(sizeof(struct g32r430xx_flash_bank));

    bank->driver_priv = g32r430xx_info;
    g32r430xx_info->probed = false;
    g32r430xx_info->register_base = FLASH_REG_BASE_B0;
    g32r430xx_info->user_bank_size = bank->size;

    /* The flash write must be aligned to a halfword boundary */
    bank->write_start_alignment = bank->write_end_alignment = 2;

    return ERROR_OK;
}

static inline int g32r430xx_get_flash_reg(struct flash_bank *bank, uint32_t reg)
{
    struct g32r430xx_flash_bank *g32r430xx_info = bank->driver_priv;
    return reg + g32r430xx_info->register_base;
}

static inline int g32r430xx_get_flash_status(struct flash_bank *bank, uint32_t *status)
{
    struct target *target = bank->target;
    return target_read_u32(target, FLASH_REG_BASE + G32_FLASH_SR, status);
}

static int g32r430xx_wait_status_busy(struct flash_bank *bank, int timeout)
{
    uint32_t status;
    int retval = ERROR_OK;

    for (;;)
    {
        retval = g32r430xx_get_flash_status(bank, &status);

        if (retval != ERROR_OK)
        {
            return retval;
        }

        if (status & G32_FLASH_SR_ERD || status & G32_FLASH_SR_PRD)
        {
            return ERROR_OK;
        }

        if (timeout-- <= 0)
        {
            LOG_DEBUG("Timed out waiting for flash: 0x%04x", status);
            return ERROR_FAIL;
        }

        alive_sleep(10);
    }

    return retval;
}

static int g32r430xx_erase(struct flash_bank *bank, unsigned int first,
                           unsigned int last)
{
    struct target *target = bank->target;

    LOG_DEBUG("g32r430xx erase: %d - %d", first, last);

    if (bank->target->state != TARGET_HALTED)
    {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    /* unlock flash registers */
    int retval = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_KEY), KEY1);
    if (retval != ERROR_OK)
        return retval;
    retval = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_KEY), KEY2);
    if (retval != ERROR_OK)
        goto flash_lock;

    for (unsigned int i = first; i <= last; i++)
    {
        retval = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_CR0), 0xC);
        if (retval != ERROR_OK)
            goto flash_lock;
        retval = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_CR0), G32_FLASH_CR0_EREQ);
        if (retval != ERROR_OK)
            goto flash_lock;
        retval = target_write_u32(target, bank->base + bank->sectors[i].offset, 0xA5A5);
        if (retval != ERROR_OK)
            goto flash_lock;

        retval = g32r430xx_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);
        if (retval != ERROR_OK)
            goto flash_lock;

        target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_SR), G32_FLASH_SR_ERD);

        LOG_DEBUG("g32r430xx erased page %d", i);
        bank->sectors[i].is_erased = 1;
    }

flash_lock:
{
    int retval2 = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_SR), G32_FLASH_SR_MAINUNLOCK);
    if (retval == ERROR_OK)
        retval = retval2;
}
    return retval;
}

static int g32r430xx_protect(struct flash_bank *bank, int set, unsigned int first, unsigned int last)
{
    return ERROR_FLASH_OPER_UNSUPPORTED;
}

static int g32r430xx_write(struct flash_bank *bank, const uint8_t *buffer,
                           uint32_t offset, uint32_t count)
{
    struct target *target = bank->target;

    LOG_DEBUG("g32r430xx flash write: 0x%x 0x%x", offset, count);

    if (target->state != TARGET_HALTED)
    {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }
    if (offset & 0x03)
    {
        LOG_ERROR("offset 0x%" PRIx32 " breaks required 4-byte alignment", offset);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }
    if (count & 0x3)
    {
        LOG_ERROR("size 0x%" PRIx32 " breaks required 4-byte alignment", count);
        return ERROR_FLASH_DST_BREAKS_ALIGNMENT;
    }

    uint32_t addr = offset;
    for (uint32_t i = 0; i < count; i += 4)
    {
        uint32_t word = (buffer[i] << 0) |
                        (buffer[i + 1] << 8) |
                        (buffer[i + 2] << 16) |
                        (buffer[i + 3] << 24);

        LOG_DEBUG("g32r430xx flash write word 0x%x 0x%x 0x%08x", i, addr, word);

        // flash memory word program
        int retval;
        retval = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_CR0), G32_FLASH_CR0_PREQ);
        if (retval != ERROR_OK)
            return retval;
        retval = target_write_u32(target, addr, word);
        if (retval != ERROR_OK)
            return retval;

        // wait
        retval = g32r430xx_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);
        if (retval != ERROR_OK)
            return retval;

        target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_SR), G32_FLASH_SR_PRD);

        addr += 4;
    }

    LOG_DEBUG("g32r430xx flash write success");
    return ERROR_OK;
}

static int g32r430xx_probe(struct flash_bank *bank)
{
    int page_size = 512;
    int num_pages = bank->size / page_size;

    LOG_INFO("g32r430xxx probe: %d pages, 0x%x bytes, 0x%x total", num_pages, page_size, bank->size);

    if (bank->sectors)
    {
        free(bank->sectors);
    }

    bank->base = 0x0;
    bank->num_sectors = num_pages;
    bank->sectors = malloc(sizeof(struct flash_sector) * num_pages);

    for (int i = 0; i < num_pages; ++i)
    {
        bank->sectors[i].offset = i * page_size;
        bank->sectors[i].size = page_size;
        bank->sectors[i].is_erased = -1;
        bank->sectors[i].is_protected = 1;
    }

    return ERROR_OK;
}

static int g32r430xx_auto_probe(struct flash_bank *bank)
{
    return g32r430xx_probe(bank);
}

static int g32r430xx_protect_check(struct flash_bank *bank)
{
    struct target *target = bank->target;
    uint32_t ob_wlock;

    target_read_u32(target, (OPT_BASE_REG + OPT_BIT_WLOCK), &ob_wlock);

    // Set page protection
    for (int i = 0; i < 256; ++i)
    {
        int bit = i / 32;

        if (ob_wlock & (1 << bit))
        {
            bank->sectors[i].is_protected = 1;
        }
        else
        {
            bank->sectors[i].is_protected = 0;
        }

        LOG_DEBUG("g32r430xx flash protect check: 0x%x 0x%x 0x%x", i, ob_wlock, bank->sectors[i].is_protected);
    }

    return ERROR_OK;
}

static int g32r430xx_info(struct flash_bank *bank, struct command_invocation *cmd)
{
    command_print_sameline(cmd, "g32r430xx");
    return ERROR_OK;
}

static int g32r430xx_mass_erase(struct flash_bank *bank)
{
    int retval;
    struct target *target = bank->target;

    if (target->state != TARGET_HALTED)
    {
        LOG_ERROR("Target not halted");
        return ERROR_TARGET_NOT_HALTED;
    }

    // WITHOUT FMC_BUSY CHECK?!

    // flash memory mass erase
    retval = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_CR0), 0x0);
    if (retval != ERROR_OK)
        goto flash_lock;
    retval = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_CR0), G32_FLASH_CR0_EREQ);
    if (retval != ERROR_OK)
        goto flash_lock;
    retval = target_write_u32(target, 0x08000000, 0xA5A5);
    if (retval != ERROR_OK)
        goto flash_lock;

    retval = g32r430xx_wait_status_busy(bank, FLASH_ERASE_TIMEOUT);
    if (retval != ERROR_OK)
        goto flash_lock;

    target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_SR), G32_FLASH_SR_ERD);

flash_lock:
{
    int retval2 = target_write_u32(target, g32r430xx_get_flash_reg(bank, G32_FLASH_SR), G32_FLASH_SR_MAINUNLOCK);
    if (retval == ERROR_OK)
        retval = retval2;
}

    return ERROR_OK;
}

COMMAND_HANDLER(g32r430xx_handle_mass_erase_command)
{
    if (CMD_ARGC < 6)
    {
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

    struct flash_bank *bank;
    int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
    if (retval != ERROR_OK)
    {
        return retval;
    }

    retval = g32r430xx_mass_erase(bank);
    if (retval == ERROR_OK)
    {
        // set all sectors as erased
        unsigned int i;
        for (i = 0; i < bank->num_sectors; i++)
            bank->sectors[i].is_erased = 1;

        // command_print(CMD_CTX, "g32r430xx mass erase complete");
    }
    else
    {
        // command_print(CMD_CTX, "g32r430xx mass erase failed");
    }

    return ERROR_OK;
}

COMMAND_HANDLER(g32r430xx_handle_test_write)
{
    if (CMD_ARGC < 6)
    {
        return ERROR_COMMAND_SYNTAX_ERROR;
    }

    struct flash_bank *bank;
    int retval = CALL_COMMAND_HANDLER(flash_command_get_bank, 0, &bank);
    if (retval != ERROR_OK)
    {
        return retval;
    }

    uint8_t buffer[32];
    for (int i = 0; i < 32; ++i)
    {
        buffer[i] = i;
    }

    retval = g32r430xx_erase(bank, 0, 0);
    if (retval != ERROR_OK)
        return retval;

    retval = g32r430xx_write(bank, buffer, 0, 32);
    if (retval == ERROR_OK)
    {
        // command_print(CMD_CTX, "g32r430xx test write complete");
    }
    else
    {
        // command_print(CMD_CTX, "g32r430xx test write failed");
    }

    return retval;
}

static const struct command_registration g32r430xx_exec_command_handlers[] = {
    {
        .name = "mass_erase",
        .handler = g32r430xx_handle_mass_erase_command,
        .mode = COMMAND_EXEC,
        .usage = "bank_id",
        .help = "test flash write",
    },
    {
        .name = "test_write",
        .handler = g32r430xx_handle_test_write,
        .mode = COMMAND_EXEC,
        .usage = "bank_id",
        .help = "test flash write",
    },
    COMMAND_REGISTRATION_DONE};

static const struct command_registration g32r430xx_command_handlers[] = {
    {
        .name = "g32r430xx",
        .mode = COMMAND_ANY,
        .help = "g32r430xx flash command group",
        .usage = "",
        .chain = g32r430xx_exec_command_handlers,
    },
    COMMAND_REGISTRATION_DONE};

const struct flash_driver g32r430_flash = {
    .name = "g32r430xx",
    .commands = g32r430xx_command_handlers,
    .flash_bank_command = g32r430xx_flash_bank_command,

    .erase = g32r430xx_erase,
    .protect = g32r430xx_protect,
    .write = g32r430xx_write,
    .read = default_flash_read,
    .probe = g32r430xx_probe,
    .auto_probe = g32r430xx_auto_probe,
    .erase_check = default_flash_blank_check,
    .protect_check = g32r430xx_protect_check,
    .info = g32r430xx_info,
};