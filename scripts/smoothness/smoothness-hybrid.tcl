set ns [new Simulator]

set nf [open smoothness-hybrid.nam w]
$ns namtrace-all $nf

set nd [open smoothness-hybrid.tr w]
$ns trace-all $nd

proc finish {} {
	global ns nf nd
	$ns flush-trace
	close $nf
	close $nd
	exit 0 }

set bottleneck [$ns node]

# create tcp nodes and agents
for {set i 0} {$i < 8} {incr i} {
	# source
	set tcp_src_n($i) [$ns node]	
	set tcp_src($i) [new Agent/TCP/Sack1]
	set tcp_traf($i) [new Application/FTP]
	$ns duplex-link $tcp_src_n($i) $bottleneck 50Mb 2ms DropTail
	$ns attach-agent $tcp_src_n($i) $tcp_src($i)
	$tcp_traf($i) attach-agent $tcp_src($i) 
	# sink
	set tcp_snk_n($i) [$ns node]
	$ns duplex-link $bottleneck $tcp_snk_n($i) 5Mb 20ms DropTail
	$ns queue-limit $bottleneck $tcp_snk_n($i) 300
	set tcp_snk($i) [new Agent/TCPSink]
	$ns attach-agent $tcp_snk_n($i) $tcp_snk($i)

	$ns connect $tcp_src($i) $tcp_snk($i)
}

# create 8 (hybrid/sctp) nodes
for {set i 0} {$i < 8} {incr i} {
	# source
	set sctp_src_n($i) [$ns node]
	set sctp_src($i) [new Agent/SCTP]
	set sctp_traf($i) [new Application/FTP]
	$ns duplex-link $sctp_src_n($i) $bottleneck 50Mb 2ms DropTail
	$ns attach-agent $sctp_src_n($i) $sctp_src($i)
	$sctp_traf($i) attach-agent $sctp_src($i) 
	# sink
	set sctp_snk_n($i) [$ns node]
	$ns duplex-link $bottleneck $sctp_snk_n($i) 5Mb 20ms DropTail
	$ns queue-limit $bottleneck $sctp_snk_n($i) 300
	set sctp_snk($i) [new Agent/SCTP]
	$ns attach-agent $sctp_snk_n($i) $sctp_snk($i)

	$ns connect $sctp_src($i) $sctp_snk($i)
}	

# create 2 udp nodes
for {set i 0} {$i < 2} {incr i} {
	# source
	set udp_src_n($i) [$ns node]
	set udp_src($i) [new Agent/UDP]
	set udp_traf($i) [new Application/Traffic/CBR]
	$ns duplex-link $udp_src_n($i) $bottleneck 50Mb 2ms DropTail
	$ns attach-agent $udp_src_n($i) $udp_src($i)
	$udp_traf($i) attach-agent $udp_src($i) 
	# sink
	set udp_snk_n($i) [$ns node]
	$ns duplex-link $bottleneck $udp_snk_n($i)  5Mb 20ms DropTail
	$ns queue-limit $bottleneck $udp_snk_n($i) 300
	set udp_snk($i) [new Agent/Null]
	$ns attach-agent $udp_snk_n($i) $udp_snk($i)

	$ns connect $udp_src($i) $udp_snk($i)
}
