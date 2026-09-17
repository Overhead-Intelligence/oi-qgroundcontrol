--[[
   Heading Hold Timeout - OI safety watchdog

   Caps how long the aircraft will fly a guided heading-hold. QGC's heading-hold
   sends MAV_CMD_GUIDED_CHANGE_HEADING (43002) once per operator action with no
   periodic resend, so without this watchdog a held heading would fly forever.

   This script watches the MAVLink stream for GUIDED_CHANGE_HEADING commands.
   Each command (re)starts a clock. If the clock reaches HHT_TIMEOUT seconds
   while still in GUIDED with no new heading command, the script switches the
   vehicle to LOITER so it stops flying a fixed heading indefinitely.

   PARAMETERS
     HHT_TIMEOUT  seconds a heading-hold command stays valid. Default 120.
                  Set 0 to disable the watchdog. Each new heading command
                  restarts the clock.

   INSTALL
     Copy this file to the autopilot's APM/scripts/ folder and copy the
     ArduPilot MAVLink module folder to APM/scripts/modules/MAVLink/.
     Requires SCR_ENABLE = 1.
]]

local SCRIPT_NAME = "HeadingHoldTimeout"

-- QGC sends MAV_CMD_GUIDED_CHANGE_HEADING as a COMMAND_LONG.
local mavlink_msgs    = require("MAVLink/mavlink_msgs")
local COMMAND_LONG_ID = mavlink_msgs.get_msgid("COMMAND_LONG")
local msg_map = {}
msg_map[COMMAND_LONG_ID] = "COMMAND_LONG"

local MAV_CMD_GUIDED_CHANGE_HEADING = 43002
local MODE_GUIDED = 15   -- ArduPlane GUIDED
local MODE_LOITER = 12   -- ArduPlane LOITER

-- Parameter table: HHT_TIMEOUT
local PARAM_TABLE_KEY    = 107
local PARAM_TABLE_PREFIX = "HHT_"
assert(param:add_table(PARAM_TABLE_KEY, PARAM_TABLE_PREFIX, 1),
       SCRIPT_NAME .. ": could not add param table")
assert(param:add_param(PARAM_TABLE_KEY, 1, "TIMEOUT", 120),
       SCRIPT_NAME .. ": could not add HHT_TIMEOUT")
local HHT_TIMEOUT = Parameter()
HHT_TIMEOUT:init("HHT_TIMEOUT")

-- State
local hold_active = false   -- a heading-hold command is currently being timed
local triggered   = false   -- LOITER has already been forced for this hold
local last_cmd_ms = millis()

mavlink:init(1, 10)
mavlink:register_rx_msgid(COMMAND_LONG_ID)

gcs:send_text(6, SCRIPT_NAME .. ": active")

function update()
    -- Drain all pending MAVLink messages, restart the clock on each heading cmd
    while true do
        local msg = mavlink:receive_chan()
        if msg == nil then
            break
        end
        local parsed = mavlink_msgs.decode(msg, msg_map)
        if parsed ~= nil and parsed.command == MAV_CMD_GUIDED_CHANGE_HEADING then
            hold_active = true
            triggered   = false
            last_cmd_ms = millis()
        end
    end

    local timeout_s = HHT_TIMEOUT:get()
    if timeout_s == nil or timeout_s <= 0 then
        return update, 500   -- watchdog disabled
    end

    if hold_active then
        if vehicle:get_mode() ~= MODE_GUIDED then
            -- Left GUIDED: the firmware already cleared the heading hold
            hold_active = false
        elseif (not triggered) and (millis() - last_cmd_ms) > (timeout_s * 1000) then
            if vehicle:set_mode(MODE_LOITER) then
                gcs:send_text(4, string.format("%s: %ds timeout - switching to LOITER",
                                               SCRIPT_NAME, math.floor(timeout_s)))
            else
                gcs:send_text(3, SCRIPT_NAME .. ": timeout - LOITER switch FAILED")
            end
            triggered   = true
            hold_active = false
        end
    end

    return update, 500
end

return update()
