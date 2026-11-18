-----------------------------------------------------------------------------------------------
-- mmcLab.lua: MPC MIDI Converter scripting lab, step 1.
-- Proves that MPC loads a user device script: when the bound MIDI device connects, MPC calls
-- mmcLab.OnLoad and a popup appears. Nothing in the project is changed.
-- Written against the MPC OS 3.10.1 device-script interface (the same one Akai's MPK mini scripts use).
-----------------------------------------------------------------------------------------------

local deviceName = "mmcLab"; -- must match the file name

function mmcLab.HandleMIDINoteOn(channel, note, velocity) end
function mmcLab.HandleMIDINoteOff(channel, note, velocity) end
function mmcLab.HandleMIDICC(channel, cc, value) end
function mmcLab.HandleMIDISysex(sysexChars) end
function mmcLab.HandleTimer() end

function mmcLab.OnLoad()
    mpc.ui.popups.ShowInfoPopup("MPC MIDI Converter lab",
        "The test script loaded.\nScripting works on this MPC.");
end

function mmcLab.OnUnload() end
