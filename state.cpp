/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#pragma once

#include "pch.h"
#include "CheckpointPlugin.h"
#include "utils/customrotator.h"

static inline void readVec(std::istream& in, Vector& v) {
	readPOD(in, v.X);
	readPOD(in, v.Y);
	readPOD(in, v.Z);
}
static inline void writeVec(std::ostream& out, const Vector& v) {
	writePOD(out, v.X);
	writePOD(out, v.Y);
	writePOD(out, v.Z);
}
static inline void readRot(std::istream& in, Rotator& r) {
	readPOD(in, r.Pitch);
	readPOD(in, r.Yaw);
	readPOD(in, r.Roll);
}
static inline void writeRot(std::ostream& out, const Rotator& r) {
	writePOD(out, r.Pitch);
	writePOD(out, r.Yaw);
	writePOD(out, r.Roll);
}

ActorState::ActorState() {
	location = Vector(0, 0, 0);
	velocity = Vector(0, 0, 0);
	rotation = Rotator(0, 0, 0);
	angVelocity = Vector(0, 0, 0);
}
ActorState::ActorState(ActorWrapper a) {
	location = a.GetLocation();
	velocity = a.GetVelocity();
	rotation = a.GetRotation();
	angVelocity = a.GetAngularVelocity();
}
// Returns the object state <percent (0-1.0)> way between lh and rh.
ActorState::ActorState(ActorState lh, ActorState rh, float percent) {
	float rhPercent = 1 - percent;
	location = lh.location * percent + rh.location * rhPercent;
	velocity = lh.velocity * percent + rh.velocity * rhPercent;

	/* Custom Rotator */
	// TODO: There's a weird blip that goes slightly sideways when crossing verticle.
	// Figure out a better way to interpolate.
	CustomRotator rotator(percent);
	CustomRotator br(rh.rotation);
	CustomRotator bdiff = CustomRotator(lh.rotation).diffTo(br) * rotator;
	rotation = (br - bdiff).ToRotator();

	angVelocity = lh.angVelocity * percent + rh.angVelocity * rhPercent;
}
ActorState::ActorState(std::istream& in) {
	readVec(in, location);
	readVec(in, velocity);
	readRot(in, rotation);
	readVec(in, angVelocity);
}
void ActorState::write(std::ostream& out) const {
	writeVec(out, location);
	writeVec(out, velocity);
	writeRot(out, rotation);
	writeVec(out, angVelocity);
}
void ActorState::apply(ActorWrapper a) const {
	a.SetLocation(location);
	a.SetVelocity(velocity);
	a.SetRotation(rotation);
	a.SetAngularVelocity(angVelocity, false);
}

ActorState ActorState::mirror() const {
	ActorState as = *this;
	as.location.X *= -1;
	as.velocity.X *= -1;
	as.angVelocity.Y *= -1;
	as.angVelocity.Z *= -1;
	auto q = RotatorToQuat(rotation);
	q.Y *= -1;
	q.Z *= -1;
	Quat r(0, 0, 0, 1);
	as.rotation = QuatToRotator(q*r);
	return as;
}

CarState::CarState() {
	actorState = ActorState();
	boostAmount = 0;
	hasDodge = false;
	lastJumped = 0;
	boosting = 0;
}
CarState::CarState(CarWrapper c) {
	actorState = ActorState(c);
	// Save last jump time only if the player jumped.
	// After applying this, we will remove the player's dodge when the jump timer expires.
	lastJumped = !c.GetbJumped() || c.GetJumpComponent().IsNull() ? -1 : c.GetJumpComponent().GetInactiveTime();
	hasDodge = !c.GetbDoubleJumped() && lastJumped < MAX_DODGE_TIME;
	boostAmount = c.GetBoostComponent().IsNull() ? 0 : c.GetBoostComponent().GetCurrentBoostAmount();
	boosting = c.GetBoostComponent().IsNull() ? 0 : c.GetBoostComponent().GetbActive();
}
CarState::CarState(CarWrapper c, float lastJumpedTime) {
	actorState = ActorState(c);
	// Save last jump time only if the player jumped.
	// After applying this, we will remove the player's dodge when the jump timer expires.
	lastJumped = lastJumpedTime;
	hasDodge = !c.GetbDoubleJumped() && lastJumped < MAX_DODGE_TIME;
	boostAmount = c.GetBoostComponent().IsNull() ? 0 : c.GetBoostComponent().GetCurrentBoostAmount();
	boosting = c.GetBoostComponent().IsNull() ? 0 : c.GetBoostComponent().GetbActive();
}
// Returns the object state <percent (0-1.0)> way between lh and rh.
CarState::CarState(CarState lh, CarState rh, float percent) {
	actorState = ActorState(lh.actorState, rh.actorState, percent);
	float rhPercent = 1 - percent;
	boostAmount = lh.boostAmount * percent + rh.boostAmount * rhPercent;
	if (lh.lastJumped == -1 || rh.lastJumped == -1) {
		lastJumped = -1;
		hasDodge = true;
	} else if (rh.lastJumped < lh.lastJumped) {
		lastJumped = 0;
		hasDodge = true;
	} else { // lh.lastJumped <= rh.lastJumped
		lastJumped = lh.lastJumped * percent + rh.lastJumped * rhPercent;
		hasDodge = lastJumped < MAX_DODGE_TIME;
	}
	boosting = lh.boosting;
}

CarState::CarState(std::istream& in) {
	actorState = ActorState(in);
	readPOD(in, boostAmount);
	readPOD(in, hasDodge);
	readPOD(in, lastJumped);
	boosting = 0;
}

void CarState::write(std::ostream& out) const {
	actorState.write(out);
	writePOD(out, boostAmount);
	writePOD(out, hasDodge);
	writePOD(out, lastJumped);
}

