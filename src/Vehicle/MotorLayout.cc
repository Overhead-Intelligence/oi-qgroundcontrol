#include "MotorLayout.h"

namespace {

/// One tabulated frame. `seq[n]` is the motor driven by test sequence `n + 1`.
struct FrameLayout {
    int                 frameClass;
    int                 frameType;
    int                 count;
    MotorLayout::Motor  seq[12];
};

/// Generated from AP_MotorsMatrix.cpp: for each `case MOTOR_FRAME_TYPE_*` the MotorDef list
/// position is the motor number and its last field is the testing order, so this is that
/// mapping inverted. Every tabulated frame was checked to use a contiguous 1..n sequence.
constexpr FrameLayout kFrameLayouts[] = {
    { 1, 0 , 4, { { 3, true }, { 1, false }, { 4, true }, { 2, false } } },  // QUAD/PLUS
    { 1, 1 , 4, { { 1, false }, { 4, true }, { 2, false }, { 3, true } } },  // QUAD/X
    { 1, 12, 4, { { 2, false }, { 1, true }, { 3, false }, { 4, true } } },  // QUAD/BF_X
    { 1, 18, 4, { { 2, true }, { 1, false }, { 3, true }, { 4, false } } },  // QUAD/BF_X_REV
    { 1, 13, 4, { { 1, false }, { 4, true }, { 3, false }, { 2, true } } },  // QUAD/DJI_X
    { 1, 14, 4, { { 1, false }, { 2, true }, { 3, false }, { 4, true } } },  // QUAD/CW_X
    { 1, 3 , 4, { { 1, true }, { 4, false }, { 2, true }, { 3, false } } },  // QUAD/H
    { 1, 6 , 4, { { 3, false }, { 1, true }, { 4, false }, { 2, true } } },  // QUAD/PLUSREV
    { 1, 19, 4, { { 1, false }, { 2, true }, { 3, false }, { 4, true } } },  // QUAD/Y4
    { 2, 0 , 6, { { 1, true }, { 4, false }, { 6, true }, { 2, false }, { 3, true }, { 5, false } } },  // HEXA/PLUS
    { 2, 1 , 6, { { 5, false }, { 1, true }, { 4, false }, { 6, true }, { 2, false }, { 3, true } } },  // HEXA/X
    { 2, 3 , 6, { { 5, false }, { 1, true }, { 4, false }, { 6, true }, { 2, false }, { 3, true } } },  // HEXA/H
    { 2, 13, 6, { { 1, false }, { 6, true }, { 5, false }, { 4, true }, { 3, false }, { 2, true } } },  // HEXA/DJI_X
    { 2, 14, 6, { { 1, false }, { 2, true }, { 3, false }, { 4, true }, { 5, false }, { 6, true } } },  // HEXA/CW_X
    { 3, 0 , 8, { { 1, true }, { 3, false }, { 8, true }, { 4, false }, { 2, true }, { 6, false }, { 7, true }, { 5, false } } },  // OCTA/PLUS
    { 3, 1 , 8, { { 1, true }, { 3, false }, { 8, true }, { 4, false }, { 2, true }, { 6, false }, { 7, true }, { 5, false } } },  // OCTA/X
    { 3, 2 , 8, { { 7, true }, { 6, false }, { 2, true }, { 4, false }, { 8, true }, { 3, false }, { 1, true }, { 5, false } } },  // OCTA/V
    { 3, 3 , 8, { { 1, true }, { 3, false }, { 8, true }, { 4, false }, { 2, true }, { 6, false }, { 7, true }, { 5, false } } },  // OCTA/H
    { 3, 15, 8, { { 2, true }, { 6, false }, { 7, true }, { 5, false }, { 1, true }, { 3, false }, { 8, true }, { 4, false } } },  // OCTA/I
    { 3, 13, 8, { { 1, false }, { 8, true }, { 7, false }, { 6, true }, { 5, false }, { 4, true }, { 3, false }, { 2, true } } },  // OCTA/DJI_X
    { 3, 14, 8, { { 1, false }, { 2, true }, { 3, false }, { 4, true }, { 5, false }, { 6, true }, { 7, false }, { 8, true } } },  // OCTA/CW_X
    { 4, 0 , 8, { { 1, false }, { 6, true }, { 4, true }, { 7, false }, { 3, false }, { 8, true }, { 2, true }, { 5, false } } },  // OCTAQUAD/PLUS
    { 4, 1 , 8, { { 1, false }, { 6, true }, { 4, true }, { 7, false }, { 3, false }, { 8, true }, { 2, true }, { 5, false } } },  // OCTAQUAD/X
    { 4, 3 , 8, { { 1, true }, { 6, false }, { 4, false }, { 7, true }, { 3, true }, { 8, false }, { 2, false }, { 5, true } } },  // OCTAQUAD/H
    { 4, 14, 8, { { 1, false }, { 2, true }, { 3, true }, { 4, false }, { 5, false }, { 6, true }, { 7, true }, { 8, false } } },  // OCTAQUAD/CW_X
    { 4, 12, 8, { { 2, false }, { 6, true }, { 1, true }, { 5, false }, { 3, false }, { 7, true }, { 4, true }, { 8, false } } },  // OCTAQUAD/BF_X
    { 4, 18, 8, { { 2, true }, { 6, false }, { 1, false }, { 5, true }, { 3, true }, { 7, false }, { 4, false }, { 8, true } } },  // OCTAQUAD/BF_X_REV
    { 4, 20, 8, { { 1, false }, { 6, false }, { 4, true }, { 7, true }, { 3, false }, { 8, false }, { 2, true }, { 5, true } } },  // OCTAQUAD/X_COR
    { 4, 21, 8, { { 1, false }, { 2, false }, { 3, true }, { 4, true }, { 5, false }, { 6, false }, { 7, true }, { 8, true } } },  // OCTAQUAD/CW_X_COR
};

} // namespace

namespace MotorLayout
{

int motorCountForClass(int frameClass)
{
    switch (frameClass) {
    case FrameClassQuad:        return 4;
    case FrameClassHexa:        return 6;
    case FrameClassOcta:        return 8;
    case FrameClassOctaQuad:    return 8;
    case FrameClassY6:          return 6;
    case FrameClassTri:         return 3;
    case FrameClassHeliQuad:    return 4;
    case FrameClassDodecaHexa:  return 12;
    case FrameClassDeca:        return 10;
    // Single, Coax, Heli, HeliDual and Tailsitter do not present a motor matrix to test, and
    // Undefined means the operator has not configured one. Reported as unknown either way.
    default:                    return 0;
    }
}

QList<Motor> forFrame(int frameClass, int frameType)
{
    for (const FrameLayout &layout : kFrameLayouts) {
        if ((layout.frameClass != frameClass) || (layout.frameType != frameType)) {
            continue;
        }

        QList<Motor> motors;
        motors.reserve(layout.count);
        for (int i = 0; i < layout.count; i++) {
            motors.append(layout.seq[i]);
        }
        return motors;
    }

    return {};
}

} // namespace MotorLayout
