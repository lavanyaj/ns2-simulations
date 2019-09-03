set ns [new Simulator]
puts "Date: [clock format [clock seconds]]"
set sim_start [clock seconds]
puts "Host: [exec uname -a]"
set total_actfl 0
source "parse-args.tcl"


proc setupB4TopologyFromFile {filename} {
    #global K TOPOLOGY_X MEAN_LINK_DELAY n a c
    global HOST_DELAY  switchAlg
    global LINK_RATE
    global ns s n queue
    global alllog
    global num_sites num_links
    global city_to_site site_to_city name_to_nodeid nodeid_to_name nodeid_to_node
    global timer_delay
    set timer_delay [new RandomVariable/Uniform]
    $timer_delay set max_ 0.1
    $timer_delay set min_ 0.9


    set f [open $filename]
    puts "setupB4TopologyFromFile: opened $f"
    set config_list [split [read $f] "\n"]

    # format first list all the site names, then all (2-way) links
    # index) site_name
    # src_site dst_site distance
    set num_sites 0
    set num_links 0
    # for each site- we setup a source/sink node s and a switch node n
    # first we set up the source/sink node so that node x is site x etc., switch node for site x is num_sites+x
    foreach config $config_list {
	set tokens [split $config]
	set i 0

	foreach tok $tokens {
	    set tok_list($i) $tok
	    incr i
	}
	puts "read $tok_list(0) $tok_list(1)"

	if {$i == 2} {	    
	    puts "processing $tok_list(0) $tok_list(1)"
	    set city_to_site($tok_list(1)) $num_sites
	    set site_to_city($num_sites) $tok_list(1)

	    set s($num_sites) [$ns node]
	    set nodeid [$s($num_sites) id]
	    set nodeid_to_name($nodeid) s$num_sites
	    set nodeid_to_node($nodeid) $s($num_sites)
	    set name_to_nodeid(s$num_sites) $nodeid

	    set src_node $s($num_sites)
	    set src s$num_sites
	    set src_node_id [$src_node id]
	    # puts "setting up site # $num_sites for $tok_list(1): s$num_sites has id $nodeid"

	    if { "$num_sites" == "$tok_list(0)" } {
		incr num_sites
	    } else {
		puts "expected site number '$num_sites', got '$tok_list(0)' $tok_list(1)"
		exit
	    }

	}

    }

    set save_num_sites $num_sites
    set num_sites 0

    foreach config $config_list {
	set tokens [split $config]
	set i 0

	foreach tok $tokens {
	    set tok_list($i) $tok
	    incr i
	}
	puts "read $tok_list(0) $tok_list(1)"

	if {$i == 2} {	    
	    puts "processing $tok_list(0) $tok_list(1)"
	    # set city_to_site($tok_list(1)) $num_sites
	    # set site_to_city($num_sites) $tok_list(1)

	    # set s($num_sites) [$ns node]
	    # set nodeid [$s($num_sites) id]
	    # set nodeid_to_name($nodeid) s$num_sites
	    # set nodeid_to_node($nodeid) $s($num_sites)
	    # set name_to_nodeid(s$num_sites) $nodeid

	    set src_node $s($num_sites)
	    set src s$num_sites
	    set src_node_id [$src_node id]

	    # puts "setting up site # $num_sites for $tok_list(1): s$num_sites has id $nodeid"

	    set n($num_sites) [$ns node]
	    set nodeid [$n($num_sites) id]
	    set nodeid_to_name($nodeid) n$num_sites
	    set nodeid_to_node($nodeid) $n($num_sites)
	    set name_to_nodeid(n$num_sites) $nodeid

	    set dst_node $n($num_sites)
	    set dst n$num_sites
	    set dst_node_id [$dst_node id]

	    puts "setting up site # $num_sites for $tok_list(1):  n$num_sites has id $nodeid"
	    set host_link_rate [expr 2 * $LINK_RATE]
	    $ns simplex-link $src_node $dst_node [set host_link_rate]Gb [expr $HOST_DELAY] $switchAlg   
	    $ns simplex-link $dst_node $src_node [set host_link_rate]Gb [expr $HOST_DELAY] $switchAlg   

	    $ns cost $src_node $dst_node 1
	    $ns cost $dst_node $src_node 1

	    set read_costf [[$ns link $src_node $dst_node] cost? ]
	    set read_costb [[$ns link $dst_node $src_node] cost? ]

	    
	    set queue($src,$dst) [[$ns link $src_node $dst_node] queue ]
	    set queue($dst,$src) [[$ns link $dst_node $src_node] queue ]

	    set l($num_links) queue($src,$dst)

	    set city $site_to_city($num_sites)

	    puts "l($num_links) site $city  from $src to $dst $HOST_DELAY s, $host_link_rate Gb/s, link cost $read_costf"

	    puts "l($num_links) site $city  from $dst to $src $HOST_DELAY s, $host_link_rate Gb/s, link cost $read_costb"

	    puts "l($num_links) site $city queue($src,$dst) from $src to $dst $HOST_DELAY s, $host_link_rate Gb/s, queue_code  $src_node_id $dst_node_id"
	    incr num_links

	    set l($num_links) queue($dst,$src)
	    puts "l($num_links) site $city queue($dst,$src) from $dst to $src $HOST_DELAY s, $host_link_rate Gb/s, queue_code $dst_node_id $src_node_id"
	    incr num_links

	    if { "$switchAlg" == "DropTail/RCP" } {
		$queue($src,$dst) set-link-capacity [expr $host_link_rate * 125000000]
		$queue($dst,$src) set-link-capacity [expr $host_link_rate * 125000000]    
	    }
	    if { "$switchAlg" == "Priority/SPERC" } {
		set val [$timer_delay value]
		set t0 [expr 0 + $val]
	     	$ns at $t0 "$queue($dst,$src) start-timeout"	  

		puts "queue($dst, $src) starts timeout at $t0"

		set val [$timer_delay value]
		set t0 [expr 0 + $val]
	     	$ns at $t0 "$queue($src,$dst) start-timeout"

		puts "queue($src, $dst) starts timeout at $t0"

		set sperc_qlogfile($dst,$src) [open sperc_queuelog_$dst\_$src.tr w]
		$queue($dst,$src) attach-sperc $sperc_qlogfile($dst,$src)

		set sperc_qlogfile($src,$dst) [open sperc_queuelog_$src\_$dst.tr w]
		$queue($src,$dst) attach-sperc $sperc_qlogfile($src,$dst)
		

	    }		    	    
	    if { "$num_sites" == "$tok_list(0)" } {
	    	incr num_sites
	    } else {
	    	puts "expected site number '$num_sites', got '$tok_list(0)' $tok_list(1)"
	    	exit
	    }
	} else {
	if {$i == 4} {
	    puts "processing $tok_list(0) $tok_list(1) $tok_list(2) $tok_list(3)"
	    
		set city1 $tok_list(1)
	        set city2 $tok_list(2)
		
		set site1 $city_to_site($city1)
		set src n$site1
		set site2 $city_to_site($city2)
		set dst n$site2
		set distance $tok_list(3)
		
		set src_node $n($site1)
		set dst_node $n($site2)
	        set src_node_id [$src_node id]
	        set dst_node_id [$dst_node id]

		set link_delay [expr (($distance*1e3)/(3*1e8))]
		$ns simplex-link $src_node $dst_node [set LINK_RATE]Gb [expr $HOST_DELAY + $link_delay] $switchAlg   
		$ns simplex-link $dst_node $src_node [set LINK_RATE]Gb [expr $HOST_DELAY + $link_delay] $switchAlg   
		
                set cost1 [expr $link_delay * 1000]
	        if {$cost1 < 1} { set cost1 1} 

	        $ns cost $src_node $dst_node $cost1
	        $ns cost $dst_node $src_node $cost1

	        set read_costf [[$ns link $src_node $dst_node] cost? ]
	        set read_costb [[$ns link $dst_node $src_node] cost? ]

		set queue($src,$dst) [[$ns link $src_node $dst_node] queue ]
		set queue($dst,$src) [[$ns link $dst_node $src_node] queue ]

		set l($num_links) queue($src,$dst)
		puts "l($num_links) queue($src,$dst) from $src ($city1) to $dst ($city2) $distance km, $link_delay s, $LINK_RATE Gb/s, queue_code $src_node_id $dst_node_id"

	    puts "l($num_links) from $src ($city1) to $dst ($city2) $distance km, $link_delay s, $LINK_RATE Gb/s, link cost $read_costf"
		incr num_links
		
		set l($num_links) queue($dst,$src)
		puts "l($num_links) queue($dst,$src) from $dst ($city2) to $src ($city1) $distance km, $link_delay s, $LINK_RATE Gb/s, queue_code $dst_node_id $src_node_id"
		puts "l($num_links) from $dst ($city2) to $src ($city1) $distance km, $link_delay s, $LINK_RATE Gb/s, link cost $read_costb"

		incr num_links

		if { "$switchAlg" == "DropTail/RCP" } {
		    $queue($src,$dst) set-link-capacity [expr $LINK_RATE * 125000000]
		    $queue($dst,$src) set-link-capacity [expr $LINK_RATE * 125000000]    
		}

		if { "$switchAlg" == "Priority/SPERC" } {
		    set val [$timer_delay value]
		    set t0 [expr 1 + $val]
		    $ns at $t0 "$queue($dst,$src) start-timeout"	  

		    puts "queue($dst, $src) starts timeout at $t0"

		    set val [$timer_delay value]
		    set t0 [expr 1 + $val]
		    $ns at $t0 "$queue($src,$dst) start-timeout"

		    puts "queue($src, $dst) starts timeout at $t0"

		    set sperc_qlogfile($dst,$src) [open sperc_queuelog_$dst\_$src.tr w]
		    $queue($dst,$src) attach-sperc $sperc_qlogfile($dst,$src)
		    
		    set sperc_qlogfile($src,$dst) [open sperc_queuelog_$src\_$dst.tr w]
		    $queue($src,$dst) attach-sperc $sperc_qlogfile($src,$dst)

		    # $ns at 1 "$queue($dst,$src) start-timeout"	  
		    # $ns at 1 "$queue($src,$dst) start-timeout"
		}	    
	}
	}
    }
}

