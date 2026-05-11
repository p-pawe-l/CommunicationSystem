.PHONY: sync sync-crazyflie build demo clean

sync:
	uv sync

sync-crazyflie:
	uv sync --extra crazyflie

build:
	uv build

demo:
	uv run python drone_gui_demo.py

clean:
	rm -rf build dist *.egg-info
