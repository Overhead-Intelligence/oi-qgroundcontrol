#pragma once

#include <QtCore/QList>

/// ArduPilot motor-test layout: which physical motor each test sequence number drives, and
/// which way it turns.
///
/// `MAV_CMD_DO_MOTOR_TEST` param1 is a **test sequence number**, not a motor number. ArduPilot
/// maps it through a per-frame table (`AP_MotorsMatrix::add_motors`, third field) and spins
/// whichever motor carries that testing order. The two orderings are not the same and for most
/// frames are not even close: on a QUAD/X, sequence 2 is motor 4.
///
/// Showing only the sequence - which is all QGC used to do, as a bare letter - tells an
/// operator to check "motor B" against an airframe whose motors are numbered 1 to 4. Mission
/// Planner shows both, and that is the right level of detail: the letter says what to press,
/// the motor number and direction say what should move.
///
/// The tables here are generated from `AP_MotorsMatrix.cpp` rather than transcribed, because
/// the orderings are arbitrary per frame and a typo would point a pilot at the wrong arm.
/// Regenerate them against the firmware the fleet flies if a new frame is adopted.
namespace MotorLayout
{

/// One entry of a frame's test order. The index in the returned list is `sequence - 1`.
struct Motor {
    int     motorNumber = 0;        ///< the autopilot's motor number, 1-based
    bool    clockwise = false;      ///< spin direction, as the frame table defines it
};

/// Frame classes, matching ArduPilot's `motor_frame_class` (`Q_FRAME_CLASS` / `FRAME_CLASS`).
enum FrameClass {
    FrameClassUndefined     = 0,
    FrameClassQuad          = 1,
    FrameClassHexa          = 2,
    FrameClassOcta          = 3,
    FrameClassOctaQuad      = 4,
    FrameClassY6            = 5,
    FrameClassHeli          = 6,
    FrameClassTri           = 7,
    FrameClassSingle        = 8,
    FrameClassCoax          = 9,
    FrameClassTailsitter    = 10,
    FrameClassHeliDual      = 11,
    FrameClassDodecaHexa    = 12,
    FrameClassHeliQuad      = 13,
    FrameClassDeca          = 14,
};

/// Number of motors a frame class carries, or 0 when the class is unknown or has no fixed
/// count. Known for every class, including those with no direction table below, so the test
/// page can still show the right number of buttons.
int motorCountForClass(int frameClass);

/// Test order for a frame, indexed by `sequence - 1`. Empty when the combination is not
/// tabulated, which is the signal to fall back to naming the sequence alone rather than
/// guessing a motor number.
QList<Motor> forFrame(int frameClass, int frameType);

} // namespace MotorLayout
