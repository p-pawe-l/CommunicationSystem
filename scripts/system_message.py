from __future__ import annotations

import dataclasses
import typing 


@dataclasses.dataclass
class SystemMessage:
    sender: str
    receivers: list[str]
    type: str
    data: dict[str, typing.Any]
    
    def to_dict(self) -> dict[str, typing.Any]:
        return {
            "sender": self.sender,
            "receivers": self.receivers,
            "type": self.type,
            "data": self.data
        }
    
