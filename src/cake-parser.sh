#!/bin/bash

CAKE_SRC=$(readlink -f $(dirname $0))

${CAKE_SRC}/parser/test_parser "$1" | ${CAKE_SRC}/../../antlr/parens-filter.sh
