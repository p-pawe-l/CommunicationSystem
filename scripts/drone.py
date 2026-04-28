from __future__ import annotations

import drone_system
import func_decorators
import abc
import typing
import dataclasses

import cflib
from cflib.crazyflie.syncCrazyflie import SyncCrazyflie
from cflib.crazyflie import Crazyflie
from Crazyflie.logconf import Crazyflie_LogConf
from Crazyflie.pos_cb import Crazyflie_PositionCallback
from Crazyflie.pos_cb import Crazyflie_Position_Values
from Crazyflie.vel_cb import Crazyflie_VelocityCallback
from Crazyflie.vel_cb import Crazyflie_Velocity_Values
from Crazyflie.callback import Crazyflie_Callback
from Crazyflie.move_dispatch import Crazyflie_Movement
from Crazyflie.move_dispatch import Crazyflie_MovementDispatch_Manager
from system_message import SystemMessage


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
    

class Crazyflie_DroneClient(DroneClient):
    def __init__(self,
                 uri: str,
                 system: drone_system.System,
                 drone_name: str,
                 data_receivers: list[str],
                 cb_logger: Crazyflie_LogConf,
                 init_callbacks: list[Crazyflie_Callback] = [Crazyflie_PositionCallback(), Crazyflie_VelocityCallback()],
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
        self.drone_velocity: Crazyflie_Velocity_Values = Crazyflie_Velocity_Values(0, 0, 0)
        
        cflib.crtp.init_drivers()
        self.drone: SyncCrazyflie = SyncCrazyflie(link_uri=uri, cf=Crazyflie(rw_cache='./cache'))
        self.drone.open_link()
        self.drone.cf.supervisor.send_arming_request(True)
        self.drone.cf.commander.send_setpoint(**dataclasses.asdict(self.drone_setpoint))

        self.drone.cf.log.add_config(self.cb_logger.get_cflib_LogConfig())
        self.cb_logger.start()
        self.movement_dispatch_manager: Crazyflie_MovementDispatch_Manager = Crazyflie_MovementDispatch_Manager(
            change_roll=self._change_roll,
            change_pitch=self._change_pitch,
            change_yawrate=self._change_yawrate,
            change_thrust=self._change_thrust,
        )        
    
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
        msg: SystemMessage = SystemMessage(
            sender=self.drone_name,
            receivers=self.data_receivers,
            data={
                "position": self.drone_position,
                "velocity": self.drone_velocity,
            }
        )
        return msg.to_dict()

    @func_decorators.processing_func
    def process_drone_data(self, message):
        data: dict[str, typing.Any] = message.get("data")
        power: float = data.get("power")
        command: Crazyflie_Movement = data.get("command")
        
        self.movement_dispatch_manager.dispatch(command, power)
