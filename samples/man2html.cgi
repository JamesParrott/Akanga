#!/bin/akanga
#

# Our baseurl on the web server.
#
baseurl = '/cgi-bin/man2html.cgi?'

# The full path to the man-database query program whatis.
#
whatis = /usr/bin/whatis

# Where is out man2html processor?
#
man2html = /usr/local/bin/man2html


#
# notfound <command>
#
#	Display a short error message if nothing was found for <command>.
#
fn notfound {
	echo 'No manual page available for:' $1
	return
	}

#
# list2man
#
#	Transform the list (output from $whatis) into something that looks
#	list man output.
#
fn list2man {
	sort -f |
	gawk '
		/./ {
			command = $1; section = $2; $1 = ""; $2 = "";
			sub(/^[ \t]+/, "", $0);
			gsub(/./, "&\010&", command);
			printf ("%s %s\n", command section, $0);
			}'
	return 0;
	}


#
# indexgrep <tmpfilevar> <index> <pattern>
#
#	This function greps a whatis derived index list a regular
#	expression. Put the resulting tempfile in <tmpfilevar>.
#
fn indexgrep {
	mktemp tmp
	grep -e $3 $2 >$tmp >[2] /dev/null
	$1 = $tmp
	return 0
	}



#
# We always send HTML to the client.
#
echo 'Content-type: text/html'
echo ''


#
# The manpage name is passed to us in the $QUERY_STRING. The section
# is optional and separated from the manpage with a plus ``+''.
#
let 'nl = "\n"'
ifs = ('+' ' ' $nl) { inp = `{ echo $QUERY_STRING } }

command = $inp(1);
section = $inp(2);

if (~ $command ()) {
	#
	# If no command set our default page.
	#
	command = man;
	section = 1;
} else if (~ $command 1 2 3 4 5 6 7 8 9 n l p o) {
	#
	# Hmm, no command looks like a section ... let's procude the
	# table of contents for that section.
	#
	pattern = $section
	section = $command
	mktemp index
	$whatis -w '*' >[2] /dev/null |
	awk '$2 == "(' ^ $section ^ ')"' >$index

	lines = `{ cat $index | wc -l }
	if (~ $lines 0) {
		echo notfound 'section ' ^ $section
		exit 0
		}

	#
	# If we have something in the $section we use it for an indexgrep.
	#
	if (! ~ $pattern ()) {
		indexgrep index $index $pattern
		}


	{ echo 'Contents of Section ' ^ $section ^ ':'
	  echo ''
	  cat $index | list2man } |
	$man2html $baseurl

	exit 0
} else if (~ $command *[ ^ '*?.' ^ ]*) {
	#
	# The command contains wildcard - send the index of matching manpages.
	#
	mktemp pagelist
	if (! $whatis -w $command >$pagelist >[2] /dev/null) {
		notfound $command
		exit 0
		}

	#
	# Again, do an indexgrep if possible.
	#
	if (! ~ $section ()) {
		indexgrep pagelist $pagelist $section
		}


	{ echo 'Available Manpages for' $command $section
	  echo
	  cat $pagelist | list2man } |
	$man2html $baseurl
	exit 0
} else if (~ $section ()) {
	#
	# If we have no section we'll look how many manpages are available.
	#
	mktemp pagelist
	if (! $whatis $command >$pagelist >[2] /dev/null) {
		#
		# There's nothing available.
		#
		notfound $command
		exit 0
		}

	#
	# How many manpages do we have?
	#
	pages = `{ cat $pagelist | wc -l }
	if (~ $pages 1) {
		#
		# Nice, only one - just fall trough.
		#
	} else {
		#
		# More than one - send the linked index page to the client
		#
		{ echo 'Available Manpages for' $command
		  echo
		  cat $pagelist | list2man } |
		$man2html $baseurl
		exit 0
		}
	}

#
# Ok, we either have a given manpage section or only one manpage.
#
mktemp manpage
/usr/bin/man $section $command >$manpage >[2] /dev/null ||  { notfound $command; exit 0 }


#
# Send the reformatted text to the client.
#
$man2html $baseurl <$manpage
exit 0

