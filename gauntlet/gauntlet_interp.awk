$0~("xbstorystart" cu "$"),$0~("xbstorystop" cu "$"){
if( $1==("xbstorystart" cu) )
	{ printf "\n\n" }
if(!($1==("xbstorystop" cu) || $1==("xbstorystart" cu)))
	{ printf "%s\n",$0 }
if($1==("xbstorystop" cu))
	{ print "\nGAME TIPS:"} }
$0~("xbtipstart" cu "$"),$0~("xbtipstop" cu "$"){
if(!($1==("xbtipstop" cu) || $1==("xbtipstart" cu)))
	{ printf "\t%s\n",$0 }
if($1==("xbtipstop" cu))
	{ print "\n\tPress ENTER to continue..."
          print "\tPress S to save."
          print "\tPress L to load saved game."
          print "\tPress Q to quit.\n"
          } }