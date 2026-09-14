#include "aion/gameserver/dao/MailDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ParamReadStH at MailDAO.java:38 (com.aionemu.gameserver.dao.MailDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at MailDAO.java:138 (com.aionemu.gameserver.dao.MailDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at MailDAO.java:165 (com.aionemu.gameserver.dao.MailDAO$3); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at MailDAO.java:181 (com.aionemu.gameserver.dao.MailDAO$4); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at MailDAO.java:192 (com.aionemu.gameserver.dao.MailDAO$5); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at MailDAO.java:224 (com.aionemu.gameserver.dao.MailDAO$6); argument 2 of insertUpdate(); storage: sync

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.MailDAO");

runtime::Ref<model::gameobjects::player::Mailbox> MailDAO::loadPlayerMailbox(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool MailDAO::haveUnread(int32_t playerId) {
	AION_UNPORTED();
}

void MailDAO::storeMailbox(model::gameobjects::player::Player& player) {
	AION_UNPORTED();
}

bool MailDAO::storeLetter(model::gameobjects::Letter& letter) {
	AION_UNPORTED();
}

bool MailDAO::saveLetter(model::gameobjects::Letter& letter) {
	AION_UNPORTED();
}

bool MailDAO::updateLetter(model::gameobjects::Letter& letter) {
	AION_UNPORTED();
}

bool MailDAO::deleteLetter(int32_t letterId) {
	AION_UNPORTED();
}

void MailDAO::updateOfflineMailCounter(model::gameobjects::player::PlayerCommonData& recipientCommonData) {
	AION_UNPORTED();
}

std::vector<int32_t> MailDAO::getUsedIDs() {
	AION_UNPORTED();
}

bool MailDAO::cleanMail(std::string_view recipient) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
