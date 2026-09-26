#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "aion/gameserver/runtime/base/Exceptions.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/utils/collections/fwd.h"

namespace aion::gameserver::utils::collections {

/**
 * C++ only: how SplitList and ListPart hold an element of Java type `Type`: `runtime::Ref<Type>` for retainable game objects (the lists split
 * items, letters, quest states, macros, ... that Java keeps alive while a packet is built), the value itself otherwise.
 */
template <class Type>
using SplitElement = std::conditional_t<runtime::Retainable<Type>, runtime::Ref<Type>, Type>;

/**
 * Java: ListPart<Type> extends ArrayList<Type>, one partition of a SplitList.
 * <p>
 * C++: a class template (unbounded utility generic, hub-headers.md §8.1) that holds its elements in a std::vector of SplitElement<Type> and
 * offers the ArrayList operations its users call (add, get, size, isEmpty, iteration); `borrowed()` gives the `std::vector<Ptr<Type>>` the
 * packet constructors take. Confined to the task that splits the list (fieldmap K5).
 *
 * @author Neon
 */
template <class Type>
class ListPart {
public:
	using Element = SplitElement<Type>;

private:
	const int32_t partNo;
	bool isLast_;
	std::vector<Element> elements;

protected:
	ListPart(int32_t partNoValue, bool isLastValue) : partNo(partNoValue), isLast_(isLastValue) {}

public:
	virtual ~ListPart() = default;
	ListPart(const ListPart&) = delete;
	ListPart& operator=(const ListPart&) = delete;

	int32_t getPartNo() const { return partNo; }

	bool isFirst() const { return partNo == 1; }

	bool isLast() const { return isLast_; }

protected:
	void setLast(bool value) { this->isLast_ = value; }

public:
	/** Java: ArrayList.add(E) - always true for the plain list; DynamicElementCountListPart also counts the element length */
	virtual bool add(const Element& type) {
		elements.push_back(type);
		return true;
	}

	/** Java: ArrayList.size() */
	int32_t size() const { return static_cast<int32_t>(elements.size()); }

	/** Java: ArrayList.isEmpty() */
	bool isEmpty() const { return elements.empty(); }

	/** Java: ArrayList.get(int) @throws IndexOutOfBoundsException */
	const Element& get(int32_t index) const {
		if (index < 0 || index >= size())
			throw runtime::IndexOutOfBoundsException("Index " + std::to_string(index) + " out of bounds for length " + std::to_string(size()));
		return elements[static_cast<size_t>(index)];
	}

	typename std::vector<Element>::const_iterator begin() const { return elements.begin(); }
	typename std::vector<Element>::const_iterator end() const { return elements.end(); }

	/** C++ only: the elements themselves */
	const std::vector<Element>& getElements() const { return elements; }

	/** C++ only: the elements as borrowed references, the collection parameter kind of the packet constructors (hub-headers.md §7.1) */
	std::vector<runtime::Ptr<Type>> borrowed() const
		requires runtime::Retainable<Type>
	{
		std::vector<runtime::Ptr<Type>> result;
		result.reserve(elements.size());
		for (const Element& element : elements)
			result.emplace_back(element);
		return result;
	}

protected:
	virtual bool fits(const Element& element) = 0;

	template <class T>
	friend class SplitList;
};

} // namespace aion::gameserver::utils::collections
