from __future__ import annotations

from cflib.crazyflie.log import LogConfig


class Crazyflie_LogConf:
    def __init__(self, name: str, period_in_ms: int) -> None:
        self.name: str = name 
        self.period_in_ms: int = period_in_ms
        self.log_config: LogConfig = LogConfig(
            name=self.name,
            period_in_ms=self.period_in_ms
        )
        
    def start(self) -> None:
        self.log_config.start()

    def get_cflib_LogConfig(self) -> LogConfig:
        return self.log_config
        
    def add_callback(self, callback) -> None:
        if isinstance(callback, list):
            for cb in callback: cb.accept(self)
        else:
            callback.accept(self)
            

