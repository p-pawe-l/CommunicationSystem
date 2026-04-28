import sys
import threading
import time
import argparse
import math
import random
from pathlib import Path

ROOT = Path(__file__).resolve().parent
sys.path.insert(0, str(ROOT / "build"))
sys.path.insert(0, str(ROOT))

import drone_system
import pygame
from pygame._sdl2.video import Renderer, Texture, Window
from scripts.func_decorators import generating_func, processing_func


WIDTH = 900
HEIGHT = 520
STATS_WIDTH = 980
STATS_HEIGHT = 720
DRONE_RADIUS = 9
ROUTE_LIMIT_PER_FRAME = 96
SMOOTH_STEP_PIXELS = 1
TARGET_REACHED_EPSILON = 2
GRID_SIZE = 25


class Drone:
    def __init__(self, system, client_id, controller_id, start_pos_ratio, color):
        self.client_id = client_id
        self.controller_id = controller_id
        self.x_ratio, self.y_ratio = start_pos_ratio
        self.target_x_ratio = self.x_ratio
        self.target_y_ratio = self.y_ratio
        self.window_width = WIDTH
        self.window_height = HEIGHT
        self.color = color
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

    @generating_func
    def generate_data(self):
        time.sleep(0.02)
        with self._lock:
            self.telemetry_sent += 1
            x_ratio = self.x_ratio
            y_ratio = self.y_ratio
            target_x_ratio = self.target_x_ratio
            target_y_ratio = self.target_y_ratio
            distance = math.hypot(
                (target_x_ratio - x_ratio) * max(1, self.window_width - 2 * DRONE_RADIUS),
                (target_y_ratio - y_ratio) * max(1, self.window_height - 2 * DRONE_RADIUS),
            )

        return {
            "receivers": [self.controller_id],
            "sender": self.client_id,
            "type": "telemetry",
            "data": {
                "x_ratio": x_ratio,
                "y_ratio": y_ratio,
                "target_x_ratio": target_x_ratio,
                "target_y_ratio": target_y_ratio,
                "target_reached": distance <= TARGET_REACHED_EPSILON,
            },
        }

    @processing_func
    def process_data(self, message):
        if message.get("type") != "command":
            return

        command = message.get("data", {}).get("command")
        with self._lock:
            self.received_commands += 1
            if command == "go_to":
                data = message.get("data", {})
                self.target_x_ratio = min(1.0, max(0.0, data.get("x_ratio", self.target_x_ratio)))
                self.target_y_ratio = min(1.0, max(0.0, data.get("y_ratio", self.target_y_ratio)))

    def update(self, width, height):
        with self._lock:
            self.window_width = width
            self.window_height = height
            x_step = SMOOTH_STEP_PIXELS / max(1, width - 2 * DRONE_RADIUS)
            y_step = SMOOTH_STEP_PIXELS / max(1, height - 2 * DRONE_RADIUS)

            if self.x_ratio < self.target_x_ratio:
                self.x_ratio = min(self.target_x_ratio, self.x_ratio + x_step)
            elif self.x_ratio > self.target_x_ratio:
                self.x_ratio = max(self.target_x_ratio, self.x_ratio - x_step)

            if self.y_ratio < self.target_y_ratio:
                self.y_ratio = min(self.target_y_ratio, self.y_ratio + y_step)
            elif self.y_ratio > self.target_y_ratio:
                self.y_ratio = max(self.target_y_ratio, self.y_ratio - y_step)

    def snapshot(self, width, height):
        with self._lock:
            self.window_width = width
            self.window_height = height
            return {
                "x": DRONE_RADIUS + self.x_ratio * (width - 2 * DRONE_RADIUS),
                "y": DRONE_RADIUS + self.y_ratio * (height - 2 * DRONE_RADIUS),
                "target_x": DRONE_RADIUS + self.target_x_ratio * (width - 2 * DRONE_RADIUS),
                "target_y": DRONE_RADIUS + self.target_y_ratio * (height - 2 * DRONE_RADIUS),
                "x_ratio": self.x_ratio,
                "y_ratio": self.y_ratio,
                "received_commands": self.received_commands,
                "telemetry_sent": self.telemetry_sent,
            }


class Controller:
    def __init__(self, system, client_id, drone_id):
        self.client_id = client_id
        self.drone_id = drone_id
        self.telemetry_received = 0
        self.commands_sent = 0
        self.last_drone_pos = None
        self.current_target = None
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
    def generate_data(self):
        time.sleep(0.02)
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
    def process_data(self, message):
        if message.get("type") != "telemetry":
            return

        with self._lock:
            self.telemetry_received += 1
            self.last_drone_pos = dict(message.get("data", {}))
            self.target_reached = bool(self.last_drone_pos.get("target_reached", False))

    def snapshot(self):
        with self._lock:
            return {
                "telemetry_received": self.telemetry_received,
                "commands_sent": self.commands_sent,
                "last_drone_pos": self.last_drone_pos,
                "current_target": self.current_target,
                "target_reached": self.target_reached,
            }


