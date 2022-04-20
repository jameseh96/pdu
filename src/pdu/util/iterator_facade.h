#pragma once

#include <iterator>
#include <type_traits>
#include <utility>

struct EndSentinel {};

/**
 * Iterator helper used through CRTP.
 *
 * Some deriving type:
 *
 *  class Foobar: public generator_iterator<FooBar, BazValue> {...}
 *
 * Need only implement:
 *
 * void increment();
 * const DeserialisedSeries& dereference() const;
 * bool is_end() const;
 *
 * If a new value can be generated, it should be stored in `value`
 * and then return true.
 * Otherwise, returning false marks the iterator as finished, i.e.,
 * itr == end(itr) becomes true.
 *
 * Expected to be used in range based for loops:
 *
 *  for (const auto& foo : make_some_iterator()) {...}
 *
 * begin and end are supplied as templated functions.
 *
 */
template <class Derived, class ValueType>
class iterator_facade {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    iterator_facade& operator++() {
        advance();
        return *this;
    }

    Derived& derived() {
        return static_cast<Derived&>(*this);
    }

    const Derived& derived() const {
        return static_cast<const Derived&>(*this);
    }

    void advance() {
        derived().increment();
    }

    const value_type& operator*() const {
        return derived().dereference();
    }

    const value_type* operator->() const {
        return &derived().dereference();
    }

    bool operator==(const EndSentinel& sentinel) const {
        return finished();
    }
    bool operator!=(const EndSentinel& other) const {
        return !(*this == other);
    }

    bool finished() const {
        return derived().is_end();
    }

protected:
    iterator_facade() = default;
};

template <class Derived, class ValueType>
const Derived& begin(const iterator_facade<Derived, ValueType>& itr) noexcept {
    return static_cast<const Derived&>(itr);
}

template <class Derived, class ValueType>
EndSentinel end(const iterator_facade<Derived, ValueType>& itr) noexcept {
    return {};
}