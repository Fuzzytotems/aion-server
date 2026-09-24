#pragma once

#include <cstdint>
#include <map>
#include <memory>
#include <mutex>

namespace aion::chatserver::model {
class ChatClient;
}

namespace aion::chatserver::model::message {
class Message;
}

namespace aion::chatserver::service {

/**
 * The authenticated clients, which receive the messages of their channels. Thread safe.
 * <p>
 * Java: com.aionemu.chatserver.service.BroadcastService
 *
 * @author ATracer
 */
class BroadcastService {
public:
	static BroadcastService& getInstance();

	/** Adds the client, replacing a client with the same id. */
	void addClient(std::shared_ptr<model::ChatClient> client);

	/** Removes the client with the id of the given client (like Java, also if that is another ChatClient object with the same id). */
	void removeClient(const model::ChatClient& client);

	/**
	 * Sends SM_CHANNEL_MESSAGE to every client in the message's channel (the sender included). Deviation: the clients are visited in id order
	 * (Java: ConcurrentHashMap order); if sending to one throws (a packet too big for the send buffer), the rest do not get the message, like in
	 * Java.
	 */
	void broadcastMessage(const model::message::Message& message);

	/** @throws commons::utils::IllegalStateException if the client has no connection (Java: NullPointerException) */
	void sendMessage(const model::ChatClient& chatClient, const model::message::Message& message);

private:
	BroadcastService() = default;

	/** guards clients */
	std::mutex mutex;
	std::map<int32_t, std::shared_ptr<model::ChatClient>> clients;
};

} // namespace aion::chatserver::service
