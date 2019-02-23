#!/bin/env python

import os

src = open("lfq_test_sorted.txt")

current = 0

for line in src:
	if (int(line) == current):
		current += 1
	else:
		print("Well, you f'ed up")
		break