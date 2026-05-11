from __future__ import annotations

"""Crazyflie velocity telemetry callback.

This module stores the latest velocity values reported by cflib logging.
``Crazyflie_VelocityCallback`` registers the state estimate velocity variables
and keeps an in-memory snapshot for future drone telemetry messages.
"""

import dataclasses
from Crazyflie import callback as cb


@dataclasses.dataclass
class Crazyflie_Velocity_Values:
    """Latest Crazyflie velocity telemetry values."""

    x: int | float
    y: int | float
    z: int | float

    def to_dict(self) -> dict[str, int | float]:
        """Return velocity telemetry as a plain dictionary."""
        return dataclasses.asdict(self)


class Crazyflie_VelocityCallback(cb.CrazyflieDataReceive_Callback):
    """Callback that updates velocity telemetry from Crazyflie log packets."""

    def __init__(self, vars: tuple[tuple[str, str]] = (
        ('stateEstimate.vx', 'float'),
        ('stateEstimate.vy', 'float'),
        ('stateEstimate.vz', 'float')
    )) -> None:
        """Create the callback with the Crazyflie variables it should read."""
        super().__init__(vars=vars)
        self._drone_velocity: Crazyflie_Velocity_Values = Crazyflie_Velocity_Values(0, 0, 0)

    @property
    def drone_velocity(self) -> Crazyflie_Velocity_Values:
        """Return the latest velocity telemetry snapshot."""
        return self._drone_velocity

    def log_func(self, timestamp, data, logconf) -> None:
        """Update the velocity snapshot from one cflib log packet."""
        self._drone_velocity.x = data.get(self.vars[0][0])
        self._drone_velocity.y = data.get(self.vars[1][0])
        self._drone_velocity.z = data.get(self.vars[2][0])
