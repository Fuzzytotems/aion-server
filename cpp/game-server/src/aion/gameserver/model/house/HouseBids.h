#pragma once

#include <cstdint>
#include <vector>

#include "aion/gameserver/runtime/collections/ArrayList.h"
#include "aion/gameserver/runtime/fields/Atomic.h"
#include "aion/gameserver/runtime/fields/Field.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/runtime/lifetime/RefCounted.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"
#include "aion/gameserver/model/house/fwd.h"

namespace aion::gameserver::model::house {

/**
 * S0c declaration header (docs/design/hub-headers.md §3.5). RefCounted (fieldmap K4, `House.bids`, `HousingBidService.bids`), created with
 * create(). Bid is a Java inner class that reads the outer listIndex, houseObjectId and registrationFee: the C++ Bid copies them when it is created
 * instead of referencing the outer object (no Bid -> HouseBids cycle, cycles review S0B-098), so these are C++-only const members. The
 * constructor reads HousingConfig (registration fee), so it stays `AION_UNPORTED` after the member initializers. The removed bids returned by
 * deleteOrDisableBids are held by the returned Refs.
 *
 * @author Neon
 */
class HouseBids : public runtime::RefCounted {
	AION_MAKE_REF_FRIEND
public:
	class Bid : public runtime::RefCounted {
		AION_MAKE_REF_FRIEND
		friend class HouseBids;

	private:
		runtime::Field<int32_t> playerObjectId;
		const int64_t kinah;
		const int64_t time;
		// fieldmap.toml [cpp_members]: copies of the outer HouseBids fields the inner class reads (the Java this$0 capture is dropped, S0B-098)
		const int32_t listIndex;
		// fieldmap.toml [cpp_members]: copy of HouseBids.houseObjectId
		const int32_t houseObjectId;
		// fieldmap.toml [cpp_members]: copy of HouseBids.registrationFee
		const int64_t registrationFee;

		/** Java private Bid(playerObjectId, kinah, time) of the outer bid list */
		Bid(HouseBids& outer, int32_t playerObjectId, int64_t kinah, int64_t time);

		/** Java: new Bid(playerObjectId, kinah, time) inside HouseBids */
		static runtime::Ref<Bid> create(HouseBids& outer, int32_t playerObjectId, int64_t kinah, int64_t time);

	protected:
		~Bid() override;

	public:
		int32_t getListIndex() const { return listIndex; }

		int32_t getHouseObjectId() const { return houseObjectId; }

		int32_t getPlayerObjectId() const { return playerObjectId.get(); }

		int64_t getKinah() const { return kinah; }

		int64_t getTime() const { return time; }

		int64_t calculateSalesCommission();

		int64_t calculateSaleRewardKinah();
	};

private:
	static inline runtime::AtomicInteger counter{AION_LOCK_CLASS(HouseBids::counter)};
	const int32_t listIndex;
	const int32_t houseObjectId;
	const int64_t registrationFee;
	runtime::ArrayList<runtime::Ref<HouseBids::Bid>> bids{AION_LOCK_CLASS(HouseBids::bids)};

protected:
	HouseBids(int32_t houseObjectId, int64_t initialPrice);
	HouseBids(int32_t houseObjectId, int64_t initialPrice, int64_t time);
	~HouseBids() override;

public:
	/** Java: new HouseBids(houseObjectId, initialPrice) */
	static runtime::Ref<HouseBids> create(int32_t houseObjectId, int64_t initialPrice);

	/** Java: new HouseBids(houseObjectId, initialPrice, time) */
	static runtime::Ref<HouseBids> create(int32_t houseObjectId, int64_t initialPrice, int64_t time);

	int32_t getListIndex() const { return listIndex; }

	int32_t getHouseObjectId() const { return houseObjectId; }

	/** @return the new bid, null if the bid is too low */
	runtime::Ptr<HouseBids::Bid> bid(gameobjects::player::Player& player, int64_t bidKinah);

	/** synchronized. @return the new bid, null if the bid is too low */
	runtime::Ptr<HouseBids::Bid> bid(int32_t playerObjectId, int64_t bidKinah, int64_t time);

	bool isHighestBidder(gameobjects::player::Player& player);

	/** synchronized */
	runtime::Ptr<HouseBids::Bid> getHighestBid();

	/** synchronized. @return the player's latest bid, null if there is none */
	runtime::Ptr<HouseBids::Bid> getLatestBid(gameobjects::player::Player& player);

	/** synchronized */
	runtime::Ptr<HouseBids::Bid> getInitialOffer();

	/** synchronized */
	int32_t getBidCount();

	/** synchronized. @return the removed bids (held by the returned Refs) */
	std::vector<runtime::Ref<HouseBids::Bid>> deleteOrDisableBids(int32_t playerObjectId);
};

} // namespace aion::gameserver::model::house
