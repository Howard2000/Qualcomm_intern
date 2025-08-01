#!/bin/bash

number=$((RANDOM % 100 + 1))
echo "guess a number"
while true
do
	read guess

	if ((guess < number))
	then
		echo "too small, try again"
	elif ((guess > number))
	then
		echo "too big, try again"
	else
		echo "you win"
		break
	fi
done
