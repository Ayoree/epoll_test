#!/bin/bash

mkdir build
cd "$_"
cmake ..
cmake --build . --config Release --target all