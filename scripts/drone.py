from __future__ import annotations

import drone_system
import func_decorators
import abc
import typing
import dataclasses

import cflib
import enum
from cflib.crazyflie.syncCrazyflie import SyncCrazyflie
from cflib.crazyflie import Crazyflie
from Crazyflie.logconf import Crazyflie_LogConf
from Crazyflie.pos_cb import Crazyflie_PositionCallback
from Crazyflie.callback import Crazyflie_Callback

class DroneClient(abc.ABC):
    def __init__(self,
                 system: drone_system.System, 
                 drone_name: str, 
                 is_mother: bool = False) -> None:
        self.drone_name: str = drone_name
        self.is_mother: bool = is_mother
        self.client_instance: drone_system.Client = drone_system.Client(
            self.drone_name, system, self.generate_drone_data, self.process_drone_data, False
        )
        self.client_instance.start()
        
        
    @abc.abstractmethod
    @func_decorators.generating_func
    def generate_drone_data(self) -> dict[str, typing.Any]:
        pass
        
        
    @abc.abstractmethod
    @func_decorators.processing_func
    def process_drone_data(self, message) -> None:
        pass
    

@dataclasses.dataclass
class Crazyflie_SetPoint_Values:
    roll: int
    pitch: int
    yawrate: int
    thrust: int
    
    
@dataclasses.dataclass
class Crazyflie_Position_Values:
    x: int | float
    y: int | float
    z: int | float
    

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
    


class Crazyflie_DroneClient(DroneClient):
    ROLL_MAX_VALUE: float = 10.0
    ROLL_CENTER_VALUE: float = 0.0
    ROLL_MIN_VALUE: float = -10.0
    
    PITCH_MAX_VALUE: float = 10.0
    PITCH_CENTER_VALUE: float = 0.0
    PITCH_MIN_VALUE: float = -10.0
    
    YAWRATE_MAX_VALUE: float = 50.0
    YAWRATE_NO_ROTATION_VALUE: float = 0.0
    YAWRATE_MIN_VALUE: float = -50.0
    
    THRUST_MIN_VALUE: float = 10_000.0
    THRUST_HOVER_VALUE: float = 35_000.0
    THRUST_MAX_VALUE: float = 60_000.0
    
    def __init__(self,
                 uri: str,
                 system: drone_system.System,
                 drone_name: str,
                 data_receivers: list[str],
                 cb_logger: Crazyflie_LogConf,
                 init_callbacks: list[Crazyflie_Callback] = [Crazyflie_PositionCallback()],
                 is_mother: bool = False) -> None:
        super().__init__(system, drone_name, is_mother)
        self.uri: str = uri
        self.drone_name: str = drone_name
        self.data_receivers: list[str] = data_receivers
        self.cb_logger: Crazyflie_LogConf = cb_logger
        self.cb_logger.add_callback(init_callbacks)
        # Starting values for each drone
        self.drone_setpoint: Crazyflie_SetPoint_Values = Crazyflie_SetPoint_Values(0, 0, 0, 0)
        self.drone_position: Crazyflie_Position_Values = Crazyflie_Position_Values(0, 0, 0)
        
        cflib.crtp.init_drivers()
        self.drone: SyncCrazyflie = SyncCrazyflie(link_uri=uri, cf=Crazyflie(rw_cache='./cache'))
        self.drone.open_link()
        self.drone.cf.supervisor.send_arming_request(True)
        self.drone.cf.commander.send_setpoint(**dataclasses.asdict(self.drone_setpoint))

        self.drone.cf.log.add_config(self.cb_logger.get_cflib_LogConfig())
        self.cb_logger.start()
        
        self._MOVEMENT_DISPATCH_FUNCTIONS: dict[Crazyflie_Movement, typing.Callable[[float], None]] = {
            Crazyflie_Movement.LEFT_ROLL: lambda power=1.0: self._change_roll(self.ROLL_MIN_VALUE * power),
            Crazyflie_Movement.CENTER_ROLL: lambda power=1.0: self._change_roll(self.ROLL_CENTER_VALUE),
            Crazyflie_Movement.RIGHT_ROLL: lambda power=1.0: self._change_roll(self.ROLL_MAX_VALUE * power),
            
            Crazyflie_Movement.BACKWARD_PITCH: lambda power=1.0: self._change_pitch(self.PITCH_MIN_VALUE * power),
            Crazyflie_Movement.CENTER_PITCH: lambda power=1.0: self._change_pitch(self.PITCH_CENTER_VALUE),
            Crazyflie_Movement.FORWARD_PITCH: lambda power=1.0: self._change_pitch(self.PITCH_MAX_VALUE * power), 
            
            Crazyflie_Movement.ROTATION_LEFT: lambda power=1.0: self._change_yawrate(self.YAWRATE_MIN_VALUE * power),
            Crazyflie_Movement.NO_ROTATION: lambda power=1.0: self._change_yawrate(self.YAWRATE_NO_ROTATION_VALUE),
            Crazyflie_Movement.ROTATION_RIGHT: lambda power=1.0: self._change_yawrate(self.YAWRATE_MAX_VALUE * power),

            Crazyflie_Movement.VERTICAL_UP_MOVE: lambda power=1.0: self._change_thrust(
                self.THRUST_HOVER_VALUE + ((self.THRUST_MAX_VALUE - self.THRUST_HOVER_VALUE) * power)
            ),
            Crazyflie_Movement.VERTICAL_DOWN_MOVE: lambda power=1.0: self._change_thrust(
                self.THRUST_HOVER_VALUE - ((self.THRUST_HOVER_VALUE - self.THRUST_MIN_VALUE) * power)
            ),
            Crazyflie_Movement.VERTICAL_HOVER: lambda power=1.0: self._change_thrust(self.THRUST_HOVER_VALUE),
        }
            
    
    def _update_drone_setpoint(self) -> None:
        self.drone.cf.commander.send_setpoint(**dataclasses.asdict(self.drone_setpoint))

    def _change_roll(self, new_roll: float) -> None:
        self.drone_setpoint.roll = new_roll
        self._update_drone_setpoint()

    def _change_pitch(self, new_pitch: float) -> None:
        self.drone_setpoint.pitch = new_pitch
        self._update_drone_setpoint()
        
    def _change_yawrate(self, new_yawrate: float) -> None:
        self.drone_setpoint.yawrate = new_yawrate
        self._update_drone_setpoint()

    def _change_thrust(self, new_thrust: float) -> None:
        self.drone_setpoint.thrust = new_thrust
        self._update_drone_setpoint()

        
    @func_decorators.generating_func
    def generate_drone_data(self) -> dict[str, typing.Any]:
        return {
            "sender": self.drone_name,
            "receivers": self.data_receivers,
            "data": {
                "position": self.drone_position
            }
        }

    @func_decorators.processing_func
    def process_drone_data(self, message):
        data: dict[str, typing.Any] = message.get("data")
        power: float = data.get("power")
        command: Crazyflie_Movement = data.get("command")
        
        self._MOVEMENT_DISPATCH_FUNCTIONS[command](power)
