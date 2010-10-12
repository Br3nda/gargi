#!/bin/bash
git submodule add -b $2 git@github.com:Br3nda/drupal-module-$1.git sites/all/modules/$1
