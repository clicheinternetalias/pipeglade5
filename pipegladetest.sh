#! /usr/bin/env bash

VALGRIND=0

TESTDIR=TEST

# Another possible shebang line:
#! /usr/bin/env mksh

# Pipeglade tests; they should be invoked in the build directory.
#
# Failure of a test can cause failure of one or more subsequent tests.

export LC_ALL=C
FIN=to-g.fifo
FOUT=from-g.fifo
FERR=err.fifo
BAD_FIFO=bad_fifo
rm -f $FIN $FOUT $FERR $BAD_FIFO

TESTS=0
FAILS=0
OKS=0

count_fail() {
    (( TESTS+=1 ))
    (( FAILS+=1 ))
}

count_ok() {
    (( TESTS+=1 ))
    (( OKS+=1 ))
}

check_rm() {
    if test -e $1; then
        count_fail
        echo " FAIL $1 should be deleted"
    else
        count_ok
        echo " OK   $1 deleted"
    fi
}


# BATCH ONE
#
# Situations where pipeglade should exit immediately.  These tests
# should run automatically
######################################################################

check_call() {
    r=$2
    e=$3
    o=$4
    output=$($1 2>tmperr.txt)
    retval=$?
    error=$(<tmperr.txt)
    rm tmperr.txt
    echo "CALL $1"
    if test "$output" = "" -a "$o" = "" || (echo "$output" | grep -Fqe "$o"); then
        count_ok
        echo " OK   STDOUT $output"
    else
        count_fail
        echo " FAIL STDOUT $output"
        echo "    EXPECTED $o"
    fi
    if test "$error" = "" -a "$e" = "" || test "$retval" -eq "$r" && (echo "$error" | grep -Fqe "$e"); then
        count_ok
        echo " OK   EXIT/STDERR $retval $error"
    else
        count_fail
        echo " FAIL EXIT/STDERR $retval $error"
        echo "         EXPECTED $r $e"
    fi
}

check_call "./pipeglade $TESTDIR/nonexistent.ui" 1 "nonexistent.ui" ""
check_call "./pipeglade $TESTDIR/bad_window.ui" 1 "missing top-level window named 'window'" ""
touch $BAD_FIFO
check_call "./pipeglade -i bad_fifo $TESTDIR/pipeglade.ui" 1 "failed to open fifo" ""
check_call "./pipeglade -o bad_fifo $TESTDIR/pipeglade.ui" 1 "failed to open fifo" ""
rm $BAD_FIFO
check_call "./pipeglade -h" 0 "" "Usage:"
check_call "./pipeglade -V" 0 "" "."
check_call "./pipeglade -X" 1 "Unknown option -X" ""
check_call "./pipeglade -i" 1 "Missing argument for -i" ""
check_call "./pipeglade -o" 1 "Missing argument for -o" ""
check_call "./pipeglade" 1 "missing glade .ui file(s)" ""
check_call "./pipeglade $TESTDIR/pipeglade.ui yyy" 1 "Failed to open" ""
mkfifo $FIN
echo -e "statusbar1 pop\n _ main_quit" > $FIN &
check_call "./pipeglade -i $FIN $TESTDIR/pipeglade.ui" 0 "" ""

sleep .5
check_rm $FIN
check_rm $FOUT



#exit
# BATCH TWO
#
# Error handling tests---bogus actions leading to appropriate error
# messages.  These tests should run automatically.
######################################################################

mkfifo $FERR

check_error() {
    echo "SEND $1"
    echo -e "$1" >$FIN
    read r <$FERR
    if test "$2" = "$r"; then
        count_ok
        echo " OK $r"
    else
        count_fail
        echo " FAIL     $r"
        echo " EXPECTED $2"
    fi
}

read r 2< $FERR &
./pipeglade -i $FIN $TESTDIR/pipeglade.ui 2> $FERR &

# wait for $FIN to appear
while test ! \( -e $FIN \); do :; done

