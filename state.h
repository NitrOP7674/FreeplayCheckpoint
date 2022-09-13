/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"

class ActorState {
public:
	Vector location;
	Vector velocity;
	Rotator rotation;
	Vector angVelocity;

	ActorState();
	ActorState(ActorWrapper a);
	ActorState(ActorState lh, ActorState rh, float percent);
	ActorState(std::istream& in);

	void write(std::ostream& out) const;
	void apply(ActorWrapper a) const;
	ActorState mirror() const;
};

class CarState {
public:
	ActorState actorState;
	float boostAmount;
	bool hasDodge;
	float lastJumped; // cannot apply; used to reset dodge in record().
	long boosting;

	CarState();
	CarState(CarWrapper c);
	CarState(CarWrapper c, float lastJumpedTime);
	CarState(CarState lh, CarState rh, float percent);
	CarState(std::istream& in);

	void write(std::ostream& out) const;
	void apply(CarWrapper c, bool showBoost) const;
	CarState mirror() const;
};

class GameState {
public:
	ActorState ball;
	CarState car;
	float time; // -1 if not in a timed mode

	GameState();
	GameState(std::shared_ptr<GameWrapper> gw);
	GameState(std::shared_ptr<GameWrapper> gw, float lastJumpedTime);
	GameState(CarWrapper cw, BallWrapper bw);
	GameState(const GameState& lh, const GameState& rh, float percent);
	GameState(std::istream& in);
	GameState(std::string str);

	void write(std::ostream& out) const;
	void apply(std::shared_ptr<GameWrapper> gw, bool showBoost) const;
	const std::string toString() const;
	GameState mirror() const;
};
