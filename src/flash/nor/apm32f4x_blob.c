/* Flash algorithm for APM32F4xx 512kB Flash
 *
 * DAPLink Interface Firmware
 * Copyright (c) 2009-2025 Arm Limited, All Rights Reserved
 * SPDX-License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *     http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

// Generated from 'APM32F4xx_512.FLM' (APM32F4xx 512kB Flash)
// digest = 922ebe337c33b4caed9c8a5fe74f77e5ba76f4fc35b964f136fd92ac3a51325b, file size = 13972
// algo version = 0x101, algo size = 332 (0x14c)
static const uint32_t APM32F4xx_512_flash_prog_blob[] = {
    0xe7fdbe00,
    0x0e000300, 0xd3022820, 0x1d000940, 0x28104770, 0x0900d302, 0x47701cc0, 0x47700880, 0x49414842,
    0x49426041, 0x21006041, 0x68c16001, 0x431122f0, 0x694060c1, 0xd4060680, 0x493d483e, 0x21066001,
    0x493d6041, 0x20006081, 0x48374770, 0x05426901, 0x61014311, 0x47702000, 0x4833b510, 0x24046901,
    0x61014321, 0x03a26901, 0x61014311, 0x4a314933, 0x6011e000, 0x03db68c3, 0x6901d4fb, 0x610143a1,
    0xbd102000, 0xf7ffb530, 0x4927ffbb, 0x23f068ca, 0x60ca431a, 0x610c2402, 0x06c0690a, 0x43020e00,
    0x6908610a, 0x431003e2, 0x48246108, 0xe0004a21, 0x68cd6010, 0xd4fb03ed, 0x43a06908, 0x68c86108,
    0x0f000600, 0x68c8d003, 0x60c84318, 0xbd302001, 0x4d15b570, 0x08891cc9, 0x008968eb, 0x433326f0,
    0x230060eb, 0x4b16612b, 0x692ce017, 0x612c431c, 0x60046814, 0x03e468ec, 0x692cd4fc, 0x00640864,
    0x68ec612c, 0x0f240624, 0x68e8d004, 0x60e84330, 0xbd702001, 0x1f091d00, 0x29001d12, 0x2000d1e5,
    0x0000bd70, 0x45670123, 0x40023c00, 0xcdef89ab, 0x00005555, 0x40003000, 0x00000fff, 0x0000aaaa,
    0x00000201, 0x00000000
};

// Start address of flash
static const uint32_t flash_start = 0x08000000;
// Size of flash
static const uint32_t flash_size = 0x00080000;

/**
* List of start and size for each size of flash sector - even indexes are start, odd are size
* The size will apply to all sectors between the listed address and the next address
* in the list.
* The last pair in the list will have sectors starting at that address and ending
* at address flash_start + flash_size.
*/
static const sector_info_t sectors_info[] = {
    {0x08000000, 0x00004000},
    {0x08010000, 0x00010000},
    {0x08020000, 0x00020000},
};

static const program_target_t flash = {
    0x20000021, // Init
    0x2000004f, // UnInit
    0x2000005d, // EraseChip
    0x20000089, // EraseSector
    0x200000d5, // ProgramPage
    0x00000000, // Verify

    // BKPT : start of blob + 1
    // RSB  : blob start + header + rw data offset
    // RSP  : stack pointer
    {
        0x20000001,
        0x20000148,
        0x20000c00
    },

    // mem buffer location
    0x20000c00,
    // location to write prog_blob in target RAM
    0x20000000,
    // prog_blob size
    sizeof(APM32F4xx_512_flash_prog_blob),
    // address of prog_blob
    APM32F4xx_512_flash_prog_blob,
    // ram_to_flash_bytes_to_be_written
    0x00000400
};