# Non-existent name
check_error "" "error: missing name or action in \"\"\"\"\"\""
check_error "nnn" "error: missing name or action in \"\"\"nnn\"\"\""
check_error "nnn set_text FFFF" "error: unknown name in \"\"\"nnn set_text FFFF\"\"\""
check_error "label1 nnn" "error: unknown command in \"\"\"label1 nnn\"\"\""
check_error "image1 nnn" "error: unknown command in \"\"\"image1 nnn\"\"\""
check_error "textview1 nnn" "error: unknown command in \"\"\"textview1 nnn\"\"\""
check_error "button1 nnn" "error: unknown command in \"\"\"button1 nnn\"\"\""
check_error "switch1 nnn" "error: unknown command in \"\"\"switch1 nnn\"\"\""
check_error "togglebutton1 nnn" "error: unknown command in \"\"\"togglebutton1 nnn\"\"\""
check_error "checkbutton1 nnn" "error: unknown command in \"\"\"checkbutton1 nnn\"\"\""
check_error "radiobutton1 nnn" "error: unknown command in \"\"\"radiobutton1 nnn\"\"\""
check_error "spinbutton1 nnn" "error: unknown command in \"\"\"spinbutton1 nnn\"\"\""
check_error "filechooserbutton1 nnn" "error: unknown command in \"\"\"filechooserbutton1 nnn\"\"\""
check_error "open_dialog nnn" "error: unknown command in \"\"\"open_dialog nnn\"\"\""
check_error "fontbutton1 nnn" "error: unknown command in \"\"\"fontbutton1 nnn\"\"\""
check_error "colorbutton1 nnn" "error: unknown command in \"\"\"colorbutton1 nnn\"\"\""
check_error "scale1 nnn" "error: unknown command in \"\"\"scale1 nnn\"\"\""
check_error "progressbar1 nnn" "error: unknown command in \"\"\"progressbar1 nnn\"\"\""
check_error "spinner1 nnn" "error: unknown command in \"\"\"spinner1 nnn\"\"\""
check_error "statusbar1 nnn" "error: unknown command in \"\"\"statusbar1 nnn\"\"\""
check_error "comboboxtext1 nnn" "error: unknown command in \"\"\"comboboxtext1 nnn\"\"\""
check_error "entry1 nnn" "error: unknown command in \"\"\"entry1 nnn\"\"\""

echo "_ main_quit" >$FIN

sleep .5
check_rm $FIN
rm $FERR

#exit
# BATCH THREE
#
# Tests for the principal functionality---valid actions leading to
# correct results.  Manual intervention is required.  Instructions
# will be given on the statusbar of the test GUI.
######################################################################

mkfifo $FOUT

check() {
    N=$1
    echo "SEND $2"
    echo -e "$2" >$FIN
    i=0
    while [ $i -lt $N -a -e "$FOUT" ]; do
        read r <$FOUT
        if test "$r" != ""; then
            if test "$r" = "$3"; then
                count_ok
                echo " OK  ($i)  $r"
            else
                count_fail
                echo " FAIL($i)  $r"
                echo " EXPECTED $3"
            fi
            shift
            (( i+=1 ))
        fi
    done
}

if [ $VALGRIND -eq 1 ]; then
  valgrind 2> valground.txt ./pipeglade -e '!' -s "''" -i $FIN -o $FOUT $TESTDIR/pipeglade.ui &
else
  ./pipeglade -e '!' -s "''" -i $FIN -o $FOUT $TESTDIR/pipeglade.ui &
fi

# wait for $FIN and $FOUT to appear
while test ! \( -e $FIN -a -e $FOUT \); do :; done

check 1 "entry1 set_text FFFF\n entry1 send_value" "entry1 value 'FFFF'"
check 1 "entry1 set_text GGGG\n entry1 send_value" "entry1 value 'GGGG'"
check 1 "spinbutton1 set_value 33.0\n spinbutton1 send_value" "spinbutton1 value '33.0'"
check 2 "radiobutton2 set_active 1" "radiobutton1 value '0'" "radiobutton2 value '1'"
check 2 "radiobutton1 set_active 1" "radiobutton2 value '0'" "radiobutton1 value '1'"
check 1 "switch1 set_active 1" "switch1 value '1'"
check 1 "switch1 set_active 0" "switch1 value '0'"
check 1 "togglebutton1 set_active 1" "togglebutton1 value '1'"

