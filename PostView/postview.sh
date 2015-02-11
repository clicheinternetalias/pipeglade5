#!/bin/bash
# PostView - Render, Display, and Convert a PostScript File
# All because people keep insisting on -DSAFER in their GS-based viewers.

# TODO:
# command line to simply ps/pdf/png conversion
#
# postview [opts] [input [output]]
# -p --page-size=[N,N|a4|letter|...]
# -r --resolution=N[,N]
#   if input & output given (even as - - for stdio), assume BATCH
#   output of course supports page%02d fmt

######################################################################
# Variables
######################################################################

# the PostScript input file (required on cmdline)
FILE=

# page size: -sPAPERSIZE=name -dDEVICEWIDTHPOINTS=w -dDEVICEHEIGHTPOINTS=h
# default page size is no option (GS knows its default)
GSOPT_PAGE=

# resolution: -rNxN
# default resolution is no option (GS knows its default)
GSOPT_RES=

# bool: use a custom background color
GSOPT_BG=1

# pre-processing (is a file under $DIR)
GSOPT_BG_FILE=

# currently visible page (scrolling)
CURPAGE=0

# number of pages
PAGE_TOTAL=0

######################################################################
# Parse Options
######################################################################

usage () {
  echo "Usage: $0 postscript-file"
}
if [[ -z "$1" ]]; then usage; exit; fi
if [[ "$1" == "-h" || "$1" == "--help" ]]; then usage; exit; fi

FILE="$1"

######################################################################
# Temporary Files
######################################################################

# debugging:
# DIR=postview.temporary
# mkdir -p postview.temporary
# rm -f postview.temporary/*.fifo

DIR=$(mktemp -d postview.XXXXX)
atexit () { rm -rf $DIR; }
trap atexit EXIT

GUI_IN=$DIR/in.fifo
GUI_OUT=$DIR/out.fifo
LAST_UPDATE=$DIR/timestamp

send_command () {
  echo "$1" > $GUI_IN
}

######################################################################
# Page Size
######################################################################

# from gs_statd.ps
# leading/trailing space is needed for substring matching
PAGE_SIZES=" Default a0 a1 a2 a3 a4 a4small a5 a6 a7 a8 a9 a10 \
  ANSI_A ANSI_B ANSI_C ANSI_D ANSI_E ANSI_F \
  archA archB archC archD archE \
  b0 b1 b2 b3 b4 b5 b6 c0 c1 c2 c3 c4 c5 c6 \
  flsa flse hagaki halfletter \
  isob0 isob1 isob2 isob3 isob4 isob5 isob6 \
  jisb0 jisb1 jisb2 jisb3 jisb4 jisb5 jisb6 \
  ledger legal letter lettersmall note pa4 tabloid "

init_page_sizes () { # add page sizes to dropdown box
  send_command "combo-page-size append $PAGE_SIZES"
  send_command "combo-page-size select 0"
}

set_page_size () {
  if [[ "Default" == "$1" ]]; then
    GSOPT_PAGE=
  elif [[ "$PAGE_SIZES" == *" $1 "* ]]; then
    GSOPT_PAGE="-sPAPERSIZE=$1"
  else
    GSOPT_PAGE=$(echo $1 | sed 's/\([0-9]\+\)[^0-9]\+\([0-9]\+\)/-dDEVICEWIDTHPOINTS=\1 -dDEVICEHEIGHTPOINTS=\2/')
  fi
}

######################################################################
# Resolution
######################################################################

# leading/trailing space is needed for substring matching
RESOLUTIONS=" Default 9x9 18x18 36x36 72x72 144x144 288x288 576x576 "

init_resolutions () { # add pre-defined resolutions to dropdown box
  send_command "combo-resolution append $RESOLUTIONS"
  send_command "combo-resolution select 0"
}

set_resolution () {
  if [[ "Default" == "$1" ]]; then
    GSOPT_RES=
  elif [[ "$RESOLUTIONS" == *" $1 "* ]]; then
    GSOPT_RES="-r$value"
  else
    GSOPT_PAGE=$(echo $1 | sed 's/\([0-9]\+\)[^0-9]\+\([0-9]\+\)/-r=\1x\2/')
  fi
}

######################################################################
# Background Color
######################################################################

# tried doing this via -c option, but {...} has to be one token (stupid shell quoting rules)

DEFAULT_COLOR='rgb(255,255,255)'

# always include border because we can't get GtkTreeView's style to cooperate
set_background_color () {
  local rgb=$(echo $1 | sed 's/^[^0-9]*\([0-9]\+\)[^0-9]\+\([0-9]\+\)[^0-9]\+\([0-9]\+\).*$/\1 255 div \2 255 div \3 255 div/')
  cat > $DIR/gsprolog-bg.ps <<EOF
  /postview--background {
    gsave
      $GSOPT_BG 0 ne { $rgb setrgbcolor clippath fill } if
      0 setgray 1 setlinewidth clippath stroke
    grestore
  } bind def
  /postview--showpage /showpage load def
  /showpage { postview--showpage postview--background } bind def
  postview--background
EOF
  GSOPT_BG_FILE=$DIR/gsprolog-bg.ps
}

init_background_color () {
  set_background_color $DEFAULT_COLOR
  send_command "colorpicker-background set_color $DEFAULT_COLOR"
  send_command "colorpicker-background set_sensitive $GSOPT_BG"
  send_command "checkbutton-background set_active $GSOPT_BG"
}

######################################################################
# Error Display
######################################################################

init_errors () {
  send_command "scroll-error hide"
}

set_errors () {
  local errors=$(echo "$1" | sed ':a;N;$!ba;s/\n/\\n/g')
  send_command "label-error set_text '$errors'"
  send_command "scroll-error show"
  send_command "scroll-pages hide"
}

