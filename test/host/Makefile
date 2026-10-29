# Host-side tests (run on your PC, no Pico needed).
#   make test         physics scenarios with sanitizers on
#   make syntax       syntax-check ALL firmware sources against stand-in SDK headers
#   make screenshots  render PNGs with the real renderer (needs Python + Pillow)
CXX ?= g++
FLAGS = -std=gnu++17 -O2 -Wall -Wextra -I../../include -I../../src
SRC = ../../src

test:
	$(CXX) $(FLAGS) -fsanitize=undefined,address -fno-sanitize-recover=undefined \
	  test_particles.cpp $(SRC)/particles.cpp -o /tmp/test_particles && /tmp/test_particles

syntax:
	for f in $(SRC)/*.cpp; do $(CXX) $(FLAGS) -Wshadow -fsyntax-only -I../mocks $$f || exit 1; done; echo "syntax OK"

screenshots:
	$(CXX) $(FLAGS) make_screenshots.cpp $(SRC)/particles.cpp $(SRC)/motion.cpp $(SRC)/render.cpp -o /tmp/make_shots
	/tmp/make_shots && python3 make_screenshots.py

.PHONY: test syntax screenshots
