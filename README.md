# libwlnk

[![License: BSD-3-Clause](https://img.shields.io/badge/License-BSD_3--Clause-blue.svg)](LICENSE)

A C library for controlling Linux Wi-Fi interfaces.
It gets you a tuned, exclusively held monitor interface and gives it back exactly as it found it, replacing the `iw`/`ip`/`nmcli` subprocess glue that Wi-Fi tools hand-roll today.

This does not move frames and does not parse them.
A claimed interface is an ordinary monitor interface, so capture with libpcap, a packet socket, or whatever your programming language already has.
