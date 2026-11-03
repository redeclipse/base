/*
 *  Red Eclipse: the mutex type for MBEDTLS_THREADING_ALT on Windows.
 *  engine/httpnet.cpp backs it with an SRW lock through mbedtls_threading_set_alt().
 */
#ifndef REDECLIPSE_THREADING_ALT_H
#define REDECLIPSE_THREADING_ALT_H

typedef struct mbedtls_threading_mutex_t
{
    void *lock;
    char is_valid;
} mbedtls_threading_mutex_t;

#endif
