#!/bin/bash

# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.

BASE_DIR=$(dirname "$0")
OZKS_ROOT_DIR=$BASE_DIR/../../
shopt -s globstar
clang-format -i $OZKS_ROOT_DIR/oZKS/**/*.h
clang-format -i $OZKS_ROOT_DIR/oZKS/**/*.cpp