void CarState::apply(CarWrapper c, bool showBoost) const {
	actorState.apply(c);
	if (!c.GetBoostComponent().IsNull()) {
		c.GetBoostComponent().SetCurrentBoostAmount(boostAmount);
	}
	c.SetbDoubleJumped(!hasDodge);
	c.SetbJumped(!hasDodge);
	if (!c.GetBoostComponent().IsNull()) {
		c.GetBoostComponent().SetActivityTime(0);
		c.GetBoostComponent().SetActive(showBoost && boosting);
	}
}

CarState CarState::mirror() const {
	CarState cs = *this;
	cs.actorState = cs.actorState.mirror();
	return cs;
}

GameState::GameState() {
	ball = ActorState();
	car = CarState();
	time = -1;
}

GameState::GameState(std::istream& in) {
	readVec(in, ball.location);
	readVec(in, car.actorState.location);
	readVec(in, ball.velocity);
	readVec(in, car.actorState.velocity);
	readRot(in, ball.rotation);
	readRot(in, car.actorState.rotation);
	readVec(in, ball.angVelocity);
	readVec(in, car.actorState.angVelocity);
	readPOD(in, car.boostAmount);
	readPOD(in, car.hasDodge);
	readPOD(in, car.lastJumped);
	time = -1;
}

void GameState::write(std::ostream& out) const {
	writeVec(out, ball.location);
	writeVec(out, car.actorState.location);
	writeVec(out, ball.velocity);
	writeVec(out, car.actorState.velocity);
	writeRot(out, ball.rotation);
	writeRot(out, car.actorState.rotation);
	writeVec(out, ball.angVelocity);
	writeVec(out, car.actorState.angVelocity);
	writePOD(out, car.boostAmount);
	writePOD(out, car.hasDodge);
	writePOD(out, car.lastJumped);
}

GameState::GameState(std::shared_ptr<GameWrapper> gw) {
	ServerWrapper sw = gw->GetGameEventAsServer();
	ball = ActorState(sw.GetBall());
	car = CarState(sw.GetGameCar());
	if (gw->IsInCustomTraining()) {
		time = gw->GetCurrentGameState().GetGameTimeRemaining();
	} else {
		time = -1;
	}
}

GameState::GameState(std::shared_ptr<GameWrapper> gw, float lastJumpedTime) {
	ServerWrapper sw = gw->GetGameEventAsServer();
	ball = ActorState(sw.GetBall());
	car = CarState(sw.GetGameCar(), lastJumpedTime);
	if (gw->IsInCustomTraining()) {
		time = gw->GetCurrentGameState().GetGameTimeRemaining();
	} else {
		time = -1;
	}
}

GameState::GameState(CarWrapper cw, BallWrapper bw) {
	ball = ActorState(bw);
	car = CarState(cw);
	time = -1;
}

// Returns the game state <percent (0-1.0)> way between lh and rh.
GameState::GameState(const GameState &lh, const GameState &rh, float percent) {
	ball = ActorState(lh.ball, rh.ball, percent);
	car = CarState(lh.car, rh.car, percent);
	if (lh.time != -1 && rh.time != -1) {
		time = (lh.time + rh.time) / 2;
	} else {
		time = -1;
	}
}

void GameState::apply(std::shared_ptr<GameWrapper> gw, bool showBoost) const {
	ServerWrapper sw = gw->GetGameEventAsServer();
	if (sw.GetBall().IsNull() || sw.GetGameCar().IsNull()) {
		return;
	}
	if (gw->IsInCustomTraining()) {
		if (time == -1) {
			// Don't allow loading non-CT state into CT.
			return;
		} else {
			gw->GetCurrentGameState().SetGameTimeRemaining(time);
		}
	}
	ball.apply(sw.GetBall());
	car.apply(sw.GetGameCar(), showBoost);
}

GameState GameState::mirror() const {
	GameState gs;
	gs.car = car.mirror();
	gs.ball = ball.mirror();
	return gs;
}

/*
 * base64enc and base64dec from https://stackoverflow.com/a/34571089.  No license
 * information provided.
 */

const std::string_view b64 = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

std::string base64enc(const std::string in) {
	std::string out;

	unsigned val = 0;
	int valb = -6;
	for (unsigned char c : in) {
		val = (val << 8) + c;
		valb += 8;
		while (valb >= 0) {
			out.push_back(b64[(val >> valb) & 0x3F]);
			valb -= 6;
		}
	}
	if (valb > -6) out.push_back(b64[((val << 8) >> (valb + 8)) & 0x3F]);
	while (out.size() % 4) out.push_back('=');
	return out;
}

const std::string base64dec(const std::string in) {
	std::string out;

	std::vector<int> T(256, -1);
	for (int i = 0; i < 64; i++) T[b64[i]] = i;

	unsigned val = 0;
	int valb = -8;
	for (unsigned char c : in) {
		if (T[c] == -1) break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0) {
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

GameState::GameState(const std::string enc) {
	std::string dec = base64dec(enc);
	std::istringstream stream(dec);
	readVec(stream, ball.location);
	readVec(stream, car.actorState.location);
	readVec(stream, ball.velocity);
	readVec(stream, car.actorState.velocity);
	readRot(stream, ball.rotation);
	readRot(stream, car.actorState.rotation);
	readVec(stream, ball.angVelocity);
	readVec(stream, car.actorState.angVelocity);
	readPOD(stream, car.boostAmount);
	readPOD(stream, car.hasDodge);
	readPOD(stream, car.lastJumped);
	time = -1;
}

const std::string GameState::toString() const {
	std::ostringstream dec;
	write(dec);
	return base64enc(dec.str());
}
