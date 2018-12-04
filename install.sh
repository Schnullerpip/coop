#!/bin/bash

echo "creating ~/.coop_build"
mkdir -p ~/.coop_build/src_mod_templates
cp coop ~/.coop_build
cp -r src_mod_templates/* ~/.coop_build/src_mod_templates
echo "make sure to create path variables - optionally you could add COOP to PATH for comfort"
echo "COOP -> ~/.coop_build"
echo "COOP_TEMPLATES -> ~/.coop_build/src_mod_templates"