class DemoRouter:
    def __init__(self, clients):
        self.clients = clients
        self.routed = 0

    def pump_client(self, source_id):
        source = self.clients[source_id]
        for _ in range(ROUTE_LIMIT_PER_FRAME):
            message = source.pop_outbox()
            if message is None:
                return

            for receiver in message.get("receivers", []):
                target = self.clients.get(receiver)
                if target is not None:
                    target.push_inbox(message)
                    self.routed += 1

    def pump(self):
        for client_id in self.clients:
            self.pump_client(client_id)


def draw_text(surface, font, text, pos, color=(230, 238, 242)):
    surface.blit(font.render(text, True, color), pos)


def draw_centered_text(surface, font, text, center, color=(230, 238, 242)):
    rendered = font.render(text, True, color)
    surface.blit(rendered, rendered.get_rect(center=center))


def draw_grid(surface, width, height):
    grid_color = (37, 45, 52)
    axis_color = (58, 68, 78)

    for x in range(0, width + 1, GRID_SIZE):
        color = axis_color if x == width // 2 else grid_color
        pygame.draw.line(surface, color, (x, 0), (x, height), 1)

    for y in range(0, height + 1, GRID_SIZE):
        color = axis_color if y == height // 2 else grid_color
        pygame.draw.line(surface, color, (0, y), (width, y), 1)


