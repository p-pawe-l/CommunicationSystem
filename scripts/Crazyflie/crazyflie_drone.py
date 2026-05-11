"""Crazyflie drone client implementation.

This module contains the Crazyflie-specific ``DroneClient`` implementation,
including cflib setup, telemetry callback wiring, setpoint updates, and
movement command dispatch. The generic base class remains in ``drone.py`` so
other drone types can be added without importing Crazyflie hardware
dependencies.
"""

from __future__ import annotations

import dataclasses
import logging
import typing
from dataclasses import fields

import cflib
import drone_system
import func_decorators
from cflib.crazyflie import Crazyflie
from cflib.crazyflie.syncCrazyflie import SyncCrazyflie
from drone import DroneClient
from system_message import SystemMessage

from Crazyflie.callback import Crazyflie_Callback
from Crazyflie.engine_cb import Crazyflie_Engine_Values, Crazyflie_EngineCallback
from Crazyflie.logconf import Crazyflie_LogConf
from Crazyflie.move_dispatch import Crazyflie_MovementDispatch_Manager
from Crazyflie.pos_cb import Crazyflie_Position_Values, Crazyflie_PositionCallback
from Crazyflie.vel_cb import Crazyflie_Velocity_Values

LOGGER = logging.getLogger(name=__name__)


@dataclasses.dataclass
class Crazyflie_SetPoint_Values:
    """Current commander setpoint sent to the Crazyflie."""

    roll: float
    pitch: float
    yawrate: float
    thrust: int

    def to_dict(self) -> dict[str, int | float]:
        """Return values using the keyword names expected by cflib."""
        return {
            "roll": self.roll,
            "pitch": self.pitch,
            "yawrate": self.yawrate,
            "thrust": self.thrust,
        }


@dataclasses.dataclass
class CrazyflieDroneState_Data:
    """Snapshot of telemetry values read from Crazyflie callbacks."""

    position: Crazyflie_Position_Values | None
    velocity: Crazyflie_Velocity_Values | None
    enigne: Crazyflie_Engine_Values | None

    @classmethod
    def from_dict(cls, data: dict[str, typing.Any]) -> CrazyflieDroneState_Data:
        """Build a state snapshot from a dictionary with matching field names."""
        if any(key.name not in data for key in fields(cls)):
            raise KeyError("Invalid keys provided for transformation")
        return CrazyflieDroneState_Data(**data)

    def to_dict(self) -> dict[str, typing.Any]:
        """Return telemetry values as plain dictionaries."""
        data: dict[str, typing.Any] = {}
        for key in fields(self):
            value = getattr(self, key.name)
            data[key.name] = {} if value is None else value.to_dict()

        return data


class NoDataReceiversException(Exception):
    """Raised when a Crazyflie client is created without message receivers."""


class NoCallbackFoundException(Exception):
    """Raised when required telemetry callback data is unavailable."""

    def __init__(self, er_msg: str, missing_cb_key: str) -> None:
        super().__init__(er_msg)
        self.missing_cb_key: str = missing_cb_key


class InvalidKeyException(Exception):
    """Raised when an incoming command message is missing a required key."""

    def __init__(self, er_msg: str, wrong_key: str) -> None:
        super().__init__(er_msg)
        self.wrong_key: str = wrong_key


