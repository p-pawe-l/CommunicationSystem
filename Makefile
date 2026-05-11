PYTHON_SOURCES := scripts
CPP_SOURCES := $(shell find include src -type f \( -name '*.hpp' -o -name '*.cpp' \)) main.cpp
PYTHON_TESTS := tests/tests_py
CPP_TESTS := tests/tests_cpp

.PHONY: sync sync-dev sync-crazyflie lock build format format-python format-cpp lint lint-python lint-cpp cpp-test python-test test check clean

sync:
	uv sync

sync-dev:
	uv sync --group dev

sync-crazyflie:
	uv sync --extra crazyflie

lock:
	uv lock

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

cpp-test:
	@if [ -d "$(CPP_TESTS)" ]; then \
		cmake --build build --target test; \
		ctest --test-dir build --output-on-failure; \
	else \
		echo "No C++ tests found in $(CPP_TESTS)"; \
	fi

python-test:
	@if [ -d "$(PYTHON_TESTS)" ]; then \
		uv run pytest $(PYTHON_TESTS); \
	else \
		echo "No Python tests found in $(PYTHON_TESTS)"; \
	fi

test: cpp-test python-test

check: lint test

clean:
	rm -rf build dist *.egg-info
