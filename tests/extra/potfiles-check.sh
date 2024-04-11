#!/usr/bin/env bash

# SPDX-License-Identifier: CC0-1.0
# SPDX-FileCopyrightText: No rights reserved

srcdirs="src/libvalent src/plugins"
datadirs="data src/plugins"
uidirs="src/libvalent/ui src/plugins"

# find source files that contain gettext keywords
# shellcheck disable=SC2086
files=$(grep -lR --include='*.c' '\(gettext\|[^_)]_\)(' $srcdirs)

# find ui files that contain translatable string
# shellcheck disable=SC2086
files="$files "$(grep -lRi --include='*.ui' 'translatable="[ty1]' $uidirs)

# find GSettings schemas that contain gettext-domain string
# shellcheck disable=SC2086
files="$files "$(grep -lRi --include='*.gschema.xml' 'gettext-domain="[^"]\+"' $datadirs)

# find .desktop files
# shellcheck disable=SC2086
files="$files "$(find $datadirs -name '*.desktop*')

# filter out excluded files
if [ -f po/POTFILES.skip ]; then
  files=$(for f in $files; do ! grep -q ^"$f" po/POTFILES.skip && echo "$f"; done)
fi

# find those that aren't listed in POTFILES.in
missing=$(for f in $files; do ! grep -q ^"$f" po/POTFILES.in && echo "$f"; done)

if [ ${#missing} -eq 0 ]; then
  exit 0
fi

cat >&2 <<EOT

The following files are missing from po/POTFILES.in:

EOT
for f in $missing; do
  echo "  $f" >&2
done
echo >&2

if [ "${GITHUB_ACTIONS}" = "true" ]; then
    {
      echo "### POTFILES"
      echo "Missing from po/POTFILES.in:"
      echo "\`\`\`"
      for f in $missing; do
        echo "$f"
      done
      echo "\`\`\`"
    } >> "${GITHUB_STEP_SUMMARY}";
fi

exit 1

