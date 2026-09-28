package com.aionemu.gameserver.model.gameobjects;

/**
 * Objects that are saved to the database.
 */
public interface Persistable {

	int MAX_BATCH = 50;

	PersistentState getPersistentState();

	void setPersistentState(PersistentState state);

	default boolean isChanged() {
		return getPersistentState() != PersistentState.UPDATED;
	}

	enum PersistentState {
		NEW, UPDATED, UPDATE_REQUIRED
	}
}
