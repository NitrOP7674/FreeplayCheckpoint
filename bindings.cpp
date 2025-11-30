/*
 * Copyright (c) 2021
 * All rights reserved.
 *
 * This source code is licensed under the MIT-style license found in the
 * LICENSE file in the root directory of this source tree.
 */
#include "pch.h"
#include "CheckpointPlugin.h"
#include "utils/parser.h"
#include <functional>

#include "bindings.h"

using namespace std::placeholders;

KEYBIND_ASSIGNWHICH which_is_being_bound = KEYBIND_ASSIGNWHICH::NONE;

void CheckpointPlugin::removeBindKeys(std::vector<std::string> params) {
	removeBind(cvarManager->getCvar("cpt_freeze_key").getStringValue(), "cpt_freeze");
	removeBind(cvarManager->getCvar("cpt_do_checkpoint_key").getStringValue(), "cpt_do_checkpoint");
	removeBind(cvarManager->getCvar("cpt_prev_checkpoint_key").getStringValue(), "cpt_prev_checkpoint");
	removeBind(cvarManager->getCvar("cpt_next_checkpoint_key").getStringValue(), "cpt_next_checkpoint");
	removeBind(cvarManager->getCvar("cpt_freeze_ball_key").getStringValue(), "cpt_freeze_ball");
	removeBind(cvarManager->getCvar("cpt_mirror_state_key").getStringValue(), "cpt_mirror_state");
}

void CheckpointPlugin::applyBindKeys(std::vector<std::string> params) {
	addBind(cvarManager->getCvar("cpt_freeze_key").getStringValue(), "cpt_freeze");
	addBind(cvarManager->getCvar("cpt_do_checkpoint_key").getStringValue(), "cpt_do_checkpoint");
	addBind(cvarManager->getCvar("cpt_prev_checkpoint_key").getStringValue(), "cpt_prev_checkpoint");
	addBind(cvarManager->getCvar("cpt_next_checkpoint_key").getStringValue(), "cpt_next_checkpoint");
	addBind(cvarManager->getCvar("cpt_freeze_ball_key").getStringValue(), "cpt_freeze_ball");
	addBind(cvarManager->getCvar("cpt_mirror_state_key").getStringValue(), "cpt_mirror_state");
}

void CheckpointPlugin::resetDefaultBindKeys(std::vector<std::string> params) {
	cvarManager->getCvar("cpt_freeze_key").setValue("XboxTypeS_RightThumbStick");
	cvarManager->getCvar("cpt_do_checkpoint_key").setValue("XboxTypeS_Back");
	cvarManager->getCvar("cpt_prev_checkpoint_key").setValue("XboxTypeS_DPad_Left");
	cvarManager->getCvar("cpt_next_checkpoint_key").setValue("XboxTypeS_DPad_Right");
	cvarManager->getCvar("cpt_freeze_ball_key").setValue("XboxTypeS_DPad_Up");
	cvarManager->getCvar("cpt_mirror_state_key").setValue("XboxTypeS_DPad_Down");
    cvarManager->getCvar("cpt_freeze_ball_key").setValue("XboxTypeS_DPad_Up");
    cvarManager->getCvar("cpt_rewind_key").setValue("XboxTypeS_LeftX-");
    cvarManager->getCvar("cpt_fastforward_key").setValue("XboxTypeS_LeftX+");
}

void CheckpointPlugin::registerBindingCVars() {
	cvarManager->registerCvar("cpt_freeze_key", "XboxTypeS_RightThumbStick", "Key to bind cpt_freeze to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_do_checkpoint_key", "XboxTypeS_Back", "Key to bind cpt_do_checkpoint to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_prev_checkpoint_key", "XboxTypeS_DPad_Left", "Key to bind cpt_prev_checkpoint to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_next_checkpoint_key", "XboxTypeS_DPad_Right", "Key to bind cpt_next_checkpoint to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_freeze_ball_key", "XboxTypeS_DPad_Up", "Key to bind cpt_freeze_ball to on cpt_apply_bindings");
	cvarManager->registerCvar("cpt_mirror_state_key", "XboxTypeS_DPad_Down", "Key to bind cpt_mirror_state to on cpt_apply_bindings");
    cvarManager->registerCvar("cpt_rewind_key", "XboxTypeS_LeftX-", "Key to bind to cpt_rewind on cpt_apply_bindings");
    cvarManager->registerCvar("cpt_fastforward_key", "XboxTypeS_LeftX+", "Key to bind to cpt_fastforward on cpt_apply_bindings");
	cvarManager->registerNotifier("cpt_remove_bindings", bind(&CheckpointPlugin::removeBindKeys, this, _1),
		"Removes the configured button bindings for the Freeplay Checkpoint plugin", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_apply_bindings", bind(&CheckpointPlugin::applyBindKeys, this, _1),
		"Applys the configured button bindings for the Freeplay Checkpoint plugin", PERMISSION_ALL);
	cvarManager->registerNotifier("cpt_reset_default_bindings", bind(&CheckpointPlugin::resetDefaultBindKeys, this, _1),
		"Resets bindings to the default values", PERMISSION_ALL);
}