class Crazyflie_DroneClient(DroneClient):
    """Drone client backed by a Bitcraze Crazyflie connection."""

    def __init__(
        self,
        uri: str,
        system: drone_system.System,
        drone_name: str,
        data_receivers: list[str],
        cb_logger: Crazyflie_LogConf,
        is_mother: bool = False,
        generating_func: typing.Callable | None = None,
        processing_func: typing.Callable | None = None,
    ) -> None:
        """Connect to a Crazyflie and register telemetry and movement handlers."""
        if not data_receivers:
            LOGGER.error("Set data receivers before attaching client to the system")
            raise NoDataReceiversException("No data receivers")

        super().__init__(
            system=system,
            drone_name=drone_name,
            data_receivers=data_receivers,
            is_mother=is_mother,
            generating_func=generating_func,
            processing_func=processing_func,
        )
        self.uri: str = uri
        self.cb_logger: Crazyflie_LogConf = cb_logger

        self.callbacks: dict[str, Crazyflie_Callback] = {
            "POSITION_CALLBACK": Crazyflie_PositionCallback(),
            # Will be added in the future - data about velocity.
            # "VELOCITY_CALLBACK": Crazyflie_VelocityCallback()
            "ENGINE_CALLBACK": Crazyflie_EngineCallback(),
        }

        self.drone_setpoint: Crazyflie_SetPoint_Values = Crazyflie_SetPoint_Values(0, 0, 0, 0)

        cflib.crtp.init_drivers()
        self.drone: SyncCrazyflie = SyncCrazyflie(link_uri=uri, cf=Crazyflie(rw_cache="./cache"))
        self.drone.open_link()
        self.drone.cf.supervisor.send_arming_request(True)
        self._update_drone_setpoint()

        self.drone.cf.log.add_config(self.cb_logger.get_cflib_LogConfig())
        self.cb_logger.start()

        self.movement_dispatch_manager: Crazyflie_MovementDispatch_Manager = Crazyflie_MovementDispatch_Manager(
            change_roll=self._change_roll,
            change_pitch=self._change_pitch,
            change_yawrate=self._change_yawrate,
            change_thrust=self._change_thrust,
        )

    def _update_drone_setpoint(self) -> None:
        """Send the current setpoint to the Crazyflie commander."""
        self.drone.cf.commander.send_setpoint(**self.drone_setpoint.to_dict())

    def _change_roll(self, new_roll: float) -> None:
        """Update the roll setpoint and immediately send it to the Crazyflie."""
        self.drone_setpoint.roll = new_roll
        self._update_drone_setpoint()

    def _change_pitch(self, new_pitch: float) -> None:
        """Update the pitch setpoint and immediately send it to the Crazyflie."""
        self.drone_setpoint.pitch = new_pitch
        self._update_drone_setpoint()

    def _change_yawrate(self, new_yawrate: float) -> None:
        """Update the yaw-rate setpoint and immediately send it to the Crazyflie."""
        self.drone_setpoint.yawrate = new_yawrate
        self._update_drone_setpoint()

    def _change_thrust(self, new_thrust: int) -> None:
        """Update the thrust setpoint and immediately send it to the Crazyflie."""
        self.drone_setpoint.thrust = new_thrust
        self._update_drone_setpoint()

    def _get_error_system_msg(self, err_msg: str) -> SystemMessage:
        """Create a serialized system error message for this drone."""
        return SystemMessage(
            sender=self.drone_name,
            receivers=self.data_receivers,
            type="error",
            data={
                "content": err_msg,
            },
        )

    def _get_telemetry_system_msg(
        self,
        position: Crazyflie_Position_Values,
        # velocity: Crazyflie_VelocityValues,
        engine: Crazyflie_Engine_Values,
    ) -> SystemMessage:
        """Create a system telemetry message from the latest callback values."""
        return SystemMessage(
            sender=self.drone_name,
            receivers=self.data_receivers,
            type="telemetry",
            data={
                "position": position.to_dict(),
                # "velocity": velocity.to_dict(),
                "engine": engine.to_dict(),
            },
        )

    def _read_callback_data(self, cb_id: str, missing_cb_msg: str) -> typing.Any:
        """Read data from a named telemetry callback."""
        callback_data = self.callbacks.get(cb_id, None)
        if callback_data is None:
            raise NoCallbackFoundException(missing_cb_msg, cb_id)

        if cb_id == "POSITION_CALLBACK" and isinstance(callback_data, Crazyflie_PositionCallback):
            return callback_data.get_drone_position()
        if cb_id == "ENGINE_CALLBACK" and isinstance(callback_data, Crazyflie_EngineCallback):
            return callback_data.get_drone_engine_data()

        raise NoCallbackFoundException(missing_cb_msg, cb_id)

    @func_decorators.generating_func
    def generate_drone_data(self) -> dict[str, typing.Any]:
        """Build the outgoing telemetry message for this drone.

        Reads the current telemetry snapshots from required callbacks and
        returns them as a serialized system message. If any required callback
        is missing, returns a serialized error message instead.
        """
        try:
            position_data = self._read_callback_data(
                "POSITION_CALLBACK",
                "Missing callback for reading drone position",
            )
            # velocity_data = self._read_callback_data("VELOCITY_CALLBACK", "Missing callback for reading drone velocity")
            engine_data = self._read_callback_data(
                "ENGINE_CALLBACK",
                "Missing callback for reading drone engine data",
            )

            return self._get_telemetry_system_msg(
                position=position_data,
                # velocity=velocity_data.get_drone_velocity(),
                engine=engine_data,
            ).to_dict()
        except NoCallbackFoundException as e:
            LOGGER.debug(f"Missing {e.missing_cb_key} in callbacks | Drone {self.drone_name}")
            return self._get_error_system_msg(str(e)).to_dict()

    def _get_dict_data(self, pckg: dict[str, typing.Any], key: str) -> typing.Any:
        """Return a required key from a dictionary-like message payload."""
        data: typing.Any = pckg.get(key, None)
        if data is None:
            raise InvalidKeyException(er_msg="Provided invalid key", wrong_key=key)
        return data

    @func_decorators.processing_func
    def process_drone_data(self, message: dict[str, typing.Any]) -> None:
        """Apply an incoming movement command to the drone.

        Expects a message containing a ``data`` payload with ``power`` and
        ``command`` fields. If any required field is missing, logs the issue
        and ignores the message.
        """
        try:
            data: dict[str, typing.Any] = self._get_dict_data(message, "data")
            power: float = self._get_dict_data(data, "power")
            command: str = self._get_dict_data(data, "command")

            self.movement_dispatch_manager.dispatch(command, power)
        except InvalidKeyException as e:
            LOGGER.debug(f"Missing data field: {e.wrong_key} in incoming package | Drone {self.drone_name}")
            return