proc setupClosTopology {} {
    # given TOPOLOGY_X only, over-write everything else
    global K TOPOLOGY_X
    global HOST_DELAY MEAN_LINK_DELAY switchAlg
    global LINK_RATE
    global ns s n a c queue
    global alllog
    # we'll set the following here
    global num_servers num_tors num_aggs num_cores

    set K 4
    # layer 0, use s(0) for first server. K/2 * K/2 per pod
    set num_servers [expr ($K * $K * $K)/4 ]
    # layer 1, use n(0) for first tor switch. K/2 per pod
    set num_tors [expr ($K * $K)/2 ]


    # layer 2, use a(0) for first agg switch. K/2 per pod
    set num_aggs [ expr ($K * $K)/2 ]
    # layer 3, use c(0) for first core switch. K/2 * K/2 total
    set num_cores [ expr ($K * $K)/4 ]
    set servers_per_pod [ expr $num_servers/$K ]
    set servers_per_tor [ expr $num_servers/$num_tors ]
    set tors_per_pod [ expr $num_tors/$K ]
    set aggs_per_pod [ expr $num_aggs/$K ]    
    set cores_per_agg [ expr $K/2 ]

    set server_uplink_rate $LINK_RATE

    # ($LINK_RATE)
    # up from tor: tor_uplink_rate * aggs_per_pod (K/2)
    # down from tor: LINK_RATE * servers_per_tor (K/2)
    set tor_uplink_rate [expr ($server_uplink_rate * ($servers_per_tor)/($aggs_per_pod))/($TOPOLOGY_X)]

    # ($tor_uplink_rate)
    # down from agg: tor_uplink_rate * tors_per_pod (K/2)
    # up from agg: agg_uplink_rate * cores_per_agg (K/2)
    # we're not letting agg_uplink be oversubscribed so both equal
    set agg_uplink_rate [expr ($tor_uplink_rate * ($tors_per_pod)/($cores_per_agg))]

    puts "setting up clos topology, links are of type $switchAlg"
    puts "server uplink rate: $server_uplink_rate"
    puts "tor uplink rate: $tor_uplink_rate" 
    puts "agg uplink rate: $agg_uplink_rate" 
    puts " given over-subscription ratio $TOPOLOGY_X"
    puts "$num_servers servers, $num_tors tors, $num_aggs aggs, 1 core"
    puts "each server connects to one tor, each tor connects to $servers_per_tor servers in its pod"
    puts "full mesh between $tors_per_pod tors and $aggs_per_pod aggs in each pod"
    puts "ith agg in a pod connects to ith $cores_per_agg core switches"

    puts "for a tor: $tor_uplink_rate * $aggs_per_pod G up $server_uplink_rate * $servers_per_tor G down"
    puts "for an agg: $agg_uplink_rate * $cores_per_agg G up $tor_uplink_rate * $tors_per_pod G down"

    # monitor queues use names of switches
    # agents setup using index of server
    #  agtagr(1,2) between s(1) and s(2)
    

    # setup nodes
    for {set i 0} {$i < $num_servers} {incr i} {
	set s($i) [$ns node]
    }
    for {set i 0} {$i < $num_tors} {incr i} {
	set n($i) [$ns node]
    }
    for {set i 0} {$i < $num_aggs} {incr i} {
	set a($i) [$ns node]
    }
    for {set i 0} {$i < $num_cores} {incr i} {
	set c($i) [$ns node]
    }

    map_nodes_to_names

    set num_links 0
    # setup links
    # K pods. each pod has K/2 TORs and K/2 * K/2
    # or (S/K) servers. each TOR in charge of
    # K/2 servers or (S/(K/2)) servers.
    for {set j 0} {$j < $num_tors} {incr j} {
	for {set k 0} {$k < $servers_per_tor} {incr k} {
	    set i [expr ($j * $servers_per_tor) + $k ]
	    $ns simplex-link $s($i) $n($j) [set server_uplink_rate]Gb \
		[expr $HOST_DELAY + $MEAN_LINK_DELAY] $switchAlg        
	    $ns simplex-link $n($j) $s($i) [set server_uplink_rate]Gb \
		[expr $HOST_DELAY + $MEAN_LINK_DELAY] $switchAlg
	    set queue(s$i,n$j) [[$ns link $s($i) $n($j)] queue ]
	    set queue(n$j,s$i) [[$ns link $n($j) $s($i)] queue ]
	    
	    set src [$s($i) id]
	    set dst [$n($j) id]
	    set delay [expr $HOST_DELAY + $MEAN_LINK_DELAY]
	    set l($num_links) queue(s$i,n$j)
	    puts "l($num_links) server-tor queue(s$i,n$j) from s$i to n$j $delay s, $server_uplink_rate Gb/s, queue_code  $src $dst"
	    incr num_links
	    set l($num_links) queue(n$j,s$i)
	    puts "l($num_links) tor-server queue(n$j, s$i) from n$j to s$i $delay s, $server_uplink_rate Gb/s, queue_code  $dst $src"
	    incr num_links

	    puts $alllog "connect s$i and n$j"
	    if { "$switchAlg" == "DropTail/RCP" } {
		$queue(n$j,s$i) set-link-capacity [expr [set server_uplink_rate] * 125000000]
		$queue(s$i,n$j) set-link-capacity [expr [set server_uplink_rate] * 125000000]    
	    }
	    if { "$switchAlg" == "DRR/SPERC" } {
		$ns at 1 "$queue(n$j,s$i) start-timeout"	    
		$ns at 1 "$queue(s$i,n$j) start-timeout"
	    }		    
	    if { "$switchAlg" == "Priority/SPERC" } {
		set val [$timer_delay value]
		set t0 [expr 1 + $val]
		$ns at $t0 "$queue(n$j,s$i) start-timeout"	  
		
		puts "queue(n$j, s$i) starts timeout at $t0"
		
		set val [$timer_delay value]
		set t0 [expr 1 + $val]
		$ns at $t0 "$queue(s$i,n$j) start-timeout"

		puts "queue(s$i, n$j) starts timeout at $t0"
		
		set sperc_qlogfile(n$j,s$i) [open sperc_queuelog_$dst_$src.tr w]
		$queue(n$j,s$i) attach-sperc $sperc_qlogfile(n$j,s$i)
		    
		set sperc_qlogfile(s$i,n$j) [open sperc_queuelog_$src_$dst.tr w]
		$queue(s$i,n$j) attach-sperc $sperc_qlogfile(s$i,n$j)

		# $ns at 1 "$queue(n$j,s$i) start-timeout"	    
		# $ns at 1 "$queue(s$i,n$j) start-timeout"
	    }		    

	}
    }

    # for each pod, connect all K/2 tors (t) in pod
    # to all K/2 agg switches (a) in pod
    for {set pod 0} { $pod < $K } {incr pod} {
	set start_index [ expr $pod * $tors_per_pod ]
	set last_index [ expr (($pod+1)*($tors_per_pod))-1]
	for {set i $start_index} {$i <= $last_index} {incr i} {
	    for {set j $start_index} {$j <= $last_index} {incr j} {
		$ns duplex-link $n($i) $a($j) [set tor_uplink_rate]Gb $MEAN_LINK_DELAY $switchAlg	
		set queue(n$i,a$j) [[$ns link $n($i) $a($j)] queue ]
		set queue(a$j,n$i) [[$ns link $a($j) $n($i)] queue ]

		set src [$n($i) id]
		set dst [$a($j) id]
		set delay [expr $MEAN_LINK_DELAY]
		set l($num_links) queue(n$i,a$j)
		puts "l($num_links) tor-agg queue(n$i,a$j) from n$i to a$j $delay s, $tor_uplink_rate Gb/s, queue_code  $src $dst"
		incr num_links
		set l($num_links) queue(a$j,n$i)
		puts "l($num_links) agg-tor queue(a$j, n$i) from a$j to n$i $delay s, $tor_uplink_rate Gb/s, queue_code  $dst $src"
		incr num_links

		if { "$switchAlg" == "DropTail/RCP" } {
		    $queue(n$i,a$j) set-link-capacity [expr [set server_uplink_rate] * 125000000]
		    $queue(a$j,n$i) set-link-capacity [expr [set server_uplink_rate] * 125000000]    
		}
		if { "$switchAlg" == "DRR/SPERC" } {
		    $ns at 1 "$queue(n$i,a$j) start-timeout"	    
		    $ns at 1 "$queue(a$j,n$i) start-timeout"
		}		    
		if { "$switchAlg" == "Priority/SPERC" } {

		    set val [$timer_delay value]
		    set t0 [expr 1 + $val]
		    $ns at $t0 "$queue(a$j,n$i) start-timeout"	  
		    
		    puts "queue(a$j, n$i) starts timeout at $t0"

		    set val [$timer_delay value]
		    set t0 [expr 1 + $val]
		    $ns at $t0 "$queue(n$i,a$j) start-timeout"
		    
		    puts "queue(n$i, a$j) starts timeout at $t0"
		    
		    set sperc_qlogfile(a$j,n$i) [open sperc_queuelog_$dst_$src.tr w]
		    $queue(a$j,n$i) attach-sperc $sperc_qlogfile(a$j,n$i)
		    
		    set sperc_qlogfile(n$i,a$j) [open sperc_queuelog_$src_$dst.tr w]
		    $queue(n$i,a$j) attach-sperc $sperc_qlogfile(n$i,a$j)

		    # $ns at 1 "$queue(n$i,a$j) start-timeout"	    
		    # $ns at 1 "$queue(a$j,n$i) start-timeout"
		}		    

	    }
	}
    }

    # set num_aggs [ ($K * $K)/2 ]
    # set aggs_per_pod [ $num_aggs/$K ]
    # set num_cores [ ($K * $K)/4 ]
    
    # 1st agg in each pod to each of 1st K/2 core
    # 2nd agg in each pod to each of 2nd K/2 core and so on
    # K/2th agg  ..                  K/2th K/2 core.

    for {set pod 0} { $pod < $K } {incr pod} {
	set agg_start [ expr $pod * $aggs_per_pod ]
	for {set k 0} {$k<$aggs_per_pod} {incr k} {
	    set core_start [ expr $k * $cores_per_agg ]
	    for {set l 0} {$l<$cores_per_agg} {incr l} {
		set i [ expr $agg_start + $k]
		set j [ expr $core_start + $l]		
		$ns duplex-link $a($i) $c($j) [set agg_uplink_rate]Gb $MEAN_LINK_DELAY $switchAlg
		set queue(a$i,c$j) [[$ns link $a($i) $c($j)] queue ]
		set queue(c$j,a$i) [[$ns link $c($j) $a($i)] queue ]


		set src [$a($i) id]
		set dst [$c($j) id]
		set delay [expr $MEAN_LINK_DELAY]
		set l($num_links) queue(a$i,c$j)
		puts "l($num_links) agg-core queue(a$i,c$j) from a$i to c$j $delay s, $agg_uplink_rate Gb/s, queue_code  $src $dst"
		incr num_links
		set l($num_links) queue(c$j,a$i)
		puts "l($num_links) core-agg queue(c$j, a$i) from c$j to a$i $delay s, $agg_uplink_rate Gb/s, queue_code  $dst $src"
		incr num_links

		if { "$switchAlg" == "DropTail/RCP" } {
		    $queue(a$i,c$j) set-link-capacity [expr [set server_uplink_rate] * 125000000]
		    $queue(c$j,a$i) set-link-capacity [expr [set server_uplink_rate] * 125000000]    
		}
		if { "$switchAlg" == "DRR/SPERC" } {
		    $ns at 1 "$queue(a$i,c$j) start-timeout"	    
		    $ns at 1 "$queue(c$j,a$i) start-timeout"
		}		    

		if { "$switchAlg" == "Priority/SPERC" } {
		    set val [$timer_delay value]
		    set t0 [expr 1 + $val]
		    $ns at $t0 "$queue(c$j,a$i) start-timeout"	  

		    puts "queue(c$j, a$i) starts timeout at $t0"

		    set val [$timer_delay value]
		    set t0 [expr 1 + $val]
		    $ns at $t0 "$queue(a$i,c$j) start-timeout"

		    puts "queue(a$i, c$j) starts timeout at $t0"

		    set sperc_qlogfile(c$j,a$i) [open sperc_queuelog_${dst}_${src}.tr w]
		    $queue(c$j,a$i) attach-sperc $sperc_qlogfile(c$j,a$i)
		    
		    set sperc_qlogfile(a$i,c$j) [open sperc_queuelog_${src}_${dst}.tr w]
		    $queue(a$i,c$j) attach-sperc $sperc_qlogfile(a$i,c$j)

		    # $ns at 1 "$queue($dst,$src) start-timeout"	  
		    # $ns at 1 "$queue($src,$dst) start-timeout"

		    # $ns at 1 "$queue(a$i,c$j) start-timeout"	    
		    # $ns at 1 "$queue(c$j,a$i) start-timeout"
		}		    

	    }
	}
    }
}
proc map_nodes_to_names {} {
    global s n a c
    global num_servers num_tors num_aggs num_cores num_spines TOPOLOGY_TYPE
    global nodeid nodeid_to_name nodeid_to_node

    for {set i 0} {$i < $num_servers} {incr i} {
	set nodeid [$s($i) id]    
	set nodeid_to_name($nodeid) "s$i"
	set nodeid_to_node($nodeid) $s($i)
	puts "nodeid $nodeid name $nodeid_to_name($nodeid) nodeid_to_node $nodeid_to_node($nodeid) its id [$nodeid_to_node($nodeid) id]"
    }
    for {set i 0} {$i < $num_tors} {incr i} {
	set nodeid [$n($i) id]
	set nodeid_to_name($nodeid) "n$i"
	set nodeid_to_node($nodeid) $n($i)
	puts "nodeid $nodeid name $nodeid_to_name($nodeid) nodeid_to_node $nodeid_to_node($nodeid) its id [$nodeid_to_node($nodeid) id]"
    }
    
    if {$TOPOLOGY_TYPE == "spine-leaf"} {
	for {set j 0} {$j < $num_spines} {incr j} {
	    set nodeid [$a($j) id]
	    set nodeid_to_name($nodeid) "a$j"
	    set nodeid_to_node($nodeid) $a($j)
	    puts "nodeid $nodeid name $nodeid_to_name($nodeid) nodeid_to_node $nodeid_to_node($nodeid) its id [$nodeid_to_node($nodeid) id]"
	}

    }

    if {$TOPOLOGY_TYPE == "clos"} {
	for {set j 0} {$j < $num_aggs} {incr j} {
	    set nodeid [$a($j) id]
	    set nodeid_to_name($nodeid) "a$j"
	    set nodeid_to_node($nodeid) $a($j)
	    puts "nodeid $nodeid name $nodeid_to_name($nodeid) nodeid_to_node $nodeid_to_node($nodeid) its id [$nodeid_to_node($nodeid) id]"
	}

	for {set j 0} {$j < $num_cores} {incr j} {
	    set nodeid [$c($j) id]
	    set nodeid_to_name($nodeid) "c$j"
	    set nodeid_to_node($nodeid) $c($j)
	    puts "nodeid $nodeid name $nodeid_to_name($nodeid) nodeid_to_node $nodeid_to_node($nodeid) its id [$nodeid_to_node($nodeid) id]"
	}
    }
    
}
# setup Spine Leaf topology - 2 level, 144 hosts
proc setupSpineLeafTopology {} {
    # given TOPOLOGY_X (oversubscription ratio)
    global LINK_RATE  TOPOLOGY_X
    global ns s n a queue
    global HOST_DELAY MEAN_LINK_DELAY switchAlg
    global TRACE_QUEUES
    global TOPOLOGY_TYPE

    global timer_delay
    set timer_delay [new RandomVariable/Uniform]
    $timer_delay set max_ 0.1
    $timer_delay set min_ 0.9

    # currently, we make a topology over-subscribed by adjusting the tor uplink rate
    # number of servers, tors and spine switches are fixed.
    # we set the following here
    global num_servers num_tors num_spines
    set num_servers 144
    set num_tors 9
    set num_spines 4
    set servers_per_tor [expr $num_servers/$num_tors]
    set server_uplink_rate $LINK_RATE
    set tor_uplink_rate [expr $LINK_RATE * $servers_per_tor / $num_spines / $TOPOLOGY_X] ; #uplink rate

    # (10 * 16/4)/1
    puts "$num_servers servers $num_tors tors $num_spines spines"
    puts "server uplink rate: $server_uplink_rate"
    puts "tor uplink rate: $tor_uplink_rate" 
    puts " given over-subscription ratio $TOPOLOGY_X"
    puts "each server connects to one tor, each tor connects to $servers_per_tor servers"
    puts "full mesh between $num_tors tors and $num_spines spine switches"
    puts "for a tor: $tor_uplink_rate * $num_spines G up $server_uplink_rate * $servers_per_tor G down"

    for {set i 0} {$i < $num_servers} {incr i} {
	set s($i) [$ns node]
    }
    for {set i 0} {$i < $num_tors} {incr i} {
	set n($i) [$ns node]
    }   
    for {set i 0} {$i < $num_spines} {incr i} {
	set a($i) [$ns node]
    }
    
    map_nodes_to_names
    set num_links 0
    for {set i 0} {$i < $num_servers} {incr i} {
	set j [expr $i/$servers_per_tor]
	$ns simplex-link $s($i) $n($j) [set server_uplink_rate]Gb [expr $HOST_DELAY + $MEAN_LINK_DELAY] $switchAlg        
	$ns simplex-link $n($j) $s($i) [set server_uplink_rate]Gb [expr $HOST_DELAY + $MEAN_LINK_DELAY] $switchAlg
	set queue(s$i,n$j) [[$ns link $s($i) $n($j)] queue ]
	set queue(n$j,s$i) [[$ns link $n($j) $s($i)] queue ]

	set src [$s($i) id]
	set dst [$n($j) id]
	set delay [expr $HOST_DELAY + $MEAN_LINK_DELAY]
	set l($num_links) queue(s$i,n$j)
	puts "l($num_links) server-tor queue(s$i,n$j) from s$i to n$j $delay s, $server_uplink_rate Gb/s, queue_code  $src $dst"
	incr num_links
	set l($num_links) queue(n$j,s$i)
	puts "l($num_links) tor-server queue(n$j, s$i) from n$j to s$i $delay s, $server_uplink_rate Gb/s, queue_code  $dst $src"
	incr num_links

	if { "$switchAlg" == "DropTail/RCP" } {
	    $queue(n$j,s$i) set-link-capacity [expr [set server_uplink_rate] * 125000000]
	    $queue(s$i,n$j) set-link-capacity [expr [set server_uplink_rate] * 125000000]    
	}
	# here we can schedule the queue timeouts to start at 1, also resets state at link
	if { "$switchAlg" == "DRR/SPERC" } {
	    $ns at 1 "$queue(n$j,s$i) start-timeout"	    
	    $ns at 1 "$queue(s$i,n$j) start-timeout"	
	}
	if { "$switchAlg" == "Priority/SPERC" } {
	    set val [$timer_delay value]
	    set t0 [expr 1 + $val]
	    $ns at $t0 "$queue(n$j,s$i) start-timeout"	  
	    
	    puts "queue(n$j, s$i) starts timeout at $t0"
		
	    set val [$timer_delay value]
	    set t0 [expr 1 + $val]
	    $ns at $t0 "$queue(s$i,n$j) start-timeout"
	    
	    puts "queue(s$i, n$j) starts timeout at $t0"

	    # just monitor links in LOG_QUEUES file		
	    # set sperc_qlogfile(n$j,s$i) [open sperc_queuelog_n$j\_s$i.tr w]
	    # $queue(n$j,s$i) attach-sperc $sperc_qlogfile(n$j,s$i)
		    
	    # set sperc_qlogfile(s$i,n$j) [open sperc_queuelog_s$i\_n$j.tr w]
	    # $queue(s$i,n$j) attach-sperc $sperc_qlogfile(s$i,n$j)

	    # $ns at 1 "$queue(n$j,s$i) start-timeout"	    
	    # $ns at 1 "$queue(s$i,n$j) start-timeout"	
	}

    }

    for {set i 0} {$i < $num_tors} {incr i} {
	for {set j 0} {$j < $num_spines} {incr j} {
	    $ns duplex-link $n($i) $a($j) [set tor_uplink_rate]Gb $MEAN_LINK_DELAY $switchAlg		    
	    set queue(n$i,a$j) [[$ns link $n($i) $a($j)] queue ]
	    set queue(a$j,n$i) [[$ns link $a($j) $n($i)] queue ]

	    set src [$n($i) id]
	    set dst [$a($j) id]
	    set delay [expr $MEAN_LINK_DELAY]
	    set l($num_links) queue(n$i,a$j)
	    puts "l($num_links) tor-agg queue(n$i,a$j) from n$i to a$j $delay s, $tor_uplink_rate Gb/s, queue_code  $src $dst"
	    incr num_links
	    set l($num_links) queue(a$j,n$i)
	    puts "l($num_links) agg-tor queue(a$j, n$i) from a$j to n$i $delay s, $tor_uplink_rate Gb/s, queue_code  $dst $src"
	    incr num_links


	    if { "$switchAlg" == "DropTail/RCP" } {
		$queue(a$j,n$i) set-link-capacity [expr [set tor_uplink_rate] * 125000000]
		$queue(n$i,a$j) set-link-capacity [expr [set tor_uplink_rate] * 125000000]    
	    }
	    if { "$switchAlg" == "DRR/SPERC" } {
		$ns at 0.100010 "$queue(n$i,a$j) start-timeout"	    
		$ns at 0.100010 "$queue(a$j,n$i) start-timeout"	    
	    }
	    if { "$switchAlg" == "Priority/SPERC" } {
		# $ns at 0.100010 "$queue(n$i,a$j) start-timeout"	    
		# $ns at 0.100010 "$queue(a$j,n$i) start-timeout"	    
		set val [$timer_delay value]
		set t0 [expr 1 + $val]
		$ns at $t0 "$queue(a$j,n$i) start-timeout"	  
		    
		puts "queue(a$j, n$i) starts timeout at $t0"

		set val [$timer_delay value]
		set t0 [expr 1 + $val]
		$ns at $t0 "$queue(n$i,a$j) start-timeout"
		    
		puts "queue(n$i, a$j) starts timeout at $t0"

		# just monitor links in LOG_QUEUES file
		# set sperc_qlogfile(a$j,n$i) [open sperc_queuelog_${dst}_${src}.tr w]
		# $queue(a$j,n$i) attach-sperc $sperc_qlogfile(a$j,n$i)
		    
		# set sperc_qlogfile(n$i,a$j) [open sperc_queuelog_${src}_${dst}.tr w]
		# $queue(n$i,a$j) attach-sperc $sperc_qlogfile(n$i,a$j)

	    }

	}
    }
}

