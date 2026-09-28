#pragma once

#include <cstdint>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <string>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"

namespace aion::gameserver::runtime {

/**
 * Java array of a shared class (design §3.2: `final T[]` → `const Ref<Array<T'>>`, non-final → `Field<Ref<Array<T'>>>`; §3.3, J2).
 * A heap object with a fixed length and one Field<T> slot per element, so element reads and writes are torn-free and reference elements
 * (`Array<FutureRef>`, `Array<Ref<Npc>>`) use the Field<Ref> read barrier. Elements are value-initialized (0, false, null).
 *
 * - `(*array)[i]` returns the slot (Field<T>&): `(*periodicTasks)[i] = std::move(task);`, `if ((*periodicTasks)[i]) ...`.
 * - Index checks throw ArrayIndexOutOfBoundsException (Java message format).
 * - Range-for yields Borrowed<T> read slot by slot at iteration time (`for (Ptr<Future> task : *periodicTasks)`), like Java's enhanced for.
 * Thread-safety: slot operations are thread-safe (Field semantics); length() is immutable.
 */
template <class T>
class Array final : public RefCounted {
	AION_MAKE_REF_FRIEND

public:
	using value_type = T;

	/** Java `new T[length]`. @throws IllegalArgumentException (Java NegativeArraySizeException) if length < 0 */
	static Ref<Array> make(int32_t length) { return makeRef<Array>(length); }
	/** Java `new T[] { values... }` */
	static Ref<Array> of(std::initializer_list<T> values) {
		Ref<Array> array = makeRef<Array>(static_cast<int32_t>(values.size()));
		int32_t index = 0;
		for (const T& value : values)
			array->slots_[static_cast<size_t>(index++)] = value;
		return array;
	}

	int32_t length() const noexcept { return length_; }

	/** @throws ArrayIndexOutOfBoundsException */
	Field<T>& operator[](int32_t index) { return slots_[checkIndex(index)]; }
	/** @throws ArrayIndexOutOfBoundsException */
	const Field<T>& operator[](int32_t index) const { return slots_[checkIndex(index)]; }

	/** @throws ArrayIndexOutOfBoundsException */
	Borrowed<T> get(int32_t index) const { return slots_[checkIndex(index)].get(); }

	/** Reads every slot once, in order. */
	std::vector<Borrowed<T>> snapshot() const {
		std::vector<Borrowed<T>> values;
		values.reserve(static_cast<size_t>(length_));
		for (int32_t i = 0; i < length_; ++i)
			values.push_back(slots_[static_cast<size_t>(i)].get());
		return values;
	}

	class Iterator {
	public:
		using value_type = Borrowed<T>;
		using difference_type = std::ptrdiff_t;
		Iterator() = default;
		Iterator(const Array* array, int32_t index) : array(array), index(index) {}
		Borrowed<T> operator*() const { return array->slots_[static_cast<size_t>(index)].get(); }
		Iterator& operator++() {
			++index;
			return *this;
		}
		Iterator operator++(int) {
			Iterator old = *this;
			++index;
			return old;
		}
		bool operator==(const Iterator& other) const = default;

	private:
		const Array* array = nullptr;
		int32_t index = 0;
	};

	Iterator begin() const { return Iterator(this, 0); }
	Iterator end() const { return Iterator(this, length_); }

protected:
	explicit Array(int32_t length) : length_(checkLength(length)), slots_(std::make_unique<Field<T>[]>(static_cast<size_t>(length_))) {}
	~Array() override = default;

private:
	static int32_t checkLength(int32_t length) {
		if (length < 0)
			throw IllegalArgumentException("Negative array size: " + std::to_string(length));
		return length;
	}
	size_t checkIndex(int32_t index) const {
		if (index < 0 || index >= length_) [[unlikely]]
			throw ArrayIndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(length_));
		return static_cast<size_t>(index);
	}

	const int32_t length_;
	const std::unique_ptr<Field<T>[]> slots_;
};

} // namespace aion::gameserver::runtime
