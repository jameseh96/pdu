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
 *  bool next(BazValue& value);
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
class generator_iterator {
public:
    using iterator_category = std::input_iterator_tag;
    using value_type = ValueType;
    using difference_type = std::ptrdiff_t;
    using pointer = value_type*;
    using reference = value_type&;

    generator_iterator& operator++() {
        advance();
        return *this;
    }

    void advance() {
        if (!finished) {
            finished = !static_cast<Derived&>(*this).next(currentValue);
        }
    }

    const value_type& operator*() const {
        return currentValue;
    }

    const value_type* operator->() const {
        return &currentValue;
    }

    bool operator==(const EndSentinel& sentinel) const {
        return finished;
    }
    bool operator!=(const EndSentinel& other) const {
        return !(*this == other);
    }

protected:
    generator_iterator() = default;

private:
    value_type currentValue{};
    bool finished = false;
};

template <class Derived, class ValueType>
const Derived& begin(
        const generator_iterator<Derived, ValueType>& itr) noexcept {
    return static_cast<const Derived&>(itr);
}

template <class Derived, class ValueType>
EndSentinel end(const generator_iterator<Derived, ValueType>& itr) noexcept {
    return {};
}