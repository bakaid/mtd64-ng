/* mtd64-ng - a lightweight multithreaded C++11 DNS64 server
 * Based on MTD64 (https://github.com/Yoso89/MTD64)
 * Copyright (C) 2015  Daniel Bakai <bakaid@kszk.bme.hu>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301,
 * USA.
 */

/** @file
 *  @brief Header for the ThreadPool and related classes.
 */

#ifndef POOL_H_INCLUDED
#define POOL_H_INCLUDED

#include <atomic>
#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <thread>
#include <vector>

class ThreadPool;

/**
 * A worker thread class for ThreadPool.
 * This functor is used to start the main loop in the threads of the pool.
 */
class WorkerThread {
private:
  ThreadPool &pool_; /**< The parent pool. */
public:
  /**
   * Constructor.
   * @param pool the parent ThreadPool
   */
  WorkerThread(ThreadPool &pool);

  /**
   * Function call operator to make this class a functor.
   * Starts the main loop.
   */
  void operator()();
};

/**
 * Main class for thread pool implementation.
 * Starts the configured number of threads, then queues and executes the tasks.
 */
class ThreadPool {
private:
  /*
   * WorkerThread uses the ThreadPool
   */
  friend class WorkerThread;
  std::vector<std::thread> threads_;            /**< The threads of the pool. */
  std::deque<std::function<void(void)>> tasks_; /**< The task queue. */
  std::mutex m_;                       /**< Mutex for the ThreadPool. */
  std::condition_variable work_to_do_; /**< Condition variable to signal
                                          avaliable tasks to sleeping workers.
                                        */
  std::atomic<bool>
      stop_; /**< Atomic variable used to thread-safely stop the pool. */
public:
  /**
   * Constructor
   * @param n the number of threads to start (default: 10)
   */
  ThreadPool(size_t n = 10);

  /**
   * Destructor.
   */
  ~ThreadPool();

  /**
   * Adds a task to the queue.
   * Using the std::function template and the move semantics enables
   * this function to efficiently add a function, functor or lambda expression
   * to the task queue.
   * @param task the task to add: a function, a functor or a lambda
   */
  void addTask(std::function<void(void)> &&task);

  /**
   * Stops the pool.
   * Waits for all running jobs to finish and shuts down the pool.
   */
  void stop();

  /**
   * Getter for the stop_ variable.
   * @return whether the pool is stopped
   */
  inline bool isStopped() const { return stop_; }

  /**
   * Getter for the number of the waiting tasks.
   * @return the number of the waiting tasks
   */
  size_t size();
};

#endif
