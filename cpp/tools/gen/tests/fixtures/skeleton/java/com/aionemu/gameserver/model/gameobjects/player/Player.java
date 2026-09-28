package com.aionemu.gameserver.model.gameobjects.player;

import java.util.Collection;
import java.util.List;
import java.util.Map;
import java.util.Optional;
import java.util.Set;
import java.util.concurrent.Future;
import java.util.function.Predicate;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.aionemu.gameserver.model.gameobjects.Creature;
import com.aionemu.gameserver.model.gameobjects.Persistable;
import com.aionemu.gameserver.model.templates.item.ItemTemplate;
import com.aionemu.gameserver.network.aion.AionConnection;

/**
 * A player.
 */
public final class Player extends Creature implements Persistable {

	private static final Player NOBODY = null;
	private final Logger instanceLog = LoggerFactory.getLogger(getClass());
	private PersistentState state = PersistentState.NEW;
	private AionConnection connection;
	private Map<Integer, Player> friends;
	private Creature.Stats stats;
	private int count;
	private boolean release;

	public Player(int objectId, String name) {
		super(objectId, null);
		setName(name);
	}

	@Override
	public void onDie(Creature lastAttacker) {
	}

	@Override
	public PersistentState getPersistentState() {
		return state;
	}

	@Override
	public void setPersistentState(PersistentState state) {
		this.state = state;
	}

	public AionConnection getClientConnection() {
		return connection;
	}

	public void setClientConnection(AionConnection connection) {
		this.connection = connection;
	}

	public void setCount(int count) {
		this.count = count;
	}

	public boolean isRelease() {
		return release;
	}

	public void send(List<ItemTemplate> items) {
	}

	public void send(Collection<ItemTemplate> items) {
	}

	public void register(Object listener) {
	}

	public void register() {
	}

	public void delete(int... ids) {
	}

	public Map<Integer, Player> getFriends() {
		return friends;
	}

	public Set<Integer> friendIds(byte[] data, Future<?> task) {
		return null;
	}

	public Optional<Player> findFriend(String name) {
		return Optional.empty();
	}

	public boolean removeFriends(Predicate<Player> filter) {
		return false;
	}

	public <T> T generic(Class<T> type) {
		return null;
	}
}
