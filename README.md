<p align="center"><img src="banner.png" width="350"></p>

**Setup:**

1. Open bakkesmod window (F2 by default)
2. Click "Plugins"
3. Find "Freeplay Checkpoint" in the plugin list on the left.
4. Optional: choose new button bindings.
5. Click "Apply Bindings"

Alternatively, or for KBM users: assign the `cpt_` commands (listed in the Command
Reference section below) as desired in the "Bindings" tab.

**Basic Usage:**

Note: assumes default bindings from above.

1. Start freeplay mode.  Set up a shot.
2. Enter rewind mode by pressing the right thumb stick.
3. Steer left/right to rewind/advance time.
4. When you find a point in time to save, press the Back (or Select or Share) button.
5. Resume driving.
6. Press Back again to return to that checkpoint.
7. Press Back (twice) on a currently frozen checkpoint to delete it.
8. Press left/right on the dpad to navigate between multiple saved checkpoints.

**Command Reference:**

- `cpt_freeze`: Activates rewind mode
- `cpt_do_checkpoint`:
  - In rewind mode, not at a saved checkpoint: saves the current state as a checkpoint
  - In rewind mode, at a saved checkpoint: deletes the current checkpoint (press twice)
  - While playing: loads the latest checkpoint
  - In a replay, saves the currently selected car & ball as a checkpoint
- `cpt_prev_checkpoint` / `cpt_next_checkpoint`: loads the previous/next saved checkpoint
- `cpt_rand_checkpoint`: loads a random saved checkpoint
- `cpt_lock_checkpoint`: locks/unlocks the current checkpoint to prevent/allow its deletion.
- `cpt_freeze_ball`:
  - In rewind mode: unfreezes the player's car while keeping the ball frozen
  - While playing: freezes the ball without freezing the player's car
- `cpt_copy`\*:
  - In rewind mode: copy the current state to the clipboard
  - While playing: copy the last loaded checkpoint or quick checkpoint to the clipboard
  - In a replay: copy the currently selected car & ball to the clipboard
- `cpt_paste`\*: load a checkpoint from the clipboard as a quick checkpoint

\* - The `cpt_copy` and `cpt_paste` commands can be entered in the F6 console of bakkesmod.

**Settings Reference:**

- **Bindings**:
  - KBM players should manually bind the cpt_ keys in the bindings section.
  - Controller players may hold the button they wish to bind and click the action they
    wish to bind to that button.
- **Variance**:
  - When resuming play from frozen, apply randomness to the scenario.
- **Randomly mirror when loading checkpoint**:
  - Randomly mirrors shots when loading to practice opposite angles.
  **Load random checkpoint**:
  - When not frozen and `cpt_do_checkpoint` is pressed, load a random checkpoint instead
    of the latest one.
- **Auto-reset checkpoint**:
  - Allows drilling a shot or running through shots like a training pack.
- **Ignore cmds except freeze & do_checkpoint when not frozen**:
  - Allows for binding dpad buttons to prev/next/freeze ball while not interfering with
    bakkesmod default commands.
- **Clean History**:
  - When rewinding and restoring an old state, deletes history after that restored point.
- **History Length**: amount of history to save
- **History Refresh Rate**:
  - Interval between saved state points.  Set small for maximum smoothness in history data,
    but at the possible expense of worse performance.

**Uninstalling:**

To conveniently remove bindings from buttons, click the "Remove Bindings" button
in the settings pane.

**Contact:**

Feel free to submit feature requests or bugs on Github, or contact me on Discord at NitroP#7674.