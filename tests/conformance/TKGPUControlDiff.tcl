package require Tk 8.6
proc probe {args} {
    set rc [catch {uplevel #0 $args} value]
    if {$rc} {set value ""}
    puts "$rc:$value"
}
probe button .b -text Apply -command {incr clicks}
probe .b cget -command
probe .b cget -state
probe .b configure -state disabled
probe .b cget -state
probe entry .e
probe .e insert 0 "A\u00e9Z"
probe .e index end
probe .e index -1
probe .e index 999
probe .e delete 1 2
probe .e get
probe .e icursor 1
probe .e index insert
probe .e configure -state readonly
probe .e insert end Q
probe .e get
probe .e configure -state normal
probe .e delete 0
probe .e get
probe ttk::frame .f
probe ttk::button .f.b -text OK -command {set x 1}
probe .f.b cget -text
probe .f.b cget -command
probe ttk::label .l -text Label
probe .l cget -text
probe ttk::entry .t
probe .t insert end hello
probe .t get
probe destroy .f
probe winfo exists .f.b
destroy .
