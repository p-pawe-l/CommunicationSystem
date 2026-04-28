from __future__ import annotations

import dataclasses
import enum
import time
import typing


class DistanceLineMode(enum.Enum):
    NONE = "none"
    ALL = "all"
    MOTHER_ONLY = "mother_only"


@dataclasses.dataclass
class DroneVisualState:
    drone_id: str
    label: str
    color: tuple[int, int, int]
    is_mother: bool = False
    x_ratio: float = 0.5
    y_ratio: float = 0.5
    telemetry: dict[str, typing.Any] = dataclasses.field(default_factory=dict)
    last_update: float = dataclasses.field(default_factory=time.perf_counter)

    def position(self, width: int, height: int, radius: int) -> tuple[int, int]:
        x = radius + self.x_ratio * max(1, width - 2 * radius)
        y = radius + self.y_ratio * max(1, height - 2 * radius)
        return int(x), int(y)
