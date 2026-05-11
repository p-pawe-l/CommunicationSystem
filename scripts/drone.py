"""Base drone client abstraction for the communication system.

This module intentionally contains only hardware-independent drone client
logic. Concrete drone implementations, such as Crazyflie support, live in
their own package modules.
"""

from __future__ import annotations

import drone_system
import func_decorators
import abc
import typing


class DroneClient(abc.ABC):
    """Base client wrapper for a drone connected to ``drone_system``.

    Subclasses provide hardware-specific telemetry generation and inbound
    message processing. Optional custom callables can override those default
    subclass methods when registering the underlying ``drone_system.Client``.
    """

    def __init__(self,
                 system: drone_system.System,
                 data_receivers: list[str], 
                 drone_name: str, 
                 is_mother: bool = False,
                 generating_func: typing.Callable | None = None,
                 processing_func: typing.Callable | None = None) -> None:        
        self.drone_name: str = drone_name
        self.is_mother: bool = is_mother
        self.data_receivers: list[str] = data_receivers

        self.gen_func: typing.Callable = self.generate_drone_data if generating_func is None else generating_func
        self.proc_func: typing.Callable = self.process_drone_data if processing_func is None else processing_func
        
        self.client_instance: drone_system.Client = drone_system.Client(
            self.drone_name, system, self.gen_func, self.proc_func, True
        )
        
        
    @abc.abstractmethod
    @func_decorators.generating_func
    def generate_drone_data(self) -> dict[str, typing.Any]:
        """Build one outbound system message with this drone's current telemetry."""
        pass
        
        
    @abc.abstractmethod
    @func_decorators.processing_func
    def process_drone_data(self, message) -> None:
        """Handle one inbound system message addressed to this drone client."""
        pass
