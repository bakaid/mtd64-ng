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

#include "pool.h"
#include <iostream>

WorkerThread::WorkerThread(ThreadPool &pool) : pool_{pool} {}

void WorkerThread::operator()() {
  while (1) {
    std::unique_lock<std::mutex> lock{pool_.m_};
    while (!pool_.stop_ && pool_.tasks_.size() == 0)
      pool_.work_to_do_.wait(lock);
    if (pool_.stop_)
      break;
    std::function<void(void)> task = std::move(pool_.tasks_.front());
    pool_.tasks_.pop_front();
    lock.unlock();
    task();
  }
}

ThreadPool::ThreadPool(size_t n) : stop_{false} {
  for (int i = 0; i < n; i++) {
    threads_.push_back(std::thread{WorkerThread{*this}});
  }
}

ThreadPool::~ThreadPool() {}

void ThreadPool::addTask(std::function<void(void)> &&task) {
  std::unique_lock<std::mutex> lock{m_};
  tasks_.push_back(std::move(task));
  lock.unlock();
  work_to_do_.notify_one();
}

void ThreadPool::stop() {
  stop_ = true;
  work_to_do_.notify_all();
  for (auto &thread : threads_) {
    thread.join();
  }
}

size_t ThreadPool::size() {
  std::unique_lock<std::mutex> lock{m_};
  return tasks_.size();
}
