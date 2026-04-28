from __future__ import annotations

import pygame
from pygame._sdl2.video import Renderer, Texture, Window

from .drawing import draw_panel, draw_text
from .models import DroneVisualState


GOLD = (255, 205, 84)


class StatsWindow:
    def __init__(self, size: tuple[int, int] = (980, 720)) -> None:
        self.surface = pygame.Surface(size)
        self.window = Window("Drone System Statistics", size=size, resizable=True)
        self.renderer = Renderer(self.window)
        self.title_font = pygame.font.SysFont("consolas", 26, bold=True)
        self.card_font = pygame.font.SysFont("consolas", 16)
        self.small_font = pygame.font.SysFont("consolas", 14)
        self.is_open = True

    @property
    def window_id(self) -> int:
        return self.window.id

    def matches_event(self, event: pygame.event.Event) -> bool:
        return getattr(event, "window", None) == self.window_id

    def close(self) -> None:
        if self.is_open:
            self.is_open = False
            self.window.destroy()

    def draw_metric(self, label: str, value: object, rect: pygame.Rect, row: int) -> None:
        y = rect.y + 58 + row * 24
        draw_text(self.surface, self.small_font, label, (rect.x + 16, y), (132, 145, 160))
        draw_text(self.surface, self.card_font, str(value), (rect.x + 160, y - 2))

    def draw(self, drones: list[DroneVisualState], routed_messages: int) -> None:
        if not self.is_open:
            return

        width, height = self.window.size
        if self.surface.get_size() != (width, height):
            self.surface = pygame.Surface((width, height))

        self.surface.fill((12, 17, 22))
        header = pygame.Rect(20, 18, width - 40, 78)
        draw_panel(self.surface, header)
        draw_text(self.surface, self.title_font, "Drone System Statistics", (44, 34))
        draw_text(
            self.surface,
            self.card_font,
            f"Messages routed: {routed_messages}",
            (44, 66),
            (164, 175, 188),
        )

        columns = 3 if width >= 1180 else 2 if width >= 760 else 1
        gap = 18
        margin = 20
        top = 116
        card_width = max(280, (width - margin * 2 - gap * (columns - 1)) // columns)
        card_height = 214

        for index, drone in enumerate(drones):
            col = index % columns
            row = index // columns
            rect = pygame.Rect(
                margin + col * (card_width + gap),
                top + row * (card_height + gap),
                card_width,
                card_height,
            )
            if rect.y > height:
                continue

            outline = GOLD if drone.is_mother else (48, 60, 72)
            draw_panel(self.surface, rect, outline)
            pygame.draw.circle(self.surface, drone.color, (rect.x + 26, rect.y + 28), 8)
            if drone.is_mother:
                pygame.draw.circle(self.surface, GOLD, (rect.x + 26, rect.y + 28), 14, 2)
            draw_text(self.surface, self.card_font, drone.label, (rect.x + 46, rect.y + 18), drone.color)

            self.draw_metric("role", "mother" if drone.is_mother else "drone", rect, 0)
            self.draw_metric("position", f"{drone.x_ratio:.3f}, {drone.y_ratio:.3f}", rect, 1)
            self.draw_metric("last update", f"{drone.last_update:.2f}", rect, 2)

            metric_row = 3
            for key, value in sorted(drone.telemetry.items()):
                if key in {"x", "y", "x_ratio", "y_ratio", "is_mother", "color"}:
                    continue
                self.draw_metric(str(key), value, rect, metric_row)
                metric_row += 1
                if metric_row > 6:
                    break

        self.renderer.draw_color = (12, 17, 22, 255)
        self.renderer.clear()
        texture = Texture.from_surface(self.renderer, self.surface)
        texture.draw()
        self.renderer.present()