class StatsWindow:
    def __init__(self, font):
        self.surface = pygame.Surface((STATS_WIDTH, STATS_HEIGHT))
        self.window = Window("Drone System Stats", size=(STATS_WIDTH, STATS_HEIGHT), resizable=True)
        self.renderer = Renderer(self.window)
        self.font = font
        self.title_font = pygame.font.SysFont("consolas", 26, bold=True)
        self.card_font = pygame.font.SysFont("consolas", 16)
        self.small_font = pygame.font.SysFont("consolas", 14)

    def draw_metric(self, surface, label, value, rect, row, color=(230, 238, 242)):
        y = rect.y + 56 + row * 23
        draw_text(surface, self.small_font, label, (rect.x + 16, y), (132, 145, 160))
        draw_text(surface, self.card_font, str(value), (rect.x + 168, y - 2), color)

    def draw(self, snapshots, routed_messages):
        width, height = self.window.size
        if self.surface.get_size() != (width, height):
            self.surface = pygame.Surface((width, height))

        self.surface.fill((13, 17, 21))
        pygame.draw.rect(self.surface, (20, 26, 32), (20, 18, width - 40, 74), border_radius=18)
        draw_text(self.surface, self.title_font, "Drone System Stats", (44, 34), (230, 238, 242))
        draw_text(
            self.surface,
            self.card_font,
            f"Messages routed: {routed_messages}",
            (44, 64),
            (164, 175, 188),
        )

        columns = 3 if width >= 1180 else 2 if width >= 760 else 1
        gap = 18
        margin = 20
        top = 112
        card_width = max(260, (width - margin * 2 - gap * (columns - 1)) // columns)
        card_height = 190

        for index, item in enumerate(snapshots):
            drone = item["drone"]
            state = item["drone_state"]
            controller_state = item["controller_state"]
            target = controller_state["current_target"]
            reached = "yes" if controller_state["target_reached"] else "no"

            col = index % columns
            row = index // columns
            rect = pygame.Rect(
                margin + col * (card_width + gap),
                top + row * (card_height + gap),
                card_width,
                card_height,
            )

            pygame.draw.rect(self.surface, (19, 24, 30), rect, border_radius=16)
            pygame.draw.rect(self.surface, (46, 57, 68), rect, width=1, border_radius=16)
            pygame.draw.circle(self.surface, drone.color, (rect.x + 24, rect.y + 27), 7)
            draw_text(self.surface, self.font, item["label"], (rect.x + 42, rect.y + 16), drone.color)

            target_text = "none" if target is None else f"({target[0]:.2f}, {target[1]:.2f})"
            self.draw_metric(self.surface, "position", f"{state['x']:.1f}, {state['y']:.1f}", rect, 0)
            self.draw_metric(self.surface, "targets in", state["received_commands"], rect, 1)
            self.draw_metric(self.surface, "targets out", controller_state["commands_sent"], rect, 2)
            self.draw_metric(self.surface, "telemetry", controller_state["telemetry_received"], rect, 3)
            self.draw_metric(self.surface, "target", target_text, rect, 4)
            self.draw_metric(
                self.surface,
                "reached",
                reached,
                rect,
                5,
                (149, 213, 107) if reached == "yes" else (245, 180, 82),
            )

        self.renderer.draw_color = (17, 20, 24, 255)
        self.renderer.clear()
        texture = Texture.from_surface(self.renderer, self.surface)
        texture.draw()
        self.renderer.present()

    def destroy(self):
        self.window.destroy()


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--duration", type=float, default=None)
    args = parser.parse_args()

    pygame.init()
    screen = pygame.display.set_mode((WIDTH, HEIGHT), pygame.RESIZABLE)
    pygame.display.set_caption("Drone Swarm Map")
    clock = pygame.time.Clock()
    font = pygame.font.SysFont("consolas", 18)
    stats_window = StatsWindow(font)
    window_width, window_height = screen.get_size()

    system = drone_system.System()

    specs = [
        {
            "label": "Alpha",
            "drone_id": "drone_alpha",
            "controller_id": "controller_alpha",
            "start": (0.86, 0.33),
            "color": (72, 207, 173),
        },
        {
            "label": "Bravo",
            "drone_id": "drone_bravo",
            "controller_id": "controller_bravo",
            "start": (0.50, 0.86),
            "color": (245, 180, 82),
        },
        {
            "label": "Charlie",
            "drone_id": "drone_charlie",
            "controller_id": "controller_charlie",
            "start": (0.18, 0.42),
            "color": (116, 169, 255),
        },
        {
            "label": "Delta",
            "drone_id": "drone_delta",
            "controller_id": "controller_delta",
            "start": (0.72, 0.72),
            "color": (232, 121, 249),
        },
        {
            "label": "Echo",
            "drone_id": "drone_echo",
            "controller_id": "controller_echo",
            "start": (0.30, 0.18),
            "color": (149, 213, 107),
        },
    ]

    swarm = []
    for spec in specs:
        drone = Drone(
            system,
            spec["drone_id"],
            spec["controller_id"],
            spec["start"],
            spec["color"],
        )
        controller = Controller(
            system,
            spec["controller_id"],
            spec["drone_id"],
        )
        swarm.append({
            "label": spec["label"],
            "drone": drone,
            "controller": controller,
        })

    for item in swarm:
        system.attach_client(item["drone"].client)
        system.attach_client(item["controller"].client)

    system.start()

    for item in swarm:
        item["drone"].client.start()
        item["controller"].client.start()

    running = True
    deadline = None if args.duration is None else time.perf_counter() + args.duration
    try:
        while running:
            if deadline is not None and time.perf_counter() >= deadline:
                running = False

            for event in pygame.event.get():
                if event.type == pygame.QUIT:
                    running = False
                elif event.type == pygame.WINDOWCLOSE:
                    running = False
                elif event.type == pygame.VIDEORESIZE:
                    window_width = max(event.w, 420)
                    window_height = max(event.h, 300)
                    screen = pygame.display.set_mode((window_width, window_height), pygame.RESIZABLE)

            window_width, window_height = screen.get_size()
            snapshots = []
            for item in swarm:
                drone = item["drone"]
                controller = item["controller"]
                drone.update(window_width, window_height)
                snapshots.append({
                    "label": item["label"],
                    "drone": drone,
                    "drone_state": drone.snapshot(window_width, window_height),
                    "controller_state": controller.snapshot(),
                })

            screen.fill((20, 24, 28))
            draw_grid(screen, window_width, window_height)

            positions = []
            for item in snapshots:
                state = item["drone_state"]
                positions.append((
                    item,
                    (int(state["x"]), int(state["y"])),
                    (int(state["target_x"]), int(state["target_y"])),
                ))

            for i, (_, pos_a, _) in enumerate(positions):
                for _, pos_b, _ in positions[i + 1:]:
                    distance = math.dist(pos_a, pos_b)
                    midpoint = (
                        (pos_a[0] + pos_b[0]) // 2,
                        (pos_a[1] + pos_b[1]) // 2,
                    )
                    pygame.draw.line(screen, (118, 132, 144), pos_a, pos_b, 2)
                    draw_centered_text(
                        screen,
                        font,
                        f"{distance:.1f}px",
                        (midpoint[0], midpoint[1] - 14),
                        (230, 238, 242),
                    )

            for item, pos, target in positions:
                drone = item["drone"]
                pygame.draw.circle(screen, drone.color, target, 14, 1)
                pygame.draw.line(screen, drone.color, pos, target, 1)
                pygame.draw.circle(screen, drone.color, pos, DRONE_RADIUS)

            routed_messages = system.routed_messages()
            draw_text(screen, font, f"Messages routed: {routed_messages}", (24, window_height - 36))

            pygame.display.flip()
            stats_window.draw(snapshots, routed_messages)
            clock.tick(60)
    finally:
        for item in swarm:
            item["drone"].client.stop()
            item["controller"].client.stop()
        system.stop()
        stats_window.destroy()
        pygame.quit()


if __name__ == "__main__":
    main()
