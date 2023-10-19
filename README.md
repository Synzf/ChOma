# MachO

A relatively simple library for parsing and manipulating MachO files. Written while I was working on exploiting [CVE-2023-41991](https://support.apple.com/en-gb/HT213926), a vulnerability in the CoreTrust kernel extension used in iOS, macOS and associated operating systems. It allows for a code-signing bypass and arbitrary entitlements to be granted to the binary of choice (although this is not doable out of the box with this library - I hope to add an example file to do this).

The library works primarily on iOS binaries, I have seen issues (segmentation faults) when trying to parse macOS binaries, so your mileage may vary with such executables. It's written entirely in C, so it's both fast and portable to iOS (for TrollStore or similar apps) as well as most other devices - however, due to the fact that it is in C, a malformed MachO could likely cause a memory fault quite easily, so I cannot guarantee that this parser will work correctly.

This library relies on zero external libraries or dependences, so you can simply build `main.c` with `gcc *.c -o parser`.