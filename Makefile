all: run

run: build
	bash ./run_all.sh

structure: build
	bash ./run_structure.sh

write: build
	bash ./run_write.sh

replace: build
	bash ./run_replace.sh

build:
	cd src && g++ main.cpp -o main

debug:
	cd src && g++ main.cpp -g -o main

.PHONY: all run