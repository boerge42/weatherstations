#!/bin/sh
# A Tcl comment, whose contents don't matter \
exec tclsh "$0" "$@"
#
#                
#                ==================
#                 Uwe Berger; 2020
#
#
#
#  wiki.bralug.de/Tcl/Tk-Benutzeroberflächen_für_gnuplot_programmieren
#
#  ---------
#  Have fun!
# 

#
#  ---------------------------------------------------------------------

package require Tk

# theoretisch potentielle Konfigurationeinstellungen
set device						/dev/ttyUSB1
set baud						230400
set databits					8
set stopbits					1
set parity						n

set gui(color_term_bg)			black
set gui(color_term_fg)			green
set gui(color_term_serial_rx)	white
set gui(term_font)				"Courier 12"

# interne Variablen...
set mode					"$baud,$parity,$databits,$stopbits"
set com						""
set com_is_open				0
set serial_is_running   	0
set csv_file_path       	"data"
set csv_filename        	""
set gp_param_file       	"gnuplot_config.gp"
set canvas_filename     	"canvas.tk"
set keep_csv            	1
set watch_history       	0
set history_csv_filename	""

# *******************************************
proc gui_init {} {
	global device mode com esp_cmd nodemcu_echo server_echo gui gp_param_file
	global csv_file_path keep_csv watch_history
	wm title . "serial2plot" 
	wm resizable .
	wm minsize . 600 500

	frame .f1
	pack  .f1 -side bottom -fill x

	# Serielle Schnittstelle zu ESP-Modul
	labelframe .f1.f_con  -text "Serial Port"
	label .f1.f_con.l_port -text "Port"
	entry .f1.f_con.e_port -textvariable device -width 15
	label .f1.f_con.l_mode -text "Mode"
	entry .f1.f_con.e_mode -textvariable mode -width 15
	button .f1.f_con.b_con_open -text "Connect..." -command {serial_open}
	pack .f1.f_con -side left -fill x -pady 2 -padx 2
	pack .f1.f_con.l_port .f1.f_con.e_port\
		 .f1.f_con.l_mode .f1.f_con.e_mode\
		 .f1.f_con.b_con_open\
		 -side left  -pady 2 -padx 2
	
	# Gnuplot Parameter
	labelframe .f1.f_gp  -text "Gnuplot Parameterfile"
	label .f1.f_gp.l_fn -text $gp_param_file
	button .f1.f_gp.b_fn_open -text "..." -command {set_gp_param_filename}
	pack .f1.f_gp -side left -fill x -pady 2 -padx 2
	pack .f1.f_gp.l_fn .f1.f_gp.b_fn_open\
		 -side left  -pady 2 -padx 2

	# csv-File Parameter
	labelframe .f1.f_csv  -text "csv-Files"
	label .f1.f_csv.l_csv_path -text "path: $csv_file_path"
	button .f1.f_csv.b_fn_open -text "..." -command {set_csv_file_path}
    label .f1.f_csv.l_keep_csv -text "keep csv-files:"
	checkbutton .f1.f_csv.cb_keep_csv -variable keep_csv
	pack .f1.f_csv -side left -fill x -pady 2 -padx 2
	pack .f1.f_csv.l_csv_path .f1.f_csv.b_fn_open\
		 .f1.f_csv.l_keep_csv .f1.f_csv.cb_keep_csv\
		 -side left  -pady 2 -padx 2

	# History
	labelframe .f1.f_history  -text "History"
    label .f1.f_history.l_history -text "watch history:"
	checkbutton .f1.f_history.cb_history -variable watch_history -command {set_history_gui $watch_history}
	button .f1.f_history.b_file_open -text "Display csv-file" -command {display_history_csv_file}
	button .f1.f_history.b_refresh -text "Refresh" -command {refresh_history_csv_file}
	pack .f1.f_history -side left -fill x -pady 2 -padx 2
	pack .f1.f_history.l_history .f1.f_history.cb_history\
		 .f1.f_history.b_file_open\
		 .f1.f_history.b_refresh\
		 -side left  -pady 2 -padx 2
	set_history_gui $watch_history

	# ...sonstige Kommandos
	labelframe .f1.f_prog  -text "Program"
	button .f1.f_prog.term_clear -text "Clear Console" -command {term_clear} 
	button .f1.f_prog.exit -text "Exit" -command {exit} 
	pack .f1.f_prog -side right -fill x -pady 2 -padx 2
	pack .f1.f_prog.exit .f1.f_prog.term_clear -side right -padx 2 -pady 2
	
	# serielle Konsole
	labelframe .f_term -text "Terminal"
	text .f_term.term -bd 2 -bg $gui(color_term_bg) -fg $gui(color_term_fg)\
		-font $gui(term_font)\
		-yscrollcommand ".f_term.yscroll set"
	scrollbar .f_term.yscroll -command {.f_term.term yview}
	pack .f_term -side left -fill both -expand yes -padx 2 -pady 2
	pack .f_term.yscroll -side right -fill y
	pack .f_term.term -fill both -expand yes
	# Konsolenfarben
	.f_term.term tag configure color_rx -background $gui(color_term_bg) -foreground $gui(color_term_serial_rx)

	# --> hier packen wir das gnuplot-Canvas rein
	labelframe .f_dia  -text "Diagram"
	canvas .g -width 1500 -height 1000 -bg white
	pack .f_dia -side left -fill both -padx 2 -pady 2
	pack .g -in .f_dia
	
}

