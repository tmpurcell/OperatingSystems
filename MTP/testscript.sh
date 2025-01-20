#!/usr/bin/env bash

export TMPDIR=$HOME/.tmp
mkdir -p "$TMPDIR"

if [ $# -lt 1 ]
then
  printf 'Usage: %s binary\n' "$0"
  exit 1
fi

readonly bin=`realpath "${1:-./mtp}"`

if ! [ -x "$bin" ]
then
  printf '%s: %s: not an executable file\n' "$0" "$bin"
fi

readonly tests_path="/Users/tylerpurcell/Desktop/OSU/Operating Systems/MTP Test/tests"
readonly ref="${tests_path}"/ref/mtp_multi_thread

header() {
  printf '\n'
  tput setaf 4 bold
  printf '%s\n' "$@"
  tput sgr0
}

score=0

doscore() {
  if [ $# -eq 1 ] || [ "$1" -eq "$2" ] 
  then
    tput setaf 2 bold
    printf 'Passed! +%d/%d\n' "$1" "$1"
    tput sgr0
  else
    tput setaf 1 bold
    printf 'Failed -- %d/%d\n' "$1" "$2"
    tput sgr0
  fi
  ((score += $1))
}

dotest() {
  pts=$1
  shift
  if "${@}"
  then
    doscore "$pts"
    return 0
  else
    doscore 0 "$pts"
    return 1
  fi
}

passed_a_test=0
needs_pass=0
doscore() {
  if [ $# -eq 1 ] || [ "$1" -eq "$2" ] 
  then
    if [ "$passed_a_test" -eq 0 ] && 
      [ "$needs_pass" -eq 1 ]
    then
      tput setaf 1 bold
      printf 'Passed for %d points, but did not pass any previous tests\n' $1
      tput sgr0
    else
      tput setaf 2 bold
      printf 'Passed! +%d/%d\n' "$1" "$1"
      tput sgr0
      passed_a_test=1
      ((score += $1))
    fi
  else
    tput setaf 1 bold
    printf 'Failed -- %d/%d\n' "$1" "$2"
    tput sgr0
    ((score += $1))
  fi
}

header 'Line separator test'
dotest 5 "${tests_path}"/00-line-sep.sh "$ref" "$bin"

header 'Plus signs to caret (++ -> ^) test'
dotest 15 "${tests_path}"/01-caret.sh "$ref" "$bin"

header 'Output lines always 80 chars test'
dotest 5 "${tests_path}"/02-80char.sh "$ref" "$bin"

header 'Only complete lines before STOP test'
dotest 10 "${tests_path}"/03-complete-lines.sh "$ref" "$bin"

header 'Terminates after writing all 80 char lines test'
dotest 5 "${tests_path}"/04-terminates.sh "$ref" "$bin"

needs_pass=1

header 'Immediate output test (Must pass at least one other test)'
dotest 15 "${tests_path}"/05-immediate-output.sh "$ref" "$bin"




header '=== Threading Tests (must pass tests in order shown) ==='

log() {
  strace-log-merge "$out"/trace 2>&- |
    sed 's/^\([[:digit:]]\+\) \+[[:digit:]]\+:[[:digit:]]\+:[[:digit:]]\+.[[:digit:]]\+ \+\([^(]*\)\((.*)\) *= *\(.*\)/\1 \2 \4 \3/g'
}

readonly out=`mktemp -d`
trap 'exec 3>&-; sleep 0.05; kill -9 $proc &>/dev/null; wait; rm -rf $out;' EXIT
mkfifo "$out/stdin"
strace -qqq -ff -v -s0 -tt -o "$out/trace" \
       --status=successful \
       -e trace=clone,clone3,futex,read,write,execve \
       "$bin" &>/dev/null <"$out/stdin" &
proc=$!
exec 3>"$out/stdin"
sleep 0.05

failed_a_test=0
doscore() {
  if [ $# -eq 1 ] || [ "$1" -eq "$2" ] 
  then
    if [ $failed_a_test -eq 1 ]
    then
      tput setaf 1 bold
      printf 'Passed for %d points, but failed a previous test\n' $1
      tput sgr0
    else
      tput setaf 2 bold
      printf 'Passed! +%d/%d\n' "$1" "$1"
      tput sgr0
      ((score += $1))
    fi
  else
    tput setaf 1 bold
    printf 'Failed -- %d/%d\n' "$1" "$2"
    tput sgr0
    ((score += $1))
    failed_a_test=1
  fi
}

header 'Checking for four child threads'
procs=($(log | awk 'BEGIN { getline; main=$1; print main; } $1 == main && $2 ~ /^clone/ && /CLONE_THREAD/ { print $3; } END { exit 1 }'))
if [ ${#procs[@]} -ne 5  ]
then
  printf 'Detected %d separate threads. Expected 5 (main thread, and four pipeline threads).\n' "${#procs[@]}"
  doscore 0 15
else
  doscore 15
fi
main=${procs[0]}
unset ${procs[0]}

header 'Checking for busy-waiting with no input'
n=$(log | wc -l)
sleep 0.05
if [ "$(log | wc -l)" -ne $n ]
then
  printf 'Detected busy-waiting\n'
  doscore 0 5
else
  doscore 5
fi

header 'Checking for input thread to immediately read a single byte of input'
printf 'x' >&3
sleep 0.1

input_thread=0
pids=($(log | tail -n+$((n - 1)) | awk '$1 != "'$main'" && $2 == "read" {print $1}' | uniq))
n=$(log | wc -l)
if [ ${#pids[@]} -gt 1 ]
then
  printf 'Multiple threads appear to be calling read()\n'
  doscore 0 10
elif
  [ ${#pids[@]} -eq 0 ]
then
  printf 'Did not see input thread reading input when provided\n'
  doscore 0 10
else
  input_thread=${pids[0]}
  doscore 10
fi

header 'Checking for data synchronization via mutexes'
printf 'xxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxxx\n' >&3
sleep 0.05
if [ $(log | awk '$2 != "futex" { next; } { print $1 }' | sort | uniq | wc -l) -ne 4 ]
then
  printf 'Threads do not appear to be synchronizing data access with mutexes.\n'
  doscore 0 15
else
  doscore 15
fi

header 'Checking for output thread behavior'
if ! $( log | awk '$1 != "'$main'" && 
                   $1 != "'$input_thread'" && 
                   $2 == "write" { n += $3 }
                   END { if (n == 81) exit 0; else exit 1 }'  )
then
  printf 'Output thread is not calling write() to print.\n'
  doscore 0 5
else
  doscore 5
fi
printf '\n\n'
tput setaf 4 bold
printf 'Final Score: %d/105\n' "$score"
tput sgr0
