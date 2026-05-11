from __future__ import annotations

"""Crazyflie telemetry logging configuration helpers.

The ``Crazyflie_LogConf`` class wraps cflib's ``LogConfig`` object and gives
the rest of the project one place to register telemetry callbacks, start log
streaming, and later grow richer logging lifecycle support.
"""

import typing

from cflib.crazyflie import Crazyflie
from cflib.crazyflie.log import LogConfig


class Crazyflie_LogConf:
    """Project wrapper around a cflib ``LogConfig`` instance.

    Parameters
    ----------
    name:
        Name of the logging block registered with the Crazyflie.
    period_in_ms:
        Sampling period for the log block in milliseconds.
    """

    def __init__(self, name: str, period_in_ms: int) -> None:
        self.name: str = name 
        self.period_in_ms: int = period_in_ms
        self.log_config: LogConfig = LogConfig(
            name=self.name,
            period_in_ms=self.period_in_ms
        )
        self.is_started: bool = False
        
    def start(self) -> None:
        """Start telemetry logging for this configuration."""
        self.log_config.start()
        self.is_started = True

    def stop(self) -> None:
        """TODO: Stop telemetry logging for this configuration."""
        raise NotImplementedError("Stopping Crazyflie log configs will be implemented in the future")

    def get_cflib_LogConfig(self) -> LogConfig:
        """Return the wrapped cflib log config.

        This method keeps the original project naming used by existing
        Crazyflie callback code.
        """
        return self.log_config

    def get_cflib_log_config(self) -> LogConfig:
        """Return the wrapped cflib log config using snake_case naming."""
        return self.get_cflib_LogConfig()

    def attach_to(self, cf: Crazyflie) -> None:
        """TODO: Attach this log configuration to a Crazyflie instance.

        Future implementation should call into the Crazyflie logging API and
        register ``self.log_config`` with the provided Crazyflie object.
        """
        raise NotImplementedError("Attaching Crazyflie log configs will be implemented in the future")

    def validate_log_variable(self, variable: tuple[str, str]) -> None:
        """TODO: Validate one Crazyflie log variable declaration.

        Variables should be shaped like ``("stateEstimate.x", "float")``.
        """
        raise NotImplementedError("Log variable validation will be implemented in the future")
        
    def add_callback(self, callback) -> None:
        """Register one callback or a list of callbacks with this log config.

        Callback objects are expected to provide an ``accept(logger)`` method.
        The callback is responsible for adding its variables and cflib event
        handler to the wrapped log configuration.
        """
        if isinstance(callback, list):
            for cb in callback: cb.accept(self)
        else:
            callback.accept(self)

    def add_callbacks(self, callbacks: typing.Iterable[typing.Any]) -> None:
        """TODO: Register multiple callbacks with duplicate tracking.

        Future implementation should accept any iterable of callback objects,
        track already registered callbacks, and avoid duplicate cflib variable
        registration.
        """
        raise NotImplementedError("Batch callback registration will be implemented in the future")

    @classmethod
    def position(cls, period_in_ms: int) -> Crazyflie_LogConf:
        """TODO: Create a preset log config for position telemetry."""
        raise NotImplementedError("Position log presets will be implemented in the future")

    @classmethod
    def engine(cls, period_in_ms: int) -> Crazyflie_LogConf:
        """TODO: Create a preset log config for engine telemetry."""
        raise NotImplementedError("Engine log presets will be implemented in the future")

    @classmethod
    def default_telemetry(cls, period_in_ms: int) -> Crazyflie_LogConf:
        """TODO: Create a preset log config for default telemetry."""
        raise NotImplementedError("Default telemetry presets will be implemented in the future")
            
