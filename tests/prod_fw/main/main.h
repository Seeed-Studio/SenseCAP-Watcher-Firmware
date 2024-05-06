/*
 * SPDX-FileCopyrightText: 2024 Seeed Technology Co., Ltd.
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#pragma once

/** Major version number (X.x.x) */
#define PRODUCT_FW_VERSION_MAJOR 0
/** Minor version number (x.X.x) */
#define PRODUCT_FW_VERSION_MINOR 0
/** Patch version number (x.x.X) */
#define PRODUCT_FW_VERSION_PATCH 1

/**
 * Macro to convert version number into an integer
 *
 * To be used in comparisons, such as PRODUCT_FW_VERSION >= PRODUCT_FW_VERSION_VAL(4, 0, 0)
 */
#define PRODUCT_FW_VERSION_VAL(major, minor, patch) ((major << 16) | (minor << 8) | (patch))

/**
 * Current version, as an integer
 *
 * To be used in comparisons, such as PRODUCT_FW_VERSION >= PRODUCT_FW_VERSION_VAL(4, 0, 0)
 */
#define PRODUCT_FW_VERSION PRODUCT_FW_VERSION_VAL(PRODUCT_FW_VERSION_MAJOR, \
                                                PRODUCT_FW_VERSION_MINOR, \
                                                PRODUCT_FW_VERSION_PATCH)