proc monitorQueues {filename} {
    global ns nodeid_to_node nodeid_to_name queue QUEUE_SAMPLING_INTVAL switchAlg
    global common_alogfile

    set f [open $filename]
    puts "monitorQueues: opened $f"
    set config_list [split [read $f] "\n"]
    puts "monitorQueues: reading config list"

    foreach config $config_list {
	set tokens [split $config]
	set i 0
	
	foreach tok $tokens {
	    set tok_list($i) $tok
	    incr i
	}
	
	if {$i == 2} {
	    set egress_src $tok_list(0)
	    set egress_dst $tok_list(1)

	    set n1 $nodeid_to_name($egress_src)
	    set n2 $nodeid_to_name($egress_dst)
	    
	    set node1 $nodeid_to_node($egress_src)
	    set node2 $nodeid_to_node($egress_dst)

	    if {[info exists queue($n1,$n2)]} {

		set queue_inst $queue($n1,$n2)	    
		# logging sample updates at all links might be too much
	
		if { "$switchAlg" == "DropTail/RCP" } {
		    set qlogfile($n1,$n2) [open queuelog_$n1\_$n2.tr w]
		    $queue_inst attach $qlogfile($n1,$n2)
		    # 		if { $n1 == "s0" } {}
		}

		if { "$switchAlg" == "DRR/SPERC" } {
		    set sperc_qlogfile($n1,$n2) [open sperc_queuelog_$n1\_$n2.tr w]
		    $queue_inst attach-sperc $sperc_qlogfile($n1,$n2)
		}

		# commented out for final 100 runs of 18 x 10 Gb/s
		# if { "$switchAlg" == "Priority/SPERC" } {
		#     set sperc_qlogfile($n1,$n2) [open sperc_queuelog_$n1\_$n2.tr w]
		#     $queue_inst attach-sperc $sperc_qlogfile($n1,$n2)
		# }

		# logging sample updates at all links might be too much
		# set qfile($n1,$n2) [$ns monitor-queue $node1 $node2 [open queue_$n1\_$n2.tr w] $QUEUE_SAMPLING_INTVAL]
		# [$ns link $node1 $node2] queue-sample-timeout
	    
		#puts "monitoring queue($n1, $n2) = $queue_inst between node $node1 and $node2 given $egress_src $egress_dst"
	    }
	}
    }

}

