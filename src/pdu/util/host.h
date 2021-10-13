#pragma once

#include <cstddef>
#include <cstdint>

template <class T>
T to_host(T);

template <>
uint8_t to_host(uint8_t v);

template <>
uint16_t to_host(uint16_t v);

template <>
uint32_t to_host(uint32_t v);

template <>
uint64_t to_host(uint64_t v);

template <class T>
T from_host(T);

template <>
uint8_t from_host(uint8_t v);

template <>
uint16_t from_host(uint16_t v);

template <>
uint32_t from_host(uint32_t v);

template <>
uint64_t from_host(uint64_t v);