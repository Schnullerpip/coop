#!/bin/bash

echo "creating ~/.coop_build and ~/.coop_build/src_mod_templates/"
mkdir -p ~/.coop_build/src_mod_templates
cp coop ~/.coop_build
cp -r src_mod_templates/* ~/.coop_build/src_mod_templates
echo "make sure to create path variables:" 
echo "COOP -> ~/.coop_build"
echo "COOP_TEMPLATES -> ~/.coop_build/src_mod_templates"
echo "you could add COOP to PATH for comfort"