# *******************************************
proc error_msg {msg} {
	tk_messageBox -title "Error!" -message $msg -icon error -type ok
}

# *******************************************
proc set_widget_state {tl state childs} {
	
	if {$childs == 1} {
		foreach w [winfo children $tl] {
			$w configure -state $state
		}
	} else {
		$tl configure -state $state
	}
}

# *******************************************
proc set_history_gui {state} {
	if {$state == 1} {
		set_widget_state .f1.f_history.b_file_open normal 0
		set_widget_state .f1.f_history.b_refresh normal 0
	} else {
		set_widget_state .f1.f_history.b_file_open disable 0
		set_widget_state .f1.f_history.b_refresh disable 0
	}
}

# *******************************************
proc display_history_csv_file {} {
	global gp_param_file csv_file_path csv_filename history_csv_filename
	set history_csv_filename [tk_getOpenFile -title "Open a historical file..." -initialdir $csv_file_path -initialfile $history_csv_filename]
	if {$history_csv_filename != ""} {
		generate_diagramm $history_csv_filename $gp_param_file
	}
}

# *******************************************
proc refresh_history_csv_file {} {
	global gp_param_file history_csv_filename
	if {$history_csv_filename != ""} {
		generate_diagramm $history_csv_filename $gp_param_file
	}
}



# *******************************************
proc load_display_canvas {} {
	global canvas_filename
	source $canvas_filename
	gnuplot .g
	file delete $canvas_filename
}

# *******************************************
proc gp_param_file_replaces {s csv} {
	global csv_filename canvas_filename
	set s [string map [list ::csv_filename:: $csv] $s]
	set s [string map [list ::dx:: [.g cget -width]] $s]
	set s [string map [list ::dy:: [.g cget -height]] $s]
	set s [string map [list ::canvas_filename:: $canvas_filename] $s]
	return $s
}

# *******************************************
proc generate_diagramm {csv cfg} {
	# gnuplot-Parameter aus Datei auslesen
	if {[catch {set fid [open $cfg r]} err]} {
		error_msg $err
		return			
	}
	set cfg_data [read $fid [file size $cfg]]
	close $fid
	# Content parsen/ersetzen
	set cfg_data [gp_param_file_replaces $cfg_data $csv]
	# gnuplot oeffnen und Parameterfile laden/ausfuehren
	set pid [open "| gnuplot" "w"]
	puts $pid $cfg_data
	close $pid
    # gnuplot-canvas laden/anzeigen
	load_display_canvas
	.f_dia  configure -text "Diagram for $csv"
}

# *******************************************
proc  generate_csv_filename {path} {
	set s [clock format [clock seconds] -format "%Y%m%d_%H%M%S"]
    return "$path/$s.csv"
}

# *******************************************
proc  generate_csv {} {
	global serial_is_running gp_param_file csv_filename csv_file_path
	global keep_csv watch_history
	set serial_is_running 0
	# Terminalinhalt in eine Datei sichern
	set csv_filename [generate_csv_filename $csv_file_path]
	set csv_data [.f_term.term get 1.0 end]
	set fid [open $csv_filename w]
	puts $fid $csv_data
	close $fid
	# ...und daraus ein Diagramm erstellen, wenn nicht im history-Modus
	if {$watch_history != 1} {
		generate_diagramm $csv_filename $gp_param_file
	}
	# eventuell csv-Datei loeschen?
	if {$keep_csv != 1} {
		file delete $csv_filename
	}
}

# *******************************************
proc set_gp_param_filename {} {
	global gp_param_file
	set fn [tk_getOpenFile -title "Parameterfile for gnuplot"]
	if {$fn != ""} {
		set gp_param_file $fn
		.f1.f_gp.l_fn configure -text $gp_param_file
	}
}

# *******************************************
proc set_csv_file_path {} {
		global csv_file_path
		set dir [tk_chooseDirectory -title "Directory for csv-files"]
		if {$dir != ""} {
			set csv_file_path $dir
			.f1.f_csv.l_csv_path configure -text "path: $csv_file_path"
		}
}


# *******************************************
proc term_clear {} {
	.f_term.term delete 0.0 end
}

# *******************************************
proc serial_open {} {
	global device mode com com_is_open
	if {$com_is_open == 0} {
		if {[catch {set com [open $device RDONLY]} err]} {
			error_msg $err
			return			
		}
		fconfigure $com -mode $mode -translation binary -buffering none -blocking 0
		fileevent $com readable {serial_rx $com}
		set com_is_open 1
		set_widget_state .f1.f_con.e_port disabled 0
		set_widget_state .f1.f_con.e_mode disabled 0
		.f1.f_con.b_con_open configure -text "Disconnect..."
	} else {
		close $com
		set com_is_open 0
		set_widget_state .f1.f_con.e_port normal 0
		set_widget_state .f1.f_con.e_mode normal 0
		.f1.f_con.b_con_open configure -text "Connect..."
	}
}

# *******************************************
proc serial_rx {com} {
	global serial_is_running
	if {$serial_is_running == 0} term_clear
	set serial_is_running 1
	.f_term.term insert end [read $com 1] color_rx
	.f_term.term see end
	after cancel generate_csv
	set after_id [after 1000 generate_csv]
}

# *******************************************
# *******************************************
# *******************************************

# GUI...
gui_init
