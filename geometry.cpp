/*
 * Copyright (c) 2021, Douglas Fawley
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */

#include "pch.h"
#include "CheckpointPlugin.h"
#include "utils/parser.h"

Vector deflect(Vector velocity, float dir, float speed);

GameState CheckpointPlugin::applyVariance(GameState& s) {
	int maxVar = cvarManager->getCvar("cpt_variance_tot").getIntValue();
	if (maxVar == 0) {
		return s;
	}
	float carDir = random(.0f, cvarManager->getCvar("cpt_variance_car_dir").getFloatValue());
	float carSpd = cvarManager->getCvar("cpt_variance_car_spd").getFloatValue();
	carSpd = random(-carSpd, carSpd);
	float ballDir = random(.0f, cvarManager->getCvar("cpt_variance_ball_dir").getFloatValue());
	float ballSpd = cvarManager->getCvar("cpt_variance_ball_spd").getFloatValue();
	ballSpd = random(-ballSpd, ballSpd);
	float totVar = abs(carDir) + abs(carSpd) + abs(ballDir) + abs(ballSpd);
	if (totVar < 1) {
		return s;
	}
	GameState o(s);
	if (totVar > maxVar) {
		float scale = maxVar / totVar;
		carDir *= scale;
		carSpd *= scale;
		ballDir *= scale;
		ballSpd *= scale;
	}
	if (debug) {
		log("applying variance: ball(" +
			std::to_string(ballDir) + "," + std::to_string(ballSpd) + "); car(" +
			std::to_string(carDir) + "," + std::to_string(carSpd) + "); tot: " +
			std::to_string(totVar));
	}
	o.carVelocity = deflect(o.carVelocity, carDir, 1 + (carSpd / 100.0f));
	o.ballVelocity = deflect(o.ballVelocity, ballDir, 1 + (ballSpd / 100.0f));
	return o;
}

Rot VectorToRot(Vector vVector) {
	Rot rRotation;
	rRotation.Yaw = atan2(vVector.Y, vVector.X) * CONST_RadToUnrRot;
	rRotation.Pitch = atan2(vVector.Z, sqrtf(vVector.X * vVector.X + vVector.Y * vVector.Y)) * CONST_RadToUnrRot;
	rRotation.Roll = 0;
	return rRotation;
}

Quat RotToQuat(Rot rot) {
	float rotatorToRadian = ((CONST_PI_F / 180.f) * .5f) / CONST_DegToUnrRot;
	float sinPitch = sinf(rot.Pitch * rotatorToRadian);
	float cosPitch = cosf(rot.Pitch * rotatorToRadian);
	float sinYaw = sinf(rot.Yaw * rotatorToRadian);
	float cosYaw = cosf(rot.Yaw * rotatorToRadian);
	float sinRoll = sinf(rot.Roll * rotatorToRadian);
	float cosRoll = cosf(rot.Roll * rotatorToRadian);
	Quat convertedQuat;
	convertedQuat.X = (cosRoll * sinPitch * sinYaw) - (sinRoll * cosPitch * cosYaw);
	convertedQuat.Y = (-cosRoll * sinPitch * cosYaw) - (sinRoll * cosPitch * sinYaw);
	convertedQuat.Z = (cosRoll * cosPitch * sinYaw) - (sinRoll * sinPitch * cosYaw);
	convertedQuat.W = (cosRoll * cosPitch * cosYaw) + (sinRoll * sinPitch * sinYaw);
	return convertedQuat;
}

Vector deflect(Vector velocity, float dir, float speed) {
	Quat velQ = RotToQuat(VectorToRot(velocity));
	Quat pitchQ = RotToQuat({ dir * CONST_DegToUnrRot, 0, 0 });  // Deflect dir degrees
	Quat rollQ = RotToQuat({ 0, 0, random(-32768.0f, 32764.0f) }); // Random direction
	return RotateVectorWithQuat(RotateVectorWithQuat(RotateVectorWithQuat({ velocity.magnitude() * speed, 0, 0 }, pitchQ), rollQ), velQ);
}
