"""System message data container and serialization helpers."""

from __future__ import annotations

import dataclasses
import typing


@dataclasses.dataclass
class SystemMessage:
    """Message exchanged between system components."""

    sender: str
    receivers: list[str]
    type: str
    data: dict[str, typing.Any]

    def to_dict(self) -> dict[str, typing.Any]:
        """Return a dictionary representation of the message."""
        return {
            "sender": self.sender,
            "receivers": self.receivers,
            "type": self.type,
            "data": self.data
        }
