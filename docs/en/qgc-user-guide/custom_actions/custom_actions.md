# Custom Mavlink Action

Both the Fly View and Joysticks support the ability execute arbitrary mavlink commands to the active vehicle. In the Fly View these will show up in the Toolstrip Action list. With Joysticks you can assign then to button presses.

## Mavlink Actions File

The actions available are defined in a JSON file. The format of that file is as follows:

```
{
    "version":    1,
    "fileType":   "MavlinkActions",
    "actions": [
        {
            "label":        "First Mavlink Command",
            "description":  "This is the first command",
            "mavCmd":       10,
            "compId":       100,
            "param1":       1,
            "param2":       2,
            ...
        },
        {
            "label":        "Second Mavlink Command",
            "description":  "This is the second command",
            "mavCmd":       20,
            ...
        }
    ]
}
```

Fields:

* actions (required) - An array of json objects, one for each command
* label (required) - The user visible short description for the command. This is used as the button text for the Fly View - Actions command list. For Joysticks, this is the command you select from the dropdown. For Joysticks, make sure your name doesn't conflict with the built in names.
* description (required) - This is a longer description of the command used in the Fly View - Action list. This is not used by joysticks.
* mavCmd (required) - The command id of the mavlink command you want to send.
* compId (options) - The component id for where you want to send the command to. If not specified `MAV_COMP_ID_AUTOPILOT1` is used.
* param1 thru param7 (optional) - The parameters for the command. Parameters which are not specified will default to 0.0

## Choosing which files are active

Mavlink action files should be located in the MavlinkActions directory of the QGC save location. For example on Linux that would be `~/Documents/QGroundControl/MavlinkActions` or `~/Documents/QGroundControl Daily/MavlinkActions`.

Fly View Settings > MAVLink Actions lists every JSON file in that directory, with a tick box for the Fly View action list and one for joystick button assignment. **More than one file can be active at a time**, and the two lists are independent. `Import` copies a file into the directory for you; `Remove` deletes it.

Ticking or unticking a file takes effect immediately - the Fly View action list and the joystick button dropdown both update without restarting QGC.

Keeping actions in several files is worth doing when some of them only apply to some aircraft: a command that drives a servo the airframe does not have, or that relies on a Lua script it is not running, can sit in its own file and be switched on only for the vehicles it suits, instead of being merged into one list that is then wrong everywhere else.

A file which is named in the settings but has since been deleted is skipped silently. A file which is malformed is reported by name, and the other files still load.
