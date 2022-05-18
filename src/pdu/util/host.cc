#include "host.h"

template <>
uint8_t to_host(uint8_t v) {
    return v;
}

#if (defined __APPLE__)
#include <arpa/inet.h>
template <>
uint16_t to_host(uint16_t v) {
    return ntohs(v);
}

template <>
uint32_t to_host(uint32_t v) {
    return ntohl(v);
}

template <>
uint64_t to_host(uint64_t v) {
    return ntohll(v);
}
#else
#include <endian.h>

template <>
uint16_t to_host(uint16_t v) {
    return be16toh(v);
}

template <>
uint32_t to_host(uint32_t v) {
    return be32toh(v);
}

template <>
uint64_t to_host(uint64_t v) {
    return be64toh(v);
}
#endif

template <>
uint8_t from_host(uint8_t v) {
    return v;
}

#if (defined __APPLE__)
#include <arpa/inet.h>
template <>
uint16_t from_host(uint16_t v) {
    return htons(v);
}

template <>
uint32_t from_host(uint32_t v) {
    return htonl(v);
}

template <>
uint64_t from_host(uint64_t v) {
    return htonll(v);
}
#else
#include <endian.h>

template <>
uint16_t from_host(uint16_t v) {
    return htobe16(v);
}

template <>
uint32_t from_host(uint32_t v) {
    return htobe32(v);
}

template <>
uint64_t from_host(uint64_t v) {
    return htobe64(v);
}
#endif