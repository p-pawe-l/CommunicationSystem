from __future__ import annotations

from dataclasses import dataclass, asdict
from typing import Any
from enum import Enum

class SystemMessageType(Enum):
    PING = "PING"


@dataclass
class SystemMessage:
    sender: str
    receiver: str
    type: SystemMessageType
    data: dict[str, Any]
    
    def toDict(self) -> dict[str, Any]:
        return asdict(self)