from __future__ import annotations

import pygame


GRID_SIZE = 25


def draw_text(
    surface: pygame.Surface,
    font: pygame.font.Font,
    text: str,
    pos: tuple[int, int],
    color: tuple[int, int, int] = (230, 238, 242),
) -> None:
    surface.blit(font.render(text, True, color), pos)


def draw_centered_text(
    surface: pygame.Surface,
    font: pygame.font.Font,
    text: str,
    center: tuple[int, int],
    color: tuple[int, int, int] = (230, 238, 242),
) -> None:
    rendered = font.render(text, True, color)
    surface.blit(rendered, rendered.get_rect(center=center))


def draw_engineering_grid(surface: pygame.Surface, width: int, height: int) -> None:
    surface.fill((12, 17, 22))
    grid_color = (31, 40, 48)
    strong_grid_color = (43, 54, 64)
    axis_color = (68, 82, 94)

    for x in range(0, width + 1, GRID_SIZE):
        if x == width // 2:
            color = axis_color
        else:
            color = strong_grid_color if x % (GRID_SIZE * 4) == 0 else grid_color
        pygame.draw.line(surface, color, (x, 0), (x, height), 1)

    for y in range(0, height + 1, GRID_SIZE):
        if y == height // 2:
            color = axis_color
        else:
            color = strong_grid_color if y % (GRID_SIZE * 4) == 0 else grid_color
        pygame.draw.line(surface, color, (0, y), (width, y), 1)


def draw_panel(
    surface: pygame.Surface,
    rect: pygame.Rect,
    border_color: tuple[int, int, int] = (50, 62, 73),
) -> None:
    pygame.draw.rect(surface, (15, 21, 27), rect, border_radius=14)
    pygame.draw.rect(surface, border_color, rect, width=1, border_radius=14)
