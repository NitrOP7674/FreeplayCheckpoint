# Freeplay Checkpoint
### Rewind, Save, and Restore Checkpoints in Freeplay, Replays, and Custom Training!
<p align="center"><img src="banner.png" width="350"></p>

**Setup:**

1. Open bakkesmod window (F2 by default)
2. Click "Plugins"
3. Find "Freeplay Checkpoint" in the plugin list on the left.
4. Optional: choose new button bindings.
5. Click "Apply Bindings"

Alternatively, or for KBM users: assign the `cpt_` commands (listed in the Command
Reference section below) as desired in the "Bindings" tab.

It is recommended to disable goal scoring in bakkesmod while using this plugin.

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

- `cpt_freeze`: activates rewind mode in Freeplay or Custom Training
- `cpt_do_checkpoint`:
  - In rewind mode, not at a saved checkpoint: saves the current state as a checkpoint
  - In rewind mode, at a saved checkpoint: deletes the current checkpoint (press twice)
  - While playing: loads the latest checkpoint
  - In a replay, saves the currently selected car & ball as a checkpoint
  - If **Disable delete on checkpoint button** is checked, this command never deletes checkpoints
- `cpt_prev_checkpoint` / `cpt_next_checkpoint`: loads the previous/next saved checkpoint
- `cpt_rand_checkpoint`: loads a random saved checkpoint
- `cpt_lock_checkpoint`: locks/unlocks the current checkpoint to prevent/allow its deletion.
- `cpt_mirror_state`: when frozen, mirrors car and ball to opposide side of field.
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
  - Recommended: Do *not* bind cpt_checkpoint to the same button as "reset shot".
  - Disable delete on checkpoint button (default off):
    - When checked, the checkpoint binding (`cpt_do_checkpoint`; commonly D-pad down)
      will not delete a frozen checkpoint. Saving and loading checkpoints still work.
    - Unchecked (default) keeps the existing press-twice-to-delete behavior.
  - Ignore prev/next/freeze ball when not frozen (recommended):
    - Avoids interfering with bakkesmod default commands.
  - Disable binds in custom training: ignore commands in custom training
  - Disable binds in workshop: ignore commands on workshop maps
  - Reset button loads last checkpoint instead of resetting:
    - If a checkpoint was loaded within the last few seconds (set by this amount),
      the reset shot button will load that checkpoint instead of resetting training.
- **Variance**:
  - When resuming play from frozen, apply randomness to the scenario.
  - **Randomly mirror when loading checkpoint**:
    - Randomly mirrors shots when loading to practice opposite angles.
  - **Load random checkpoint**:
    - When not frozen and `cpt_do_checkpoint` is pressed, load a random checkpoint instead
      of the latest one.
- **Auto-reset checkpoint**:
  - Allows drilling a shot or running through shots like a training pack.
- **Presets**:
  - Presets allow you to keep separate checkpoints for different types of training.
  - Use the Preset dropdown to switch between presets.
  - Create, rename, and delete presets from the plugin settings.
  - You can also import other preset files from plugin settings.
  - Quick checkpoints (*freezes*) are global and remain available when switching presets. 
    - This allows you to switch to another preset and save the shot there.
  - Imported presets are copied into the plugin's preset directory and will not overwrite an existing preset with the same name.
  - The default and currently selected Freeplay Checkpoint save files are automatically migrated to the preset system.
  - Since one preset must always exist, if there is only one preset you will not be able to delete it unless you make or import another one.
- **Other Options**:
  - **Delete ALL Shots**:
    - Deletes every saved checkpoint in the current file, even locked shots.  Check the
      "Enable" checkbox first to enable the button - there is no warning or confirmation
      once this is checked.
  - **Show player boost while rewinding**:
    - If set, boost will be shown when rewinding if the player was boosting while recording.
      Does not affect checkpoints.
  - **Clean History**:
    - When rewinding and restoring an old state, deletes history after that restored point.
  - **History Length**: amount of history to save
  - **History Refresh Rate**:
    - Interval between saved state points.  Set small for maximum smoothness in history data,
      but at the possible expense of worse performance.
  - **Debug**:
    - Shows some additional debugging data.  Probably not useful.
    
**Other CVars**
- `cpt_car_frozen`/`cpt_ball_frozen`:
  - These are set by this plugin whenever the car or ball or both are frozen in freeplay.

**Uninstalling:**

To conveniently remove bindings from buttons, click the "Remove Bindings" button
in the settings pane.

**Contact:**

Feel free to submit feature requests or bugs on Github, or contact me on Discord at NitroP#7674.

**Please read this document fully before contacting me about bugs.  Thank you!**
