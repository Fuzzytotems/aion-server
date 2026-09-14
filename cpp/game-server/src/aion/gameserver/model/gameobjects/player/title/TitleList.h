#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/HashMap.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/gameobjects/player/title/fwd.h"

namespace aion::gameserver::model::gameobjects::player::title {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). A late-bound part of Player (`PartSlot<TitleList>`, cycles review): Java creates the
 * list without an owner (`new TitleList()`, DAO and Player constructor) and binds it with setOwner in Player.setTitleList. The C++-only
 * `TitleList(Player& partOwner)` binds only the part owner (the Player constructor's initial list; Java's owner stays null), and setOwner binds
 * the part owner unless it is already bound, then sets the Java owner. The owner is read as null before setOwner (addTitle checks it), which a
 * Final cannot express, so it is a non-retaining `Field<Player*>`. getOwner() returns `Ptr` (null before setOwner).
 *
 * @author xavier, cura, xTz
 */
class TitleList : public runtime::OwnedPart {
private:
	runtime::LinkedHashMap<int32_t, runtime::Ref<Title>> titles{AION_LOCK_CLASS(TitleList::titles)};
	// fieldmap: late-bound part owner that Java reads as null before setOwner (fieldmap.toml says Final<Player*>, S0B-115)
	runtime::Field<Player*> owner{nullptr};

public:
	/** Java: new TitleList() (the part owner is bound by setOwner) */
	TitleList();

	/** C++ only: a list bound to its part owner whose Java owner stays null (Player constructor) */
	explicit TitleList(Player& partOwner);

	~TitleList() override;

	/** Binds the part owner unless it is bound already (to this player), then sets the Java owner */
	void setOwner(Player& owner);

	/** @return the owner, null before setOwner */
	runtime::Ptr<Player> getOwner() const;

	bool contains(int32_t titleId);

	/** @throws IllegalArgumentException for an invalid title id */
	void addEntry(int32_t titleId, int32_t remaining);

	bool addTitle(int32_t titleId, bool questReward, int32_t time);

	void setDisplayTitle(int32_t titleId);

	void setBonusTitle(int32_t bonusTitleId);

	void removeTitle(int32_t titleId);

	int32_t size();

	std::vector<runtime::Ptr<Title>> getTitles();
};

} // namespace aion::gameserver::model::gameobjects::player::title
