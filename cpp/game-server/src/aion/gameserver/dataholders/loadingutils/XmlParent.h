#pragma once

#include <string_view>
#include <typeindex>
#include <typeinfo>

namespace aion::gameserver::xml {

/**
 * The `Object parent` argument of JAXB's afterUnmarshal(Unmarshaller, Object) (docs/design/static-data.md §3.2): the object whose element
 * encloses the one just bound. Used by 2 Java hooks (HouseAddress -> HousingLand, SpawnsData checking `parent instanceof EventTemplate`).
 *
 * `type` is the exact type the runtime bound the parent as (the concrete class for polymorphic @XmlElements entries), so `as<T>()` matches
 * the dynamic type exactly; a base class of the parent does not match. Holders get the loader's root parent (LoadContext::root(), the
 * StaticData object when DataManager loads, otherwise empty). Elements of an @XmlElementWrapper get the object that declares the wrapper.
 * A parent is only valid during the hook call.
 */
struct XmlParent {
	std::type_index type = typeid(void);
	void* object = nullptr;
	/** XML element name of the parent (empty for the root parent) */
	std::string_view elementName;

	template <class T>
	static XmlParent of(T& parent, std::string_view elementName = {}) noexcept {
		return XmlParent{typeid(T), &parent, elementName};
	}

	/** the parent if it was bound exactly as T, otherwise nullptr (Java: parent instanceof T ? (T) parent : null) */
	template <class T>
	T* as() const noexcept {
		return object != nullptr && type == typeid(T) ? static_cast<T*>(object) : nullptr;
	}

	template <class T>
	bool is() const noexcept {
		return as<T>() != nullptr;
	}

	bool empty() const noexcept { return object == nullptr; }
};

} // namespace aion::gameserver::xml
