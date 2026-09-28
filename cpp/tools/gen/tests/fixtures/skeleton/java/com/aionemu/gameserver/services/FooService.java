package com.aionemu.gameserver.services;

import java.util.concurrent.Future;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.aionemu.gameserver.controllers.observer.ActionObserver;
import com.aionemu.gameserver.model.gameobjects.player.Player;
import com.aionemu.gameserver.network.aion.serverpackets.SM_FOO;

/**
 * Does foo.
 *
 * @author Tester
 */
public class FooService {

	private static final Logger log = LoggerFactory.getLogger(FooService.class);
	public static final int INT_VALUE = 1_000;
	public static final long LONG_VALUE = 5 * 60 * 1000L;
	public static final int MASK = 0xFFFFFFFF;
	public static final int NEGATIVE = -(1 << 4);
	public static final float RATE = 1f;
	public static final double FACTOR = .5;
	public static final boolean ENABLED = true;
	public static final char SEPARATOR = ';';
	public static final char QUOTE = '\'';
	public static final String NAME = "foo \"bar\"\né";
	private static final String NOT_LITERAL = String.valueOf(INT_VALUE);
	private volatile Future<?> task;

	private FooService() {
	}

	public void observe(Player player) {
		player.getName();
		new ActionObserver() {

			@Override
			public void moved() {
			}
		};
	}

	public void schedule(Runnable r) {
	}

	public Future<?> getTask() {
		return task;
	}

	public SM_FOO packet(int value) {
		return new SM_FOO(value);
	}

	public static FooService getInstance() {
		return SingletonHolder.instance;
	}

	private static class SingletonHolder {

		protected static final FooService instance = new FooService();
	}
}
