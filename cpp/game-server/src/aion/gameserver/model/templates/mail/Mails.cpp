#include "aion/gameserver/model/templates/mail/Mails.h"

#include "aion/commons/utils/StringUtils.h"

namespace aion::gameserver::model::templates::mail {

void Mails::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const SysMail& mailTemplate : sysMailTemplates) {
		std::string sysMailName = commons::utils::StringUtils::toLowerCase(mailTemplate.getName());
		sysMailByName.insertOrAssign(sysMailName, &mailTemplate);
	}
}

const MailTemplate* Mails::getMailTemplate(std::string_view name, std::string_view eventName, Race playerRace) const {
	const SysMail* const* mail = sysMailByName.find(commons::utils::StringUtils::toLowerCase(name));
	if (mail == nullptr)
		return nullptr;
	return (*mail)->getTemplate(eventName, playerRace);
}

} // namespace aion::gameserver::model::templates::mail