void CheckpointPlugin::addBind(std::string key, std::string cmd) {
	std::string old = cvarManager->getBindStringForKey(key);
	std::vector<std::string> cmds;

	for (char* token = strtok(const_cast<char*>(old.c_str()), ";");
		token != nullptr;
		token = strtok(nullptr, ";"))
	{
		auto tok = std::string(token);
		trim(tok);
		if (tok != "") {
			cmds.push_back(std::string(tok));
		}
	}
	if (std::find(cmds.begin(), cmds.end(), cmd) == cmds.end()) {
		cmds.push_back(trim(cmd));
	}
	std::stringstream s;
	std::copy(cmds.begin(), cmds.end() - 1, std::ostream_iterator<std::string>(s, ";"));
	s << cmds.back();
	cvarManager->setBind(key, s.str());
}

void CheckpointPlugin::removeBind(std::string key, std::string cmd) {
	std::string old = cvarManager->getBindStringForKey(key);
	std::vector<std::string> cmds;
	log("removing " + cmd + " from " + key);
	for (char* token = strtok(const_cast<char*>(old.c_str()), ";");
		token != nullptr;
		token = strtok(nullptr, ";"))
	{
		auto tok = std::string(token);
		trim(tok);
		if (tok != "" && tok != cmd) {
			log("pushing " + tok);
			cmds.push_back(std::string(tok));
		}
	}
	if (cmds.size() == 0) {
		cvarManager->executeCommand("unbind " + key);
		return;
	}
	std::stringstream s;
	std::copy(cmds.begin(), cmds.end() - 1, std::ostream_iterator<std::string>(s, ";"));
	s << cmds.back();
	log("setting " + key + " to " + s.str());
	cvarManager->setBind(key, s.str());
}

 void OnKeyPressed(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    ActorWrapper aw,
    void* params,
    std::string eventName) {
    s* p = reinterpret_cast<s*>(params);
    if (p->t.e != 0) { return; }

    ImGui::CloseCurrentPopup();
    cvarManager->executeCommand("closemenu checkpointplugin", false);
    gameWrapper->UnhookEvent("Function TAGame.GameViewportClient_TA.HandleKeyPress");
    gameWrapper->UnhookEvent("Function TAGame.GameViewportClient_TA.HandleAxisPress");
    std::string key = gameWrapper->GetFNameByIndex(p->k.i);

    // this is because of mem-access issues ... even though they may still exist
    gameWrapper->Execute([=, et = p->t.e](GameWrapper* gw) {
        cvarManager->getCvar(keys_to_cvars.at(which_is_being_bound)).setValue(key);
        which_is_being_bound = KEYBIND_ASSIGNWHICH::NONE;
        });
}

void OnKeyAxisInput(
    std::shared_ptr<CVarManagerWrapper> cvarManager,
    std::shared_ptr<GameWrapper> gameWrapper,
    ActorWrapper aw,
    void* params,
    std::string eventName) {
    s* p = reinterpret_cast<s*>(params);

    // this is done in ways to emulate how the game does it
    if (std::fabs(p->t.d) <= 0.8) {
        // commit to a direction, lol
        return;
    }

    std::string key = gameWrapper->GetFNameByIndex(p->k.i);
    if (key.contains("Mouse") && std::abs(p->t.d) < 3) {
        // a delta of 3 seems to be the threshhold for a mouse axis input
        return;
    }

    ImGui::CloseCurrentPopup();
    cvarManager->executeCommand("closemenu checkpointplugin", false);
    gameWrapper->UnhookEvent("Function TAGame.GameViewportClient_TA.HandleAxisPress");
    gameWrapper->UnhookEvent("Function TAGame.GameViewportClient_TA.HandleKeyPress");

    // this is because of mem-access issues ... even though they may still exist
    gameWrapper->Execute([=, d = p->t.d](GameWrapper* gw) {
        if (key == "XboxTypeS_LeftTriggerAxis" || key == "XboxTypeS_RightTriggerAxis") {
            // this doesn't have a positive/negative direction, so it's not 2 inputs in one
            cvarManager->getCvar(keys_to_cvars.at(which_is_being_bound)).setValue(key);
        } else {
            std::string first_set = signbit(d) ? "-" : "+";
            std::string second_set = signbit(d) ? "+" : "-";
            switch (which_is_being_bound) {
            case KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY:
                cvarManager->getCvar(keys_to_cvars.at(KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY)).setValue(key + first_set);
                cvarManager->getCvar(keys_to_cvars.at(KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY)).setValue(key + second_set);
                break;
            case KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY:
                cvarManager->getCvar(keys_to_cvars.at(KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY)).setValue(key + first_set);
                cvarManager->getCvar(keys_to_cvars.at(KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY)).setValue(key + second_set);
                break;
            }

        }
        which_is_being_bound = KEYBIND_ASSIGNWHICH::NONE;
        });
}
