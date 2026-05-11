"""Crazyflie position telemetry callback.

This module stores the latest position values reported by cflib logging.
``Crazyflie_PositionCallback`` registers the state estimate variables and
keeps an in-memory snapshot for the drone client to read.
"""

import dataclasses
from Crazyflie import callback as cb
from typing import Any


@dataclasses.dataclass
class Crazyflie_Position_Values:
    """Latest Crazyflie position telemetry values."""

    x: int | float
    y: int | float
    z: int | float
    
    def to_dict(self) -> dict[str, Any]:
        """Return position telemetry as a plain dictionary."""
        return dataclasses.asdict(self)


class Crazyflie_PositionCallback(cb.CrazyflieDataReceive_Callback):
    """Callback that updates position telemetry from Crazyflie log packets."""

    def __init__(self, vars: tuple[tuple[str, str]] = (
        ('stateEstimate.x', 'float'),
        ('stateEstimate.y', 'float'),
        ('stateEstimate.z', 'float') 
    )) -> None:
        """Create the callback with the Crazyflie variables it should read."""
        super().__init__(vars=vars)
        self._drone_position: Crazyflie_Position_Values = Crazyflie_Position_Values(0, 0, 0)
        
    def get_drone_position(self) -> Crazyflie_Position_Values:
        """Return the latest position telemetry snapshot."""
        return self._drone_position
        
    def log_func(self, timestamp, data, logconf) -> None:
        """Update the position snapshot from one cflib log packet."""
        self._drone_position.x = data.get(self.vars[0][0])
        self._drone_position.y = data.get(self.vars[1][0])
        self._drone_position.z = data.get(self.vars[2][0])
        
