#include "aion/gameserver/model/templates/mail/SysMail.h"

#include "aion/commons/utils/StringUtils.h"

namespace aion::gameserver::model::templates::mail {

void SysMail::afterUnmarshal(xml::LoadContext& /*ctx*/, const xml::XmlParent& /*parent*/) {
	for (const MailTemplate& mailTemplate : templates) {
		std::string caseName = commons::utils::StringUtils::toLowerCase(mailTemplate.getName());
		mailCaseTemplates[caseName].push_back(&mailTemplate);
	}
}

const MailTemplate* SysMail::getTemplate(std::string_view eventName, Race playerRace) const {
	const std::vector<const MailTemplate*>* sysTemplates = mailCaseTemplates.find(commons::utils::StringUtils::toLowerCase(eventName));
	if (sysTemplates == nullptr)
		return nullptr;
	for (const MailTemplate* mailTemplate : *sysTemplates) {
		if (mailTemplate->getRace() == playerRace || mailTemplate->getRace() == Race::PC_ALL)
			return mailTemplate;
	}
	return nullptr;
}

} // namespace aion::gameserver::model::templates::mail