check 1 "treeview1 set_cell 2 0 1 \n treeview1 send_value 2 0" "treeview1 value '1'"
check 1 "treeview1 set_cell 2 1 -30000 \n treeview1 send_value 2 1" "treeview1 value '-30000'"
check 1 "treeview1 set_cell 2 2 66 \n treeview1 send_value 2 2" "treeview1 value '66'"
check 1 "treeview1 set_cell 2 3 -2000000000 \n treeview1 send_value 2 3" "treeview1 value '-2000000000'"
check 1 "treeview1 set_cell 2 4 4000000000 \n treeview1 send_value 2 4" "treeview1 value '4000000000'"
check 1 "treeview1 set_cell 2 5 -2000000000 \n treeview1 send_value 2 5" "treeview1 value '-2000000000'"
check 1 "treeview1 set_cell 2 6 4000000000 \n treeview1 send_value 2 6" "treeview1 value '4000000000'"
check 1 "treeview1 set_cell 2 7 3.141 \n treeview1 send_value 2 7" "treeview1 value '3.141000'"
check 1 "treeview1 set_cell 2 8 3.141 \n treeview1 send_value 2 8" "treeview1 value '3.141000'"
check 1 "treeview1 set_cell 2 9 TEXT \n treeview1 send_value 2 9" "treeview1 value 'TEXT'"
check 1 "treeview1 set_cell 2 10 TEST/pipegladetest2.png \n treeview1 send_value 2 10" "treeview1 value 'TEST/pipegladetest2.png'"
check 1 "treeview1 insert 2 \n treeview1 send_value 2 3" "treeview1 value '0'"
check 1 "treeview1 move 3 1 \n treeview1 send_value 1 2" "treeview1 value '66'"
check 1 "treeview1 remove 1 \n treeview1 send_value 1 2" "treeview1 value '0'"
check 1 "treeview1 scroll end \n treeview1 send_value 1 2" "treeview1 value '0'"
check 1 "treeview1 set_cell 2 10 TEST/pipegladetest2.png \n treeview1 send_value 2 10" "treeview1 value 'TEST/pipegladetest2.png'"

check 1 "iconview1 set_cell 2 1 TEXT \n iconview1 send_value 2 1" "iconview1 value 'TEXT'"
check 1 "iconview1 set_cell 2 0 TEST/pipegladetest2.png \n iconview1 send_value 2 0" "iconview1 value 'TEST/pipegladetest2.png'"
check 1 "iconview1 insert 2 TEST/pipegladetest.png INS \n iconview1 send_value 2 1" "iconview1 value 'INS'"
check 1 "iconview1 insert 2 TEST/pipegladetest.png REM \n iconview1 insert 2 TEST/pipegladetest.png REM \n iconview1 remove 2 4 \niconview1 send_value 2 1" "iconview1 value 'INS'"
check 1 "iconview1 move 3 1 \n iconview1 send_value 1 1" "iconview1 value 'TEXT'"
check 1 "iconview1 remove 1 \n iconview1 send_value 1 1" "iconview1 value 'B'"
check 1 "iconview1 scroll end \n iconview1 send_value 1 1" "iconview1 value 'B'"
check 0 "iconview1 set_cell 1 VV TEST/pipegladetest2.png "

check 1 "textview1 send_value" "textview1 value 'some text!netc!n'"
check 1 "textview1 moveto 0 5\n textview1 insert 'MORE '\ntextview1 send_value" "textview1 value 'some MORE text!netc!n'"
check 1 "textview1 moveto 1\n textview1 insert 'ETC '\ntextview1 send_value" "textview1 value 'some MORE text!nETC etc!n'"
check 1 "textview1 delete\ntextview1 send_value" "textview1 value ''"
check 1 "textview1 moveto 1 \ntextview1 insert 'A!nB!nC!nD!nE!nF!nG!nH!nI!nJ!nK!nL!nM!nN!nO!nP!nQ!nR!nS!nT!nU!nV!nW!nX!nY!nZ!na!nb!nc!nd!ne!nf!ng!nh!ni!nj!nk!nl!nm!nn!no!np!nq!nr!ns!nt!nu!nv!nw!nx!ny!nz'\n textview1 moveto 46 \n textview1 scroll_to_cursor\n textview1 select 46 0 46 1\n textview1 send_selection" "textview1 selection 'u'"
check 1 "textview1 moveto end\n textview1 scroll_to_cursor\n textview1 select line \n textview1 send_selection" "textview1 selection 'z'"
check 1 "textview1 moveto 0 \n textview1 scroll_to_cursor\ntextview1 select 0 \n textview1 send_selection" "textview1 selection 'A'"
check 2 "scale1 set_value 10\n scale1 send_value" "scale1 value '10.000000'" "scale1 value '10.000000'"

check 2 "radiobutton2 set_active 1" "radiobutton1 value '0'" "radiobutton2 value '1'"

# function comment2 () {

check 1 "statusbar1 push 'Press \"Okay\"'\n open_dialog set_filename TEST/pipegladetest.png \n open_dialog show" "open_dialog 1 '$PWD/TEST/pipegladetest.png'"
check 1 "statusbar1 push 'Press \"Okay\" again' \n save_as_dialog set_filename /somewhere/crazy_idea \n save_as_dialog show" "save_as_dialog 1 '/somewhere/crazy_idea'"

