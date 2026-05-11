PYTHON_SOURCES := scripts
CPP_SOURCES := $(shell find include src -type f \( -name '*.hpp' -o -name '*.cpp' \)) main.cpp

.PHONY: sync sync-dev sync-crazyflie build format format-python format-cpp lint lint-python lint-cpp check clean

sync:
	uv sync

sync-dev:
	uv sync --group dev

sync-crazyflie:
	uv sync --extra crazyflie

build:
	uv build


format: format-python format-cpp

format-python:
	uv run ruff check --fix $(PYTHON_SOURCES)
	uv run ruff format $(PYTHON_SOURCES)

format-cpp:
	clang-format -i $(CPP_SOURCES)

lint: lint-python lint-cpp

lint-python:
	uv run ruff format --check $(PYTHON_SOURCES)
	uv run ruff check $(PYTHON_SOURCES)

lint-cpp:
	clang-format --dry-run --Werror $(CPP_SOURCES)
	clang-tidy $(CPP_SOURCES) -p build --quiet

check: lint

clean:
	rm -rf build dist *.egg-info
