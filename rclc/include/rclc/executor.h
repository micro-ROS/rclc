// Copyright (c) 2020 - for information on the respective copyright owner
// see the NOTICE file and/or the repository https://github.com/ros2/rclc.
// Copyright 2014 Open Source Robotics Foundation, Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef RCLC__EXECUTOR_H_
#define RCLC__EXECUTOR_H_

#if __cplusplus
extern "C"
{
#endif

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>  // for NuttX sporadic scheduling

#include <rcl/error_handling.h>
#include <rcutils/logging_macros.h>

#include "rclc/executor_handle.h"
#include "rclc/types.h"

/*! \file executor.h
    \brief The RCLC-Executor provides an Executor based on RCL in which all callbacks are
    processed in a user-defined order.
*/

/* defines the semantics of data communication
   RCLCPP_EXECUTOR - same semantics as in the rclcpp Executor ROS2(Eloquent)
   LET             - logical execution time
*/
typedef enum
{
  RCLCPP_EXECUTOR,
  LET
} rclc_executor_semantics_t;

/// explicitly list here all variables the worker thread needs access to
/// handle->new_msg_cond;
/// handle->new_msg_lock;
/// handle->callback
/// handle->data
/// executor->gc;
/// executor->thread_state_mutex;
typedef struct
{
  pthread_mutex_t * thread_state_mutex;
  pthread_mutex_t * micro_ros_mutex;
  rclc_executor_handle_t * handle;
  // rcl_guard_condition_t * gc;
}
rclc_executor_worker_thread_param_t;

/// Type definition for trigger function. With the parameters:
/// - array of executor_handles
/// - size of array
/// - application specific struct used in the trigger function
typedef bool (* rclc_executor_trigger_t)(rclc_executor_handle_t *, unsigned int, void *);

/// Container for RCLC-Executor
typedef struct
{
  /// Context (to get information if ROS is up-and-running)
  rcl_context_t * context;
  /// Container for dynamic array for DDS-handles
  rclc_executor_handle_t * handles;
  /// Maximum size of array 'handles'
  size_t max_handles;
  /// Index to the next free element in array handles
  size_t index;
  /// Container to memory allocator for array handles
  const rcl_allocator_t * allocator;
  /// Wait set (is initialized only in the first call of the rclc_executor_spin_some function)
  rcl_wait_set_t wait_set;
  /// Statistics objects about total number of subscriptions, timers, clients, services, etc.
  rclc_executor_handle_counters_t info;
  /// timeout in nanoseconds for rcl_wait() used in rclc_executor_spin_once(). Default 100ms
  uint64_t timeout_ns;
  /// timepoint used for spin_period()
  rcutils_time_point_value_t invocation_time;
  /// trigger function, when to process new data
  rclc_executor_trigger_t trigger_function;
  /// application specific data structure for trigger function
  void * trigger_object;
  /// data communication semantics
  rclc_executor_semantics_t data_comm_semantics;

  /// sporadic scheduling for NuttX
  /// synchronization of worker threads to executor
  // rcl_guard_condition_t gc_some_thread_is_ready;
  pthread_mutex_t thread_state_mutex;
  pthread_mutex_t micro_ros_mutex;


} rclc_executor_t;

/**
 *  Return a rclc_executor_t struct with pointer members initialized to `NULL`
 *  and member variables to 0.
 */
rclc_executor_t
rclc_executor_get_zero_initialized_executor(void);

/**
 *  Initializes an executor.
 *  It creates a dynamic array with size \p number_of_handles using the
 *  \p allocator.
 *
 *
 *  * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param[inout] e preallocated rclc_executor_t
 * \param[in] context RCL context
 * \param[in] number_of_handles size of the handle array
 * \param[in] allocator allocator for allocating memory
 * \return `RCL_RET_OK` if the executor was initialized successfully
 * \return `RCL_RET_INVALID_ARGUMENT` if any null pointer as argument
 * \return `RCL_RET_ERROR` in case of failure
 */
rcl_ret_t
rclc_executor_init(
  rclc_executor_t * executor,
  rcl_context_t * context,
  const size_t number_of_handles,
  const rcl_allocator_t * allocator);

/**
 *  Set timeout in nanoseconds for rcl_wait (called during {@link rclc_executor_spin_once()}).
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to an initialized executor
 * \param [in] timeout_ns  timeout in nanoseconds for the rcl_wait (DDS middleware)
 * \return `RCL_RET_OK` if timeout was set successfully
 * \return `RCL_RET_INVALID_ARGUMENT` if \p executor is a null pointer
 * \return `RCL_RET_ERROR` in an error occured
 */
rcl_ret_t
rclc_executor_set_timeout(
  rclc_executor_t * executor,
  const uint64_t timeout_ns);

/**
 *  Set data communication semantics
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to an initialized executor
 * \param [in] valid semantics value as defined in enum type {@link rclc_executor_semantics_t}
 * \return `RCL_RET_OK` if semantics was set successfully
 * \return `RCL_RET_INVALID_ARGUMENT` if \p executor is a null pointer
 */
rcl_ret_t
rclc_executor_set_semantics(
  rclc_executor_t * executor,
  rclc_executor_semantics_t semantics);

/**
 *  Cleans up executor.
 *  Deallocates dynamic memory of {@link rclc_executor_t.handles} and
 *  resets all other values of {@link rclc_executor_t}.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \return `RCL_RET_OK` if reset operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if \p executor is a null pointer
 * \return `RCL_RET_INVALID_ARGUMENT` if \p executor.handles is a null pointer
 * \return `RCL_RET_ERROR` in an error occured (aka executor was not initialized)
 */
rcl_ret_t
rclc_executor_fini(rclc_executor_t * executor);

/**
 *  Adds a subscription to an executor.
 * * An error is returned, if {@link rclc_executor_t.handles} array is full.
 * * The total number_of_subscriptions field of {@link rclc_executor_t.info}
 *   is incremented by one.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] subscription pointer to an allocated subscription
 * \param [in] msg pointer to an allocated message
 * \param [in] callback    function pointer to a callback
 * \param [in] invocation  invocation type for the callback (ALWAYS or only ON_NEW_DATA)
 * \return `RCL_RET_OK` if add-operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_add_subscription(
  rclc_executor_t * executor,
  rcl_subscription_t * subscription,
  void * msg,
  rclc_callback_t callback,
  rclc_executor_handle_invocation_t invocation);


/**
 *  Adds a subscription to an executor with scheduling policy
 * * An error is returned, if {@link rclc_executor_t.handles} array is full.
 * * The total number_of_subscriptions field of {@link rclc_executor_t.info}
 *   is incremented by one.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] subscription pointer to an allocated subscription
 * \param [in] msg pointer to an allocated message
 * \param [in] callback    function pointer to a callback
 * \param [in] invocation  invocation type for the callback (ALWAYS or only ON_NEW_DATA)
 * \param [in] sched_param scheduling parameters for the thread that is executing the callback
 * \return `RCL_RET_OK` if add-operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_add_subscription_sched(
  rclc_executor_t * executor,
  rcl_subscription_t * subscription,
  void * msg,
  rclc_callback_t callback,
  rclc_executor_handle_invocation_t invocation,
  rclc_executor_sched_parameter_t * param);

/**
 *  Adds a timer to an executor.
 * * An error is returned, if {@link rclc_executor_t.handles} array is full.
 * * The total number_of_timers field of {@link rclc_executor_t.info} is
 *   incremented by one.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] timer pointer to an allocated timer
 * \return `RCL_RET_OK` if add-operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_add_timer(
  rclc_executor_t * executor,
  rcl_timer_t * timer);


/**
 *  Adds a client to an executor.
 * * An error is returned if {@link rclc_executor_t.handles} array is full.
 * * The total number_of_clients field of {@link rclc_executor_t.info}
 *   is incremented by one.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] client pointer to a allocated and initialized client
 * \param [in] request_msg type-erased ptr to an allocated request message
 * \param [in] callback    function pointer to a callback function
 * \return `RCL_RET_OK` if add-operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_add_client(
  rclc_executor_t * executor,
  rcl_client_t * client,
  void * response_msg,
  rclc_client_callback_t callback);

/**
 *  Adds a client to an executor.
 * * An error is returned if {@link rclc_executor_t.handles} array is full.
 * * The total number_of_clients field of {@link rclc_executor_t.info}
 *   is incremented by one.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] client pointer to a allocated and initialized client
 * \param [in] request_msg type-erased ptr to an allocated request message
 * \param [in] callback    function pointer to a callback function with request_id
 * \return `RCL_RET_OK` if add-operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_add_client_with_request_id(
  rclc_executor_t * executor,
  rcl_client_t * client,
  void * response_msg,
  rclc_client_callback_with_request_id_t callback);

/**
 *  Adds a service to an executor.
 * * An error is returned if {@link rclc_executor_t.handles} array is full.
 * * The total number_of_services field of {@link rclc_executor_t.info}
 *   is incremented by one.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] service pointer to an allocated and initialized service
 * \param [in] request_msg type-erased ptr to an allocated request message
 * \param [in] response_msg type-erased ptr to an allocated response message
 * \param [in] callback    function pointer to a callback function
 * \return `RCL_RET_OK` if add-operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_add_service(
  rclc_executor_t * executor,
  rcl_service_t * service,
  void * request_msg,
  void * response_msg,
  rclc_service_callback_t callback);

/**
 *  Adds a service to an executor.
 * * An error is returned if {@link rclc_executor_t.handles} array is full.
 * * The total number_of_services field of {@link rclc_executor_t.info}
 *   is incremented by one.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] service pointer to an allocated and initialized service
 * \param [in] request_msg type-erased ptr to an allocated request message
 * \param [in] response_msg type-erased ptr to an allocated response message
 * \param [in] callback    function pointer to a callback function with request_id
 * \return `RCL_RET_OK` if add-operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_add_service_with_request_id(
  rclc_executor_t * executor,
  rcl_service_t * service,
  void * request_msg,
  void * response_msg,
  rclc_service_callback_with_request_id_t callback);

/**
 *  Adds a guard_condition to an executor.
 * * An error is returned if {@link rclc_executor_t.handles} array is full.
 * * The total number_of_guard_conditions field of {@link rclc_executor_t.info}
 *   is incremented by one.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] gc pointer to an allocated and initialized guard_condition
 * \param [in] callback    function pointer to a callback function
 * \return `RCL_RET_OK` if add-operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_add_guard_condition(
  rclc_executor_t * executor,
  rcl_guard_condition_t * gc,
  rclc_gc_callback_t callback);

/**
 *  The spin-some function checks one-time for new data from the DDS-queue.
 * * the timeout is defined in {@link rclc_executor_t.timeout_ns} and can
 *   be set by calling {@link rclc_executor_set_timeout()} function (default value is 100ms)
 *
 * The static-LET executor performs the following actions:
 * * initializes the wait_set with all handle of the array executor->handles
 * * waits for new data from DDS queue with rcl_wait() with timeout executor->timeout_ns
 * * takes all ready handles from the wait_set with rcl_take()
 * * processes all handles in the order, how they were added to the executor with the respective add-functions
 *   by calling respective callback (thus implementing first-read, process, semantic of LET)
 *
 * Memory is dynamically allocated within rcl-layer, when DDS queue is accessed with rcl_wait_set_init()
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 *
 * \param [inout] executor pointer to initialized executor
 * \param[in] timeout_ns  timeout in millisonds
 * \return `RCL_RET_OK` if spin_once operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if any parameter is a null pointer
 * \return `RCL_RET_TIMEOUT` if rcl_wait() returned timeout (aka no data is avaiable during until the timeout)
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_spin_some(
  rclc_executor_t * executor,
  const uint64_t timeout_ns);

/**
 *  The spin function checks for new data at DDS queue as long as ros context is available.
 *  It calls {@link rclc_executor_spin_some()} as long as rcl_is_context_is_valid() returns true.
 *
 *  Memory is dynamically allocated within rcl-layer, when DDS queue is accessed with rcl_wait_set_init()
 *  (in spin_some function)
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 *
 * \param [inout] executor pointer to initialized executor
 * \return `RCL_RET_OK` if spin operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if executor is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_spin(rclc_executor_t * executor);

/**
 *  The spin_period function checks for new data at DDS queue as long as ros context is available.
 *  It is called every period nanoseconds.
 *  It calls {@link rclc_executor_spin_some()} as long as rcl_is_context_is_valid() returns true.
 *
 *  Memory is dynamically allocated within rcl-layer, when DDS queue is accessed with rcl_wait_set_init()
 *  (in spin_some function)
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] period in nanoseconds
 * \return `RCL_RET_OK` if spin operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if executor is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_spin_period(
  rclc_executor_t * executor,
  const uint64_t period);

/**
 * The reason for splitting up the rclc_executor_spin_period function, is only to write a
 * unit test for testing the accuracy of the period duration.
 *
 * The rclc_executor_spin_period is an endless loop, therefore it is not possible to stop
 * after x iterations. The function rclc_executor_spin_one_period implements one iteration.
 * The unit test for rclc_executor_spin_period covers only rclc_executor_spin_one_period.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | Yes
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] period in nanoseconds
 * \return `RCL_RET_OK` if spin operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if executor is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_spin_one_period(
  rclc_executor_t * executor,
  const uint64_t period);

/**
 * Set the trigger condition.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 *
 * \param [inout] executor pointer to initialized executor
 * \param [in] trigger_function function of the trigger condition
 * \param [in] trigger_object  pointer to a rcl-handle used in the trigger
 * \return `RCL_RET_OK` if spin operation was successful
 * \return `RCL_RET_INVALID_ARGUMENT` if executor is a null pointer
 * \return `RCL_RET_ERROR` if any other error occured
 */
rcl_ret_t
rclc_executor_set_trigger(
  rclc_executor_t * executor,
  rclc_executor_trigger_t trigger_function,
  void * trigger_object);

/**
 * Trigger condition: all, returns true if all handles are ready.
 *
 * Parameter obj is not used.
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [in] handles pointer to array of handles
 * \param [in] size size of array
 * \param [in] obj trigger_object set by rclc_executor_set_trigger (not used)
 * \return true - if all handles are ready (subscriptions have new data, timers are ready)
 * \return false - otherwise
 */
bool
rclc_executor_trigger_all(
  rclc_executor_handle_t * handles,
  unsigned int size,
  void * obj);

/**
 * Trigger condition: any, returns true if at least one handles is ready.
 *
 * Parameter obj is not used.
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [in] handles pointer to array of handles
 * \param [in] size size of array
 * \param [in] obj trigger_object set by rclc_executor_set_trigger (not used)
 * \return true - if at least one handles is ready (subscriptions have new data, timers are ready)
 * \return false - otherwise
 */
bool
rclc_executor_trigger_any(
  rclc_executor_handle_t * handles,
  unsigned int size,
  void * obj);

/**
 * Trigger condition: always, returns always true.
 *
 * Parameter handles, size and obj are not used.
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [in] handles pointer to array of handles (not used)
 * \param [in] size size of array (not used)
 * \param [in] obj trigger_object set by rclc_executor_set_trigger (not used)
 * \return true always
 */
bool
rclc_executor_trigger_always(
  rclc_executor_handle_t * handles,
  unsigned int size,
  void * obj);

/**
 * Trigger condition: one, returns true, if rcl handle obj is ready
 * (when obj is a subscription, if new data available,
 *  when obj is a timer, if the timer is ready)
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [in] handles pointer to array of handles (not used)
 * \param [in] size size of array (not used)
 * \param [in] obj trigger_object set by rclc_executor_set_trigger
 * \return true if rcl-handle obj is ready
 * \return false otherwise
 */
bool
rclc_executor_trigger_one(
  rclc_executor_handle_t * handles,
  unsigned int size,
  void * obj);

/**
 * Initialization of real-time scheduling with sporadic server for
 * NuttX operating system.
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to pre-allocated rclc_executor_t
 */
void
rclc_executor_real_time_scheduling_init(rclc_executor_t * e);

/**
 * Start multi-threading scheduling for NuttX
 *
 * <hr>
 * Attribute          | Adherence
 * ------------------ | -------------
 * Allocates Memory   | No
 * Thread-Safe        | No
 * Uses Atomics       | No
 * Lock-Free          | Yes
 *
 * \param [inout] executor pointer to pre-allocated rclc_executor_t
 */
rcl_ret_t
rclc_executor_start_multi_threading_for_nuttx(rclc_executor_t * e);



rcl_ret_t rclc_executor_publish(const rcl_publisher_t * publisher, const void * ros_message, 
  rmw_publisher_allocation_t * allocation);
#if __cplusplus
}
#endif

#endif  // RCLC__EXECUTOR_H_
