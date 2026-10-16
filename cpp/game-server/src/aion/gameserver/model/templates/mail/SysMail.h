#pragma once

#include <string>
#include <string_view>
#include <vector>

#include "aion/gameserver/model/templates/detail/FlatMap.h"
#include "aion/gameserver/model/templates/mail/SysMail.xml.h"

namespace aion::gameserver::model::templates::mail {

/**
 * Java com.aionemu.gameserver.model.templates.mail.SysMail.
 * <p>
 * C++: the @XmlTransient `mailCaseTemplates` is a C++-only map (detail::FlatMap: bound templates need noexcept moves) of template
 * pointers into the bound templates (Java nulls the list).
 *
 * @author Rolandas
 */
class SysMail : public ::aion::gameserver::runtime::StaticTemplate {
#include "aion/gameserver/model/templates/mail/SysMail.xml.inc"
private:
	/** Java @XmlTransient Map<String, List<MailTemplate>> mailCaseTemplates, keyed by the lower-case template name */
	::aion::gameserver::model::templates::detail::FlatMap<std::string, std::vector<const MailTemplate*>> mailCaseTemplates;

public:
	/** @return the first template of the event for the race or for all races, nullptr (Java null) if there is none */
	const MailTemplate* getTemplate(std::string_view eventName, Race playerRace) const;
};

} // namespace aion::gameserver::model::templates::mail