proc monitorAgents {filename} {
    set common_alogfile [open agent_aggr_common.tr w]
    global agtagr alogfile agentPairType
    set f [open $filename]
    puts "monitorAgents: opened $f"
    set config_list [split [read $f] "\n"]
    puts "monitorAgents: reading config list"
    foreach config $config_list {
	set tokens [split $config]
	set i 0	
	foreach tok $tokens {
	    set tok_list($i) $tok
	    incr i
	}	

	if {$i == 2} {
	    set src $tok_list(0)
	    set dst $tok_list(1)
	    if {[info exists agtagr($src,$dst)]} {		
		#set alogfile($src,$dst) [open agent_aggr_$src\_$dst.tr w]
		if {[info exists alogfile($src)]} {
		} else {
		    set alogfile($src) [open agent_aggr_$src.tr w]
		}
		puts "attach tracefile $alogfile($src) for agents $src $dst"
		$agtagr($src,$dst) attach-tracefile $alogfile($src)

		# don't attach common tracefile, size goes upto 160 GB in WAN 4s experiments, intval ..
		# if {$agentPairType == "SPERC_pair"} {
		#     $agtagr($src,$dst) attach-common-tracefile $common_alogfile
		# }

	    } else {
		puts "can't attach tracefile for agents $src $dst, agents don't exist"
	    }
	}
    }
}


