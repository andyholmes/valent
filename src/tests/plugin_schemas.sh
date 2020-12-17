#!/bin/bash

# Prepare
rm -rf ${MESON_BUILD_ROOT}/schemas
mkdir -p ${MESON_BUILD_ROOT}/schemas

# Application & Plugin GSchemas
cp ${MESON_SOURCE_ROOT}/data/gsettings/*.gschema.xml ${MESON_BUILD_ROOT}/schemas/
cp ${MESON_SOURCE_ROOT}/src/plugins/*/*.gschema.xml ${MESON_BUILD_ROOT}/schemas/

