#pragma once

#include <cstdint>

#include "aion/gameserver/runtime/collections/ConcurrentHashMap.h"
#include "aion/gameserver/runtime/lifetime/Parts.h"
#include "aion/gameserver/runtime/lifetime/Ref.h"
#include "aion/gameserver/model/gameobjects/player/fwd.h"

namespace aion::gameserver::model::gameobjects::player {

/**
 * Manages the asking of and responding to <tt>SM_QUESTION_WINDOW</tt>
 * <p>
 * S0c declaration header (docs/design/hub-headers.md §3.5). A part of Player (`const std::unique_ptr<ResponseRequester>`), bound to the player
 * in the constructor. putRequest checks the handler for null (`Ptr`).
 *
 * @author Ben
 */
class ResponseRequester : public runtime::OwnedPart {
private:
	runtime::OwnerRef<Player> player;
	runtime::ConcurrentHashMap<int32_t, runtime::Ref<RequestResponseHandler>> activeRequests{AION_LOCK_CLASS(ResponseRequester::activeRequests#stripe)};

public:
	explicit ResponseRequester(Player& player);

	~ResponseRequester() override;

	/**
	 * Adds this handler to this messageID, returns false if there already exists one
	 *
	 * @return true or false
	 */
	bool putRequest(int32_t messageId, runtime::Ptr<RequestResponseHandler> handler);

	/**
	 * Responds to the given message ID with the given response Returns success
	 *
	 * @return Success
	 */
	bool respond(int32_t messageId, int32_t responseCode);

	/** Automatically responds 0 to all requests, passing the given player as the responder */
	void denyAll();

	bool remove(int32_t messageId);
};

} // namespace aion::gameserver::model::gameobjects::player
