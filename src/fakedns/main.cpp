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
#include "../pool.h"
#include "server.h"
#include <cstring>
#include <iostream>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <syslog.h>
#include <unistd.h>

Server *server;

/* Handler for SIGTERM signal */
void shutdown(int singal) {
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

  /* Initializing RNG seed */
  struct timeval tv;
  gettimeofday(&tv, NULL);
  srand(tv.tv_usec);

  /* Starting server*/
  syslog(LOG_DAEMON | LOG_INFO, "Starting fakeDNS...");
  struct sigaction sact;
  memset(&sact, 0x00, sizeof(sact));
  sact.sa_handler = shutdown;
  sact.sa_flags = 0;
  sigaction(SIGTERM, &sact,
            NULL);     // Registering SIGTERM handler for clean shutdown
  server = new Server; // Creating new Server instance
  try {
    server->loadConfig("/etc/fakedns.conf"); // Loading server config
    /* Setting logmask to suppress verbose output when not in debugging mode */
    if (server->debug()) {
      setlogmask(LOG_UPTO(LOG_DEBUG));
    } else {
      setlogmask(LOG_UPTO(LOG_ERR));
    }
    server->start();
  } catch (std::exception &e) {
    syslog(LOG_DAEMON | LOG_ERR, "%s", e.what());
  }
  delete server;
  syslog(LOG_DAEMON | LOG_INFO, "Stopping fakeDNS..");
  closelog(); // Opening syslog connection
  exit(EXIT_SUCCESS);
}
