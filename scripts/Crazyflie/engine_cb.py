import dataclasses
from Crazyflie import callback as cb
from typing import Any


@dataclasses.dataclass
class Crazyflie_Engine_Values:
    roll: float
    pitch: float
    yawrate: float
    thrust: int
    
    def to_dict(self) -> dict[str, int | float]:
        return {
            "roll": self.roll,
            "pitch": self.pitch,
            "yawrate": self.yawrate,
            "thrust": self.thrust
        }


class Crazyflie_EngineCallback(cb.CrazyflieDataReceive_Callback):
    def __init__(self, vars: tuple[tuple[str, str]] = (
        ('stabilizer.roll', 'float'),
        ('stabilizer.pitch', 'float'),
        ('stabilizer.yaw', 'float'),
        ('stabilizer.thrust', 'int') 
    )) -> None:
        super().__init__(vars=vars)
        self._drone_engine: Crazyflie_Engine_Values = Crazyflie_Engine_Values(0, 0, 0)
        
    def get_drone_engine_data(self) -> Crazyflie_Engine_Values:
        return self._drone_engine
        
    def log_func(self, timestamp, data, logconf) -> None:
        self._drone_engine.roll = data.get(self.vars[0][0])
        self._drone_engine.pitch = data.get(self.vars[1][0])
        self._drone_engine.yawrate = data.get(self.vars[2][0])
        self._drone_engine.thrust = data.get(self.vars[3][0])        
