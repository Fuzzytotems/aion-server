#pragma once

#include <cstdint>
#include <optional>
#include <vector>

#include "aion/gameserver/runtime/collections/Rc.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/commons/database/SqlTypes.h"
#include "aion/gameserver/model/account/fwd.h"
#include "aion/gameserver/model/gameobjects/fwd.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::account {

/**
 * This class is holding information about player, that is displayed on char selection screen, such as: player commondata, player's appearance and
 * creation/deletion time.
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Account (`PartMap` players, RR-19): the C++ constructors take the owning
 * account first (Java creates the object before `account.addPlayerAccountData(data)`, which then takes the `std::unique_ptr`). Refs to it
 * (`Player.playerAccountData`) retain the account. The constructors are ported; their last statement, updateBoundingRadius(), interns the
 * run-time BoundRadius (BoundRadius::intern: immortal like the static data templates PlayerCommonData.boundRadius points to).
 * The Java visible item list parameter is stored: `std::vector<Ref<VisibleItem>>` by value (§7.1). creationDate and deletionDate are
 * nullable (AccountService.java:56 compares deletionDate with null; creationDate is null until CM_CREATE_CHARACTER sets it, and
 * Player::getCreationDate returns `std::optional`).
 *
 * @see PlayerCommonData
 * @see PlayerAppearance
 * @author Luno
 */
class PlayerAccountData : public runtime::OwnedPart {
public:
	/** Java record VisibleItem(byte slotType, int itemId, int godStoneId, Integer color) */
	class VisibleItem : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
	private:
		const int8_t slotType_;
		const int32_t itemId_;
		const int32_t godStoneId_;
		const std::optional<int32_t> color_;

	protected:
		VisibleItem(int8_t slotType, int32_t itemId, int32_t godStoneId, std::optional<int32_t> color);
		~VisibleItem() override;

	public:
		/** Java: new VisibleItem(slotType, itemId, godStoneId, color) (canonical record constructor) */
		static runtime::Ref<VisibleItem> create(int8_t slotType, int32_t itemId, int32_t godStoneId, std::optional<int32_t> color);

		int8_t slotType() const { return slotType_; }

		int32_t itemId() const { return itemId_; }

		int32_t godStoneId() const { return godStoneId_; }

		std::optional<int32_t> color() const { return color_; }

		/** Java record equals (all components) */
		bool equals(const VisibleItem& obj) const;

		/** Java record hashCode */
		int32_t hashCode() const;
	};

private:
	const runtime::Ref<gameobjects::player::PlayerCommonData> playerCommonData;
	runtime::Field<runtime::Ref<gameobjects::player::PlayerAppearance>> appearance{};
	runtime::Field<runtime::Ref<CharacterBanInfo>> cbi{};
	runtime::Field<runtime::Ref<runtime::RcArrayList<runtime::Ref<PlayerAccountData::VisibleItem>>>> visibleItems{};
	// fieldmap.toml: null until setCreationDate (CM_CREATE_CHARACTER.java:71), Player::getCreationDate returns std::optional (hub-headers.md §6)
	runtime::Field<std::optional<commons::database::Timestamp>> creationDate{};
	// fieldmap.toml: Java compares it with null (AccountService.java:56), hub-headers.md §6
	runtime::Field<std::optional<commons::database::Timestamp>> deletionDate{};

public:
	/** Java: new PlayerAccountData(playerCommonData, appearance), then account.addPlayerAccountData(...) */
	PlayerAccountData(Account& account, gameobjects::player::PlayerCommonData& playerCommonData, gameobjects::player::PlayerAppearance& appearance);

	/** @param cbi null if the character is not banned */
	PlayerAccountData(Account& account, gameobjects::player::PlayerCommonData& playerCommonData, gameobjects::player::PlayerAppearance& appearance,
		runtime::Ptr<CharacterBanInfo> cbi, std::vector<runtime::Ref<PlayerAccountData::VisibleItem>> visibleItems);

	~PlayerAccountData() override;

	runtime::Ptr<CharacterBanInfo> getCharBanInfo() const { return cbi.get(); }

	void setCharBanInfo(runtime::Ptr<CharacterBanInfo> cbi);

	std::optional<commons::database::Timestamp> getCreationDate() const { return creationDate.get(); }

	/** Sets deletion date. */
	void setDeletionDate(std::optional<commons::database::Timestamp> value) { deletionDate.set(value); }

	/**
	 * Get deletion date.
	 *
	 * @return Timestamp date when char should be deleted.
	 */
	std::optional<commons::database::Timestamp> getDeletionDate() const { return deletionDate.get(); }

	/**
	 * Get time in seconds when this player will be deleted ( 0 if player was not set to be deleted )
	 *
	 * @return deletion time in seconds
	 */
	int32_t getDeletionTimeInSeconds();

	/** @return the playerCommonData */
	runtime::Ptr<gameobjects::player::PlayerCommonData> getPlayerCommonData() const { return playerCommonData; }

	runtime::Ptr<gameobjects::player::PlayerAppearance> getAppearance() const { return appearance.get(); }

	void setAppearance(gameobjects::player::PlayerAppearance& appearance);

	void updateBoundingRadius();

	void setCreationDate(std::optional<commons::database::Timestamp> value) { creationDate.set(value); }

	/** @return the live visible item list */
	runtime::Ptr<runtime::RcArrayList<runtime::Ref<PlayerAccountData::VisibleItem>>> getVisibleItems() const { return visibleItems.get(); }

	void setVisibleItems(const std::vector<runtime::Ptr<gameobjects::Item>>& equipment);
};

} // namespace aion::gameserver::model::account
