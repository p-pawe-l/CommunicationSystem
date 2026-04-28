from __future__ import annotations

import math
import random
import time
import typing

import drone_system
import pygame

from .drawing import draw_centered_text, draw_engineering_grid, draw_panel, draw_text
from .models import DistanceLineMode, DroneVisualState
from .stats_window import GOLD, StatsWindow


WIDTH = 980
HEIGHT = 720
DRONE_RADIUS = 9
INBOX_DRAIN_LIMIT = 512
SAFE_DISTANCE = 210.0
WARNING_DISTANCE = 115.0
CRITICAL_LINE = (244, 82, 82)
WARNING_LINE = (245, 180, 82)
SAFE_LINE = (80, 220, 145)


class DroneMapGUI:
    def __init__(
        self,
        system: drone_system.System,
        client_id: str = "gui",
        size: tuple[int, int] = (WIDTH, HEIGHT),
        line_mode: DistanceLineMode | str = DistanceLineMode.ALL,
    ) -> None:
        pygame.init()
        self.system = system
        self.client_id = client_id
        self.screen = pygame.display.set_mode(size, pygame.RESIZABLE)
        pygame.display.set_caption("Drone Swarm Map")
        self.clock = pygame.time.Clock()
        self.font = pygame.font.SysFont("consolas", 17)
        self.small_font = pygame.font.SysFont("consolas", 14)
        self.title_font = pygame.font.SysFont("consolas", 20, bold=True)
        self.drones: dict[str, DroneVisualState] = {}
        self.stats_window: StatsWindow | None = None
        self.line_mode = self._line_mode_from_value(line_mode)
        self.show_distance_labels = True
        self.running = False
        self.width, self.height = size
        self.show_stats_button = pygame.Rect(18, 16, 210, 36)
        self._system_registered = False
        self._palette_index = 0
        self._palette = [
            (72, 207, 173),
            (116, 169, 255),
            (232, 121, 249),
            (149, 213, 107),
            (245, 180, 82),
            (95, 229, 255),
        ]
        self._register_system_inbox()

    def _register_system_inbox(self) -> None:
        if self._system_registered:
            return

        self.system.attach_client(self.client_id)
        self._system_registered = True

    def _line_mode_from_value(self, value: DistanceLineMode | str) -> DistanceLineMode:
        if isinstance(value, DistanceLineMode):
            return value
        return DistanceLineMode(value)

    def set_line_mode(self, line_mode: DistanceLineMode | str) -> None:
        self.line_mode = self._line_mode_from_value(line_mode)

    def add_drone_instance(
        self,
        drone_id: str,
        label: str | None = None,
        color: tuple[int, int, int] | None = None,
        is_mother: bool = False,
    ) -> DroneVisualState:
        if drone_id in self.drones:
            drone = self.drones[drone_id]
            drone.label = label or drone.label
            drone.color = color or drone.color
            drone.is_mother = is_mother or drone.is_mother
            return drone

        if color is None:
            color = self._palette[self._palette_index % len(self._palette)]
            self._palette_index += 1

        drone = DroneVisualState(
            drone_id=drone_id,
            label=label or drone_id,
            color=GOLD if is_mother else color,
            is_mother=is_mother,
        )
        self.drones[drone_id] = drone
        return drone

    def _ensure_mother_drone(self) -> None:
        if not self.drones:
            return
        if any(drone.is_mother for drone in self.drones.values()):
            return

        mother = random.choice(list(self.drones.values()))
        mother.is_mother = True
        mother.color = GOLD

    def _open_stats_window(self) -> None:
        if self.stats_window is None or not self.stats_window.is_open:
            self.stats_window = StatsWindow()

    def _close_stats_window(self) -> None:
        if self.stats_window is not None:
            self.stats_window.close()
            self.stats_window = None

    def _toggle_stats_window(self) -> None:
        if self.stats_window is None or not self.stats_window.is_open:
            self._open_stats_window()
        else:
            self._close_stats_window()

    def _handle_events(self) -> None:
        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                self.running = False
            elif event.type == pygame.WINDOWCLOSE:
                if self.stats_window is not None and self.stats_window.matches_event(event):
                    self._close_stats_window()
                else:
                    self.running = False
            elif event.type == pygame.VIDEORESIZE:
                self.width = max(event.w, 520)
                self.height = max(event.h, 360)
                self.screen = pygame.display.set_mode((self.width, self.height), pygame.RESIZABLE)
            elif event.type == pygame.MOUSEBUTTONDOWN and event.button == 1:
                if self.stats_window is not None and self.stats_window.matches_event(event):
                    continue
                if self.show_stats_button.collidepoint(event.pos):
                    self._toggle_stats_window()

    def _drain_inbox(self) -> None:
        for _ in range(INBOX_DRAIN_LIMIT):
            message = self.system.receive(self.client_id)
            if message is None:
                return
            self._process_message(message)

    def _process_message(self, message: dict[str, typing.Any]) -> None:
        if message.get("type") not in {"telemetry", "drone_state"}:
            return

        drone_id = str(message.get("sender", "unknown_drone"))
        data = dict(message.get("data", {}))
        drone = self.add_drone_instance(
            drone_id,
            label=str(data.get("label", drone_id)),
            is_mother=bool(data.get("is_mother", False)),
        )

        x_value = data.get("x_ratio", data.get("x", drone.x_ratio))
        y_value = data.get("y_ratio", data.get("y", drone.y_ratio))
        drone.x_ratio = self._normalize_position(float(x_value), self.width)
        drone.y_ratio = self._normalize_position(float(y_value), self.height)
        drone.telemetry = data
        drone.last_update = time.perf_counter()
        if bool(data.get("is_mother", False)):
            drone.is_mother = True
            drone.color = GOLD

    def _normalize_position(self, value: float, axis_size: int) -> float:
        if 0.0 <= value <= 1.0:
            return value
        return min(1.0, max(0.0, value / max(1, axis_size)))

    def _line_color(self, distance: float) -> tuple[int, int, int]:
        if distance <= WARNING_DISTANCE:
            return CRITICAL_LINE
        if distance <= SAFE_DISTANCE:
            return WARNING_LINE
        return SAFE_LINE

    def _distance_pairs(self) -> list[tuple[DroneVisualState, DroneVisualState]]:
        drones = list(self.drones.values())
        if self.line_mode == DistanceLineMode.NONE:
            return []
        if self.line_mode == DistanceLineMode.ALL:
            return [(a, b) for index, a in enumerate(drones) for b in drones[index + 1:]]

        mothers = [drone for drone in drones if drone.is_mother]
        if not mothers:
            return []
        mother = mothers[0]
        return [(mother, drone) for drone in drones if drone.drone_id != mother.drone_id]

    def _draw_button(self) -> None:
        mouse_pos = pygame.mouse.get_pos()
        border = (255, 205, 84) if self.show_stats_button.collidepoint(mouse_pos) else (58, 72, 84)
        draw_panel(self.screen, self.show_stats_button, border)
        draw_text(self.screen, self.font, "Show window statistic", (34, 24), (230, 238, 242))

    def _draw_hud(self) -> None:
        routed = self.system.routed_messages()
        rect = pygame.Rect(18, self.height - 74, 300, 52)
        draw_panel(self.screen, rect)
        draw_text(self.screen, self.small_font, f"GUI inbox: {self.client_id}", (34, self.height - 62), (132, 145, 160))
        draw_text(self.screen, self.font, f"Messages routed: {routed}", (34, self.height - 42))

    def _draw_distance_lines(self) -> None:
        for drone_a, drone_b in self._distance_pairs():
            pos_a = drone_a.position(self.width, self.height, DRONE_RADIUS)
            pos_b = drone_b.position(self.width, self.height, DRONE_RADIUS)
            distance = math.dist(pos_a, pos_b)
            color = self._line_color(distance)
            midpoint = ((pos_a[0] + pos_b[0]) // 2, (pos_a[1] + pos_b[1]) // 2)
            pygame.draw.line(self.screen, color, pos_a, pos_b, 2)
            if self.show_distance_labels:
                draw_centered_text(
                    self.screen,
                    self.small_font,
                    f"{distance:.1f}px",
                    (midpoint[0], midpoint[1] - 12),
                    color,
                )

    def _draw_regular_drone(self, drone: DroneVisualState, pos: tuple[int, int]) -> None:
        pygame.draw.circle(self.screen, drone.color, pos, DRONE_RADIUS)
        pygame.draw.circle(self.screen, (218, 228, 236), pos, DRONE_RADIUS + 5, 1)
        pygame.draw.line(self.screen, drone.color, (pos[0] - 14, pos[1]), (pos[0] + 14, pos[1]), 1)
        pygame.draw.line(self.screen, drone.color, (pos[0], pos[1] - 14), (pos[0], pos[1] + 14), 1)

    def _draw_mother_drone(self, drone: DroneVisualState, pos: tuple[int, int]) -> None:
        pygame.draw.circle(self.screen, (44, 32, 12), pos, DRONE_RADIUS + 10)
        pygame.draw.circle(self.screen, GOLD, pos, DRONE_RADIUS + 9, 2)
        pygame.draw.circle(self.screen, GOLD, pos, DRONE_RADIUS)
        crown = [
            (pos[0] - 10, pos[1] - 15),
            (pos[0] - 4, pos[1] - 24),
            (pos[0], pos[1] - 16),
            (pos[0] + 5, pos[1] - 24),
            (pos[0] + 11, pos[1] - 15),
        ]
        pygame.draw.lines(self.screen, GOLD, False, crown, 2)
        draw_centered_text(self.screen, self.small_font, "M", pos, (28, 28, 20))

    def _draw_drones(self) -> None:
        for drone in self.drones.values():
            pos = drone.position(self.width, self.height, DRONE_RADIUS)
            if drone.is_mother:
                self._draw_mother_drone(drone, pos)
            else:
                self._draw_regular_drone(drone, pos)
            draw_centered_text(self.screen, self.small_font, drone.label, (pos[0], pos[1] + 26), drone.color)

    def _draw(self) -> None:
        draw_engineering_grid(self.screen, self.width, self.height)
        self._draw_distance_lines()
        self._draw_drones()
        self._draw_button()
        self._draw_hud()
        pygame.display.flip()

        if self.stats_window is not None and self.stats_window.is_open:
            self.stats_window.draw(list(self.drones.values()), self.system.routed_messages())

    def run(self, duration: float | None = None) -> None:
        self._ensure_mother_drone()
        self.running = True
        deadline = None if duration is None else time.perf_counter() + duration
        try:
            while self.running:
                if deadline is not None and time.perf_counter() >= deadline:
                    self.running = False

                self._handle_events()
                self._drain_inbox()
                self._ensure_mother_drone()
                self._draw()
                self.clock.tick(60)
        finally:
            self._close_stats_window()
            pygame.quit()
