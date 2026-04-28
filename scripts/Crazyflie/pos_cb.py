import callback as cb 


class Crazyflie_PositionCallback(cb.CrazyflieDataReceive_Callback):
    def __init__(self, vars: tuple[tuple[str, str]] = (
        ('stateEstimate.x', 'float'),
        ('stateEstimate.y', 'float'),
        ('stateEstimate.z', 'float') 
    )) -> None:
        super().__init__(vars=vars)
        self._drone_position: tuple = (0, 0, 0)
        
    @property
    def drone_position(self) -> tuple:
        return self._drone_position
        
    def log_func(self, timestamp, data, logconf) -> None:
        self._drone_position[0] = data.get(vars[0][0])
        self._drone_position[1] = data.get(vars[1][0])
        self._drone_position[2] = data.get(vars[2][0])
        