clear_errors () {
  send_command "label-error set_text ''"
  send_command "scroll-error hide"
  send_command "scroll-pages show"
}

######################################################################
# Rendering
######################################################################

render () { # device out-file prolog-file
  send_command "window cursor watch"
  send_command "window set_sensitive 0"
  ERRORS=$(ghostscript -dBATCH -dNOPAUSE -q -sFONTPATH="$HOME/.fonts/" \
           -sDEVICE=$1 $GSOPT_PAGE $GSOPT_RES -sOutputFile="$2" \
           -f $3 "$FILE" 2>&1)
  send_command "window set_sensitive 1"
  send_command "window cursor"
}

update_document () {
  rm -f $DIR/gsout-*.png
  # tree-pages and combo-page-goto have the same list-store
  send_command "tree-pages remove"
  if [[ $GSOPT_BG -eq 1 ]]; then bg=$GSOPT_BG_FILE; else bg=; fi

  render pngalpha $DIR/gsout-%03d.png $bg
  if [[ -n $ERRORS ]]; then
    set_errors "$ERRORS"
  else
    clear_errors
    page=1
    index=0
    PAGE_TOTAL=$(ls $DIR/gsout-*.png | wc -l)
    for img in $(ls $DIR/gsout-*.png | sort)
    do
      send_command "tree-pages append $img 'Page $page of $PAGE_TOTAL' $index"
      page=$((page + 1))
      index=$((index + 1))
    done
    send_command "tree-pages scroll $CURPAGE"
    send_command "combo-page-goto select $CURPAGE"
  fi
  touch $LAST_UPDATE
}

######################################################################
# Saving
######################################################################

save_file_to () {
  local newfile="$1"
  local device=pdfwrite
  if [[ -n $newfile ]]; then # fail silently on empty file names

    if [[ $newfile == *".png" ]]; then # the obvious choice
      device=pngalpha
    elif [[ $newfile == *".svg" ]]; then # doesn't do text very well
      device=svg
    elif [[ $newfile == *".ps" ]]; then
      # this comes from experience and will not be removed
      # (even reflexively-clicked "are you sure" dialogs don't/didn't work)
      while [[ -e $newfile ]]; do
        newfile=ALMOST_OVERWROTE_$newfile
      done
      device=ps2write
    elif [[ $newfile == *".eps" ]]; then # deprecated; bloated output (eps2write)
      device=epswrite
    fi

    render $device $newfile
    if [[ -n $ERRORS ]]; then set_errors "$ERRORS"; fi
  fi
}

######################################################################
# Scrolling
######################################################################

scroll_to () { # index
  CURPAGE=$1
  send_command "tree-pages scroll $CURPAGE"
  send_command "combo-page-goto select $CURPAGE"
}

scroll_delta () { # amount
  local newpage=$((CURPAGE + $1))
  if [[ $newpage -lt 0 ]]; then newpage=0; fi
  if [[ $newpage -ge $PAGE_TOTAL ]]; then newpage=$((PAGE_TOTAL - 1)); fi
  scroll_to $newpage
}

######################################################################
# Run GUI
######################################################################

./pipeglade/pipeglade -s "''" -i $GUI_IN -o $GUI_OUT postview.glade &
PIPEGLADE=$!
while test ! \( -e $GUI_IN -a -e $GUI_OUT \); do
  # "kill -0": can we talk to it?
  if kill -0 $PIPEGLADE; then sleep .02; else exit; fi
done
send_command "window maximize"
init_page_sizes
init_resolutions
init_background_color
init_errors
update_document

# main loop
while [[ -e $GUI_OUT ]]; do # if GUI_OUT disappears, the GUI's been closed

  # process an event
  # GUI_OUT may disappear during the time-out, leading to an error message.
  # Putting the redirect before the command applies it to the 'if' which
  # is what we have to do, since the error is from outside the command
  # and not subject to its redirections (the error IS the redirections).
  if 2>/dev/null read -r -t .3 name type value < $GUI_OUT; then

    # remove quotes/escapes/extra-values from value
    value=${value/ \'*/}
    eval value=$value

    # scrolling
    if [[ $name == tree-pages && $type == scroll ]]; then
      CURPAGE=$value
      send_command "combo-page-goto select $value"
    elif [[ $name == button-page-first ]]; then
      scroll_delta -$PAGE_TOTAL
    elif [[ $name == button-page-prev ]]; then
      scroll_delta -1
    elif [[ $name == button-page-next ]]; then
      scroll_delta 1
    elif [[ $name == button-page-last ]]; then
      scroll_delta $PAGE_TOTAL
    elif [[ $name == combo-page-goto ]]; then
      scroll_to $value

    # rendering
    elif [[ $name == combo-page-size ]]; then
      set_page_size "$value"
      update_document
    elif [[ $name == combo-resolution ]]; then
      set_resolution "$value"
      update_document
    elif [[ $name == checkbutton-background ]]; then
      GSOPT_BG=$value
      send_command "colorpicker-background set_sensitive $GSOPT_BG"
      update_document
    elif [[ $name == colorpicker-background ]]; then
      set_background_color "$value"
      update_document

    # saving
    elif [[ $name == button-save ]]; then
      send_command "dialog-save-as set_filename $FILE"
      send_command "dialog-save-as show"
    elif [[ $name == dialog-save-as && $type == 1 ]]; then
      save_file_to "$value"
    fi

  # read timed-out; check if file's been modified
  elif [[ "$FILE" -nt $LAST_UPDATE ]]; then
    update_document
  fi

done # main loop

######################################################################
######################################################################
