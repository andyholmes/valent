#!/bin/sh -e

# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: No rights reserved

scan-build --status-bugs \
           -disable-checker deadcode.DeadStores \
           "$@"
