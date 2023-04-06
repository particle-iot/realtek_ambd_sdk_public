/*
 * Copyright (c) 2021 Realtek, LLC.
 * All rights reserved.
 *
 * Licensed under the Realtek License, Version 1.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License from Realtek
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

/**
 * @addtogroup OSAL
 * @{
 *
 * @brief Declares the structures and interfaces for the Operating System Abstraction Layer (OSAL) module.
 *
 * The OSAL module provide a unified interfaces that adapter to different OS. The interfaces include the
 * memory management, thread, mutex, semaphore, time.
 *
 * @since 1.0
 * @version 1.0
 */

/**
 * @file osal_mutex.h
 *
 * @brief Declares mutex types and interfaces.
 *
 * This file provides interfaces for initializing and destroying a mutex, locking a mutex,
 * locking a mutex upon timeout, and unlocking a mutex. The mutex must be destroyed after being used.
 *
 * @since 1.0
 * @version 1.0
 */
#ifndef AMEBA_BASE_OSAL_OSAL_C_INTERFACES_OSAL_MUTEX_H
#define AMEBA_BASE_OSAL_OSAL_C_INTERFACES_OSAL_MUTEX_H

#include <stdint.h>

#include "osal_c/osal_types.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifdef __linux__
#define OSALC_MUTEX_HANLDE_SIZE 44
#else
#define OSALC_MUTEX_HANLDE_SIZE 8
#endif

/**
 * @brief Defines a mutex handle.
 *
 * @since 1.0
 * @version 1.0
 */
typedef struct OsalMutex {
	uint8_t mutex[OSALC_MUTEX_HANLDE_SIZE];  /**< Pointer to a mutex object to operate */
} __attribute__((aligned(4))) OsalMutex;

/**
 * @brief Convenient macro to define a mutex handle
 *
 * @since 1.0
 * @version 1.0
 */
#define OSAL_DECLARE_MUTEX(mutex) OsalMutex mutex

/**
 * @brief Initialize a mutex handle.
 *
 * @param mutex Indicates the pointer to the mutex handle {@link OsalMutex}.
 *
 * @return Returns a value listed below: \n
 * OSAL_STATUS | Description
 * ----------------------| -----------------------
 * OSAL_OK | The operation is successful.
 * OSAL_ERR_INVALID_PARAM | Invalid function parameter.
 * OSAL_ERR_NO_MEMORY | Memory allocation fails.
 *
 * @since 1.0
 * @version 1.0
 */
rt_status_t OsalMutexInit(OsalMutex *mutex);

/**
 * @brief Release mutex handle's resource
 *
 * @param mutex Indicates the pointer to the mutex handle {@link OsalMutex}.
 *
 * @return Returns a value listed below: \n
 * OSAL_STATUS | Description
 * ----------------------| -----------------------
 * OSAL_OK | The operation is successful.
 * OSAL_ERR_INVALID_PARAM | Invalid function parameter.
 *
 * @since 1.0
 * @version 1.0
 */
rt_status_t OsalMutexDestroy(OsalMutex *mutex);

/**
 * @brief Lock the mutex.
 *
 * @param mutex Indicates the pointer to the mutex handle {@link OsalMutex}.
 *
 * @return Returns a value listed below: \n
 * OSAL_STATUS | Description
 * ----------------------| -----------------------
 * OSAL_OK | The operation is successful.
 * OSAL_ERR_INVALID_PARAM | Invalid function parameter.
 * Other non-zero number | The operation is failed.
 *
 * @since 1.0
 * @version 1.0
 */
rt_status_t OsalMutexLock(OsalMutex *mutex);

/**
 * @brief Try to lock the mutex. It returns is the mutex is in locked state.
 *
 * @param mutex Indicates the pointer to the mutex handle {@link OsalMutex}.
 *
 * @return Returns a value listed below: \n
 * OSAL_STATUS | Description
 * ----------------------| -----------------------
 * OSAL_OK | The operation is successful.
 * OSAL_ERR_INVALID_PARAM | Invalid function parameter.
 * Other non-zero number | The operation is failed.
 *
 * @since 1.0
 * @version 1.0
 */
rt_status_t OsalMutexTrylock(OsalMutex *mutex);

/**
 * @brief Unlock the mutex.
 *
 * @param mutex Indicates the pointer to the mutex handle {@link OsalMutex}.
 *
 * @return Returns a value listed below: \n
 * OSAL_STATUS | Description
 * ----------------------| -----------------------
 * OSAL_OK | The operation is successful.
 * OSAL_ERR_INVALID_PARAM | Invalid function parameter.
 * Other non-zero number | The operation is failed.
 *
 * @since 1.0
 * @version 1.0
 */
rt_status_t OsalMutexUnlock(OsalMutex *mutex);

#ifdef __cplusplus
}
#endif

#endif // AMEBA_BASE_OSAL_OSAL_C_INTERFACES_OSAL_MUTEX_H
/** @} */