proc runPoisson {} {
    global flowlog init_fid agtagr ns tbf lambda CDF s num_servers SIM_END CONNECTIONS_PER_PAIR agentPairType TBFsPerServer meanNPackets PARETO_SHAPE FLOW_SIZE_DISTRIBUTION
    global NUM_ACTIVE_SERVERS common_alogfile EXPERIMENT
    global persistent_connections

    set S1 $NUM_ACTIVE_SERVERS; # so we use first NUM_ACTIVE_SERVERS/16 TORs only
    set common_alogfile [open agent_aggr_common.tr w]

    for {set j 0} {$j < $S1 } {incr j} {
	for {set i 0} {$i < $S1 } {incr i} {
	    if {$i != $j} {	    
		set agtagr($i,$j) [new Agent_Aggr_pair]
	
		set tbfindex 0
		if { [info exists TBFsPerServer] } {
		    set tbfindex [expr $j % $TBFsPerServer]
		}

		$agtagr($i,$j) setup $s($i) $s($j) [array get tbf] tbfindex "$i-$j" $CONNECTIONS_PER_PAIR $init_fid $agentPairType 0 0 $EXPERIMENT;
		
		$agtagr($i,$j) attach-logfile $flowlog
		$agtagr($i,$j) attach-common-tracefile $common_alogfile

		#puts -nonewline "($i,$j) "
		#For Poisson/Pareto
		# pc lambda and CDF for PC (poisson arrival, custom CDF)
		# pp mean_npkts and PARETO_SHAPE for PP (poisson arrival, pareto CDF)
		# pu mean_npkts for PU (poisson arrival, uniform CDF)

		if {$FLOW_SIZE_DISTRIBUTION == "pareto"} {
		    $agtagr($i,$j) set_PParrival_process [expr $lambda/($S1 - 1)] $meanNPackets $PARETO_SHAPE [expr 17*$i+1244*$j] [expr 33*$i+4369*$j]

		} else {
		    if {$FLOW_SIZE_DISTRIBUTION == "uniform"} {
			$agtagr($i,$j) set_PUarrival_process [expr $lambda/($S1 - 1)] $meanNPackets $meanNPackets [expr 17*$i+1244*$j] [expr 33*$i+4369*$j]
		    } else {
			if {$FLOW_SIZE_DISTRIBUTION == "cdf"} {
			    $agtagr($i,$j) set_PCarrival_process  [expr $lambda/($S1 - 1)] $CDF [expr 17*$i+1244*$j] [expr 33*$i+4369*$j]
			} else {
			    puts "unsupported $FLOW_SIZE_DISTRIBUTION"
			    exit
			}
		    }
		}

		# warmup not really useful for sperc
		if {$agentPairType == "TCP_pair"} {
		    if {$persistent_connections == 1} {
			$ns at 0.1 "$agtagr($i,$j) warmup 0.5 5"
		    }
		}

		# $ns at 0.1 "$agtagr($i,$j) warmup 0.5 5"
		# puts "calling init_schedule for agtagr($i,$j)"
		$ns at 1 "$agtagr($i,$j) init_schedule"	    		
		
		# this is moot? fid_ reset for each sendfile
		set init_fid [expr $init_fid + $CONNECTIONS_PER_PAIR];
	    }
	}
    }
}

