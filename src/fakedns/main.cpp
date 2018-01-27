/* fakeDNS - a fast authoritative DNS server for DNS64 testing purposes
 * Copyright (C) 2016  Daniel Bakai <bakaid@kszk.bme.hu>
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
#include "../dns.h"
#include "config.h"
#include "server.h"
#include <cstring>
#include <iostream>
#include <memory>
#include <sched.h>
#include <set>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <syslog.h>
#include <unistd.h>

std::set<pid_t> children;
Server *server;

/* Handlers for SIGTERM signal */
void parent_shutdown(int /*singal*/) {
  for (const auto &child : children) {
    kill(child, SIGTERM);
  }
}

void shutdown(int /*singal*/) {
  server->stop(); // Stopping server
}

int main() {
  pid_t pid, sid; // pid and sid for process daemonization

  /* Daemonizing process */
  pid = fork();
  if (pid < 0) {
    exit(EXIT_FAILURE);
  }
  if (pid > 0) {
    exit(EXIT_SUCCESS);
  }

  umask(0);

  openlog("fakeDNS", LOG_PID, 0); // Opening syslog connection

  sid = setsid();
  if (sid < 0) {
    exit(EXIT_FAILURE);
  }

  if (chdir("/") < 0) {
    exit(EXIT_FAILURE);
  }

  close(STDIN_FILENO);
  close(STDOUT_FILENO);
  close(STDERR_FILENO);

  /* Loading config */
  Config config;
  try {
    config.loadConfig("/etc/fakedns.conf"); // Loading server config
    /* Setting logmask to suppress verbose output when not in debugging mode
     */
    if (config.debug_) {
      setlogmask(LOG_UPTO(LOG_DEBUG));
    } else {
      setlogmask(LOG_UPTO(LOG_ERR));
    }
  } catch (std::exception &e) {
    syslog(LOG_DAEMON | LOG_ERR, "%s", e.what());
    exit(EXIT_FAILURE);
  }

  /* Getting number of available CPUs */
  long int num_cpu = sysconf(_SC_NPROCESSORS_ONLN);
  syslog(LOG_DAEMON | LOG_ERR, "Found %ld online CPUs", num_cpu);
  if (config.start_cpu_ + config.num_servers_ >= num_cpu) {
    syslog(LOG_DAEMON | LOG_ERR,
           "Invalid configuration: only %ld CPUs available, cannot schedule "
           "servers from CPU%ld to CPU%ld",
           num_cpu, config.start_cpu_, config.start_cpu_ + config.num_servers_);
    exit(EXIT_FAILURE);
  }

  /* Starting server child processes */
  syslog(LOG_DAEMON | LOG_ERR, "Starting fakeDNS...");

  int i;
  for (i = 0; i < config.num_servers_; i++) {
    pid = fork();
    if (pid < 0) {
      exit(EXIT_FAILURE);
    } else if (pid == 0) {
      break;
    } else {
      children.insert(pid);
    }
  }

  /* The main process should wait for the child processes to stop */
  if (pid > 0) {
    struct sigaction sact;
    memset(&sact, 0x00, sizeof(sact));
    sact.sa_handler = parent_shutdown;
    sact.sa_flags = 0;
    sigaction(SIGTERM, &sact,
              NULL); // Registering SIGTERM handler for clean shutdown

    while (!children.empty()) {
      pid_t child_pid = wait(NULL);
      if (child_pid != -1) {
        children.erase(child_pid);
      }
    }
    syslog(LOG_DAEMON | LOG_ERR, "Stopping fakeDNS..");
    exit(EXIT_SUCCESS);
  }

  /* Setting CPU affinity */
  cpu_set_t mask;
  CPU_ZERO(&mask);
  long int cpu = config.start_cpu_ + i;
  CPU_SET(cpu, &mask);
  if (sched_setaffinity(0, sizeof(cpu_set_t), &mask) != 0) {
    syslog(LOG_DAEMON | LOG_ERR, "Cannot set process affinity to CPU%ld", cpu);
    exit(EXIT_FAILURE);
  }

  /* Setting port for this server */
  config.start_port_ += i;

  /* Initializing RNG seed */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand(tv.tv_usec);

  /* Starting server */
  server = new Server{config}; // Creating new Server instance
  struct sigaction sact;
  memset(&sact, 0x00, sizeof(sact));
  sact.sa_handler = shutdown;
  sact.sa_flags = 0;
  sigaction(SIGTERM, &sact,
            NULL); // Registering SIGTERM handler for clean shutdown
  try {
    server->start();
  } catch (std::exception &e) {
    syslog(LOG_DAEMON | LOG_ERR, "%s", e.what());
  }
  delete server;
  closelog(); // Closing syslog connection
  exit(EXIT_SUCCESS);
}
