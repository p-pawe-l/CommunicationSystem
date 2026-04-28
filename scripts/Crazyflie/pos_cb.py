import dataclasses
from Crazyflie import callback as cb


@dataclasses.dataclass
class Crazyflie_Position_Values:
    x: int | float
    y: int | float
    z: int | float
    

class Crazyflie_PositionCallback(cb.CrazyflieDataReceive_Callback):
    def __init__(self, vars: tuple[tuple[str, str]] = (
        ('stateEstimate.x', 'float'),
        ('stateEstimate.y', 'float'),
        ('stateEstimate.z', 'float') 
    )) -> None:
        super().__init__(vars=vars)
        self._drone_position: Crazyflie_Position_Values = Crazyflie_Position_Values(0, 0, 0)
        
    @property
    def drone_position(self) -> Crazyflie_Position_Values:
        return self._drone_position
        
    def log_func(self, timestamp, data, logconf) -> None:
        self._drone_position.x = data.get(self.vars[0][0])
        self._drone_position.y = data.get(self.vars[1][0])
        self._drone_position.z = data.get(self.vars[2][0])
        
