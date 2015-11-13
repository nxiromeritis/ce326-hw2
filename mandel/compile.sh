#!/bin/bash

gcc -Wall mandelGUI.c mandelCore.c -L /usr/X11R6/lib -lX11 -o mandelGUI
