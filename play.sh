#!/bin/bash
cd "$(dirname "$0")"
git pull
make clean && make && ./dungeon-grind
