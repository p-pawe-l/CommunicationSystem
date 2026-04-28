from __future__ import annotations

import dataclasses
from Crazyflie import callback as cb

@dataclasses.dataclass
class Crazyflie_Velocity_Values:
    x: int | float
    y: int | float
    z: int | float
    

class Crazyflie_VelocityCallback(cb.CrazyflieDataReceive_Callback):
    def __init__(self, vars: tuple[tuple[str, str]] = (
        ('stateEstimate.vx', 'float'),
        ('stateEstimate.vy', 'float'),
        ('stateEstimate.vz', 'float')
    )) -> None:
        super().__init__(vars=vars)
        self._drone_velocity: Crazyflie_Velocity_Values = Crazyflie_Velocity_Values(0, 0, 0)

    @property
    def drone_velocity(self) -> Crazyflie_Velocity_Values:
        return self._drone_velocity

    def log_func(self, timestamp, data, logconf) -> None:
        self._drone_velocity.x = data.get(self.vars[0][0])
        self._drone_velocity.y = data.get(self.vars[1][0])
        self._drone_velocity.z = data.get(self.vars[2][0])
