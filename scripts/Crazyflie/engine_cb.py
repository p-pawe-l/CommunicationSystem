from __future__ import annotations

"""Crazyflie engine telemetry callback.

This module stores the latest stabilizer values reported by cflib logging:
roll, pitch, yaw rate, and thrust. ``Crazyflie_EngineCallback`` registers the
needed Crazyflie log variables through the shared callback interface and keeps
an in-memory snapshot for the drone client to read.
"""

from Crazyflie import callback as cb
from dataclasses import dataclass


@dataclass
class Crazyflie_Engine_Values:
    """Latest Crazyflie stabilizer and thrust telemetry values."""

    roll: float
    pitch: float
    yawrate: float
    thrust: int
    
    def to_dict(self) -> dict[str, int | float]:
        """Return engine telemetry as a plain dictionary."""
        return {
            "roll": self.roll,
            "pitch": self.pitch,
            "yawrate": self.yawrate,
            "thrust": self.thrust
        }


class Crazyflie_EngineCallback(cb.CrazyflieDataReceive_Callback):
    """Callback that updates engine telemetry from Crazyflie log packets."""

    def __init__(self, vars: tuple[tuple[str, str]] = (
        ('stabilizer.roll', 'float'),
        ('stabilizer.pitch', 'float'),
        ('stabilizer.yaw', 'float'),
        ('stabilizer.thrust', 'int') 
    )) -> None:
        """Create the callback with the Crazyflie variables it should read."""
        super().__init__(vars=vars)
        self._drone_engine: Crazyflie_Engine_Values = Crazyflie_Engine_Values(0, 0, 0)
        
    def get_drone_engine_data(self) -> Crazyflie_Engine_Values:
        """Return the latest engine telemetry snapshot."""
        return self._drone_engine
        
    def log_func(self, timestamp, data, logconf) -> None:
        """Update the engine snapshot from one cflib log packet."""
        self._drone_engine.roll = data.get(self.vars[0][0])
        self._drone_engine.pitch = data.get(self.vars[1][0])
        self._drone_engine.yawrate = data.get(self.vars[2][0])
        self._drone_engine.thrust = data.get(self.vars[3][0])        
