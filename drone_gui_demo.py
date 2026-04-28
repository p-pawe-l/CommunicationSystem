from __future__ import annotations

import argparse
import math
import random
import sys
import threading
import time
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT))

import drone_system
from scripts.GUI import DistanceLineMode, DroneMapGUI
from scripts.func_decorators import generating_func, processing_func


TELEMETRY_INTERVAL = 0.02
COMMAND_INTERVAL = 0.02
SMOOTH_STEP_RATIO = 0.0025
TARGET_REACHED_EPSILON = 0.015


class Drone:
    def __init__(
        self,
        system: drone_system.System,
        client_id: str,
        controller_id: str,
        gui_id: str,
        start_pos_ratio: tuple[float, float],
        label: str,
        is_mother: bool = False,
    ) -> None:
        self.client_id = client_id
        self.controller_id = controller_id
        self.gui_id = gui_id
        self.label = label
        self.is_mother = is_mother
        self.x_ratio, self.y_ratio = start_pos_ratio
        self.target_x_ratio = self.x_ratio
        self.target_y_ratio = self.y_ratio
        self.received_commands = 0
        self.telemetry_sent = 0
        self._lock = threading.Lock()
        self.client = drone_system.Client(
            self.client_id,
            system,
            self.generate_data,
            self.process_data,
            False,
        )

    def _move_towards_target(self) -> bool:
        if self.x_ratio < self.target_x_ratio:
            self.x_ratio = min(self.target_x_ratio, self.x_ratio + SMOOTH_STEP_RATIO)
        elif self.x_ratio > self.target_x_ratio:
            self.x_ratio = max(self.target_x_ratio, self.x_ratio - SMOOTH_STEP_RATIO)

        if self.y_ratio < self.target_y_ratio:
            self.y_ratio = min(self.target_y_ratio, self.y_ratio + SMOOTH_STEP_RATIO)
        elif self.y_ratio > self.target_y_ratio:
            self.y_ratio = max(self.target_y_ratio, self.y_ratio - SMOOTH_STEP_RATIO)

        return math.hypot(self.target_x_ratio - self.x_ratio, self.target_y_ratio - self.y_ratio) <= TARGET_REACHED_EPSILON

    @generating_func
    def generate_data(self) -> dict:
        time.sleep(TELEMETRY_INTERVAL)
        with self._lock:
            self.telemetry_sent += 1
            target_reached = self._move_towards_target()

            return {
                "receivers": [self.controller_id, self.gui_id],
                "sender": self.client_id,
                "type": "telemetry",
                "data": {
                    "label": self.label,
                    "x_ratio": self.x_ratio,
                    "y_ratio": self.y_ratio,
                    "target_x_ratio": self.target_x_ratio,
                    "target_y_ratio": self.target_y_ratio,
                    "target_reached": target_reached,
                    "is_mother": self.is_mother,
                    "received_commands": self.received_commands,
                    "telemetry_sent": self.telemetry_sent,
                },
            }

    @processing_func
    def process_data(self, message: dict) -> None:
        if message.get("type") != "command":
            return

        data = message.get("data", {})
        if data.get("command") != "go_to":
            return

        with self._lock:
            self.received_commands += 1
            self.target_x_ratio = min(1.0, max(0.0, float(data.get("x_ratio", self.target_x_ratio))))
            self.target_y_ratio = min(1.0, max(0.0, float(data.get("y_ratio", self.target_y_ratio))))


class Controller:
    def __init__(self, system: drone_system.System, client_id: str, drone_id: str) -> None:
        self.client_id = client_id
        self.drone_id = drone_id
        self.telemetry_received = 0
        self.commands_sent = 0
        self.current_target: tuple[float, float] | None = None
        self.target_reached = True
        self._lock = threading.Lock()
        self.client = drone_system.Client(
            self.client_id,
            system,
            self.generate_data,
            self.process_data,
            False,
        )

    @generating_func
    def generate_data(self) -> dict:
        time.sleep(COMMAND_INTERVAL)
        with self._lock:
            if self.current_target is None or self.target_reached:
                self.current_target = (random.random(), random.random())
                self.target_reached = False
                self.commands_sent += 1

            target_x_ratio, target_y_ratio = self.current_target

        return {
            "receivers": [self.drone_id],
            "sender": self.client_id,
            "type": "command",
            "data": {
                "command": "go_to",
                "x_ratio": target_x_ratio,
                "y_ratio": target_y_ratio,
            },
        }

    @processing_func
    def process_data(self, message: dict) -> None:
        if message.get("type") != "telemetry":
            return

        with self._lock:
            self.telemetry_received += 1
            self.target_reached = bool(message.get("data", {}).get("target_reached", False))


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--duration", type=float, default=None)
    parser.add_argument(
        "--line-mode",
        choices=[mode.value for mode in DistanceLineMode],
        default=DistanceLineMode.ALL.value,
    )
    args = parser.parse_args()

    system = drone_system.System()
    gui = DroneMapGUI(system, client_id="gui", line_mode=args.line_mode)

    specs = [
        ("Alpha", "drone_alpha", "controller_alpha", (0.86, 0.33)),
        ("Bravo", "drone_bravo", "controller_bravo", (0.50, 0.86)),
        ("Charlie", "drone_charlie", "controller_charlie", (0.18, 0.42)),
        ("Delta", "drone_delta", "controller_delta", (0.72, 0.72)),
        ("Echo", "drone_echo", "controller_echo", (0.30, 0.18)),
    ]

    swarm = []
    for label, drone_id, controller_id, start in specs:
        gui.add_drone_instance(drone_id, label=label)
        drone = Drone(system, drone_id, controller_id, "gui", start, label)
        controller = Controller(system, controller_id, drone_id)
        swarm.append((drone, controller))

    for drone, controller in swarm:
        system.attach_client(drone.client)
        system.attach_client(controller.client)

    system.start()
    for drone, controller in swarm:
        drone.client.start()
        controller.client.start()

    try:
        gui.run(duration=args.duration)
    finally:
        for drone, controller in swarm:
            drone.client.stop()
            controller.client.stop()
        system.stop()


if __name__ == "__main__":
    main()
