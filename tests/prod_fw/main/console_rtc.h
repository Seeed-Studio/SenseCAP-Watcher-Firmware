/*
 * SPDX-FileCopyrightText: 2024 Seeed Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

#include "console_simple_init.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Registers the device command.
 *
 * @return
 *          - esp_err_t
 */
esp_err_t console_cmd_rtc_register(void);

#ifdef __cplusplus
}
#endif