check 1 "statusbar1 push 'Press \"OK\" if the \"lorem ipsum dolor ...\" text now reads \"LABEL\"'\n label1 set_text LABEL" "button1 command"
check 1 "statusbar1 push 'Press \"OK\" if the green dot is now an X'\n image1 set_icon gtk-no" "button1 command"
check 1 "statusbar1 push 'Press \"OK\" if the red dot has turned into a green \"Q\"'\n image1 set_file TEST/pipegladetest.png" "button1 command"
check 1 "statusbar1 push 'Select \"FIRST\" from the combobox'\n comboboxtext1 insert 0 FIRST" "comboboxtext1 value 'FIRST'"
check 1 "statusbar1 push 'Select \"LAST\" from the combobox'\n comboboxtext1 insert end LAST" "comboboxtext1 value 'LAST'"
check 1 "statusbar1 push 'Select \"AVERAGE\" from the combobox'\n comboboxtext1 insert 3 AVERAGE" "comboboxtext1 value 'AVERAGE'"
check 1 "statusbar1 push 'Select the second entry from the combobox'\n comboboxtext1 remove 0" "comboboxtext1 value 'def'"
check 2 "statusbar1 push 'Click the \"+\" of the spinbutton'" "spinbutton1 value '33.00'" "spinbutton1 value '34.00'"
check 1 "statusbar1 push 'Click the \"+\" of the spinbutton again'" "spinbutton1 value '35.00'"
check 1 "statusbar1 push 'Click the \"+\" of the spinbutton once again'" "spinbutton1 value '36.00'"
check 1 "statusbar1 push 'Using the file chooser button (now labelled \"etc\"), select \"File System\" (= \"/\")'\n filechooserbutton1 set_filename '/etc/'" "filechooserbutton1 value '/'"
check 1 "statusbar1 push 'Click the font button (now labelled \"Sans Bold 40\"), and then \"Select\"'\n fontbutton1 set_font 'Sans Bold 40'" "fontbutton1 value 'Sans Bold 40'"
check 1 "statusbar1 push 'Click the color button (now turned yellow), and then \"Select\"'\n colorbutton1 set_color yellow" "colorbutton1 value 'rgb(255,255,0)'"
check 1 "colorbutton1 set_color rgb(0,255,0)\n colorbutton1 send_value" "colorbutton1 value 'rgb(0,255,0)'"
check 1 "colorbutton1 set_color '#00f'\n colorbutton1 send_value" "colorbutton1 value 'rgb(0,0,255)'"
check 1 "colorbutton1 set_color '#ffff00000000'\n colorbutton1 send_value" "colorbutton1 value 'rgb(255,0,0)'"
check 1 "colorbutton1 set_color rgba(0,255,0,.5)\n colorbutton1 send_value" "colorbutton1 value 'rgba(0,255,0,0.5)'"
check 1 "statusbar1 push 'Press \"OK\" if there is a spinning spinner'\n spinner1 start" "button1 command"
check 1 "statusbar1 push 'Press \"OK\" if the spinner has stopped'\n spinner1 stop" "button1 command"
check 1 "statusbar1 push 'Press \"OK\" if there is now a \"Disconnect\" button'\n button2 set_visible 1\n button2 set_sensitive '0'" "button1 command"
check 1 "statusbar1 push 'Press \"Disconnect\"'\n button2 set_sensitive 1" "button2 command"
check 1 "statusbar1 push 'Press \"OK\" if the window title is now \"ALMOST DONE\"'\n window set_title 'ALMOST DONE'" "button1 command"
check 1 "statusbar1 push 'Press \"OK\" if the progress bar shows 90%'\n progressbar1 set_fraction .9" "button1 command"
check 1 "statusbar1 push 'Press \"OK\" if the progress bar text reads \"The End\"'\n progressbar1 set_text 'The End'" "button1 command"
check 1 "statusbar1 push 'Press \"No\"'\n statusbar1 push 'nonsense 1'\n statusbar1 push 'nonsense 2'\n statusbar1 push 'nonsense 3'\n statusbar1 pop\n statusbar1 pop\n statusbar1 pop" "no_button command"
# }

echo "_ main_quit" >$FIN

sleep .5
check_rm $FIN
check_rm $FOUT


echo "PASSED: $OKS/$TESTS; FAILED: $FAILS/$TESTS"

