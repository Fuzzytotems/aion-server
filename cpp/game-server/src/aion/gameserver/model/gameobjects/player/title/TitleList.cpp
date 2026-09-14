#include "aion/gameserver/model/gameobjects/player/title/TitleList.h"

#include "aion/gameserver/runtime/base/Unported.h"
#include "aion/gameserver/model/gameobjects/player/Player.h"
#include "aion/gameserver/model/gameobjects/player/title/Title.h"

namespace aion::gameserver::model::gameobjects::player::title {

TitleList::TitleList() = default;

TitleList::TitleList(Player& partOwner) : OwnedPart(partOwner) {
}

TitleList::~TitleList() = default;

void TitleList::setOwner(Player& value) {
	if (!isOwnerBound())
		bindOwner(value);
	owner.set(&value);
}

runtime::Ptr<Player> TitleList::getOwner() const {
	return runtime::Ptr<Player>(owner.get());
}

bool TitleList::contains(int32_t titleId) {
	AION_UNPORTED();
}

void TitleList::addEntry(int32_t titleId, int32_t remaining) {
	AION_UNPORTED();
}

bool TitleList::addTitle(int32_t titleId, bool questReward, int32_t time) {
	AION_UNPORTED();
}

void TitleList::setDisplayTitle(int32_t titleId) {
	AION_UNPORTED();
}

void TitleList::setBonusTitle(int32_t bonusTitleId) {
	AION_UNPORTED();
}

void TitleList::removeTitle(int32_t titleId) {
	AION_UNPORTED();
}

int32_t TitleList::size() {
	AION_UNPORTED();
}

std::vector<runtime::Ptr<Title>> TitleList::getTitles() {
	AION_UNPORTED();
}

} // namespace aion::gameserver::model::gameobjects::player::title
