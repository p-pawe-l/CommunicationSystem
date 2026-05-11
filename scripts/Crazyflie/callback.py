from __future__ import annotations

"""Base callback interfaces for Crazyflie logging.

Each callback declares the Crazyflie log variables it needs and exposes a
``log_func`` method with the signature expected by the matching cflib callback
group. The shared base class registers variables with ``Crazyflie_LogConf`` so
specific callback classes only need to declare the cflib callback group they
attach to.
"""

import abc
import typing

from Crazyflie import logconf as log

Crazyflie_LogVariable = tuple[str, str]
Crazyflie_LogVariables = tuple[Crazyflie_LogVariable, ...]


class CrazyflieBase_Callback(abc.ABC):
    """Base callback that owns shared Crazyflie log variable registration.

    Parameters
    ----------
    vars:
        Tuple of Crazyflie log variable declarations. Each item should contain
        a variable name and cflib type, for example
        ``("stateEstimate.x", "float")``.
    """

    callback_group_name: str
    
    def __init__(self, vars: Crazyflie_LogVariables) -> None:
        self.vars: Crazyflie_LogVariables = vars

    def _add_variables_to_logger(self, logger: log.Crazyflie_LogConf) -> None:
        """Add this callback's log variables to the cflib log config."""
        log_config = logger.get_cflib_LogConfig()
        for var_name, var_type in self.vars:
            log_config.add_variable(var_name, var_type)

    @abc.abstractmethod
    def log_func(self, *args: typing.Any, **kwargs: typing.Any) -> None:
        """Handle one cflib callback event."""
        ...

    def accept(self, logger: log.Crazyflie_LogConf) -> None:
        """Register variables and attach this callback to its cflib event."""
        self._add_variables_to_logger(logger)
        log_config = logger.get_cflib_LogConfig()
        callback_group = getattr(log_config, self.callback_group_name)
        callback_group.add_callback(self.log_func)


class CrazyflieDataReceive_Callback(CrazyflieBase_Callback):
    """Base class for callbacks attached to cflib ``data_received_cb``."""

    callback_group_name = "data_received_cb"
    
    @abc.abstractmethod
    def log_func(self, timestamp, data, logconf) -> None:
        """Handle one telemetry data packet from cflib."""
        ...
     
        
class CrazyflieError_Callback(CrazyflieBase_Callback):
    """Base class for callbacks attached to cflib ``error_cb``."""

    callback_group_name = "error_cb"
    
    @abc.abstractmethod
    def log_func(self, block, msg) -> None:
        """Handle one cflib logging error event."""
        ...
       
        
class CrazyflieStarted_Callback(CrazyflieBase_Callback):
    """Base class for callbacks attached to cflib ``started_cb``."""

    callback_group_name = "started_cb"
    
    @abc.abstractmethod
    def log_func(self, logconf, started) -> None:
        """Handle one cflib logging-started event."""
        ...
        
        
class CrazyflieAdded_Callback(CrazyflieBase_Callback):
    """Base class for callbacks attached to cflib ``added_cb``."""

    callback_group_name = "added_cb"
    
    @abc.abstractmethod
    def log_func(self, logconf, added) -> None:
        """Handle one cflib log-config-added event."""
        ...


Crazyflie_Callback: typing.TypeAlias = (
    CrazyflieDataReceive_Callback
    | CrazyflieError_Callback
    | CrazyflieStarted_Callback
    | CrazyflieAdded_Callback
)
