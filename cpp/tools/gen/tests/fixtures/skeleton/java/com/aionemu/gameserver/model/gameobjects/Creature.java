package com.aionemu.gameserver.model.gameobjects;

import java.util.List;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.aionemu.gameserver.controllers.observer.ActionObserver;
import com.aionemu.gameserver.model.Race;

/**
 * A living object.
 */
public class Creature extends AionObject {

	private static final Logger log = LoggerFactory.getLogger(Creature.class);
	public static final int MAX_LEVEL = 65;
	private int level;
	private String name;
	private Race race;
	private final List<ActionObserver> observers;

	public Creature(int objectId, List<ActionObserver> observers) {
		super(objectId);
		this.observers = observers;
	}

	@Override
	public String getName() {
		return name;
	}

	public int getLevel() {
		return level;
	}

	public void setLevel(int level) {
		this.level = level;
	}

	public void setName(String name) {
		this.name = name;
	}

	public Race getRace() {
		return race;
	}

	/** Called when the creature spawns. Overridden by a handler. */
	public void onSpawn() {
	}

	public void onDie(Creature lastAttacker) {
	}

	public State getState() {
		return State.ALIVE;
	}

	public enum State {
		ALIVE, DEAD
	}

	public static class Stats {

		private int hp;

		public int getHp() {
			return hp;
		}
	}
}
