#pragma once

#include <cstdint>
#include <string>
#include <string_view>

#include "aion/gameserver/model/templates/detail/FlatMap.h"
#include "aion/gameserver/model/templates/mail/Mails.xml.h"

#include "aion/gameserver/model/templates/mail/fwd.h"

namespace aion::gameserver::model::templates::mail {

/**
 * Java com.aionemu.gameserver.model.templates.mail.Mails.
 * <p>
 * C++: the @XmlTransient `sysMailByName` is a C++-only map (detail::FlatMap: bound templates need noexcept moves) of template
 * pointers into the bound system mails (Java nulls the list).
 *
 * @author Rolandas
 */
class Mails : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/mail/Mails.xml.inc"
private:
	/** Java @XmlTransient Map<String, SysMail> sysMailByName, keyed by the lower-case mail name (a later duplicate replaces an earlier one) */
	::aion::gameserver::model::templates::detail::FlatMap<std::string, const SysMail*> sysMailByName;

public:
	/** @return the template, nullptr (Java null) for an unknown mail or event */
	const MailTemplate* getMailTemplate(std::string_view name, std::string_view eventName, Race playerRace) const;

	int32_t size() const { return static_cast<int32_t>(sysMailByName.size()); }
};

} // namespace aion::gameserver::model::templates::mail