# suppose we want 20 flows from 5-36 only
proc runFromFile {filename} {
    global flowlog init_fid agtagr ns SIM_END tbf s agentPairType TBFsPerServer
    global EXPERIMENT
    global common_alogfile
    global site_to_city
    global TOPOLOGY_TYPE

    set common_alogfile [open agent_aggr_common.tr w]

    set init_fid 1
    set SIM_END 0
    #set config_str "5 36 20 1.0,10 36 1 1.0"
    #set config_list [split $config_str ',']
    set f [open $filename]
    puts "runScheduleFromFile: opened $f"
    set config_list [split [read $f] "\n"]
    puts "runScheduleFromFile: reading config list"

    foreach config $config_list {
	set tokens [split $config]
	set i 0
	
	foreach tok $tokens {
	    set tok_list($i) $tok
	    incr i
	}


	if {$i < 5} {
	    puts "expected flow_id, num_bytes, time, src, .., dst, got only $i tokens"
	    #exit
	} else {
	    set dst_index [expr $i - 1]
	    set flow_id $tok_list(0)
	    set num_bytes $tok_list(1) 	    	
	    set t $tok_list(2)	    
	    set src $tok_list(3)
	    set dst $tok_list($dst_index)
	    puts "got $i tokens flow_id $flow_id ($num_bytes B) from $src to $dst at time $t"
	    


	    if {$num_bytes > 0} {
		if {[info exists agtagr($src,$dst)]} {		
		} else {
		    set agtagr($src,$dst) [new Agent_Aggr_pair]
		    # nr_flows 0 $init_fid ..
		    set nr_flows 0

		    set tbfindex 0
		    if { [info exists TBFsPerServer] } {
			set tbfindex [expr $j % $TBFsPerServer]
		    }

		    $agtagr($src,$dst) setup $s($src) $s($dst) [array get tbf] $tbfindex "$src-$dst" $nr_flows $init_fid $agentPairType 0 0 $EXPERIMENT;
		
		    
		    if { "$TOPOLOGY_TYPE" == "b4" } {
			if {[info exists site_to_city($src)]} {		
			    if {[info exists site_to_city($dst)]} {		
				set src_name s$site_to_city($src)
				set dst_name s$site_to_city($dst)
				puts "setting up agent_aggr pair between node s$src ($src_name) and node s$dst ($dst_name)"
				set src_node_id [$s($src) id]
				set dst_node_id [$s($dst) id]
				puts "will start flow $init_fid == $flow_id between $src (node id $src_node_id) and $dst (node id $dst_node_id) at $t"

			    } else {
				puts "cannot setup agent_aggr pair between node s$src ($src_name) and node s$dst: no such node s$dst"
				exit
			    }
			} else {
			    puts "cannot setup agent_aggr pair between node s$src and node s$dst: no such node s$src"
			    exit
			}
		    } else {
			if { "$TOPOLOGY_TYPE" == "spine-leaf" } {
			    puts "setting up agent_aggr pair between node s$src and node s$dst"
			    puts "will start flow $init_fid == $flow_id between $src and $dst  at $t"

			} else {
			    puts "setting up agent_aggr pair between node s$src and node s$dst but unrecognized topology type"
			}
		    }

		    # sperc only
		    $agtagr($src,$dst) attach-logfile $flowlog

		}
		# maybe assert that init_fid == flow_fid
		$ns at $t "$agtagr($src,$dst) ct_start $init_fid $num_bytes"
		set init_fid [expr $init_fid + 1];
		set SIM_END [expr $SIM_END + 1];
	    } else {
		puts "will stop flow $flow_id between $src and $dst at $t"
		$ns at $t "$agtagr($src,$dst) ct_stop $flow_id"
	    }

	}
    }
}
