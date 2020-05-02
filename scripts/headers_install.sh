#!/bin/sh

if [ $# -lt 1 ]
then
	echo "Usage: headers_install.sh OUTDIR SRCDIR [FILES...]
	echo
	echo "Prepares kernel header files for use by user space, by removing"
	echo "all compiler.h definitions and #includes, removing any"
	echo "#ifdef __KERNEL__ sections, and putting __underscores__ around"
	echo "asm/inline/volatile keywords."
	echo
	echo "OUTDIR: directory to write each userspace header FILE to."
	echo "SRCDIR: source directory where files are picked."
	echo "FILES:  list of header files to operate on."

	exit 1
fi

# Grab arguments

OUTDIR="$1"
shift
SRCDIR="$1"
shift


# Iterate through files listed on command line

FTOP=$(dirname ${srctree})
STOP=$(dirname ${FTOP})
TTOP=$(dirname ${STOP})
TOP=$(dirname ${TTOP})


FILE=
trap 'rm -f "$OUTDIR/$FILE"' EXIT
for i in "$@"
do
	FILE="$(basename "$i")"
	${TOP}/bionic/libc/kernel/tools/clean_header.py -k $(dirname "$SRCDIR/$i") "$SRCDIR/$i" \
		> "$OUTDIR/$FILE"
done
trap - EXIT
