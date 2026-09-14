#include "aion/gameserver/dao/BrokerDAO.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/commons/logging/LoggerFactory.h"

namespace aion::gameserver::dao {

// Anonymous classes and stored lambdas of the Java class (hub-headers.md §7.3): the bodies that create them define the structs that
// `python tools/gen/fieldmap.py --class <key>` prints.
//   anonymous ReadStH at BrokerDAO.java:31 (com.aionemu.gameserver.dao.BrokerDAO$1); argument 2 of select(); storage: sync
//   anonymous IUStH at BrokerDAO.java:100 (com.aionemu.gameserver.dao.BrokerDAO$2); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at BrokerDAO.java:123 (com.aionemu.gameserver.dao.BrokerDAO$3); argument 2 of insertUpdate(); storage: sync
//   anonymous IUStH at BrokerDAO.java:140 (com.aionemu.gameserver.dao.BrokerDAO$4); argument 2 of insertUpdate(); storage: sync

static const auto log = commons::logging::LoggerFactory::getLogger("com.aionemu.gameserver.dao.BrokerDAO");

std::vector<runtime::Ref<model::gameobjects::BrokerItem>> BrokerDAO::loadBroker() {
	AION_UNPORTED();
}

bool BrokerDAO::store(runtime::Ptr<model::gameobjects::BrokerItem> item) {
	AION_UNPORTED();
}

bool BrokerDAO::insertBrokerItem(model::gameobjects::BrokerItem& item) {
	AION_UNPORTED();
}

bool BrokerDAO::deleteBrokerItem(model::gameobjects::BrokerItem& item) {
	AION_UNPORTED();
}

bool BrokerDAO::updateBrokerItem(model::gameobjects::BrokerItem& item) {
	AION_UNPORTED();
}

} // namespace aion::gameserver::dao
