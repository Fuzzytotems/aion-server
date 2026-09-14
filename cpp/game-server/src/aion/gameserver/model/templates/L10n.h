#pragma once

#include <cstdint>
#include <string>

#include "aion/gameserver/model/templates/fwd.h"

namespace aion::gameserver::model::templates {

/**
 * This interface should be implemented by all templates that include a client description ID field (also known as name ID)
 * <p>
 * C++: an abstract class without data members (docs/design/hub-headers.md §9.2). The methods are const, unlike the §9.1 default: most
 * implementors are static data templates reached through `const X*` (SkillTemplate already declares `getL10nId() const`). Written with the S0b
 * objects group because Item (a hub) implements it; static data classes that implement it in Java (SkillTemplate, TitleTemplate, ...) can now
 * list it as a base.
 *
 * @author Neon
 */
class L10n {
public:
	/** @return The ID of the given client string */
	virtual int32_t getL10nId() const = 0;

	/**
	 * Java default method.
	 *
	 * @return String identifier for a client message.
	 * @see ChatUtil#l10n(int)
	 */
	virtual std::string getL10n() const;

	virtual ~L10n() = default;

protected:
	L10n() = default;
	L10n(const L10n&) = default;
	L10n& operator=(const L10n&) = default;
};

} // namespace aion::gameserver::model::templates
