#!/bin/bash

# SPDX-License-Identifier: GPL-3.0-or-later
# SPDX-FileCopyrightText: 2021 Andy Holmes <andrew.g.r.holmes@gmail.com>


# Clean old files
rm -rf ${GSETTINGS_SCHEMA_DIR}
mkdir -p ${GSETTINGS_SCHEMA_DIR}

# Copy application and plugin GSettingsSchemas
cp ${MESON_SOURCE_ROOT}/data/gsettings/*.gschema.xml ${GSETTINGS_SCHEMA_DIR}
cp ${MESON_SOURCE_ROOT}/src/plugins/*/*.gschema.xml ${GSETTINGS_SCHEMA_DIR}

