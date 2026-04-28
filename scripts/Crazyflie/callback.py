from __future__ import annotations

import abc
from Crazyflie import logconf as log


class CrazyflieDataReceive_Callback(abc.ABC):
    
    def __init__(self, vars: tuple[str, str]) -> None:
        self.vars: tuple[str, str] = vars
    
    @abc.abstractmethod
    def log_func(self, timestamp, data, logconf) -> None:
        ...
        
    def accept(self, logger: log.Crazyflie_LogConf) -> None:
        print("Added data receive callback!")
        for var in self.vars:
            var_name: str = var[0]
            var_type: str = var[1]
            logger.get_cflib_LogConfig().add_variable(var_name, var_type)
        logger.get_cflib_LogConfig().data_received_cb.add_callback(self.log_func)
     
        
class CrazyflieError_Callback(abc.ABC):

    def __init__(self, vars: tuple[str, str]) -> None:
        self.vars: tuple[str, str] = vars

    @abc.abstractmethod
    def log_func(self, block, msg) -> None:
        ...
        
    def accept(self, logger: log.Crazyflie_LogConf) -> None:
        for var in self.vars:
            var_name: str = var[0]
            var_type: str = var[1]
            logger.get_cflib_LogConfig().add_variable(var_name, var_type)
        logger.get_cflib_LogConfig().error_cb.add_callback(self.log_func)
       
        
class CrazyflieStarted_Callback(abc.ABC):
    
    def __init__(self, vars: tuple[str, str]) -> None:
        self.vars: tuple[str, str] = vars
    
    @abc.abstractmethod
    def log_func(self, logconf, started) -> None:
        ...
      
    def accept(self, logger: log.Crazyflie_LogConf) -> None:
        for var in self.vars:
            var_name: str = var[0]
            var_type: str = var[1]
            logger.get_cflib_LogConfig().add_variable(var_name, var_type)
        logger.get_cflib_LogConfig().started_cb.add_callback(self.log_func)
        
        
class CrazyflieAdded_Callback(abc.ABC):
    
    def __init__(self, vars: tuple[str, str]) -> None:
        self.vars: tuple[str, str] = vars
    
    @abc.abstractmethod
    def log_func(self, logconf, added) -> None:
        ...
        
    def accept(self, logger: log.Crazyflie_LogConf) -> None:
        print("Added on add callback!")
        for var in self.vars:
            var_name: str = var[0]
            var_type: str = var[1]
            logger.get_cflib_LogConfig().add_variable(var_name, var_type)
        logger.get_cflib_LogConfig().added_cb.add_callback(self.log_func)


Crazyflie_Callback = \
CrazyflieDataReceive_Callback | \
CrazyflieError_Callback | \
CrazyflieStarted_Callback | \
CrazyflieAdded_Callback 
