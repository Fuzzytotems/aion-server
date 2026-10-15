#pragma once

#include <iterator>
#include <utility>

#include "aion/commons/utils/Rnd.h"
#include "aion/gameserver/model/fwd.h"

namespace aion::gameserver::model {

/**
 * Any class implementing this interface will enable calling {@link #selectElement(Iterable)} on collections of such elements.
 * <p>
 * C++: an interface (hub-headers.md §9.2) implemented mostly by static data templates, so getChance is const (§9.1). selectElement is a static
 * member function template over a container of nullable element references (`const X*` templates, `Ptr`/`Ref` objects) and returns the
 * selected element, or a null (value-initialized) element like Java's null.
 *
 * @author Neon
 */
class Chance {
public:
	virtual float getChance() const = 0;

	/** @return Random element selected by its chance. */
	template <class Container>
	static typename Container::value_type selectElement(Container& elements) {
		return selectElement(elements, false);
	}

	/** @return Random element selected by its chance. The item will be removed from the input elements if {@code remove} is true. */
	template <class Container>
	static typename Container::value_type selectElement(Container& elements, bool remove) {
		float sumOfChances = 0;
		for (const auto& element : elements)
			sumOfChances += element->getChance();
		if (sumOfChances > 0) {
			float randomChance = commons::utils::Rnd::nextFloat(sumOfChances);
			float luck = 0;
			for (auto iterator = std::begin(elements); iterator != std::end(elements); ++iterator) {
				luck += (*iterator)->getChance();
				if (randomChance <= luck) {
					typename Container::value_type element = *iterator;
					if (remove)
						elements.erase(iterator);
					return element;
				}
			}
		}
		return typename Container::value_type{};
	}

	virtual ~Chance() = default;

protected:
	Chance() = default;
	Chance(const Chance&) = default;
	Chance& operator=(const Chance&) = default;
};

} // namespace aion::gameserver::model
