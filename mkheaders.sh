#!/bin/bash

gpl() {
cat << EOT
/*
 *    This file is part of LiSA Command Line Interface.
 *
 *    LiSA Command Line Interface is free software; you can redistribute it 
 *    and/or modify it under the terms of the GNU General Public License 
 *    as published by the Free Software Foundation; either version 2 of the 
 *    License, or (at your option) any later version.
 *
 *    LiSA Command Line Interface is distributed in the hope that it will be 
 *    useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *    GNU General Public License for more details.
 *
 *    You should have received a copy of the GNU General Public License
 *    along with LiSA Command Line Interface; if not, write to the Free 
 *    Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, 
 *    MA  02111-1307  USA
 */

EOT
}

do_swsock() {
gpl

echo "#ifndef _SWSOCK_H"
echo "#define _SWSOCK_H"
echo
grep "^#define AF_SWITCH" $BASEPATH/linux-2.6/include/linux/socket.h
grep "^#define PF_SWITCH" $BASEPATH/linux-2.6/include/linux/socket.h
echo
echo "#endif"

}

BASEPATH="`dirname $0`"

case "$1" in
swsock.h)
	do_swsock
	;;
*)
	echo "Unknown header"
	exit 1
	;;
esac
