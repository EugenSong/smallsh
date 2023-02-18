#!/usr/bin/env bash

if [ $# -ne 1 ]
then
  printf "Usage: $0 /path/to/smallsh\n" >&2
  exit 1
fi
export TMPDIR=/scratch
readonly workdir=`mktemp -d`
readonly homedir=`mktemp -d`
readonly outdir=`mktemp -d`

trap 'rm -rf "$workdir" "$homedir" "$outdir";' EXIT 

hr=$(perl -E 'say "\xe2\x80\x95" x 120' 2>/dev/null)

randval=$((RANDOM % 254+1))

cp "$1" "$workdir/smallsh"
cd "$workdir"

printf 'Beginning Test Script!\n'

timeout 5 env - PS1='' HOME="$homedir" setsid bash <<__OUTEREOF__ &
export IFS=$'\t'$'\n'
exec ./smallsh << __EOF__
echo	$hr


echo	$hr
printf	$(tput bold setaf 4)Testing built-in exit command$(tput sgr0)\n
printf	Testing: $(tput bold)exit $randval$(tput sgr0)\n
exit	$randval
__EOF__
__OUTEREOF__
bgpid=$!
wait $!
