#include "brubeck.h"
#include "getopt.h"

int main(int argc, char *argv[]) {
  static struct option longopts[] = {
      {"log", required_argument, NULL, 'l'},
      {"config", required_argument, NULL, 'c'},
      {"version", no_argument, NULL, 'v'},
      {"no-setproctitle", no_argument, NULL, 'n'},
      {NULL, 0, NULL, 0}};

  struct brubeck_server server;
  server.set_proctitle = true;
  const char *config_file = "config.default.json";
  const char *log_file = NULL;
  int opt;

  while ((opt = getopt_long(argc, argv, ":l:c:vn", longopts, NULL)) != -1) {
    switch (opt) {
    case 'l':
      log_file = optarg;
      break;
    case 'c':
      config_file = optarg;
      break;
    case 'v':
      puts("brubeck " GIT_SHA);
      return 0;
    case 'n':
      server.set_proctitle = false;
      break;
    default:
      printf("Usage: %s [--log LOG_FILE] [--config CONFIG_FILE] [--version] "
             "[--no-setproctitle] \n",
             argv[0]);
      return 1;
    }
  }

  if (server.set_proctitle)
    initproctitle(argc, argv);
  gh_log_open(log_file);
  brubeck_server_init(&server, config_file);
  return brubeck_server_run(&server);
}
