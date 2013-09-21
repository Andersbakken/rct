#!/bin/bash

git remote set-url origin git@github.com:Andersbakken/rct.git
git push . HEAD:master
git checkout master
