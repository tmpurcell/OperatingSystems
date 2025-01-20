#!/usr/bin/env bash

exe=${1:-build/release/base64}

tput bold
printf 'Testing basic input strings\n'
printf '===========================\n\n'

for input in f fo foo foob fooba foobar
do
  tput bold
  tput setaf 9
  printf 'Input: %s%s\n%s' "$(tput setaf 5)" "$f" "$(tput setaf 9)"
  printf 'Expecting output: %s%s%s\n' "$(tput setaf 5)" "$(printf '%s' "$input" | base64)" "$(tput setaf 9)"
  tput sgr0
  printf '%s' "$input" | "$exe"
  printf '\n'
done

tput bold
printf 'Testing null bytes\n'
printf '==================\n\n'

for size in {1..10}
do
  tput bold
  tput setaf 9
  printf 'Input: %d null bytes\n' "$size" 
  printf 'Expecting output `%s%s%s`:\n' "$(tput setaf 5)" "$(head -c $size </dev/zero | base64)" "$(tput setaf 9)"
  tput sgr0
  head -c $size </dev/zero | "$exe"
  printf '\n'
done

tput bold
printf 'Testing random bytes\n'
printf '====================\n\n'

tmp=$(mktemp)
trap 'rm  "$tmp"' EXIT
for size in {1..10}
do
  tput bold
  tput setaf 9
  printf 'Input: %d random bytes\n' "$size"
  
  head -c $size /dev/random >"$tmp"
  printf 'Expecting output `%s%s%s`:\n' "$(tput setaf 5)" "$(base64 "$tmp")" "$(tput setaf 9)"
  tput sgr0
  "$exe" "$tmp"
  printf '\n'
done
