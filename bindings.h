#pragma once
#include "pch.h"

enum class KEYBIND_ASSIGNWHICH {
    NONE = 0,
    CPT_FREEZE_KEY,
    CPT_DO_CHECKPOINT_KEY,
    CPT_PREV_CHECKPOINT_KEY,
    CPT_NEXT_CHECKPOINT_KEY,
    CPT_FREEZE_BALL_KEY,
    CPT_MIRROR_STATE_KEY,
    CPT_REWIND_KEY,
    CPT_FASTFORWARD_KEY
};

const static inline std::map<KEYBIND_ASSIGNWHICH, std::string> keys_to_cvars = {
    {KEYBIND_ASSIGNWHICH::CPT_FREEZE_KEY, "cpt_freeze_key"},
    {KEYBIND_ASSIGNWHICH::CPT_DO_CHECKPOINT_KEY, "cpt_do_checkpoint_key"},
    {KEYBIND_ASSIGNWHICH::CPT_PREV_CHECKPOINT_KEY,"cpt_prev_checkpoint_key"},
    {KEYBIND_ASSIGNWHICH::CPT_NEXT_CHECKPOINT_KEY, "cpt_next_checkpoint_key"},
    {KEYBIND_ASSIGNWHICH::CPT_FREEZE_BALL_KEY, "cpt_freeze_ball_key"},
    {KEYBIND_ASSIGNWHICH::CPT_MIRROR_STATE_KEY, "cpt_mirror_state_key"},
    {KEYBIND_ASSIGNWHICH::CPT_REWIND_KEY, "cpt_rewind_key"},
    {KEYBIND_ASSIGNWHICH::CPT_FASTFORWARD_KEY, "cpt_fastforward_key"},
};

void OnKeyAxisInput(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper, ActorWrapper aw, void* params, std::string eventName);
void OnKeyPressed(std::shared_ptr<CVarManagerWrapper> cvarManager, std::shared_ptr<GameWrapper> gameWrapper, ActorWrapper aw, void* params, std::string eventName);