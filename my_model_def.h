/***************************************************************************//**
 * @file my_model_def.h
 * @brief Vendor model definitions
 *******************************************************************************
 * # License
 * <b>Copyright 2022 Silicon Laboratories Inc. www.silabs.com</b>
 *******************************************************************************
 *
 * SPDX-License-Identifier: Zlib
 *
 * The licensor of this software is Silicon Laboratories Inc.
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 *
 ******************************************************************************
 * # Experimental Quality
 * This code has not been formally tested and is provided as-is. It is not
 * suitable for production environments. In addition, this code will not be
 * maintained and there may be no bug maintenance planned for these resources.
 * Silicon Labs may update projects from time to time.
 ******************************************************************************/

#ifndef MY_MODEL_DEF_H
#define MY_MODEL_DEF_H

// Company ID của nhóm
#define MY_COMPANY_ID          0x1221

// Model ID dành cho CLIENT
#define MY_CLIENT_MODEL_ID     0x2222

// Opcode riêng cho bài lab
#define OPCODE_MSSV            0x02
#define OPCODE_UPTIME          0x03
#define OPCODE_LED             0x04

// Group Address (client publish → server subscribe)
#define GROUP_ADDR_STATUS      0xC001

#endif
