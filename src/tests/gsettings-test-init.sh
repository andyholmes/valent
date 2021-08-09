#!/bin/bash

# Clean old files
rm -rf ${GSETTINGS_SCHEMA_DIR}
mkdir -p ${GSETTINGS_SCHEMA_DIR}

# Copy application and plugin GSettingsSchemas
cp ${MESON_SOURCE_ROOT}/data/gsettings/*.gschema.xml ${GSETTINGS_SCHEMA_DIR}
cp ${MESON_SOURCE_ROOT}/src/plugins/*/*.gschema.xml ${GSETTINGS_SCHEMA_DIR}

