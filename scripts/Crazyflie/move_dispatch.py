from __future__ import annotations

import dataclasses
import enum
import typing


class Crazyflie_Movement(enum.Enum):
    LEFT_ROLL =             "LEFT_ROLL",
    CENTER_ROLL =           "CENTER_ROLL",
    RIGHT_ROLL =            "RIGHT_ROLL",
    BACKWARD_PITCH =        "BACKWARD_PITCH",
    CENTER_PITCH =          "CENTER_PITCH",
    FORWARD_PITCH =         "FORWARD_PITCH",
    ROTATION_LEFT =         "ROTATION_LEFT",
    NO_ROTATION =           "NO_ROTATION",
    ROTATION_RIGHT =        "ROTATION_RIGHT",
    VERTICAL_UP_MOVE =      "VERTICAL_UP_MOVE",
    VERTICAL_DOWN_MOVE =    "VERTICAL_DOWN_MOVE",
    VERTICAL_HOVER =        "VERTICAL_HOVER"


@typing.final
@dataclasses.dataclass(frozen=True)
class Crazyflie_MovementRanges:
    roll: typing.ClassVar[dict[str, int | float]] = {
        "MIN": -10.0,
        "CENTER": 0.0,
        "MAX": 10.0,
    }
    pitch: typing.ClassVar[dict[str, int | float]] = {
        "MIN": -10.0,
        "CENTER": 0.0,
        "MAX": 10.0,
    }
    yaw: typing.ClassVar[dict[str, int | float]] = {
        "MIN": -50.0,
        "CENTER": 0.0,
        "MAX": 50.0,
    }
    thrust: typing.ClassVar[dict[str, int | float]] = {
        "MIN": 10_000.0,
        "HOVER": 35_000.0,
        "MAX": 60_000.0,
    }


@typing.final
class Crazyflie_MovementDispatch_Manager:
    def __init__(
        self,
        change_roll: typing.Callable[[float], None],
        change_pitch: typing.Callable[[float], None],
        change_yawrate: typing.Callable[[float], None],
        change_thrust: typing.Callable[[float], None],
    ) -> None:
        self._MOVEMENT_DISPATCH_FUNCTIONS: dict[Crazyflie_Movement, typing.Callable[[float], None]] = {
            Crazyflie_Movement.LEFT_ROLL: lambda power=1.0: change_roll(Crazyflie_MovementRanges.roll["MIN"] * power),
            Crazyflie_Movement.CENTER_ROLL: lambda power=1.0: change_roll(Crazyflie_MovementRanges.roll["CENTER"]),
            Crazyflie_Movement.RIGHT_ROLL: lambda power=1.0: change_roll(Crazyflie_MovementRanges.roll["MAX"] * power),

            Crazyflie_Movement.BACKWARD_PITCH: lambda power=1.0: change_pitch(Crazyflie_MovementRanges.pitch["MIN"] * power),
            Crazyflie_Movement.CENTER_PITCH: lambda power=1.0: change_pitch(Crazyflie_MovementRanges.pitch["CENTER"]),
            Crazyflie_Movement.FORWARD_PITCH: lambda power=1.0: change_pitch(Crazyflie_MovementRanges.pitch["MAX"] * power),

            Crazyflie_Movement.ROTATION_LEFT: lambda power=1.0: change_yawrate(Crazyflie_MovementRanges.yaw["MIN"] * power),
            Crazyflie_Movement.NO_ROTATION: lambda power=1.0: change_yawrate(Crazyflie_MovementRanges.yaw["CENTER"]),
            Crazyflie_Movement.ROTATION_RIGHT: lambda power=1.0: change_yawrate(Crazyflie_MovementRanges.yaw["MAX"] * power),

            Crazyflie_Movement.VERTICAL_UP_MOVE: lambda power=1.0: change_thrust(
                Crazyflie_MovementRanges.thrust["HOVER"] +
                ((Crazyflie_MovementRanges.thrust["MAX"] - Crazyflie_MovementRanges.thrust["HOVER"]) * power)
            ),
            Crazyflie_Movement.VERTICAL_DOWN_MOVE: lambda power=1.0: change_thrust(
                Crazyflie_MovementRanges.thrust["HOVER"] -
                ((Crazyflie_MovementRanges.thrust["HOVER"] - Crazyflie_MovementRanges.thrust["MIN"]) * power)
            ),
            Crazyflie_Movement.VERTICAL_HOVER: lambda power=1.0: change_thrust(Crazyflie_MovementRanges.thrust["HOVER"]),
        }

    def dispatch(self, movement: Crazyflie_Movement | str, power: float = 1.0) -> None:
        if isinstance(movement, str):
            movement = Crazyflie_Movement(movement)

        self._MOVEMENT_DISPATCH_FUNCTIONS[movement](power)
