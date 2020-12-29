#pragma once

#include "SocketIncludes.h"

typedef struct {
    char unknown0[16];
    int sock;
    void *sslContext;
} service_conn_t;

typedef service_conn_t * ServiceConnRef;
struct ServiceInfo {
  HANDLE socket;
  ServiceConnRef connection;
  int connection_id;
  const char *service_name;
};
