# ----------------------------
# Makefile Options
# ----------------------------

NAME = HUNCHBAK
ICON = icon.png
DESCRIPTION = "CE port of the 1983 coin-op original"
COMPRESSED = YES
COMPRESSED_MODE = zx0

CFLAGS = -Wall -Wextra -Oz
CXXFLAGS = -Wall -Wextra -Oz

# ----------------------------

include $(shell cedev-config --makefile)
